#include "ConfigParser.hpp"
#include "Socket.hpp"
#include "ServerCopy.hpp"
#include <iostream>
#include "ServerConfig.hpp"

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <path_to_configuration_file>" << std::endl;
		return 1;
	}

	std::string configFile = argv[1];
	ConfigParser configParser;
	if (configParser.parseConfigFile(configFile)) {
		const std::vector<ServerConfig>& serverConfigs = configParser.getServerConfigs();
		std::cout << "Number of configured servers: " << serverConfigs.size() << std::endl;

		// Boucle sur tous les serveurs configurés
		for (size_t i = 0; i < serverConfigs.size(); ++i) {
			const ServerConfig& serverConfig = serverConfigs[i];

			std::cout << "Server " << i + 1 << " :" << std::endl;
			std::cout << "  Server name: " << serverConfig.serverName << std::endl;
			std::cout << "  Root: " << serverConfig.root << std::endl;
			std::cout << "  Index: " << serverConfig.index << std::endl;
			std::cout << "  Host: " << serverConfig.host << std::endl;

			std::cout << "  Ports: ";
			for (size_t j = 0; j < serverConfig.ports.size(); ++j) {
				std::cout << serverConfig.ports[j] << " ";
			}
			std::cout << std::endl;

			std::cout << "  Error pages:" << std::endl;
			for (std::map<int, std::string>::const_iterator it = serverConfig.errorPages.begin(); it != serverConfig.errorPages.end(); ++it) {
				std::cout << "    Error code: " << it->first << " -> Page: " << it->second << std::endl;
			}

			std::cout << "  Locations:" << std::endl;
			for (size_t j = 0; j < serverConfig.locations.size(); ++j) {
				const Location& location = serverConfig.locations[j];
				std::cout << "    Path: " << location.path << std::endl;
				for (std::map<std::string, std::string>::const_iterator it = location.options.begin(); it != location.options.end(); ++it) {
					std::cout << "      " << it->first << ": " << it->second << std::endl;
				}
			}

			// Créer un serveur avec chaque configuration
			Server server(serverConfig);
			Socket socket(serverConfig.ports[0]);  // Utilise le premier port de la configuration
			socket.build_sockets();

			// Boucle pour accepter des connexions et les traiter
			while (true) {
				server.stockClientsSockets(socket);  // Accepter les connexions
				server.receiveAndSend();  // Gérer les requêtes des clients
			}
		}
	} else {
		std::cerr << "Failed to parse the configuration file." << std::endl;
		return 1;
	}

	return 0;
}
