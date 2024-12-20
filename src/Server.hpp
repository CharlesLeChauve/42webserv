// Server.hpp
#ifndef SERVER_HPP
#define SERVER_HPP

#include "Socket.hpp"
#include "ServerConfig.hpp"
#include "CGIHandler.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "SessionManager.hpp"
#include "ClientConnection.hpp"

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>

#include <poll.h>

class Socket;

class Server
{
private:
    const ServerConfig& _config;

    void receiveRequest(int client_fd, HTTPRequest& request);
    void handleGetOrPostRequest(int client_fd, ClientConnection& connection);
    void handleDeleteRequest(ClientConnection& connection);
    void serveStaticFile(int client_fd, const std::string& filePath, HTTPResponse& response, const HTTPRequest& request);
    void handleFileUpload(const HTTPRequest& request, HTTPResponse& response, const std::string& boundary);
	bool isPathAllowed(const std::string& path, const std::string& uploadPath);
	std::string sanitizeFilename(const std::string& filename);
	std::string generateDirectoryListing(const std::string& directoryPath, const std::string& requestPath);

    bool hasCgiExtension(const std::string& extension) const;
    bool endsWith(const std::string& str, const std::string& suffix) const;
	std::string getInterpreterForExtension(const std::string& extension, const Location* location) const;
public:
    Server(const ServerConfig& config);
    ~Server();

    void handleHttpRequest(int client_fd, ClientConnection& connection);

    int acceptNewClient(int server_fd);

    void handleClient(int client_fd, ClientConnection& connection);
    void handleResponseSending(int client_fd, ClientConnection& connection);
    const ServerConfig& getConfig() const;
	std::string getFileExtension(const std::string& path) const;
};

#endif
