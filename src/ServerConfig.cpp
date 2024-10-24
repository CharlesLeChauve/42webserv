#include "ServerConfig.hpp"
#include <iostream>

ServerConfig::ServerConfig() : root("www/"), index("index.html"), host("0.0.0.0") {
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
	}
	return *this;
}

ServerConfig::~ServerConfig() {}

bool ServerConfig::isValid() const {
	if (ports.empty()) {
		std::cerr << "Erreur : Aucun port n'est spécifié." << std::endl;
		return false;
	}
	if (root.empty()) {
		std::cerr << "Erreur : Le chemin racine est vide." << std::endl;
		return false;
	}
	return true;
}
