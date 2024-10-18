#include "ConfigParser.hpp"
#include "socket.hpp"
#include <iostream>

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
		// for (size_t i = 0; i < serverConfigs.size(); ++i) {
			// const ServerConfig& serverConfig = serverConfigs[i];
			// std::cout << "Server " << i + 1 << " : " << std::endl;
			// std::cout << "Name: " << serverConfig.serverName << std::endl;
			// std::cout << "Root: " << serverConfig.root << std::endl;
			// std::cout << "Index: " << serverConfig.index << std::endl;
			// std::cout << "Ports: ";
			// for (size_t j = 0; j < serverConfig.ports.size(); ++j) {
				// std::cout << serverConfig.ports[j] << " ";
			// }
			// std::cout << std::endl;
		// }
	} else {
		std::cerr << "Failed to parse the configuration file." << std::endl;
		return 1;
	}

	Socket socket;
	socket.socket_creation();
// les prints de jeza en anglais et en francais c'est incr
	socket.socket_binding();


	return 0;
}
