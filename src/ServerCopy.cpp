#include <fstream>
#include "ServerCopy.hpp"
#include "HTTPRequest.hpp"
#include "CGIHandler.hpp"
#include "ServerConfig.hpp"
#include <sys/stat.h>  // Pour utiliser la fonction stat

#include <time.h>

Server::Server(const ServerConfig& config) : _config(config) {
	// Affichage des informations de la configuration du serveur
	std::cout << "Server initialized with the following configuration:" << std::endl;

	// Afficher le nom du serveur
	std::cout << "Server Name: " << _config.serverName << std::endl;

	// Afficher l'hôte et les ports
	std::cout << "Host: " << _config.host << std::endl;
	std::cout << "Ports: ";
	for (size_t i = 0; i < _config.ports.size(); ++i) {
		std::cout << _config.ports[i];
		if (i != _config.ports.size() - 1)
			std::cout << ", ";
	}
	std::cout << std::endl;

	// Afficher le chemin racine et le fichier index
	std::cout << "Root directory: " << _config.root << std::endl;
	std::cout << "Index file: " << _config.index << std::endl;

	// Afficher les pages d'erreur
	std::cout << "Error Pages: " << std::endl;
	for (std::map<int, std::string>::const_iterator it = _config.errorPages.begin(); it != _config.errorPages.end(); ++it) {
		std::cout << "  Error Code " << it->first << ": " << it->second << std::endl;
	}

	// Afficher les locations et leurs options
	std::cout << "Locations: " << std::endl;
	for (size_t i = 0; i < _config.locations.size(); ++i) {
		const Location& location = _config.locations[i];
		std::cout << "  Location path: " << location.path << std::endl;
		for (std::map<std::string, std::string>::const_iterator it = location.options.begin(); it != location.options.end(); ++it) {
			std::cout << "    " << it->first << ": " << it->second << std::endl;
		}
	}

	// Vérifier si la configuration est valide
	if (!_config.isValid()) {
		std::cerr << "Server configuration is invalid." << std::endl;
	} else {
		std::cout << "Server configuration is valid." << std::endl;
	}
}


Server::~Server() {}

#include <fcntl.h>

void setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		std::cerr << "fcntl(F_GETFL) failed: " << strerror(errno) << std::endl;
		return;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cerr << "fcntl(F_SETFL) failed: " << strerror(errno) << std::endl;
	}
}


template <typename T>
std::string to_string(T value) {
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

std::string getSorryPath() {
	srand(time(NULL));
	int num = rand() % 6;
	num++;
	return "images/" + to_string(num) + "-sorry.gif";
}

// Génération de la page d'erreur en utilisant les informations de configuration du serveur
std::string Server::generateErrorPage(int errorCode, const std::string& errorMessage) {
	std::stringstream page;
	page << "<html><head><title>Error " << errorCode << " - " << _config.serverName << "</title>";
	page << "<link rel=\"stylesheet\" href=\"css/err_style.css\"></head>";
	page << "<body><h1>Error " << errorCode << ": " << errorMessage << "</h1>";
	page << "<img src=\"" << getSorryPath() << "\" alt=\"Error Image\">";
	page << "<p>The server encountered an issue processing your request.</p>";
	page << "<p>Server Root: " << _config.root << "</p>";
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
std::string Server::receiveRequest(int client_fd) {
	if (client_fd <= 0) {
		std::cerr << "Invalid client FD before reading: " << client_fd << std::endl;
		return "";
	}

	char buffer[1024] = {0};
	int bytes_received = read(client_fd, buffer, sizeof(buffer) - 1);

	if (bytes_received == 0) {
		std::cerr << "Client closed the connection: FD " << client_fd << std::endl;
		return "";
	} else if (bytes_received < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// Data pas encore disponible, le socket est non-bloquant
			return "";
		}
		std::cerr << "Error reading from client: " << strerror(errno) << std::endl;
		return "";
	}

	// Retourner la chaîne lue
	return std::string(buffer, bytes_received);
}



void Server::sendErrorResponse(int client_fd, int errorCode) {
	std::string errorMessage = getErrorMessage(errorCode);
	std::string errorPage = generateErrorPage(errorCode, errorMessage);

	HTTPResponse response;
	response.setStatusCode(errorCode);
	response.setReasonPhrase(errorMessage);
	response.setHeader("Content-Type", "text/html");
	response.setHeader("Content-Length", to_string(errorPage.size()));
	response.setBody(errorPage);

	std::string responseString = response.toString();
	write(client_fd, responseString.c_str(), responseString.size());
}


// Méthode pour gérer la requête HTTP en fonction de la méthode
void Server::handleHttpRequest(int client_fd, const HTTPRequest& request) {
	std::string fullPath = _config.root + request.getPath();  // Utilisation du chemin racine de _config
	std::cout << std::endl << " Test : " << _config.ports.at(0) << std::endl << std::endl;
	if (request.getMethod() == "GET" || request.getMethod() == "POST") {
		// Utilisez la même méthode pour gérer GET et POST
		handleGetOrPostRequest(client_fd, request);
	} else if (request.getMethod() == "DELETE") {
		handleDeleteRequest(client_fd, request);
	} else {
		sendErrorResponse(client_fd, 405);  // Méthode non autorisée
	}
}


// Méthode pour gérer les requêtes GET et POST
void Server::handleGetOrPostRequest(int client_fd, const HTTPRequest& request) {
	std::string fullPath = _config.root + request.getPath();
	//Changer ca pour détecter des vrais extrensions de fichiers...
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
	std::string fullPath = _config.root + request.getPath();
	if (access(fullPath.c_str(), F_OK) == -1) {
		sendErrorResponse(client_fd, 404);
	} else {
		if (remove(fullPath.c_str()) == 0) {
			HTTPResponse response;
			response.setStatusCode(200);
			response.setReasonPhrase("OK");
			response.setHeader("Content-Type", "text/html");
			std::string body = "<html><body><h1>File deleted successfully</h1></body></html>";
			response.setHeader("Content-Length", to_string(body.size()));
			response.setBody(body);

			std::string responseString = response.toString();
			write(client_fd, responseString.c_str(), responseString.size());
		} else {
			sendErrorResponse(client_fd, 500);
		}
	}
}


// Méthode pour servir les fichiers statiques
void Server::serveStaticFile(int client_fd, const std::string& filePath) {
	struct stat pathStat;
	if (stat(filePath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
		// Gérer les répertoires en cherchant un fichier index
		std::string indexPath = filePath + "/" + _config.index;
		if (access(indexPath.c_str(), F_OK) != -1) {
			serveStaticFile(client_fd, indexPath);
		} else {
			sendErrorResponse(client_fd, 404);
		}
	} else {
		std::ifstream file(filePath.c_str(), std::ios::binary);
		if (file) {
			std::stringstream buffer;
			buffer << file.rdbuf();
			std::string content = buffer.str();

			HTTPResponse response;
			response.setStatusCode(200);
			response.setReasonPhrase("OK");

			// Déterminer le type de contenu
			std::string contentType = "text/html";
			size_t extPos = filePath.find_last_of('.');
			if (extPos != std::string::npos) {
				std::string extension = filePath.substr(extPos);
				if (extension == ".css") contentType = "text/css";
				else if (extension == ".js") contentType = "application/javascript";
				else if (extension == ".png") contentType = "image/png";
				else if (extension == ".jpg" || extension == ".jpeg") contentType = "image/jpeg";
				else if (extension == ".gif") contentType = "image/gif";
				// Ajoutez d'autres types si nécessaire
			}

			response.setHeader("Content-Type", contentType);
			response.setHeader("Content-Length", to_string(content.size()));

			// Gestion des cookies si nécessaire
			// response.setHeader("Set-Cookie", "theme=light; Path=/;");

			response.setBody(content);

			std::string responseString = response.toString();
			write(client_fd, responseString.c_str(), responseString.size());
		} else {
			sendErrorResponse(client_fd, 404);
		}
	}
}



int Server::acceptNewClient(int server_fd) {
	std::cout << "Tentative d'acceptation d'une nouvelle connexion sur le socket FD: " << server_fd << std::endl;

	// Vérification du descripteur de fichier du serveur
	if (server_fd <= 0) {
		std::cerr << "Invalid server FD: " << server_fd << std::endl;
		return -1;
	}

	sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(client_addr));
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
	if (client_fd == -1) {
		std::cerr << "Erreur lors de l'acceptation de la connexion : " << strerror(errno) << " (errno " << errno << ")" << std::endl;
		return -1;
	}

	std::cout << "Connexion acceptée : FD du client : " << client_fd << std::endl;
	std::cout << "Adresse du client : " << inet_ntoa(client_addr.sin_addr) << " Port : " << ntohs(client_addr.sin_port) << std::endl;

	return client_fd;
}





void Server::handleClient(int client_fd) {
	// Vérification que le descripteur client est valide
	if (client_fd <= 0) {
		std::cerr << "Invalid client FD: " << client_fd << std::endl;
		return;
	}

	std::string requestString = receiveRequest(client_fd);
	if (requestString.empty()) {
		std::cerr << "Erreur lors de la réception de la requête." << std::endl;
		// close(client_fd);  // Fermer la connexion si aucune requête n'est reçue
		return;
	}

	HTTPRequest request;
	if (!request.parse(requestString)) {
		sendErrorResponse(client_fd, 400);  // Mauvaise requête
		// close(client_fd);  // Fermer la connexion après avoir envoyé la réponse
		return;
	}

	handleHttpRequest(client_fd, request);
	// close(client_fd);  // Fermer la connexion après avoir traité la requête
}
