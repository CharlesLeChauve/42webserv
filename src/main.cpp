// main.cpp

#include "ConfigParser.hpp"
#include "Socket.hpp"
#include "Server.hpp"
#include "Utils.hpp"
#include <iostream>
#include "Logger.hpp"
#include "ServerConfig.hpp"
#include "SessionManager.hpp"
#include <poll.h>
#include <unistd.h>
#include <ctime>
#include <signal.h>
#include <map>
#include "ClientConnection.hpp"

// Définition du functoOor
struct MatchFD {
    int fd;
    MatchFD(int f) : fd(f) {}
    bool operator()(const pollfd& pfd) const {
        return pfd.fd == fd;
    }
};

void initialize_random_generator() {
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    unsigned int seed;
    if (urandom.is_open()) {
        urandom.read(reinterpret_cast<char*>(&seed), sizeof(seed));
        urandom.close();
    } else {
        seed = std::time(0);
    }
    srand(seed);
}

int manageConnections(std::map<int, ClientConnection>& connections, std::vector<pollfd>& poll_fds) {
    unsigned long now = curr_time_ms();
    unsigned long min_remaining_time = TIMEOUT_MS;
    bool has_active_connections = false;

    // Vérifications des timeout
    std::map<int, ClientConnection>::iterator it_conn;
    for (it_conn = connections.begin(); it_conn != connections.end(); /* pas d'incrément ici */) {
        int client_fd = it_conn->first;
        HTTPRequest* request = it_conn->second.getRequest();
        ClientConnection& connection = it_conn->second;

        if (connection.getResponse() != NULL && connection.isResponseComplete()) {
            // La réponse a été envoyée complètement, on peut fermer la connexion
            close(client_fd);
            poll_fds.erase(
                std::remove_if(poll_fds.begin(), poll_fds.end(), MatchFD(client_fd)),
                poll_fds.end()
            );
            connections.erase(it_conn++);
            continue;
        }

        if (connection.getResponse() != NULL) {
            // Il y a une réponse à envoyer, s'assurer que POLLOUT est activé
            for (size_t i = 0; i < poll_fds.size(); ++i) {
                if (poll_fds[i].fd == client_fd) {
                    poll_fds[i].events |= POLLOUT;
                    break;
                }
            }
            ++it_conn;
            continue;
        }

        if (request->isComplete() && request->getErrorCode() == 0) {

            connection.setResponse(new HTTPResponse());
            SessionManager  session(request->getStrHeader("Cookie"));
            session.getManager(request, connection.getResponse(), client_fd, session);
            Logger::instance().log(INFO, "Parsing OK, handling request for client fd: " + to_string(client_fd));
            connection.getServer()->handleHttpRequest(client_fd, connection);

            // Préparer la réponse pour l'envoi
            connection.prepareResponse();

            // S'assurer que POLLOUT est activé pour ce client
            for (size_t i = 0; i < poll_fds.size(); ++i) {
                if (poll_fds[i].fd == client_fd) {
                    poll_fds[i].events |= POLLOUT;
                    break;
                }
            }

            ++it_conn;
            continue;
        }

        unsigned long time_since_last_activity = now - request->getLastActivity();

        if (time_since_last_activity >= TIMEOUT_MS) {
            // Connexion expirée
            Logger::instance().log(INFO, "Connection timed out for client FD: " + to_string(client_fd));

            // Créer une réponse d'erreur de timeout
            HTTPResponse* timeoutResponse = new HTTPResponse();
            timeoutResponse->beError(408); // Request Timeout
            connection.setResponse(timeoutResponse);
            connection.prepareResponse();

            // S'assurer que POLLOUT est activé pour ce client
            for (size_t i = 0; i < poll_fds.size(); ++i) {
                if (poll_fds[i].fd == client_fd) {
                    poll_fds[i].events |= POLLOUT;
                    break;
                }
            }

            // Mettre à jour le temps de dernière activité pour éviter une nouvelle expiration immédiate
            request->setLastActivity(now);

            ++it_conn;
            continue;
        } else {
            // Mettre à jour le temps restant et continuer
            unsigned long remaining_time = TIMEOUT_MS - time_since_last_activity;
            if (remaining_time < min_remaining_time) {
                min_remaining_time = remaining_time;
            }
            has_active_connections = true;
            ++it_conn;
        }
    }

    int poll_timeout;
    if (has_active_connections) {
        poll_timeout = static_cast<int>(min_remaining_time);
    } else {
        poll_timeout = -1; // Bloquer indéfiniment si aucune connexion active
    }
    return poll_timeout;
}


int main(int argc, char* argv[]) {
    Logger::instance().log(INFO, "Starting main");
    bool stopServer = false;

    std::string configFile;
    if (argc > 3) {
        std::cerr << "Usage: " << argv[0] << " [path/to/file] [-m (mute logger)]" << std::endl;
        return 1;
    } else {
        if (argc == 1 || (argc == 2 && argv[1][0] == '-')) {
            configFile = "config/server.conf";
            Logger::instance().log(DEBUG, "Default configuration file loaded : " + configFile);
        } else {
            configFile = argv[1];
            Logger::instance().log(DEBUG, "Custom configuration file loaded : " + configFile);
        }
        if ((argc == 3 && std::string(argv[2]) == "-m") || (argc == 2 && std::string(argv[1]) == "-m")) {
            Logger& logger = Logger::instance();
            logger.setMute(true);
        }
    }

    initialize_random_generator();

    ConfigParser configParser;
    try {
        configParser.parseConfigFile(configFile);
        Logger::instance().log(DEBUG, "Config file successfully parsed");
    } catch (const ConfigParserException& e) {
        Logger::instance().log(ERROR, std::string("Failure in configuration parsing: ") + e.what());
        return 1;
    }

    const std::vector<ServerConfig>& serverConfigs = configParser.getServerConfigs();
    Logger::instance().log(INFO, to_string(serverConfigs.size()) + " servers successfully configured");

    if (pipe(serverSignal::pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // Configuration du signal handler
    struct sigaction sa;
    sa.sa_handler = serverSignal::signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    std::map<int, ClientConnection> connections;

    std::vector<Server*> servers;
    std::vector<Socket*> sockets;
    std::vector<pollfd> poll_fds;
    std::map<int, Server*> fdToServerMap;

    // Ajouter le descripteur du pipe à poll_fds pour pouvoir détecter le signal d'arrêt
    {
        pollfd pfd;
        pfd.fd = serverSignal::pipe_fd[0];
        pfd.events = POLLIN;
        pfd.revents = 0; // Initialize revents to 0
        poll_fds.push_back(pfd);
    }

    // Créer les serveurs et les sockets
    for (size_t i = 0; i < serverConfigs.size(); ++i) {
        Server* server = new Server(serverConfigs[i]);
        Socket* socket = new Socket(serverConfigs[i].ports[0]);
        socket->build_sockets();

        pollfd pfd;
        pfd.fd = socket->getSocket();
        pfd.events = POLLIN;
        pfd.revents = 0; // Initialize revents to 0
        poll_fds.push_back(pfd);

        // Associer les sockets serveurs avec les serveurs
        fdToServerMap[socket->getSocket()] = server;

        servers.push_back(server);
        sockets.push_back(socket);

        Logger::instance().log(INFO, "Server launched, listening on port: " + to_string(serverConfigs[i].ports[0]));
    }

    while (!stopServer) {
        int poll_timeout = manageConnections(connections, poll_fds);

        int poll_count = poll(&poll_fds[0], poll_fds.size(), poll_timeout);

        if (poll_count < 0) {
            if (errno == EINTR) {
                // poll() a été interrompu par un signal, continuer la boucle
                continue;
            } else {
                perror("Error calling poll");
                break; // Ou gérer l'erreur de manière appropriée
            }
        }

        for (size_t i = 0; i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents == 0)
                continue;

            if (poll_fds[i].fd == serverSignal::pipe_fd[0]) {
                if (poll_fds[i].revents & POLLIN) {
                    // Lire le(s) octet(s) du pipe pour vider le buffer
                    uint8_t byte;
                    ssize_t bytesRead = read(serverSignal::pipe_fd[0], &byte, sizeof(byte));
                    if (bytesRead > 0) {
                        Logger::instance().log(INFO, "Signal received, stopping the server...");
                        stopServer = true;
                        break;
                    }
                }
                continue;
            }

            // Gérer les erreurs
            if (poll_fds[i].revents & POLLERR) {
                Logger::instance().log(ERROR, "Error on file descriptor: " + to_string(poll_fds[i].fd));
                if (fdToServerMap.find(poll_fds[i].fd) != fdToServerMap.end()) {
                    // C'est un socket serveur
                    Logger::instance().log(ERROR, "Error on server socket detected in poll");
                    // Décider comment gérer les erreurs des sockets serveurs
                } else {
                    // C'est un socket client
                    Logger::instance().log(ERROR, "Error on client socket detected in poll");
                    close(poll_fds[i].fd);
                    connections.erase(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                    --i;
                }
                continue;
            }

            // Gérer les déconnexions
            if (poll_fds[i].revents & POLLHUP) {
                Logger::instance().log(INFO, "Disconnected client FD: " + to_string(poll_fds[i].fd));
                close(poll_fds[i].fd);
                connections.erase(poll_fds[i].fd);
                poll_fds.erase(poll_fds.begin() + i);
                --i;
                continue;
            }

            if (poll_fds[i].revents & POLLNVAL) {
                Logger::instance().log(ERROR, "File descriptor not valid: " + to_string(poll_fds[i].fd));
                // Supprimer le descripteur invalide
                poll_fds.erase(poll_fds.begin() + i);
                --i;
                continue;
            }

            // Gérer POLLIN et POLLOUT
            if (poll_fds[i].revents & POLLIN) {
                if (fdToServerMap.find(poll_fds[i].fd) != fdToServerMap.end()) {
                    // C'est un socket serveur, accepter une nouvelle connexion
                    Server* server = fdToServerMap[poll_fds[i].fd];
                    int client_fd = server->acceptNewClient(poll_fds[i].fd);

                    if (client_fd != -1) {
                        // Enregistrer l'association client_fd -> server
                        std::pair<std::map<int, ClientConnection>::iterator, bool> result =
                            connections.insert(std::make_pair(client_fd, ClientConnection(server)));

                        // Créer un nouvel objet HTTPRequest et le stocker dans la connexion
                        int max_body_size = serverConfigs[0].clientMaxBodySize;
                        std::map<int, ClientConnection>::iterator conn_it = result.first;

                        conn_it->second.setRequest(new HTTPRequest(max_body_size));

                        pollfd client_pollfd;
                        client_pollfd.fd = client_fd;
                        client_pollfd.events = POLLIN | POLLHUP | POLLERR;
                        client_pollfd.revents = 0;
                        poll_fds.push_back(client_pollfd);
                        Logger::instance().log(DEBUG, "New pollfd added for client with FD: " + to_string(client_fd) + " accepted on server FD: " + to_string(poll_fds[i].fd));
                    } else {
                        Logger::instance().log(ERROR, "Failure accepting client on server FD: " + to_string(poll_fds[i].fd));
                    }
                } else {
                    // C'est un socket client, lire la requête
                    Server* server = NULL;
                    ClientConnection* connection = NULL;
                    std::map<int, ClientConnection>::iterator conn_it = connections.find(poll_fds[i].fd);
                    if (conn_it != connections.end()) {
                        server = conn_it->second.getServer();
                        connection = &conn_it->second;
                    }
                    if (server != NULL && connection != NULL) {
                        server->handleClient(poll_fds[i].fd, *connection);

                        // Préparer l'envoi de la réponse si nécessaire
                        if (connection->getRequest()->isComplete()) {
                            connection->prepareResponse();
                            // Ajouter POLLOUT pour ce socket
                            poll_fds[i].events |= POLLOUT;
                        }
                    }

                    // Vérifier si la requête est complète ou la connexion est fermée
                    // // Ne pas vérifier ici en fait, la réponse n'est plus censée etre prete
                    // if (connection != NULL && (connection->getRequest()->isComplete() || connection->getRequest()->getConnectionClosed() || connection->getRequest()->getRequestTooLarge())) {
                    //     // Si une réponse est prête à être envoyée, l'envoi sera géré dans POLLOUT
                    //     if (connection->getResponse() != NULL) {
                    //         // Ne pas fermer immédiatement, attendre que POLLOUT soit traité
                    //     } else {
                    //         // Fermer la connexion si aucune réponse n'est prête
                    //         close(poll_fds[i].fd);
                    //         connections.erase(poll_fds[i].fd);
                    //         poll_fds.erase(poll_fds.begin() + i);
                    //         --i;
                    //     }
                    // }
                }
            }

            if (poll_fds[i].revents & POLLOUT) {
                // C'est un socket prêt à écrire
                std::map<int, ClientConnection>::iterator conn_it = connections.find(poll_fds[i].fd);
                if (conn_it != connections.end()) {
                    Server* server = conn_it->second.getServer();
                    server->handleResponseSending(poll_fds[i].fd, conn_it->second);
                    if (conn_it->second.isResponseComplete()) {
                        // Supprimer POLLOUT de ce socket
                        poll_fds[i].events &= ~POLLOUT;
                        // Fermer la connexion
                        close(poll_fds[i].fd);
                        connections.erase(conn_it);
                        poll_fds.erase(poll_fds.begin() + i);
                        --i;
                        continue;
                    }
                }
            }

        }

        if (stopServer)
            break;
    }

    // Nettoyer les objets HTTPRequest restants
    connections.clear();

    // Nettoyer la mémoire
    for (size_t i = 0; i < servers.size(); ++i) {
        delete servers[i];
        delete sockets[i];
    }

    return 0;
}
