#include <fstream>
#include "SessionManager.hpp"
#include "Server.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "CGIHandler.hpp"
#include "ServerConfig.hpp"
#include <sys/stat.h>  // Pour utiliser la fonction stat
#include <fcntl.h>
#include <time.h>

Server::Server(const ServerConfig& config) : _config(config) {
	if (!_config.isValid()) {
		std::cerr << "Server configuration is invalid." << std::endl;
	} else {
		std::cout << "Server configuration is valid." << std::endl;
	}
}

Server::~Server() {}

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

std::string getSorryPath() {
	srand(time(NULL));
	int num = rand() % 6;
	num++;
	return "images/" + to_string(num) + "-sorry.gif";
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

			return "";
		}
		std::cerr << "Error reading from client: " << strerror(errno) << std::endl;
		return "";
	}

	return std::string(buffer, bytes_received);
}

void Server::sendResponse(int client_fd, HTTPResponse response) {
	std::string responseString = response.toString();
	write(client_fd, responseString.c_str(), responseString.size());
}

void Server::handleHttpRequest(int client_fd, const HTTPRequest& request,
							   HTTPResponse& response) {
	// Vérifier si le Host correspond à l'un des server_names
	std::string hostHeader = request.getHost();
	if (!hostHeader.empty()) {
		// Extraire le nom d'hôte et le port s'il est présent
		size_t colonPos = hostHeader.find(':');
		std::string hostName = (colonPos != std::string::npos)
								   ? hostHeader.substr(0, colonPos)
								   : hostHeader;
		std::string portStr = (colonPos != std::string::npos)
								  ? hostHeader.substr(colonPos + 1)
								  : "";

		bool hostMatch = false;
		for (size_t i = 0; i < _config.serverNames.size(); ++i) {
			if (hostName == _config.serverNames[i]) {
				hostMatch = true;
				break;
			}
		}

		if (!hostMatch) {
			sendErrorResponse(client_fd, 404);  // Not Found
			return;
		}

		// Optionnel : Vérifier le port
		if (!portStr.empty()) {
			int port = std::atoi(portStr.c_str());
			if (port != _config.ports.at(0)) {
				sendErrorResponse(client_fd, 404);  // Not Found
				return;
			}
		}
	} else {
		sendErrorResponse(client_fd, 400);  // Bad Request
		return;
	}

	// Traiter la requête en fonction de la méthode
	if (request.getMethod() == "GET" || request.getMethod() == "POST") {
		handleGetOrPostRequest(client_fd, request, response);
	} else if (request.getMethod() == "DELETE") {
		handleDeleteRequest(client_fd, request);
	} else {
		sendErrorResponse(client_fd, 405);  // Méthode non autorisée
	}
}

void Server::handleGetOrPostRequest(int client_fd, const HTTPRequest& request,
									HTTPResponse& response) {
	std::string fullPath = _config.root + request.getPath();
	// Changer ça pour détecter des vraies extensions de fichiers...
	if (fullPath.find(".cgi") != std::string::npos) {
		if (access(fullPath.c_str(), F_OK) == -1) {
			sendErrorResponse(client_fd, 404);
		} else {
			CGIHandler cgiHandler;
			std::string cgiOutput = cgiHandler.executeCGI(fullPath, request);
			write(client_fd, cgiOutput.c_str(), cgiOutput.length());
		}
	} else {
		serveStaticFile(client_fd, fullPath, response);
	}
}

void Server::handleDeleteRequest(int client_fd, const HTTPRequest& request) {
	std::string fullPath = _config.root + request.getPath();
	HTTPResponse response;
	if (access(fullPath.c_str(), F_OK) == -1) {
		sendErrorResponse(client_fd, 404);
	} else {
		if (remove(fullPath.c_str()) == 0) {
			response.setStatusCode(200);
			response.setReasonPhrase("OK");
			response.setHeader("Content-Type", "text/html");
			std::string body = "<html><body><h1>File deleted successfully</h1></body></html>";
			response.setHeader("Content-Length", to_string(body.size()));
			response.setBody(body);

			sendResponse(client_fd, response);
		} else {
			sendErrorResponse(client_fd, 500);
		}
	}
}

void Server::serveStaticFile(int client_fd, const std::string& filePath,
							 HTTPResponse& response) {
	struct stat pathStat;
	if (stat(filePath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
		// Gérer les répertoires en cherchant un fichier index
		std::string indexPath = filePath + "/" + _config.index;
		if (access(indexPath.c_str(), F_OK) != -1) {
			serveStaticFile(client_fd, indexPath, response);
		} else {
			sendErrorResponse(client_fd, 404);
		}
	} else {
		std::ifstream file(filePath.c_str(), std::ios::binary);
		if (file) {
			std::stringstream buffer;
			buffer << file.rdbuf();
			std::string content = buffer.str();

			response.setStatusCode(200);
			response.setReasonPhrase("OK");

			// Déterminer le type de contenu
			std::string contentType = "text/html";
			size_t extPos = filePath.find_last_of('.');
			if (extPos != std::string::npos) {
				std::string extension = filePath.substr(extPos);
				if (extension == ".css")
					contentType = "text/css";
				else if (extension == ".js")
					contentType = "application/javascript";
				else if (extension == ".png")
					contentType = "image/png";
				else if (extension == ".jpg" || extension == ".jpeg")
					contentType = "image/jpeg";
				else if (extension == ".gif")
					contentType = "image/gif";
				// Ajoutez d'autres types si nécessaire
			}

			response.setHeader("Content-Type", contentType);
			response.setHeader("Content-Length", to_string(content.size()));

			// Gestion des cookies si nécessaire
			// response.setHeader("Set-Cookie", "theme=light; Path=/;");

			response.setBody(content);

			std::cout << std::endl
					  << "Set-Cookie header : " << response.getStrHeader("Set-Cookie")
					  << std::endl
					  << std::endl;

			sendResponse(client_fd, response);
		} else {
			sendErrorResponse(client_fd, 404);
		}
	}
}

int Server::acceptNewClient(int server_fd) {
	std::cout << "Tentative d'acceptation d'une nouvelle connexion sur le socket FD: "
			  << server_fd << std::endl;

	if (server_fd <= 0) {
		std::cerr << "Invalid server FD: " << server_fd << std::endl;
		return -1;
	}

	sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(client_addr));
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
	if (client_fd == -1) {
		std::cerr << "Erreur lors de l'acceptation de la connexion : "
				  << strerror(errno) << " (errno " << errno << ")" << std::endl;
		return -1;
	}

	return client_fd;
}

void Server::handleClient(int client_fd) {
	if (client_fd <= 0) {
		std::cerr << "Invalid client FD: " << client_fd << std::endl;
		return;
	}

	std::string requestString = receiveRequest(client_fd);
	if (requestString.empty()) {
		std::cerr << "Erreur lors de la réception de la requête." << std::endl;
		return;
	}

	HTTPRequest request;
	HTTPResponse response;

	if (!request.parse(requestString)) {
		sendErrorResponse(client_fd, 400);  // Mauvaise requête
		return;
	}

	SessionManager session(request.getStrHeader("Cookie"));
	if (session.getFirstCon())
		response.setHeader("Set-Cookie", session.getSessionId() + "; Path=/; HttpOnly");

	handleHttpRequest(client_fd, request, response);
}

void Server::sendErrorResponse(int client_fd, int errorCode) {
	HTTPResponse response;
	response.setStatusCode(errorCode);

	std::string errorContent;
	std::map<int, std::string>::const_iterator it = _config.errorPages.find(errorCode);
	if (it != _config.errorPages.end()) {
		std::string errorPagePath = _config.root + it->second;  // Chemin complet
		std::ifstream errorFile(errorPagePath.c_str(), std::ios::binary);
		if (errorFile) {
			std::stringstream buffer;
			buffer << errorFile.rdbuf();
			errorContent = buffer.str();
		} else {
			std::cerr << "Impossible d'ouvrir le fichier d'erreur personnalisé : "
					  << errorPagePath << std::endl;
			// Laisser errorContent vide pour utiliser la page d'erreur par défaut
		}
	}

	// Passer errorContent à beError, qui utilisera la page par défaut si errorContent
	// est vide
	response.beError(errorCode, errorContent);
	sendResponse(client_fd, response);
}
