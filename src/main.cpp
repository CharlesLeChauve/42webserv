#include "ConfigParser.hpp"
#include "Socket.hpp"
#include "ServerCopy.hpp"
#include <iostream>
#include "ServerConfig.hpp"

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <chemin_vers_le_fichier_de_configuration>" << std::endl;
		return 1;
	}

	std::string configFile = argv[1];
	ConfigParser configParser;
	if (configParser.parseConfigFile(configFile)) {
		const std::vector<ServerConfig>& serverConfigs = configParser.getServerConfigs();
		std::cout << "Nombre de serveurs configurés : " << serverConfigs.size() << std::endl;

		// Créer des objets Server à partir des ServerConfigs
		std::vector<Server*> servers;
		std::vector<std::vector <Socket *> > sockets;
		sockets.resize(serverConfigs.size());  // Redimensionne le vecteur pour contenir le bon nombre de vecteurs internes

		for (size_t i = 0; i < serverConfigs.size(); ++i) {
			const ServerConfig& serverConfig = serverConfigs[i];

			// Créer un serveur et un socket avec chaque configuration
			Server* server = new Server(serverConfig);
			Socket* socket = new Socket(serverConfig.ports[0]);
			socket->build_sockets();  // Créer et lier le socket
			std::cout << "Création d'un socket pour le port : " << serverConfig.ports[0] << ", FD : " << socket->getSocket() << std::endl;

			servers.push_back(server);
			sockets[i].push_back(socket);

			std::cout << "Lancement du serveur sur le port " << serverConfig.ports[0] << std::endl;
		}

		// Boucle pour gérer les connexions et les requêtes
		while (true) {
			for (size_t i = 0; i < servers.size(); ++i) {
				servers[i]->stockClientsSockets(sockets[i]);
				std::cout << "Me produis-je ? non" << std::endl;
			}
		}

		// Nettoyage mémoire
		for (size_t i = 0; i < servers.size(); ++i) {
			delete servers[i];
			for (size_t j = 0; j < sockets[i].size(); ++j) {
				delete sockets[i][j];  // Libère chaque socket
			}
			// delete sockets[i];
		}

	} else {
		std::cerr << "Échec de l'analyse du fichier de configuration." << std::endl;
		return 1;
	}

	return 0;
}
