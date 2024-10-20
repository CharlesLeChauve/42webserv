#include <fstream>
#include "Server.hpp"
#include "HTTPRequest.hpp"
#include "CGIHandler.hpp"
#include "ServerConfig.hpp"

Server::Server() {}

Server::~Server() {}

std::string Server::generateErrorPage(int errorCode, const std::string& errorMessage) {
	std::stringstream page;
	page << "<html><head><title>Error " << errorCode << "</title></head>";
	page << "<body><h1>Error " << errorCode << ": " << errorMessage << "</h1>";
	page << "<p>The server encountered an issue processing your request.</p>";
	page << "</body></html>";
	return page.str();
}

std::string Server::getErrorMessage(int errorCode) {
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
	if (request.getMethod() == "GET" || request.getMethod() == "POST") {
		handleGetOrPostRequest(client_fd, request);
	} else if (request.getMethod() == "DELETE") {
		handleDeleteRequest(client_fd, request);
	} else {
		sendErrorResponse(client_fd, 405);
	}
}

// Méthode pour gérer les requêtes GET et POST
void Server::handleGetOrPostRequest(int client_fd, const HTTPRequest& request) {
	std::string fullPath = "www" + request.getPath();
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
	std::string fullPath = "www" + request.getPath();
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
		sendErrorResponse(client_fd, 404);
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
