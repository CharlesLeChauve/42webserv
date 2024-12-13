#include "ServerConfig.hpp"
#include "Logger.hpp"
#include <iostream>
#include <cstring>

ServerConfig::ServerConfig() : index("index.html"), host("0.0.0.0"), clientMaxBodySize(0), autoindex(false) {
	serverNames.push_back("localhost");
}

ServerConfig::ServerConfig(const ServerConfig& other) {
	ports = other.ports;
	serverNames = other.serverNames;
	root = other.root;
	index = other.index;
	errorPages = other.errorPages;
	locations = other.locations;
	host = other.host;
	cgiExtensions = other.cgiExtensions;
	clientMaxBodySize = other.clientMaxBodySize;
	cgiInterpreters = other.cgiInterpreters;
}


const Location* ServerConfig::findLocation(const std::string& path) const {
    const Location* bestMatch = NULL;
    size_t bestMatchLength = 0;

    for (size_t i = 0; i < locations.size(); ++i) {
        if (path.compare(0, locations[i].path.size(), locations[i].path) == 0 ||
            path.compare(0, locations[i].path.size() + 1, locations[i].path + "/") == 0) {
            size_t matchLength = locations[i].path.size();
            if (matchLength > bestMatchLength) {
                bestMatch = &locations[i];
                bestMatchLength = matchLength;
            }
        }
    }

    return bestMatch;
}

ServerConfig& ServerConfig::operator=(const ServerConfig& other) {
	if (this != &other) {
		ports = other.ports;
		serverNames = other.serverNames;
		root = other.root;
		index = other.index;
		errorPages = other.errorPages;
		locations = other.locations;
		host = other.host;
		cgiExtensions = other.cgiExtensions;
		clientMaxBodySize = other.clientMaxBodySize;
		autoindex = other.autoindex;
		cgiInterpreters = other.cgiInterpreters;
	}
	return *this;
}

ServerConfig::~ServerConfig() {}

bool ServerConfig::isValid() const {
	if (ports.empty()) {
		Logger::instance().log(ERROR, "Erreur : Aucun port n'est spécifié.");
		return false;
	}
	return true;
}

const std::string& ServerConfig::getHost() const {
    return host;
}
const std::vector<int>& ServerConfig::getPorts() const {
    return ports;
}
