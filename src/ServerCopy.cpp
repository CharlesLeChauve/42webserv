#include <fstream>
#include "ServerCopy.hpp"
#include "HTTPRequest.hpp"
#include "CGIHandler.hpp"
#include "ServerConfig.hpp"
#include <sys/stat.h>  // Pour utiliser la fonction stat

Server::Server(const ServerConfig& config) : _config(config) {
	// Utilisation de _config pour initialiser le serveur
	std::cout << "Server initialized with name: " << _config.serverName << std::endl;
}

Server::~Server() {}

// Génération de la page d'erreur en utilisant les informations de configuration du serveur
std::string Server::generateErrorPage(int errorCode, const std::string& errorMessage) {
	std::stringstream page;
	page << "<html><head><title>Error " << errorCode << " - " << _config.serverName << "</title></head>";  // Utilisation du nom du serveur
	page << "<body><h1>Error " << errorCode << ": " << errorMessage << "</h1>";
	page << "<p>The server encountered an issue processing your request.</p>";
	page << "<p>Server Root: " << _config.root << "</p>";  // Afficher le chemin racine configuré
	page << "</body></html>";
	return page.str();
}

// Utilisation de _config pour afficher les pages d'erreur personnalisées
std::string Server::getErrorMessage(int errorCode) {
	// Utilisation des pages d'erreur définies dans _config.errorPages
	std::map<int, std::string>::const_iterator it = _config.errorPages.find(errorCode);
	if (it != _config.errorPages.end()) {
		return it->second;  // Si une page d'erreur personnalisée existe
	}

	// Sinon, retourner des messages d'erreur par défaut
	switch (errorCode) {
		case 404:
			return "Not Found";
		case 405:
			return "Method Not Allowed";
		case 500:
			return "Internal Server Error";
		case 403:
			return "Forbidden";
		default:
			return "Unknown Error";
	}
}

void Server::receiveAndSend() {
	for (std::vector<int>::iterator it = _clientsFd.begin(); it != _clientsFd.end();) {
		int client_fd = *it;
		std::cout << "Handling client_fd: " << client_fd << std::endl;

		std::string requestString = receiveRequest(client_fd);
		if (requestString.empty()) {
			close(client_fd);
			it = _clientsFd.erase(it);
			continue;
		}

		HTTPRequest request;
		if (!request.parse(requestString)) {
			sendErrorResponse(client_fd, 400);
			close(client_fd);
			it = _clientsFd.erase(it);
			continue;
		}

		handleHttpRequest(client_fd, request);

		close(client_fd);
		std::cout << "Connection closed for client_fd: " << client_fd << std::endl;
		it = _clientsFd.erase(it);
	}
}

// Méthode pour recevoir la requête
std::string Server::receiveRequest(int client_fd) {
	char buffer[1024] = {0};
	int bytes_received = read(client_fd, buffer, sizeof(buffer) - 1);
	if (bytes_received <= 0) {
		std::cerr << "Failed to receive request or no data. errno: " << strerror(errno) << std::endl;
		return "";
	}
	return std::string(buffer, bytes_received);
}

// Méthode pour envoyer une réponse d'erreur
void Server::sendErrorResponse(int client_fd, int errorCode) {
	std::string errorMessage = getErrorMessage(errorCode);
	std::string errorPage = generateErrorPage(errorCode, errorMessage);

	std::string response = "HTTP/1.1 " + std::to_string(errorCode) + " " + errorMessage + "\r\n";
	response += "Content-Type: text/html\r\n";
	response += "Content-Length: " + std::to_string(errorPage.size()) + "\r\n\r\n";
	response += errorPage;

	write(client_fd, response.c_str(), response.size());
}

// Méthode pour gérer la requête HTTP en fonction de la méthode
void Server::handleHttpRequest(int client_fd, const HTTPRequest& request) {
	if (request.getMethod() == "GET") {
		handleGetOrPostRequest(client_fd, request);
	} else if (request.getMethod() == "POST") {
		handleGetOrPostRequest(client_fd, request);
	} else if (request.getMethod() == "DELETE") {
		handleDeleteRequest(client_fd, request);
	} else {
		sendErrorResponse(client_fd, 405);
	}
}

// Méthode pour gérer les requêtes GET et POST
void Server::handleGetOrPostRequest(int client_fd, const HTTPRequest& request) {
	std::string fullPath = _config.root + request.getPath();  // Utilisation du chemin racine de _config
	if (fullPath.find(".cgi") != std::string::npos) {
		if (access(fullPath.c_str(), F_OK) == -1) {
			sendErrorResponse(client_fd, 404);
		} else {
			CGIHandler cgiHandler(*this);
			std::string cgiOutput = cgiHandler.executeCGI(fullPath, request);
			write(client_fd, cgiOutput.c_str(), cgiOutput.length());
		}
	} else {
		serveStaticFile(client_fd, fullPath);
	}
}

// Méthode pour gérer les requêtes DELETE
void Server::handleDeleteRequest(int client_fd, const HTTPRequest& request) {
	std::string fullPath = _config.root + request.getPath();  // Utilisation du chemin racine de _config
	if (access(fullPath.c_str(), F_OK) == -1) {
		sendErrorResponse(client_fd, 404);
	} else {
		if (remove(fullPath.c_str()) == 0) {
			std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
			response += "<html><body><h1>File deleted successfully</h1></body></html>";  // Ajout du message de confirmation
			write(client_fd, response.c_str(), response.size());
		} else {
			sendErrorResponse(client_fd, 500);
		}
	}
}

// Méthode pour servir les fichiers statiques

void Server::serveStaticFile(int client_fd, const std::string& filePath) {
	struct stat pathStat;
	if (stat(filePath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
		// Le chemin est un répertoire, chercher un fichier index

		std::string indexPath;
		bool indexFound = false;

		// Rechercher dans les locations si un index est défini
		for (size_t i = 0; i < _config.locations.size(); ++i) {
			const Location& location = _config.locations[i];
			if (filePath.find(location.path) == 0) {  // Si la location correspond au chemin
				std::map<std::string, std::string>::const_iterator it = location.options.find("index");
				if (it != location.options.end()) {
					indexPath = filePath + "/" + it->second;  // Utiliser l'index défini dans la location
					indexFound = true;
					break;
				}
			}
		}

		// Si aucun index n'a été trouvé dans les locations, utiliser l'index par défaut du serveur
		if (!indexFound) {
			indexPath = filePath + "/" + _config.index;
		}

		// Vérifier si l'index existe
		if (access(indexPath.c_str(), F_OK) != -1) {
			// L'index existe, on le sert
			std::ifstream file(indexPath.c_str());
			if (file) {
				std::stringstream buffer;
				buffer << file.rdbuf();
				std::string content = buffer.str();

				std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
				response += "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n";
				response += content;
				write(client_fd, response.c_str(), response.size());
			} else {
				sendErrorResponse(client_fd, 404);  // Si l'index ne peut pas être lu
			}
		} else {
			sendErrorResponse(client_fd, 404);  // Aucun index trouvé
		}
	} else {
		// Le chemin n'est pas un répertoire, on essaie de servir le fichier directement
		std::ifstream file(filePath.c_str());
		if (file) {
			std::stringstream buffer;
			buffer << file.rdbuf();
			std::string content = buffer.str();

			std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
			response += "Content-Length: " + std::to_string(content.size()) + "\r\n\r\n";
			response += content;
			write(client_fd, response.c_str(), response.size());
		} else {
			sendErrorResponse(client_fd, 404);  // Fichier non trouvé
		}
	}
}


void Server::stockClientsSockets(Socket& socket) {
	int add_size = sizeof(socket.getAddress());

	int client_fd = accept(socket.getSocket(), (struct sockaddr *)&socket.getAddress(), (socklen_t*)&add_size);
	if (client_fd == -1) {
		std::cerr << "Failed to accept an incoming connection. errno: " << strerror(errno) << std::endl;
		return;
	}

	std::cout << "Connection accepted with client_fd: " << client_fd << std::endl;

	if (client_fd >= 0) {
		_clientsFd.push_back(client_fd);
	} else {
		std::cerr << "Invalid client file descriptor: " << client_fd << std::endl;
	}
}
