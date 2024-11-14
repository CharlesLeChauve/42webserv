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
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [path/to/file]" << std::endl;
        return 1;
    } else {
        if (argc == 1) {
            configFile = "config/server.conf";
            Logger::instance().log(DEBUG, "Default configuration file loaded : " + configFile);
        } else {
            configFile = argv[1];
            Logger::instance().log(DEBUG, "Custom configuration file loaded : " + configFile);
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

    std::vector<Server*> servers;
    std::vector<Socket*> sockets;
    std::vector<pollfd> poll_fds;
    std::map<int, Server*> fdToServerMap;
    std::map<int, Server*> clientFdToServerMap;
    std::map<int, HTTPRequest*> clientFdToRequestMap; // Map to keep track of HTTPRequest objects

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
        int poll_count = poll(&poll_fds[0], poll_fds.size(), -1); // Wait indefinitely
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
                    clientFdToServerMap.erase(poll_fds[i].fd);
                    clientFdToRequestMap.erase(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                    --i;
                }
                continue;
            }

            // Handle disconnections
            if (poll_fds[i].revents & POLLHUP) {
                Logger::instance().log(INFO, "Disconnected client FD: " + to_string(poll_fds[i].fd));
                close(poll_fds[i].fd);
                clientFdToServerMap.erase(poll_fds[i].fd);
                clientFdToRequestMap.erase(poll_fds[i].fd);
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
                        clientFdToServerMap[client_fd] = server; // Register the client_fd -> server association

                        // Create a new HTTPRequest object and store it in the map
                        int max_body_size = serverConfigs[0].clientMaxBodySize;
                        HTTPRequest* request = new HTTPRequest(max_body_size);
                        clientFdToRequestMap[client_fd] = request;

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
                    Server* server = clientFdToServerMap[poll_fds[i].fd];
                    HTTPRequest* request = clientFdToRequestMap[poll_fds[i].fd];

                    if (server != NULL && request != NULL) {
                        Logger::instance().log(INFO, "Begin to handle request for client FD: " + to_string(poll_fds[i].fd));
                        server->handleClient(poll_fds[i].fd, request);
                    }

                    // Check if the request is complete or connection is closed
                    if (request->isComplete() || request->getConnectionClosed()) {
                        // Clean up
                        delete request;
                        clientFdToRequestMap.erase(poll_fds[i].fd);

                        close(poll_fds[i].fd);
                        clientFdToServerMap.erase(poll_fds[i].fd);
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
    for (std::map<int, HTTPRequest*>::iterator it = clientFdToRequestMap.begin(); it != clientFdToRequestMap.end(); ++it) {
        delete it->second;
    }
    clientFdToRequestMap.clear();

    // Clean up memory
    for (size_t i = 0; i < servers.size(); ++i) {
        delete servers[i];
        delete sockets[i];
    }

    return 0;
}
