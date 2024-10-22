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

		std::string line;
		while (std::getline(file, line)) {
				trim(line);

				if (line.empty() || line[0] == '#') {
						continue;
				}

				if (line == "server {") {
						ServerConfig serverConfig;
						while (std::getline(file, line)) {
								trim(line);
								if (line.empty() || line[0] == '#') {
										continue;
								}
								if (line == "}") {
										break;
								}

								if (!processServerDirective(file, line, serverConfig)) {
										return false;
								}
						}
						_serverConfigs.push_back(serverConfig);
				} else {
						std::cerr << "Unknown or unexpected directive: \"" << line << "\"" << std::endl;
						return false;
				}
		}
		return true;
}

const std::vector<ServerConfig>& ConfigParser::getServerConfigs() const {
		return _serverConfigs;
}

// Modifié pour prendre en charge l'adresse IP et le port dans "listen"
bool ConfigParser::validateDirectiveValue(const std::string &directive, const std::string &value) {
	if (directive == "listen") {
		// Gérer "0.0.0.0:8080" ou "8080"
		size_t colonPos = value.find(':');
		if (colonPos != std::string::npos) {
			std::string ipAddress = value.substr(0, colonPos);
			std::string portStr = value.substr(colonPos + 1);
			int port = std::atoi(portStr.c_str());
			if (port <= 0 || port > 65535) {
				std::cerr << "Invalid port: " << port << std::endl;
				return false;
			}
			// Stocker l'adresse IP dans serverConfig.host plus tard
		} else {
			int port = std::atoi(value.c_str());
			if (port <= 0 || port > 65535) {
				std::cerr << "Invalid port: " << port << std::endl;
				return false;
			}
		}
	} else if (directive == "root" || directive == "index" || directive == "error_page") {
		if (value.empty()) {
			std::cerr << "Invalid path for '" << directive << "'" << std::endl;
			return false;
		}
		if (directive == "error_page") {
			std::istringstream valueStream(value);
			std::string token;
			while (valueStream >> token) {
				if (isdigit(token[0])) {
					int errorCode = std::atoi(token.c_str());
					if (errorCode < 100 || errorCode > 599) {
						std::cerr << "Invalid error code: " << errorCode << std::endl;
						return false;
					}
				} else {
					break;
				}
			}
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

	std::string value;
	std::getline(iss, value, ';');
	trim(value);

	  if (directive == "location" && *(value.end() - 1) == '{') {
		value = value.substr(0, value.size() - 1);
		trim(value);
	}

	if (!validateDirectiveValue(directive, value)) {
		return false;
	}

	if (directive == "listen") {
		size_t colonPos = value.find(':');
		if (colonPos != std::string::npos) {
			serverConfig.host = value.substr(0, colonPos);
			int port = std::atoi(value.substr(colonPos + 1).c_str());
			serverConfig.ports.push_back(port);
		} else {
			serverConfig.ports.push_back(std::atoi(value.c_str()));
		}
	} else if (directive == "server_name") {
		serverConfig.serverName = value;
	} else if (directive == "root") {
		serverConfig.root = value;
	} else if (directive == "index") {
		serverConfig.index = value;
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
		}
	} else if (directive == "location") {
		Location location;
		location.path = value;
		if (!processLocationBlock(file, location.options)) {
			std::cerr << "Error parsing location block." << std::endl;
			return false;
		}
		serverConfig.locations.push_back(location);
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
			continue;
		}
		if (line == "}") {
			return true;
		}

		std::istringstream iss(line);
		std::string directive;
		iss >> directive;

		// Assurer que toutes les options sont bien parsées sans ajout d'accolades
		if (directive == "root" || directive == "index" || directive == "method" || directive == "cgi" || directive == "cgi_extension") {
			std::string value;
			std::getline(iss, value, ';');
			trim(value);
			if (!validateDirectiveValue(directive, value)) {
				return false;
			}
			options[directive] = value;
		} else if (directive == "proxy_pass") {
			std::string proxyPass;
			std::getline(iss, proxyPass, ';');
			trim(proxyPass);
			options["proxy_pass"] = proxyPass;
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
