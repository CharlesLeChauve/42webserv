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

void modifyPollFD(std::vector<pollfd>& poll_fds, int fd, short events, bool addEvent) {
    for (size_t i = 0; i < poll_fds.size(); ++i) {
        if (poll_fds[i].fd == fd) {
            if (addEvent) {
                poll_fds[i].events |= events; // Add the event(s)
            } else {
                poll_fds[i].events &= ~events; // Remove the event(s)
                if (poll_fds[i].events == 0) {
                    // If no events are left, you may choose to remove the fd
                    poll_fds.erase(poll_fds.begin() + i);
                }
            }
            return;
        }
    }
    if (addEvent) {
        // FD not found; add a new pollfd entry
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = events;
        pfd.revents = 0;
        poll_fds.push_back(pfd);
    }
}


enum FDType { FD_SERVER_SOCKET, FD_CLIENT_SOCKET, FD_CGI_INPUT, FD_CGI_OUTPUT, FD_UNKNOWN };

FDType getFDType(int fd, const std::map<int, Server*>& fdToServerMap, const std::map<int, ClientConnection>& connections) {
    if (fdToServerMap.find(fd) != fdToServerMap.end()) {
        return FD_SERVER_SOCKET;
    }
    if (connections.find(fd) != connections.end()) {
        return FD_CLIENT_SOCKET;
    }
    for (std::map<int, ClientConnection>::const_iterator it = connections.begin(); it != connections.end(); ++it) {
        CGIHandler* cgiHandler = it->second.getCgiHandler();
        if (cgiHandler) {
            if (cgiHandler->getInputPipeFd() == fd) {
                return FD_CGI_INPUT;
            }
            if (cgiHandler->getOutputPipeFd() == fd) {
                return FD_CGI_OUTPUT;
            }
        }
    }
    return FD_UNKNOWN;
}

// Définition du functoOor
struct MatchFD {
    int fd;
    MatchFD(int f) : fd(f) {}
    bool operator()(const pollfd& pfd) const {
        return pfd.fd == fd;
    }
};

struct MatchCGIOutputFD {
    int fd;
    MatchCGIOutputFD(int f) : fd(f) {}
    bool operator()(const std::pair<const int, ClientConnection>& pair) const {
            CGIHandler* handler = pair.second.getCgiHandler();
            if (!handler) {
                return false;
            }
            int handler_fd = handler->getOutputPipeFd();
            return handler_fd == fd;
        }

};

struct MatchCGIInputFD {
    int fd;
    MatchCGIInputFD(int f) : fd(f) {}
    bool operator()(const std::pair<const int, ClientConnection>& pair) const {
            CGIHandler* handler = pair.second.getCgiHandler();
            if (!handler) {
                return false;
            }
            int handler_fd = handler->getInputPipeFd();
            return handler_fd == fd;
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

void manageConnections(std::map<int, ClientConnection>& connections, std::vector<pollfd>& poll_fds) {
    std::map<int, ClientConnection>::iterator it_conn;
    for (it_conn = connections.begin(); it_conn != connections.end(); ) {
        int client_fd = it_conn->first;
        HTTPRequest* request = it_conn->second.getRequest();
        ClientConnection& connection = it_conn->second;

        if (connection.getExchangeOver()) {
            connection.resetConnection();
            for (size_t i = 0; i < poll_fds.size(); ++i) {
                if (poll_fds[i].fd == client_fd) {
                    poll_fds[i].events &= ~POLLOUT;
                    poll_fds[i].events |= POLLIN;
                    break;
                }
            }
            continue;
        }
        if (connection.getCgiHandler()) {
    // Vérifier d'abord le timeout
    if (connection.getCgiHandler()->hasTimedOut()) {
        connection.getCgiHandler()->terminateCGI();
        HTTPResponse* cgiResponse = new HTTPResponse();
        cgiResponse->beError(504, "CGI script timed out");
		cgiResponse->setHeader("Connection", "close");
        connection.setResponse(cgiResponse);
        connection.prepareResponse();
		connection.setExchangeOver(true);
		close(client_fd);
        // On passe au suivant
        ++it_conn;
        continue;
    }

    int cgiStatus = connection.getCgiHandler()->isCgiDone();
    if (cgiStatus) {
        // Le CGI s'est terminé, mais peut ne pas avoir produit de sortie
        std::string cgiOutput = connection.getCgiHandler()->getCGIOutput();
        HTTPResponse* cgiResponse = new HTTPResponse();
        cgiResponse->parseCGIOutput(cgiOutput);

        // Si aucune en-tête CGI n'a été trouvée, parseCGIOutput mettra le corps brut.
        // Si cgiOutput est vide, renvoyer une page d'erreur minimaliste
        if (cgiOutput.empty()) {
            cgiResponse->beError(500, "No output from CGI script");
        }
		cgiResponse->setHeader("Connection", "close");
        connection.setResponse(cgiResponse);
        connection.prepareResponse();
    }

    ++it_conn;
    continue;
}


        //?? Bon en fait on ferme toujours la connexion, mais ca casse tout si je commente ca
        if (connection.getResponse() != NULL && connection.isResponseComplete()) {
			connection.setExchangeOver(true);
            close(client_fd);
            poll_fds.erase(
                std::remove_if(poll_fds.begin(), poll_fds.end(), MatchFD(client_fd)),
                poll_fds.end()
            );
            connections.erase(it_conn++);
            continue;
        }

        if (connection.getResponse() != NULL) {
            for (size_t i = 0; i < poll_fds.size(); ++i) {
                if (poll_fds[i].fd == client_fd) {
                    poll_fds[i].events |= POLLOUT;
                    break;
                }
            }
            ++it_conn;
            continue;
        }

        if (request && request->isComplete() && request->getErrorCode() == 0 && !connection.getCgiHandler()) {
            Logger::instance().log(INFO, "Parsing OK, handling request for client fd: " + to_string(client_fd));
            connection.getServer()->handleHttpRequest(client_fd, connection);

            if (connection.getResponse() != NULL) {
                connection.prepareResponse();

                // Ensure POLLOUT is enabled
                for (size_t i = 0; i < poll_fds.size(); ++i) {
                    if (poll_fds[i].fd == client_fd) {
                        poll_fds[i].events |= POLLOUT;
                        break;
                    }
                }
            } else if (connection.getCgiHandler()) {
                int cgi_input_fd = connection.getCgiHandler()->getInputPipeFd();
                if (cgi_input_fd != -1) {
                    pollfd cgiInput;
                    cgiInput.fd = cgi_input_fd;
                    cgiInput.events = POLLOUT;
                    cgiInput.revents = 0;
                    poll_fds.push_back(cgiInput);

                    for (size_t i = 0; i < poll_fds.size(); ++i) {
                        if (poll_fds[i].fd == client_fd) {
                            poll_fds[i].events = 0;
                        }
                    }
                }
            } else {
                Logger::instance().log(ERROR, "No response or CGI handler after handleHttpRequest");
            }
            ++it_conn;
            continue;
        }

        Logger::instance().log(DEBUG, "A connection didn't match any condition in manageConnections");
        ++it_conn;
    }
}


int manageTimeouts(std::map<int, ClientConnection>& connections, std::vector<pollfd>& poll_fds) {
    unsigned long now = curr_time_ms();
    unsigned long min_remaining_time = TIMEOUT_MS;
    bool has_active_connections = false;

    std::map<int, ClientConnection>::iterator it_conn;
    for (it_conn = connections.begin(); it_conn != connections.end(); ) {
        int client_fd = it_conn->first;
        HTTPRequest* request = it_conn->second.getRequest();
        if (!request)
            continue;
        ClientConnection& connection = it_conn->second;
        unsigned long time_since_last_activity = now - request->getLastActivity();

        if (!request->isComplete() && time_since_last_activity >= TIMEOUT_MS) {
            Logger::instance().log(INFO, "Connection timed out for client FD: " + to_string(client_fd));

            HTTPResponse* timeoutResponse = new HTTPResponse();
            timeoutResponse->beError(408); // Request Timeout
            connection.setResponse(timeoutResponse);
            connection.prepareResponse();

            for (size_t i = 0; i < poll_fds.size(); ++i) {
                if (poll_fds[i].fd == client_fd) {
                    poll_fds[i].events |= POLLOUT;
                    break;
                }
            }
            request->setLastActivity(now);
            ++it_conn;
            continue;
        } else if (!request->isComplete()) {
            // Mettre à jour le temps restant et continuer
            unsigned long remaining_time = TIMEOUT_MS - time_since_last_activity;
            if (remaining_time < min_remaining_time) {
                min_remaining_time = remaining_time;
            }
            has_active_connections = true;
            ++it_conn;
        } else {
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
    servers.push_back(server);

        for (size_t j = 0; j < serverConfigs[i].ports.size(); ++j) {
            int port = serverConfigs[i].ports[j];
            Socket* socket = new Socket(port);
            socket->build_sockets();

            pollfd pfd;
            pfd.fd = socket->getSocket();
            pfd.events = POLLIN;
            pfd.revents = 0; // Initialize revents to 0
            poll_fds.push_back(pfd);

            // Associer les sockets serveurs avec les serveurs
            fdToServerMap[socket->getSocket()] = server;

            sockets.push_back(socket);

            Logger::instance().log(INFO, "Server launched, listening on port: " + to_string(port));
        }
    }


    while (!stopServer) {
        manageConnections(connections, poll_fds);
        int poll_timeout = manageTimeouts(connections, poll_fds);

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

            FDType fdType = getFDType(poll_fds[i].fd, fdToServerMap, connections);

            if (fdType == FD_UNKNOWN)
                Logger::instance().log(DEBUG, std::string("Unknown FD type sent by poll, fd = ") + to_string(poll_fds[i].fd));

            // Gérer les erreurs
            if (poll_fds[i].revents & POLLERR) {
                Logger::instance().log(ERROR, "Error on file descriptor: " + to_string(poll_fds[i].fd));
                if (fdToServerMap.find(poll_fds[i].fd) != fdToServerMap.end()) {
                    // C'est un socket serveur
                    Logger::instance().log(ERROR, "Error on server socket detected in poll");
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
                if (fdType == FD_CLIENT_SOCKET) {
                    Logger::instance().log(INFO, "Disconnected client FD: " + to_string(poll_fds[i].fd));
                    close(poll_fds[i].fd);
                    connections.erase(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                    --i;
                } else if (fdType == FD_CGI_OUTPUT) {

                    // Trouver la connexion associée
                    std::map<int, ClientConnection>::iterator it = std::find_if(
                        connections.begin(),
                        connections.end(),
                        MatchCGIOutputFD(poll_fds[i].fd)
                    );
                    if (it != connections.end()) {
                        ClientConnection& connection = it->second;
                        connection.getCgiHandler()->readFromCGI();
                        // Fermer le descripteur de sortie du pipe
                        connection.getCgiHandler()->closeOutputPipe();
                        poll_fds.erase(poll_fds.begin() + i);
                        --i;

                        std::string cgiOutput = connection.getCgiHandler()->getCGIOutput();
                        HTTPResponse* cgiResponse = new HTTPResponse();
                        cgiResponse->parseCGIOutput(cgiOutput);

						cgiResponse->setHeader("Connection", "close");
                        connection.setResponse(cgiResponse);
                        connection.prepareResponse();

                        for (size_t j = 0; j < poll_fds.size(); ++j) {
                            if (poll_fds[j].fd == it->first) {
                                poll_fds[j].events |= POLLOUT;
                                break;
                            }
                        }
                    }
                }
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
                if (fdType == FD_SERVER_SOCKET) {
				    Logger::instance().log(DEBUG, std::string("POLLIN on server socket, new connection will be created for fd : ") + to_string(poll_fds[i].fd));
				    Server* server = fdToServerMap[poll_fds[i].fd];
				    int client_fd = server->acceptNewClient(poll_fds[i].fd);

				    if (client_fd != -1) {
				        // Enregistrer l'association client_fd -> server
				        std::pair<std::map<int, ClientConnection>::iterator, bool> result =
				            connections.insert(std::make_pair(client_fd, ClientConnection(server)));

				        // Créer un nouvel objet HTTPRequest et le stocker dans la connexion
				        int max_body_size = server->getConfig().clientMaxBodySize;
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
				    continue;
				} else if (fdType == FD_CLIENT_SOCKET) {
                    std::map<int, ClientConnection>::iterator conn_it = connections.find(poll_fds[i].fd);
                    if (conn_it != connections.end()) {
                        ClientConnection& connection = conn_it->second;
                        Server* server = connection.getServer();
                        server->handleClient(poll_fds[i].fd, connection);
                    }
                    continue;
                } else if (fdType == FD_CGI_OUTPUT) {
                        std::map<int, ClientConnection>::iterator it = std::find_if(
                        connections.begin(),
                        connections.end(),
                        MatchCGIOutputFD(poll_fds[i].fd)
                    );
                    ClientConnection& connection = it->second;
                    CGIHandler* cgiHandler = connection.getCgiHandler();
                    if (cgiHandler->hasTimedOut()) {
                        cgiHandler->terminateCGI();
                        connection.setResponse(new HTTPResponse());
                        connection.getResponse()->beError(504);
                        connection.prepareResponse();
                    } else {
                        int received = cgiHandler->readFromCGI();
                        if (!received) {
                            poll_fds.erase(poll_fds.begin() + i);
                            --i;

                            std::string cgiOutput = cgiHandler->getCGIOutput();
                            HTTPResponse* cgiResponse = new HTTPResponse();
                            cgiResponse->parseCGIOutput(cgiOutput);
							cgiResponse->setHeader("Connection", "close");
                            connection.setResponse(cgiResponse);
                            connection.prepareResponse();

                            // Activer POLLOUT sur le descripteur du client pour envoyer la réponse
                            for (size_t j = 0; j < poll_fds.size(); ++j) {
                                if (poll_fds[j].fd == it->first) {  // it->first est le fd du client associé à cette connexion
                                    poll_fds[j].events |= POLLOUT;
                                    break;
                                }
                            }
                            continue;
                        }
                    }
                } else {
                    Logger::instance().log(WARNING, std::string("Unhandled POLLIN event on fd : ") + to_string(poll_fds[i].fd));
                }
            }
            if (poll_fds[i].revents & POLLOUT) {
                // C'est un socket prêt à écrire
                if (fdType == FD_CLIENT_SOCKET) {
                    std::map<int, ClientConnection>::iterator conn_it = connections.find(poll_fds[i].fd);
                    ClientConnection& connection = conn_it->second;
                    Server* server = connection.getServer();
                    server->handleResponseSending(poll_fds[i].fd, conn_it->second);
                } else if (fdType == FD_CGI_INPUT) {
                    std::map<int, ClientConnection>::iterator it = std::find_if(
                        connections.begin(),
                        connections.end(),
                        MatchCGIInputFD(poll_fds[i].fd)
                    );
                    ClientConnection& connection = it->second;
                    int sending = connection.getCgiHandler()->writeToCGI();
                    if (!sending) {
                        poll_fds[i].events &= ~POLLOUT;
                        close(poll_fds[i].fd);
                        poll_fds.erase(poll_fds.begin() + i);
                        --i;

                        int cgi_output_fd = connection.getCgiHandler()->getOutputPipeFd();
                        if (cgi_output_fd != -1) {
                            pollfd cgiOutput;
                            cgiOutput.fd = cgi_output_fd;
                            cgiOutput.events = POLLIN;
                            cgiOutput.revents = 0;
                            poll_fds.push_back(cgiOutput);
                        }
                        continue;
                    }
                } else {
                    Logger::instance().log(WARNING, std::string("Unhandled POLLOUT event on fd : ") + to_string(poll_fds[i].fd));
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
