#include "Server.hpp"

#include "SessionManager.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "CGIHandler.hpp"
#include "ServerConfig.hpp"
#include "UploadHandler.hpp"
#include "Logger.hpp"
#include "Utils.hpp"

#include <sstream>
#include <fstream>

#include <fcntl.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

Server::Server(const ServerConfig& config) : _config(config) {
	if (!_config.isValid()) {
        Logger::instance().log(ERROR, "Server configuration is invalid.");
	} else {
        Logger::instance().log(INFO, "Server configuration is valid.");
	}
}

Server::~Server() {}

void setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
        Logger::instance().log(ERROR,std::string("fcntl(F_GETFL) failed: ") + strerror(errno));
		return;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        Logger::instance().log(ERROR,std::string("fcntl(F_GETFL) failed: ") + strerror(errno));

	}
}

std::string getSorryPath() {
	srand(time(NULL));
	int num = rand() % 6;
	num++;
    std::string path = "images/" + to_string(num) + "-sorry.gif";
	return path;
}

void readFromSocket(int client_fd, HTTPRequest& request) {
    char buffer[1024];
    int bytes_received = read(client_fd, buffer, sizeof(buffer));

    if (bytes_received == 0) {
        Logger::instance().log(WARNING, "Client closed the connection: FD " + to_string(client_fd));
        request.setConnectionClosed(true);
    } else if (bytes_received < 0) {
        Logger::instance().log(ERROR, "Error reading from client.");
        request.setConnectionClosed(true);
    } else {
        request._rawRequest.append(buffer, bytes_received);
    }
}


void Server::receiveRequest(int client_fd, HTTPRequest& request) {
    if (client_fd <= 0) {
        Logger::instance().log(ERROR, "Invalid client FD before reading: " + to_string(client_fd));
        return;
    }

    readFromSocket(client_fd, request);
    if (!request.getHeadersParsed()) {
        request.parseRawRequest(_config);
        if (request.getRequestTooLarge()) {
			request.setErrorCode(413);
            return;
        }
    }
    if (request.getRequestTooLarge()) {
		request.setErrorCode(413);
        return;
    }
    if (request.getHeadersParsed()) {
        size_t header_end_pos = request._rawRequest.find("\r\n\r\n") + 4;
        request.setBodyReceived(request._rawRequest.size() - header_end_pos);
        // Check if full body is received
        if (request.getBodyReceived() >= request.getContentLength()) {
            request.setComplete(true);
            Logger::instance().log(INFO, "Full request read.");
            return;
        }
        // Check if body size exceeds maximum
        if (request.getBodyReceived() > static_cast<size_t>(request.getMaxBodySize()) && request.getMaxBodySize()) {
            Logger::instance().log(WARNING, "Request body size exceeds the configured maximum.");
            request.setRequestTooLarge(true);
			request.setErrorCode(413);
            return;
        }
    }
}

void Server::handleHttpRequest(int client_fd, ClientConnection& connection) {
    HTTPRequest& request = *connection.getRequest();
    HTTPResponse* response = connection.getResponse();

    if (!response) {
        response = new HTTPResponse();
        connection.setResponse(response);
    }
    SessionManager  session(request.getStrHeader("Cookie"));
    session.getManager(&request, response, client_fd, session);

    const Location* location = _config.findLocation(request.getPath());

    if (location && !location->allowedMethods.empty()) {
        if (std::find(location->allowedMethods.begin(), location->allowedMethods.end(), request.getMethod()) == location->allowedMethods.end()) {
            response->beError(405); // Method not allowed
            Logger::instance().log(WARNING, "405 error (Forbidden) sent on request : \n" + request.toString());
            return;
        }
    }

    if (location && location->returnCode != 0) {
        response->setStatusCode(location->returnCode);
        response->setHeader("Location", location->returnUrl);
        Logger::instance().log(INFO, "Redirecting " + request.getPath() + " to " + location->returnUrl);
        return;
    }

    std::string hostHeader = request.getHost();
    if (!hostHeader.empty()) {
        size_t colonPos = hostHeader.find(':');
        std::string portStr = (colonPos != std::string::npos) ? hostHeader.substr(colonPos + 1) : "";
        if (!portStr.empty()) {
            int port = std::atoi(portStr.c_str());
            if (std::find(_config.ports.begin(), _config.ports.end(), port) == _config.ports.end()) {
                response->beError(400); // Bad Request
                Logger::instance().log(WARNING, "400 error (Bad request) sent on request : \n" + request.toString());
                return;
            }
        }
    } else {
        response->beError(400); // Bad Request
        Logger::instance().log(WARNING, "400 error (Bad Request) sent on request : \n" + request.toString());
        return;
    }

    // Traitement de la requête selon la méthode
    if (request.getMethod() == "GET" || request.getMethod() == "POST") {
        handleGetOrPostRequest(client_fd, connection);
    } else if (request.getMethod() == "DELETE") {
        handleDeleteRequest(connection);
    } else {
        response->beError(501); // Not implemented method
        Logger::instance().log(WARNING, "501 error (Not Implemented) sent on request : \n" + request.toString());
    }

    response = connection.getResponse();

    std::string connectionHeader = request.getStrHeader("Connection");
    delete connection.getRequest();
    connection.setRequest(NULL);
    bool keepAlive = true;
    // En HTTP/1.1, keep-alive par défaut sauf si Connection: close
    if (!connectionHeader.empty() && (connectionHeader == "close" || connectionHeader == "Close")) {
        keepAlive = false;
    }

    if (response && keepAlive) {
        response->setHeader("Connection", "keep-alive");
    } else if (response) {
        response->setHeader("Connection", "close");
    }

    // Ajouter Content-Length si absent
    if (response && response->getStrHeader("Content-Length").empty()) {
        response->setHeader("Content-Length", to_string(response->getBody().size()));
    }
}

bool Server::hasCgiExtension(const std::string& extension) const {
    for (size_t i = 0; i < _config.cgiExtensions.size(); ++i) {
        if (extension == _config.cgiExtensions[i]) {
            return true;
        }
    }
    return false;
}


bool Server::endsWith(const std::string& str, const std::string& suffix) const {
	if (str.length() >= suffix.length()) {
		return (0 == str.compare(str.length() - suffix.length(), suffix.length(), suffix));
	} else {
		return false;
	}
}

bool Server::isPathAllowed(const std::string& path, const std::string& uploadPath) {
    std::string directoryPath = path.substr(0, path.find_last_of('/'));
    char resolvedDirectoryPath[PATH_MAX];
    char resolvedUploadPath[PATH_MAX];

    if (!realpath(directoryPath.c_str(), resolvedDirectoryPath)) {
        Logger::instance().log(ERROR, "Failed to resolve directory path: " + directoryPath + " Error: " + strerror(errno));
        return false;
    }

    if (!realpath(uploadPath.c_str(), resolvedUploadPath)) {
        Logger::instance().log(ERROR, "Failed to resolve upload path: " + uploadPath + " Error: " + strerror(errno));
        return false;
    }

    std::string directoryPathStr(resolvedDirectoryPath);
    std::string uploadPathStr(resolvedUploadPath);

    Logger::instance().log(DEBUG, "Resolved directory path: " + directoryPathStr);
    Logger::instance().log(DEBUG, "Resolved upload path: " + uploadPathStr);

    return directoryPathStr.find(uploadPathStr) == 0;
}

std::string Server::sanitizeFilename(const std::string& filename) {
    std::string safeFilename;
    for (size_t i = 0; i < filename.size(); ++i) {
        char c = filename[i];
        if (isalnum(c) || c == '.' || c == '_' || c == '-') {
            safeFilename += c;
        } else {
            safeFilename += '_';
        }
    }
    return safeFilename;
}

void Server::handleFileUpload(const HTTPRequest& request, HTTPResponse& response, const std::string& boundary) {
    const Location* location = _config.findLocation(request.getPath());
    if (!location || !location->uploadOn) {
        Logger::instance().log(WARNING, "Upload not allowed for this location.");
        response.beError(403, "Upload not allowed.");
        return;
    }
    if (location->uploadPath.empty()) {
        Logger::instance().log(ERROR, "Upload path not specified for this location.");
        response.beError(403, "Upload path not specified.");
        return;
    }

    std::string uploadDir = location->uploadPath;
    if (!uploadDir.empty() && uploadDir[0] != '/') {
        uploadDir = _config.root + "/" + uploadDir;
    }

    struct stat st;
    if (stat(uploadDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        Logger::instance().log(ERROR, "Upload directory does not exist or is not a directory: " + uploadDir);
        response.beError(404, "Upload directory does not exist.");
        return;
    }

    try {
        UploadHandler uploadHandler(request, response, boundary, uploadDir, _config);
        uploadHandler.handleUpload();
    } catch (const std::exception& e) {
        Logger::instance().log(ERROR, "Exception caught while uploading file");
        return;
    }
}


void Server::handleGetOrPostRequest(int client_fd, ClientConnection& connection) {
    HTTPRequest& request = *connection.getRequest();
    HTTPResponse& response = *connection.getResponse();

    const Location* location = _config.findLocation(request.getPath());

    std::string root = _config.root;
    if (location && !location->root.empty()) {
        root = location->root;
    }

    std::string pathUnderRoot;
    if (location && !location->root.empty() && !location->path.empty()) {
        pathUnderRoot = request.getPath().substr(location->path.length());
    } else {
        pathUnderRoot = request.getPath();
    }

    if (pathUnderRoot.empty() || pathUnderRoot[0] != '/') {
        pathUnderRoot = "/" + pathUnderRoot;
    }

    std::string fullPath = root + pathUnderRoot;

    if (request.getMethod() != "GET" && request.getMethod() != "POST" && request.getMethod() != "DELETE") {
        response.beError(501); // Not Implemented
        Logger::instance().log(WARNING, "501 error (Not Implemented): Method not supported.");
        return;
    }

    bool isFileUpload = false;
    std::string contentType;
    if (request.getMethod() == "POST" && request.hasHeader("Content-Type")) {
        contentType = request.getStrHeader("Content-Type");
        if (contentType.find("multipart/form-data") != std::string::npos) {
            isFileUpload = true;
        }
    }

    if (isFileUpload) {
        if (location && location->uploadOn) {
            size_t boundaryPos = contentType.find("boundary=");
            if (boundaryPos != std::string::npos) {
                std::string boundary = contentType.substr(boundaryPos + 9);
                handleFileUpload(request, response, boundary);
                return;
            } else {
                // Missing boundary in Content-Type
                response.beError(400); // Bad Request
                Logger::instance().log(WARNING, "400 error (Bad Request): Missing boundary in Content-Type header.");
                return;
            }
        } else {
            response.beError(403); // Forbidden
            Logger::instance().log(WARNING, "403 error (Forbidden): Upload not allowed for this location.");
            return;
        }
    }

    std::string extension = getFileExtension(fullPath);
    if (hasCgiExtension(extension)) {
        Logger::instance().log(DEBUG, "CGI extension detected for path: " + fullPath);

        std::string interpreter = getInterpreterForExtension(extension, location);
        if (interpreter.empty()) {
            Logger::instance().log(ERROR, "No interpreter found for extension: " + extension);
            response.beError(500, "No interpreter configured for this CGI extension.");
            return;
        }

        if (access(fullPath.c_str(), F_OK) == -1) {
            Logger::instance().log(DEBUG, "CGI script not found: " + fullPath);
            response.beError(404); // Not Found
        } else {
            CGIHandler* cgiHandler = new CGIHandler(fullPath, interpreter, request);
            connection.setCgiHandler(cgiHandler);
            if (!cgiHandler->startCGI()) {
                response.beError(500, "Unable to start CGI Process");
            } else {
                if (connection.getResponse())
                    delete connection.getResponse();
                connection.setResponse(NULL);
                Logger::instance().log(DEBUG, "Response set to NULL to prevent sending prematurely");
            }
            return;
        }
    }

    if (request.getMethod() == "GET") {
        Logger::instance().log(DEBUG, "Serving static file for path: " + fullPath);
        serveStaticFile(client_fd, fullPath, response, request);
    } else if (request.getMethod() == "POST") {
        Logger::instance().log(INFO, "POST request to static resource.");
        response.beError(418, "POST request targeting static ressource or non-configured CGI");
        return;
    }
}

void Server::handleDeleteRequest(ClientConnection& connection) {
    const HTTPRequest& request = *connection.getRequest();
	std::string fullPath = _config.root + request.getPath();
	HTTPResponse response;
	if (access(fullPath.c_str(), F_OK) == -1) {
        Logger::instance().log(WARNING, "404 error (Not Found) sent on DELETE request for address: \n" + _config.root + request.getPath());
		response.beError(404);
	} else if (access(fullPath.c_str(), W_OK) == -1) {
        Logger::instance().log(WARNING, "403 Forbidden on DELETE request for: " + fullPath);
        response.beError(403, "No permission to delete file : " + request.getPath());
    } else {
		if (remove(fullPath.c_str()) == 0) {
			response.setStatusCode(204);
			response.setHeader("Content-Type", "text/html");
			std::string body = "<html><body><h1>File deleted successfully</h1></body></html>";
			response.setHeader("Content-Length", to_string(body.size()));
			response.setBody(body);
            Logger::instance().log(INFO, "Successful DELETE on resource : " + fullPath);
		} else {
            Logger::instance().log(WARNING, "500 error (Internal Server Error) to DELETE: " + fullPath + ": remove() failed");
			response.beError(500);
		}
	}
    if (connection.getResponse())
        delete connection.getResponse();
    connection.setResponse(new HTTPResponse(response));
}

void Server::serveStaticFile(int client_fd, const std::string& filePath,
                             HTTPResponse& response, const HTTPRequest& request) {
    struct stat pathStat;
    if (stat(filePath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
        Logger::instance().log(INFO, "Request File Path is a directory, searching for an index page...");
        std::string indexPath = filePath + "/" + _config.index;
        if (access(indexPath.c_str(), F_OK) != -1) {
            Logger::instance().log(INFO, "Found index page: " + indexPath);
            serveStaticFile(client_fd, indexPath, response, request);
        } else {
            bool autoindex = _config.autoindex;
            const Location* location = _config.findLocation(request.getPath());
            if (location && location->autoindex != -1) {
                autoindex = (location->autoindex == 1);
            }

            if (autoindex) {
                Logger::instance().log(INFO, "Index page not found. Generating directory listing for: " + filePath);
                std::string directoryListing = generateDirectoryListing(filePath, request.getPath());

                response.setStatusCode(200);
                response.setHeader("Content-Type", "text/html");
                response.setBody(directoryListing);
                response.setHeader("Content-Length", to_string(directoryListing.size()));
            } else {
                Logger::instance().log(INFO, "Index page not found and autoindex is off. Sending 403 Forbidden.");
                response.beError(403);//Forbidden
            }
        }
    } else {
        std::ifstream file(filePath.c_str(), std::ios::binary);
        if (file) {
            Logger::instance().log(INFO, "Serving static file found at: " + filePath);
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();

            response.setStatusCode(200);
            response.setReasonPhrase("OK");

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
            }

            response.setHeader("Content-Type", contentType);
            response.setHeader("Content-Length", to_string(content.size()));
            response.setBody(content);
            Logger::instance().log(DEBUG, "Set-Cookie header: " + response.getStrHeader("Set-Cookie"));
        } else {
            Logger::instance().log(WARNING, "Requested file not found: " + filePath + "; 404 error sent");
            response.beError(404);
        }
    }
}

int Server::acceptNewClient(int server_fd) {
    Logger::instance().log(INFO, "Accepting new Connection on socket FD: " + to_string(server_fd));
	if (server_fd <= 0) {
        Logger::instance().log(ERROR, "Invalid server FD: " + to_string(server_fd));
		return -1;
	}
	sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(client_addr));
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
	if (client_fd == -1) {
        Logger::instance().log(ERROR, std::string("Error while accepting connection: ") + strerror(errno));
		return -1;
	}
    setNonBlocking(client_fd);
	return client_fd;
}

void Server::handleClient(int client_fd, ClientConnection& connection) {
    if (client_fd <= 0) {
        Logger::instance().log(ERROR, "Invalid client FD: " + to_string(client_fd));
        return;
    }

    if (!connection.getRequest())
        connection.setRequest(new HTTPRequest(connection.getServer()->getConfig().clientMaxBodySize));

    receiveRequest(client_fd, *connection.getRequest());

	if (connection.getRequest()->getErrorCode() != 0) {
        HTTPResponse* errorResponse = new HTTPResponse();
        errorResponse->beError(connection.getRequest()->getErrorCode());
        if (connection.getResponse())
            delete connection.getResponse();
        connection.setResponse(errorResponse);
        connection.prepareResponse();
        return;
    }
    if (!connection.getRequest()->isComplete()) {
        // Request is incomplete, return and wait for more data
        return;
    }
    if (!connection.getRequest()->parse()) {
        Logger::instance().log(ERROR, "Failed to parse client request on fd " + to_string(client_fd));
        connection.getRequest()->setErrorCode(400);  // Bad Request
        return;
    }

}

void Server::handleResponseSending(int client_fd, ClientConnection& connection) {
    if (connection.isResponseComplete()) {
        // Response already fully sent; nothing to do
        return;
    }

    int completed = connection.sendResponseChunk(client_fd);
    if (completed == 0) {
        // Response fully sent
        HTTPResponse* res = connection.getResponse();
        std::string connHeader = res->getStrHeader("Connection");

        if (connHeader == "close") {
            Logger::instance().log(INFO, "Response fully sent, closing connection FD: " + to_string(client_fd));
        } else {
            Logger::instance().log(INFO, "Response fully sent, keeping connection alive FD: " + to_string(client_fd));

            int max_body_size = connection.getServer()->getConfig().clientMaxBodySize;
            if (connection.getRequest())
            {
                delete connection.getRequest();
            }
            connection.setRequest(new HTTPRequest(max_body_size));
        }
        connection.setExchangeOver(true);
    } else if (completed == -1) {
        Logger::instance().log(ERROR, "Error while writing to client fd :" + to_string(client_fd) + ". Closing Connection");
        connection.setExchangeOver(true);
    }
}

std::string Server::generateDirectoryListing(const std::string& directoryPath, const std::string& requestPath) {
    std::string listing;
    listing += "<html><head><title>Index of " + requestPath + "</title></head><body>";
    listing += "<h1>Index of " + requestPath + "</h1>";
    listing += "<ul>";

    DIR* dir = opendir(directoryPath.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            std::string name = entry->d_name;

            if (name == "." || name == "..")
                continue;

            std::string fullPath = requestPath;
            if (!fullPath.empty() && fullPath.size() - 1 != '/')
                fullPath += "/";
            fullPath += name;

            std::string displayName = name;
            std::string filePath = directoryPath + "/" + name;
            struct stat st;
            if (stat(filePath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                displayName += "/";
                fullPath += "/";
            }

            listing += "<li><a href=\"" + fullPath + "\">" + displayName + "</a></li>";
        }
        closedir(dir);
    } else {
        Logger::instance().log(ERROR, "Failed to open directory: " + directoryPath);
        listing += "<p>Unable to access directory.</p>";
    }

    listing += "</ul></body></html>";
    return listing;
}

const ServerConfig& Server::getConfig() const {return _config;}

std::string Server::getFileExtension(const std::string& path) const {
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos) {
        return path.substr(dotPos); // Includes the dot
    }
    return "";
}


std::string Server::getInterpreterForExtension(const std::string& extension, const Location* location) const {
    Logger::instance().log(DEBUG, "Looking for interpreter for extension: '" + extension + "'");

    // First, check if the location has an interpreter for the extension
    if (location) {
        Logger::instance().log(DEBUG, "Checking Location block for interpreter.");
        for (std::map<std::string, std::string>::const_iterator it = location->cgiInterpreters.begin();
             it != location->cgiInterpreters.end(); ++it) {
            Logger::instance().log(DEBUG, "Location cgiInterpreters key: '" + it->first + "', value: '" + it->second + "'");
        }
        std::map<std::string, std::string>::const_iterator it = location->cgiInterpreters.find(extension);
        if (it != location->cgiInterpreters.end()) {
            Logger::instance().log(DEBUG, "Found interpreter for extension '" + extension + "' in location: " + it->second);
            return it->second;
        }
    }

    // Next, check if the server config has an interpreter for the extension
    Logger::instance().log(DEBUG, "Checking ServerConfig for interpreter.");
    for (std::map<std::string, std::string>::const_iterator it = _config.cgiInterpreters.begin();
         it != _config.cgiInterpreters.end(); ++it) {
        Logger::instance().log(DEBUG, "ServerConfig cgiInterpreters key: '" + it->first + "', value: '" + it->second + "'");
    }
    std::map<std::string, std::string>::const_iterator it = _config.cgiInterpreters.find(extension);
    if (it != _config.cgiInterpreters.end()) {
        Logger::instance().log(DEBUG, "Found interpreter for extension '" + extension + "' in server config: " + it->second);
        return it->second;
    }

    // Interpreter not found
    Logger::instance().log(DEBUG, "No interpreter found for extension: '" + extension + "'");
    return "";
}
