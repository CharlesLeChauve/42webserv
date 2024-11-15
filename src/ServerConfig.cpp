#include "ServerConfig.hpp"
#include "Logger.hpp"
#include <iostream>

ServerConfig::ServerConfig() : root("www/"), index("index.html"), host("0.0.0.0"), clientMaxBodySize(0), autoindex(false) {
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
	autoindex = other.autoindex;
}


const Location* ServerConfig::findLocation(const std::string& path) const {
    for (size_t i = 0; i < locations.size(); ++i) {
        // Vérifie une correspondance exacte ou un chemin avec un '/' final pour les répertoires
        if (path == locations[i].path || path == locations[i].path + "/") {
            return &locations[i];
        }
    }
    return NULL; // Aucune location correspondante trouvée
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
	}
	return *this;
}

ServerConfig::~ServerConfig() {}

bool ServerConfig::isValid() const {
	if (ports.empty()) {
		Logger::instance().log(ERROR, "Erreur : Aucun port n'est spécifié.");
		return false;
	}
	if (root.empty()) {
		Logger::instance().log(ERROR, "Erreur : Le chemin racine est vide.");
		return false;
	}
	return true;
}
