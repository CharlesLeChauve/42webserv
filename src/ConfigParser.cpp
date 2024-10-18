#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <algorithm>

ConfigParser::ConfigParser() {}

ConfigParser::~ConfigParser() {}

bool ConfigParser::parseConfigFile(const std::string &filename) {
	std::ifstream file(filename.c_str());
	if (!file.is_open()) {
		std::cerr << "Unable to open configuration file: " << filename << std::endl;
		return false;
	}

	std::cout << "Parsing configuration file: " << filename << std::endl;

	std::string line;
	while (std::getline(file, line)) {
		trim(line);
		std::cout << "Read line: \"" << line << "\"" << std::endl;

		if (line.empty() || line[0] == '#') {
			std::cout << "Skipping empty line or comment." << std::endl;
			continue;
		}

		if (line == "server {") {
			std::cout << "Starting server block" << std::endl;
			ServerConfig serverConfig;
			while (std::getline(file, line)) {
				trim(line);
				if (line.empty() || line[0] == '#') {
					std::cout << "Skipping empty line or comment in server block." << std::endl;
					continue;
				}
				if (line == "}") {
					std::cout << "End of server block" << std::endl;
					break;
				}
				std::cout << "Directive in server block: \"" << line << "\"" << std::endl;
				if (!processServerDirective(file, line, serverConfig)) {
					std::cerr << "Error parsing server block." << std::endl;
					return false;
				}
			}
			_serverConfigs.push_back(serverConfig);
		} else {
			std::cerr << "Unknown or unexpected directive: \"" << line << "\"" << std::endl;
			return false;
		}
	}

	std::cout << "Configuration file parsing completed successfully." << std::endl;
	return true;
}

const std::vector<ServerConfig>& ConfigParser::getServerConfigs() const {
	return _serverConfigs;
}

bool ConfigParser::validateDirectiveValue(const std::string &directive, const std::string &value) {
	if (directive == "listen") {
		int port = std::atoi(value.c_str());
		if (port <= 0 || port > 65535) {
			std::cerr << "Invalid port: " << port << std::endl;
			return false;
		}
	} else if (directive == "root" || directive == "index" || directive == "error_page") {
		if (value.empty()) {
			std::cerr << "Invalid path for '" << directive << "'" << std::endl;
			return false;
		}
	} else if (directive == "server_name") {
		if (value.empty() || value.find(' ') != std::string::npos || value.length() > 255) {
			std::cerr << "Invalid server name: " << value << std::endl;
			return false;
		}
	} else if (directive == "method") {
		std::string validMethodsArray[] = {"GET", "POST", "DELETE"};
		std::vector<std::string> validMethods(validMethodsArray, validMethodsArray + 3);

		if (std::find(validMethods.begin(), validMethods.end(), value) == validMethods.end()) {
			std::cerr << "Invalid HTTP method: " << value << std::endl;
			return false;
		}
	} else if (directive == "proxy_pass") {
		if (value.find("http://") != 0 && value.find("https://") != 0) {
			std::cerr << "Invalid proxy_pass URL: " << value << std::endl;
			return false;
		}
	} else if (directive == "cgi_extension") {
		if (value.empty() || value[0] != '.' || value.find(' ') != std::string::npos) {
			std::cerr << "Invalid CGI extension: " << value << std::endl;
			return false;
		}
	}
	return true;
}

bool ConfigParser::processServerDirective(std::ifstream &file, const std::string &line, ServerConfig &serverConfig) {
	std::istringstream iss(line);
	std::string directive;
	iss >> directive;

	std::cout << "Processing directive: \"" << directive << "\"" << std::endl;

	std::string value;
	std::getline(iss, value, ';');
	trim(value);
	if (!validateDirectiveValue(directive, value)) {
		return false;
	}
	if (directive == "listen") {
		int port = std::atoi(value.c_str());
		serverConfig.ports.push_back(port);
		std::cout << "Found 'listen' directive: port = " << port << std::endl;
	} else if (directive == "server_name") {
		serverConfig.serverName = value;
		std::cout << "Found 'server_name' directive: " << serverConfig.serverName << std::endl;
	} else if (directive == "root") {
		serverConfig.root = value;
		std::cout << "Found 'root' directive: " << serverConfig.root << std::endl;
	} else if (directive == "index") {
		serverConfig.index = value;
		std::cout << "Found 'index' directive: " << serverConfig.index << std::endl;
	} else if (directive == "error_page") {
		std::istringstream valueStream(value);
		std::string token;
		std::vector<int> errorCodes;
		std::string errorPage;

		while (valueStream >> token) {
			if (isdigit(token[0])) {
				errorCodes.push_back(std::atoi(token.c_str()));
			} else {
				errorPage = token;
				break;
			}
		}

		for (size_t i = 0; i < errorCodes.size(); ++i) {
			serverConfig.errorPages[errorCodes[i]] = errorPage;
			std::cout << "Directive 'error_page' found : code = " << errorCodes[i] << ", page = " << errorPage << std::endl;
		}
	} else if (directive == "location") {
		Location location;
		location.path = value;
		std::cout << "Starting location block for path: " << location.path << std::endl;
		if (!processLocationBlock(file, location.options)) {
			std::cerr << "Error parsing location block." << std::endl;
			return false;
		}
		serverConfig.locations.push_back(location);
		std::cout << "End of location block for path: " << location.path << std::endl;
	} else {
		std::cerr << "Unknown directive: \"" << directive << "\"" << std::endl;
		return false;
	}
	return true;
}

bool ConfigParser::processLocationBlock(std::ifstream &file, std::map<std::string, std::string> &options) {
	std::string line;
	while (std::getline(file, line)) {
		trim(line);
		if (line.empty() || line[0] == '#') {
			std::cout << "Skipping empty line or comment in location block." << std::endl;
			continue;
		}
		if (line == "}") {
			std::cout << "End of location block" << std::endl;
			return true;
		}

		std::istringstream iss(line);
		std::string directive;
		iss >> directive;

		std::cout << "Directive in location block: \"" << directive << "\"" << std::endl;

		if (directive == "root" || directive == "index" || directive == "method" || directive == "cgi") {
			std::string value;
			std::getline(iss, value, ';');
			trim(value);
			if (!validateDirectiveValue(directive, value)) {
				return false;
			}
			options[directive] = value;
			std::cout << "Found option '" << directive << "': " << value << std::endl;
		} else if (directive == "proxy_pass") {
			std::string proxyPass;
			std::getline(iss, proxyPass, ';');
			trim(proxyPass);
			options["proxy_pass"] = proxyPass;
			std::cout << "Found 'proxy_pass' option: " << proxyPass << std::endl;
		} else {
			std::cerr << "Unknown directive in location block: \"" << directive << "\"" << std::endl;
			return false;
		}
	}

	std::cerr << "Error: unexpected end of file in location block." << std::endl;
	return false;
}

void ConfigParser::trim(std::string &s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	size_t end = s.find_last_not_of(" \t\r\n");
	if (start == std::string::npos || end == std::string::npos) {
		s = "";
	} else {
		s = s.substr(start, end - start + 1);
	}
}
