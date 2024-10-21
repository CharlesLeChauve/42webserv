#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <map>
#include <vector>
#include <poll.h>
#include <string>
#include <fstream>
#include "Socket.hpp"
#include "ServerConfig.hpp"
#include "CGIHandler.hpp"
#include "HTTPRequest.hpp"

class Socket;

class Server
{
private:
	std::vector<int> _clientsFd;
	std::vector<pollfd> fds;
	const ServerConfig& _config;

	std::string receiveRequest(int client_fd);
	void sendErrorResponse(int client_fd, int errorCode);
	void handleHttpRequest(int client_fd, const HTTPRequest& request);
	void handleGetOrPostRequest(int client_fd, const HTTPRequest& request);
	void handleDeleteRequest(int client_fd, const HTTPRequest& request);
	void serveStaticFile(int client_fd, const std::string& filePath);

public:
	// Modification du constructeur pour inclure ServerConfig
	Server(const ServerConfig& config);
	~Server();

	// Méthodes publiques pour la gestion des erreurs et la réception/gestion des requêtes
	std::string generateErrorPage(int errorCode, const std::string& errorMessage);
	std::string getErrorMessage(int errorCode);

	void stockClientsSockets(Socket& sockets);
	void receiveAndSend();
};

#endif
