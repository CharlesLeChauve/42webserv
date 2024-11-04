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
    Logger::instance().log(INFO, "Lancement du main");
    std::string configFile;
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [path/to/file]" << std::endl;
        return 1;
    } else {
        configFile = (argc == 1 ? "config/server.conf" : argv[1]);
    }
    initialize_random_generator();
    ConfigParser configParser;

    try {
        configParser.parseConfigFile(configFile);
    } catch (const ConfigParserException& e) {
        std::cerr << "Échec de l'analyse du fichier de configuration : " << e.what() << std::endl;
        return 1;
    }

    const std::vector<ServerConfig>& serverConfigs = configParser.getServerConfigs();
    std::cout << "Nombre de serveurs configurés : " << serverConfigs.size() << std::endl;

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

        std::cout << "Serveur lancé sur le port " << serverConfigs[i].ports[0] << std::endl;
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
                std::cerr << "Erreur sur le descripteur FD : " << poll_fds[i].fd << std::endl;
                if (fdToServerMap.find(poll_fds[i].fd) != fdToServerMap.end()) {
                    // C'est un socket serveur
                    // Vous pouvez décider de fermer le serveur ou de gérer l'erreur autrement
                } else {
                    // C'est un socket client
                    close(poll_fds[i].fd);
                    clientFdToServerMap.erase(poll_fds[i].fd);
                    poll_fds.erase(poll_fds.begin() + i);
                    --i;
                }
                continue;
            }

            // Gérer les déconnexions
            if (poll_fds[i].revents & POLLHUP) {
                std::cout << "Client déconnecté : FD " << poll_fds[i].fd << std::endl;
                close(poll_fds[i].fd);
                clientFdToServerMap.erase(poll_fds[i].fd);
                poll_fds.erase(poll_fds.begin() + i);
                --i;
                continue;
            }

            if (poll_fds[i].revents & POLLNVAL) {
                std::cerr << "Descripteur de fichier non valide : FD " << poll_fds[i].fd << std::endl;
                // Supprimer le descripteur invalide
                poll_fds.erase(poll_fds.begin() + i);
                --i;
                continue;
            }

            if (poll_fds[i].revents & POLLIN) {
                if (fdToServerMap.find(poll_fds[i].fd) != fdToServerMap.end()) {
                    // C'est un descripteur de socket serveur, accepter une nouvelle connexion
                    Server* server = fdToServerMap[poll_fds[i].fd];
                    int client_fd = server->acceptNewClient(poll_fds[i].fd);

                    if (client_fd != -1) {
                        clientFdToServerMap[client_fd] = server;  // Enregistrer l'association client_fd -> server

                        pollfd client_pollfd;
                        client_pollfd.fd = client_fd;
                        client_pollfd.events = POLLIN | POLLHUP | POLLERR;
                        client_pollfd.revents = 0;
                        poll_fds.push_back(client_pollfd);

                        std::cout << "Nouveau client FD : " << client_fd << " accepté sur le serveur FD : " << poll_fds[i].fd << std::endl;
                    } else {
                        std::cerr << "Échec de l'acceptation du client sur le serveur FD : " << poll_fds[i].fd << std::endl;
                    }
                } else {
                    // C'est un descripteur de socket client, traiter la requête
                    Server* server = clientFdToServerMap[poll_fds[i].fd];
                    if (server != NULL) {
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
