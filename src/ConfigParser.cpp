// ConfigParser.cpp
#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>
#include <algorithm>

ConfigParser::ConfigParser() {}

ConfigParser::~ConfigParser() {}

void ConfigParser::parseConfigFile(const std::string &filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        throw ConfigParserException("Unable to open configuration file: " + filename);
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

                processServerDirective(file, line, serverConfig);
            }
            _serverConfigs.push_back(serverConfig);
        } else {
            throw ConfigParserException("Unknown or unexpected directive: \"" + line + "\"");
        }
    }
}


const std::vector<ServerConfig>& ConfigParser::getServerConfigs() const {
	return _serverConfigs;
}

void ConfigParser::validateDirectiveValue(const std::string &directive, const std::string &value) {
	if (directive == "listen") {
		size_t colonPos = value.find(':');
		if (colonPos != std::string::npos) {
			std::string ipAddress = value.substr(0, colonPos);
			std::string portStr = value.substr(colonPos + 1);
			int port = std::atoi(portStr.c_str());
			if (port <= 0 || port > 65535) {
				throw ConfigParserException("Invalid port: " + portStr);
			}
			// Store the IP address later if needed
		} else {
			int port = std::atoi(value.c_str());
			if (port <= 0 || port > 65535) {
				throw ConfigParserException("Invalid port: " + value);
			}
		}
	} else if (directive == "root" || directive == "index" || directive == "error_page") {
		if (value.empty()) {
			throw ConfigParserException("Invalid path for '" + directive + "'");
		}
		if (directive == "error_page") {
			std::istringstream valueStream(value);
			std::string token;
			while (valueStream >> token) {
				if (isdigit(token[0])) {
					int errorCode = std::atoi(token.c_str());
					if (errorCode < 100 || errorCode > 599) {
						throw ConfigParserException("Invalid error code: " + token);
					}
				} else {
					break;
				}
			}
		}
	} else if (directive == "server_name") {
		if (value.empty() || value.find(' ') != std::string::npos || value.length() > 255) {
			throw ConfigParserException("Invalid server name: " + value);
		}
	} else if (directive == "method") {
		std::string validMethodsArray[] = {"GET", "POST", "DELETE"};
		std::vector<std::string> validMethods(validMethodsArray, validMethodsArray + 3);

		if (std::find(validMethods.begin(), validMethods.end(), value) == validMethods.end()) {
			throw ConfigParserException("Invalid HTTP method: " + value);
		}
	} else if (directive == "proxy_pass") {
		if (value.find("http://") != 0 && value.find("https://") != 0) {
			throw ConfigParserException("Invalid proxy_pass URL: " + value);
		}
	} else if (directive == "cgi_extension") {
		if (value.empty() || value[0] != '.' || value.find(' ') != std::string::npos) {
			throw ConfigParserException("Invalid CGI extension: " + value);
		}
	}
	// Add more validations as needed
}

void ConfigParser::processServerDirective(std::ifstream &file, const std::string &line, ServerConfig &serverConfig) {
    std::string directive_line = line;

    // Vérifier si la ligne se termine par un point-virgule ou une accolade ouvrante
    trim(directive_line);
    bool endsWithSemicolon = false;
    bool endsWithOpeningBrace = false;
    if (!directive_line.empty()) {
        if (directive_line[directive_line.size() - 1] == ';') {
            endsWithSemicolon = true;
        } else if (directive_line[directive_line.size() - 1] == '{') {
            endsWithOpeningBrace = true;
        }
    }

    std::istringstream iss(directive_line);
    std::string directive;
    iss >> directive;

    std::string value;
    if (endsWithSemicolon || endsWithOpeningBrace) {
        std::getline(iss, value, endsWithSemicolon ? ';' : '{');
        trim(value);
    } else {
        throw ConfigParserException("Missing ';' or '{' after directive '" + directive + "'");
    }

    if (endsWithOpeningBrace) {
        if (directive == "location") {
            trim(value);
            processLocationBlock(file, value, serverConfig);
        } else {
            throw ConfigParserException("Unexpected '{' after directive '" + directive + "'");
        }
    } else if (endsWithSemicolon) {
        validateDirectiveValue(directive, value);
        // Traitement des directives avec point-virgule
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
        } else {
            throw ConfigParserException("Unknown directive: \"" + directive + "\"");
        }
    } else {
        throw ConfigParserException("Invalid directive format for '" + directive + "'");
    }
}


void ConfigParser::processLocationBlock(std::ifstream &file, const std::string& locationPath, ServerConfig& serverConfig) {
    Location location;
    location.path = locationPath;

    std::string line;
    while (std::getline(file, line)) {
        trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line == "}") {
            serverConfig.locations.push_back(location);
            return;
        }

        // Vérifier le point-virgule à la fin de la directive
        // bool endsWithSemicolon = false;
        if (!line.empty() && line[line.size() - 1] == ';') {
            // endsWithSemicolon = true;
        } else {
            throw ConfigParserException("Missing ';' at the end of line: '" + line + "'");
        }

        std::istringstream iss(line);
        std::string directive;
        iss >> directive;

        std::string value;
        std::getline(iss, value, ';');
        trim(value);

        validateDirectiveValue(directive, value);
        location.options[directive] = value;
    }

    throw ConfigParserException("Error: unexpected end of file in location block.");
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
