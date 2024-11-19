// main.cpp

#include "ConfigParser.hpp"
#include "Socket.hpp"
#include "Server.hpp"
#include "Utils.hpp"
#include <iostream>
#include "Logger.hpp"
#include "ServerConfig.hpp"
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

    /*std::map<int client_fd, Exchange class;
            OR
    std::vector<std::pair<int client_fd, Exchange class> >; 

    Exchange will contain : 
        - Server pointer (wich configured server will handle this exchange)
        - Request (request send by client, read by pollin)
        - Response (to form after the request is received)
        - Client FD ? (Either in the class or as first member of pair or map, to identify th struct. What-'s the better choice ? What are th benefits ?)

    */

    std::map<int, ClientConnection> connections;

    std::vector<Server*> servers;
    std::vector<Socket*> sockets;
    std::vector<pollfd> poll_fds;
    std::map<int, Server*> fdToServerMap;

    //Add a POLLIN pipe to poll_fd so poll can receive it and stop;
    {
        pollfd pfd;
        pfd.fd = serverSignal::pipe_fd[0];
        pfd.events = POLLIN;
        pfd.revents = 0; // Initialize revents to 0
        poll_fds.push_back(pfd);
    }

    // Create servers and sockets
    for (size_t i = 0; i < serverConfigs.size(); ++i) {
        Server* server = new Server(serverConfigs[i]);
        Socket* socket = new Socket(serverConfigs[i].ports[0]);
        socket->build_sockets();

        pollfd pfd;
        pfd.fd = socket->getSocket();
        pfd.events = POLLIN;
        pfd.revents = 0; // Initialize revents to 0
        poll_fds.push_back(pfd);

        // Associate server sockets with servers
        fdToServerMap[socket->getSocket()] = server;

        servers.push_back(server);
        sockets.push_back(socket);

        Logger::instance().log(INFO, "Server launched, listening on port: " + to_string(serverConfigs[i].ports[0]));
    }

    while (!stopServer) {
        unsigned long now = curr_time_ms();
        std::vector<int> timed_out_fds;
        unsigned long min_remaining_time = TIMEOUT_MS;
        bool has_active_connections = false;

        //Timeout checks
        std::map<int, ClientConnection>::iterator it;
        for (it = connections.begin(); it != connections.end(); /* no increment here */) {
            int client_fd = it->first;
            HTTPRequest* request = (it->second).getRequest();

            unsigned long time_since_last_activity = now - request->getLastActivity();

            if (time_since_last_activity >= TIMEOUT_MS) {
                // Connection has timed out
                Logger::instance().log(INFO, "Connection timed out for client FD: " + to_string(client_fd));

                // Close the connection and clean up

                std::map<int, ClientConnection>::iterator conn_it = connections.find(client_fd);
                if (conn_it != connections.end()) {
                    Server *server = conn_it->second.getServer();
                    server->sendErrorResponse(client_fd, 408);
                    close(client_fd);
                    poll_fds.erase(
                        std::remove_if(poll_fds.begin(), poll_fds.end(), MatchFD(client_fd)),
                        poll_fds.end()
                    );
                    connections.erase(conn_it);  // Supprime le client en timeout // Redémarrer l'itération depuis le début
                } 
            } else {
                // Mise à jour du temps restant et continuation de l'itération
                unsigned long remaining_time = TIMEOUT_MS - time_since_last_activity;
                if (remaining_time < min_remaining_time) {
                    min_remaining_time = remaining_time;
                }
                has_active_connections = true;
                ++it;  // Incrément de l'itérateur
            }
        }

        //?? Je serais surement obligé de gérer ici aussi le gateway timeout
        // Call poll()
        int poll_timeout;
        if (has_active_connections) {
            poll_timeout = static_cast<int>(min_remaining_time);
        } else {
            poll_timeout = -1; // Bloquer indéfiniment si aucune Connection active
        }
        int poll_count = poll(&poll_fds[0], poll_fds.size(), poll_timeout); 
        // Ici on attend le temps le plus court avant expiration d'une des requetes. Si une requête timeout dans 500ms, on dit a poll d'attendre au maximum 500ms
        // Comme ca on sort du poll, on coupe la Connection avec le client expiré et on peut relancer une boucle poll avec le nouveau temps le plus court avant time out d'une requete 
        if (poll_count < 0) {
            if (errno == EINTR) {
                // poll() was interrupted by a signal, continue the loop
                continue;
            } else {
                perror("Error calling poll");
                break; // Or handle the error appropriately
            }
        }

        for (size_t i = 0; i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents == 0)
                continue;

            if (poll_fds[i].fd == serverSignal::pipe_fd[0]) {
                if (poll_fds[i].revents & POLLIN) {
                    // Read the byte(s) from the pipe to clear the buffer
                    uint8_t byte;
                    ssize_t bytesRead = read(serverSignal::pipe_fd[0], &byte, sizeof(byte));
                    if (bytesRead > 0) {
                        // You can set a variable to properly stop the server
                        Logger::instance().log(INFO, "Signal received, stopping the server...");
                        stopServer = true;
                        break;
                    }
                }
                continue;
            }

            // Handle errors
            if (poll_fds[i].revents & POLLERR) {
                Logger::instance().log(ERROR, "Error on file descriptor: " + to_string(poll_fds[i].fd));
                if (fdToServerMap.find(poll_fds[i].fd) != fdToServerMap.end()) {
                    // It's a server socket
                    Logger::instance().log(ERROR, "Error on server socket detected in poll");
                    // Decide how to handle server socket errors
                } else {
                    // It's a client socket
                    Logger::instance().log(ERROR, "Error on client socket detected in poll");
                    close(poll_fds[i].fd);
                    connections.erase(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                    --i;
                }
                continue;
            }

            // Handle disconnections
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
                // Remove the invalid descriptor
                poll_fds.erase(poll_fds.begin() + i);
                --i;
                continue;
            }

            if (poll_fds[i].revents & POLLIN) {
                if (fdToServerMap.find(poll_fds[i].fd) != fdToServerMap.end()) {
                    // It's a server socket descriptor, accept a new connection
                    Server* server = fdToServerMap[poll_fds[i].fd];
                    int client_fd = server->acceptNewClient(poll_fds[i].fd);

                    if (client_fd != -1) {
                        connections.insert(std::make_pair(client_fd, ClientConnection(server))); // Register the client_fd -> server association

                        // Create a new HTTPRequest object and store it in the map
                        int max_body_size = serverConfigs[0].clientMaxBodySize;
                        std::map<int, ClientConnection>::iterator conn_it = connections.find(client_fd);
                        if (conn_it == connections.end()) {
                            // Insertion uniquement si `client_fd` n'existe pas
                            std::pair<std::map<int, ClientConnection>::iterator, bool> result =
                                connections.insert(std::make_pair(client_fd, ClientConnection(server)));

                            // Accès à l'itérateur nouvellement inséré
                            conn_it = result.first;
                        }

                        // Maintenant, `conn_it` est garanti d'être valide et on peut l'utiliser
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
                    // It's a client socket descriptor, handle the request
                    Server* server = NULL;
                    HTTPRequest* request = NULL;
                    std::map<int, ClientConnection>::iterator conn_it = connections.find(poll_fds[i].fd);
                    if (conn_it != connections.end()) {
                        server = conn_it->second.getServer();
                        request = conn_it->second.getRequest();
                    }
                    request->setLastActivity(curr_time_ms());

                    if (server != NULL && request != NULL) {
                        Logger::instance().log(INFO, "Begin to handle request for client FD: " + to_string(poll_fds[i].fd));
                        server->handleClient(poll_fds[i].fd, request);
                    }
                    // Check if the request is complete or connection is closed
                    if (request->isComplete() || request->getConnectionClosed() || request->getRequestTooLarge()) {
                        // Clean up
                        close(poll_fds[i].fd);
                        connections.erase(poll_fds[i].fd);
                        poll_fds.erase(poll_fds.begin() + i);
                        --i;
                    }
                }
            }
        }

        if (stopServer)
            break;
    }

    // Clean up any remaining HTTPRequest objects
    // for (std::map<int, ClientConnection>::iterator it = connections.begin(); it != connections.end(); ++it) {
    //     delete it->second;
    // }
    connections.clear();

    // Clean up memory
    for (size_t i = 0; i < servers.size(); ++i) {
        delete servers[i];
        delete sockets[i];
    }

    return 0;
}
