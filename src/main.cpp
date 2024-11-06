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
	Logger::instance().log(INFO, to_string(serverConfigs.size()) + " servers successfully cnfigured");

    std::vector<Server*> servers;
    std::vector<Socket*> sockets;
    std::vector<pollfd> poll_fds;
    std::map<int, Server*> fdToServerMap;
    std::map<int, Server*> clientFdToServerMap;

    // Créer les serveurs et sockets
    for (size_t i = 0; i < serverConfigs.size(); ++i) {
        Server* server = new Server(serverConfigs[i]);
        Socket* socket = new Socket(serverConfigs[i].ports[0]);
        socket->build_sockets();

        pollfd pfd;
        pfd.fd = socket->getSocket();
        pfd.events = POLLIN;
        pfd.revents = 0; // Initialiser revents à 0
        poll_fds.push_back(pfd);

        // Associer les sockets serveurs aux serveurs
        fdToServerMap[socket->getSocket()] = server;

        servers.push_back(server);
        sockets.push_back(socket);

		Logger::instance().log(INFO, "Server launched, listenign on port: " + to_string(serverConfigs[i].ports[0]));
    }

    while (true) {
        int poll_count = poll(&poll_fds[0], poll_fds.size(), -1); // Attendre indéfiniment
        if (poll_count < 0) {
            perror("Erreur lors de l'appel à poll");
            continue;
        }

        for (size_t i = 0; i < poll_fds.size(); ++i) {
            if (poll_fds[i].revents == 0)
                continue;
            // Gérer les erreurs
            if (poll_fds[i].revents & POLLERR) {
				Logger::instance().log(ERROR, "Error on file descriptor: " + to_string(poll_fds[i].fd));
                if (fdToServerMap.find(poll_fds[i].fd) != fdToServerMap.end()) {
                    // C'est un socket serveur
                    // Vous pouvez décider de fermer le serveur ou de gérer l'erreur autrement
                    Logger::instance().log(ERROR, "Error on Server socket detected in poll");
					//?? Et du coup on décide quoi ?
                } else {
                    // C'est un socket client
                    Logger::instance().log(ERROR, "Error on Client socket detected in poll");
                    close(poll_fds[i].fd);
                    clientFdToServerMap.erase(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                    --i;
                }
                continue;
            }

            // Gérer les déconnexions
            if (poll_fds[i].revents & POLLHUP) {
				Logger::instance().log(INFO, "Disconnected client FD : " + to_string(poll_fds[i].fd));
                close(poll_fds[i].fd);
                clientFdToServerMap.erase(poll_fds[i].fd);
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

            if (poll_fds[i].revents & POLLIN) {
                if (fdToServerMap.find(poll_fds[i].fd) != fdToServerMap.end()) {
                    // C'est un descripteur de socket serveur, accepter une nouvelle connexion
                    Server* server = fdToServerMap[poll_fds[i].fd];
                    //?? la fonction acceptNewClient semble etre appelée 2 fois à chaque connexion
                    int client_fd = server->acceptNewClient(poll_fds[i].fd);

                    if (client_fd != -1) {
                        clientFdToServerMap[client_fd] = server;  // Enregistrer l'association client_fd -> server

                        pollfd client_pollfd;
                        client_pollfd.fd = client_fd;
                        client_pollfd.events = POLLIN | POLLHUP | POLLERR;
                        client_pollfd.revents = 0;
                        poll_fds.push_back(client_pollfd);
						Logger::instance().log(DEBUG, "New pollfd added for client with FD: " + to_string(client_fd) + " accepted on server FD: " + to_string(poll_fds[i].fd));
                    } else {
						Logger::instance().log(ERROR, "Failure accepting client " + to_string(client_fd) + " on server FD : " + to_string(poll_fds[i].fd));
                    }
                } else {
                    // C'est un descripteur de socket client, traiter la requête
                    Server* server = clientFdToServerMap[poll_fds[i].fd];
                    if (server != NULL) {
                        Logger::instance().log(INFO, "Begin to handle request for client FD: " + to_string(poll_fds[i].fd));
                        server->handleClient(poll_fds[i].fd);
                    }
                    // Fermer et supprimer la connexion du client du poll_fds
                    close(poll_fds[i].fd);
                    clientFdToServerMap.erase(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                    --i;
                }
            }
        }
    }

    // Nettoyage de la mémoire
    for (size_t i = 0; i < servers.size(); ++i) {
        delete servers[i];
        delete sockets[i];
    }

    return 0;
}
