// Server.hpp
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
#include "HTTPResponse.hpp"

class Socket;

class Server
{
private:
    const ServerConfig& _config;

    std::string receiveRequest(int client_fd);
    void sendResponse(int client_fd, HTTPResponse response);
    void handleHttpRequest(int client_fd, const HTTPRequest& request, HTTPResponse& response);
    void handleGetOrPostRequest(int client_fd, const HTTPRequest& request, HTTPResponse& response);
    void handleDeleteRequest(int client_fd, const HTTPRequest& request);
    void serveStaticFile(int client_fd, const std::string& filePath, HTTPResponse& response);

    // Ajout des méthodes auxiliaires pour gérer les extensions CGI supplémentaires
    bool hasCgiExtension(const std::string& path) const;
    bool endsWith(const std::string& str, const std::string& suffix) const;

public:
    // Constructeur pour inclure ServerConfig
    Server(const ServerConfig& config);
    ~Server();

    // Méthodes pour la gestion des erreurs et la réception/gestion des requêtes
    void sendErrorResponse(int client_fd, int errorCode);

    // Accepter une nouvelle connexion client
    int acceptNewClient(int server_fd);

    // Gérer les requêtes d'un client connecté
    void handleClient(int client_fd);
};

#endif
