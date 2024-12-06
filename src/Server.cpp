#include <fstream>
#include "SessionManager.hpp"
#include "Server.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "CGIHandler.hpp"
#include "ServerConfig.hpp"
#include "UploadHandler.hpp"
#include "Logger.hpp"
#include <sys/stat.h>  // Pour utiliser la fonction stat
#include <sstream>
#include <fcntl.h>
#include <time.h>
#include "Utils.hpp"
#include <limits.h>    // Pour PATH_MAX
#include <stdlib.h>    // Pour realpath
#include <dirent.h>


#include <string.h>

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
    Logger::instance().log(DEBUG, "path for error sorry gif : " + path);
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
    if (request.getRequestTooLarge()) {
		request.setErrorCode(413);
        return;
    }
    if (!request.getHeadersParsed()) {
        request.parseRawRequest(_config);
        if (request.getRequestTooLarge()) {
			request.setErrorCode(413);
            return;
        }
    }
    if (request.getHeadersParsed()) {
        // Calculate body received
        size_t header_end_pos = request._rawRequest.find("\r\n\r\n") + 4;
        request.setBodyReceived(request._rawRequest.size() - header_end_pos);
        // Check if full body is received
        if (request.getBodyReceived() >= request.getContentLength()) {
            request.setComplete(true);
            Logger::instance().log(INFO, "Full request read.");
            return; // Full request received
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
    const Location* location = _config.findLocation(request.getPath());

    if (location && !location->allowedMethods.empty()) {
        if (std::find(location->allowedMethods.begin(), location->allowedMethods.end(), request.getMethod()) == location->allowedMethods.end()) {
            response->beError(405); // Méthode non autorisée
            Logger::instance().log(WARNING, "405 error (Forbidden) sent on request : \n" + request.toString());
            return;
        }
    }

	if (location && location->returnCode != 0) {
        response->setStatusCode(location->returnCode);
        response->setHeader("Location", location->returnUrl);
        //sendResponse(client_fd, response);
        Logger::instance().log(INFO, "Redirecting " + request.getPath() + " to " + location->returnUrl);
        return ;
    }

    // Vérification de l'hôte et du port (code existant)
    std::string hostHeader = request.getHost();
	if (!hostHeader.empty()) {
	    size_t colonPos = hostHeader.find(':');
	    std::string hostName = (colonPos != std::string::npos) ? hostHeader.substr(0, colonPos) : hostHeader;
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
        response->beError(400); // Mauvaise requête
        Logger::instance().log(WARNING, "400 error (Bad Request) sent on request : \n" + request.toString());
        return;
    }

    // Traitement de la requête selon la méthode
    if (request.getMethod() == "GET" || request.getMethod() == "POST") {
        handleGetOrPostRequest(client_fd, connection);
    } else if (request.getMethod() == "DELETE") {
        handleDeleteRequest(request);
    } else {
        response->beError(501); // Not implemented method
        Logger::instance().log(WARNING, "501 error (Not Implemented) sent on request : \n" + request.toString());
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
    // Extraire le chemin du répertoire à partir du chemin complet
    std::string directoryPath = path.substr(0, path.find_last_of('/'));

    // Résoudre les chemins absolus
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

    // Logger les chemins résolus pour le débogage
    Logger::instance().log(DEBUG, "Resolved directory path: " + directoryPathStr);
    Logger::instance().log(DEBUG, "Resolved upload path: " + uploadPathStr);

    // Vérifier que le chemin du répertoire commence par le chemin autorisé
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
        Logger::instance().log(ERROR, "Upload not allowed for this location.");
        response.beError(403, "Upload not allowed.");
        return;
    }
    if (location->uploadPath.empty()) {
        Logger::instance().log(ERROR, "Upload path not specified for this location.");
        response.beError(403, "Upload path not specified.");
        return;
    }

    // Préparer le répertoire de téléchargement
    std::string uploadDir = location->uploadPath;
    if (!uploadDir.empty() && uploadDir[0] != '/') {
        uploadDir = _config.root + "/" + uploadDir;
    }

    // Vérifier que le répertoire de téléchargement existe
    struct stat st;
    if (stat(uploadDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        Logger::instance().log(ERROR, "Upload directory does not exist or is not a directory: " + uploadDir);
        response.beError(500, "Internal Server Error: Upload directory does not exist.");
        return;
    }

    try {
        // Déléguer le traitement à UploadHandler
        UploadHandler uploadHandler(request, response, boundary, uploadDir, _config);
        uploadHandler.handleUpload();
    } catch (const std::exception& e) {
        // Logger::instance().log(ERROR, std::string("Error while handling file upload: ") + e.what());
        // response.setStatusCode(500);
        // response.setBody("Internal Server Error: Error during file upload.");
        return;
    }
}


void Server::handleGetOrPostRequest(int client_fd, ClientConnection& connection) {
    HTTPRequest& request = *connection.getRequest();
    HTTPResponse& response = *connection.getResponse();

    // Find the corresponding Location
    const Location* location = _config.findLocation(request.getPath());


	// Determine the root to use
    std::string root = _config.root;
    if (location && !location->root.empty()) {
        root = location->root;
    }

    std::string pathUnderRoot;
    if (location && !location->root.empty() && !location->path.empty()) {
        // La Location a son propre root, on enlève location->path du chemin de la requête
        pathUnderRoot = request.getPath().substr(location->path.length());
    } else {
        // On utilise le root global et le chemin complet de la requête
        pathUnderRoot = request.getPath();
    }

    if (pathUnderRoot.empty() || pathUnderRoot[0] != '/') {
        pathUnderRoot = "/" + pathUnderRoot;
    }

    std::string fullPath = root + pathUnderRoot;

    // Log to verify the complete path
    Logger::instance().log(DEBUG, "handleGetOrPostRequest: fullPath = " + fullPath);

    // Check if the method is supported
    if (request.getMethod() != "GET" && request.getMethod() != "POST" && request.getMethod() != "DELETE") {
        response.beError(501); // Not Implemented
        Logger::instance().log(WARNING, "501 error (Not Implemented): Method not supported.");
        return;
    }

    // Handle file upload for POST requests with multipart/form-data
    bool isFileUpload = false;
    std::string contentType;
    if (request.getMethod() == "POST" && request.hasHeader("Content-Type")) {
        contentType = request.getStrHeader("Content-Type");
        if (contentType.find("multipart/form-data") != std::string::npos) {
            isFileUpload = true;
        }
    }

    if (isFileUpload) {
        // Handle file upload
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
            // Upload not allowed in this location
            response.beError(403); // Forbidden
            Logger::instance().log(WARNING, "403 error (Forbidden): Upload not allowed for this location.");
            return;
        }
    }

    // Check if the file has a CGI extension
       // Check if the file has a CGI extension
    std::string extension = getFileExtension(fullPath);
    if (hasCgiExtension(extension)) {
        Logger::instance().log(DEBUG, "CGI extension detected for path: " + fullPath);

        // Get the interpreter for this extension
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
                connection.setResponse(NULL);
                Logger::instance().log(DEBUG, "Response set to NULL to prevent sending prematurely");
            }
            return;
        }
    }

    // Handle GET and POST methods
    if (request.getMethod() == "GET") {
        // Serve the static file
        Logger::instance().log(DEBUG, "Serving static file for path: " + fullPath);
        serveStaticFile(client_fd, fullPath, response, request);
    } else if (request.getMethod() == "POST") {
        // Handle other POST requests (e.g., forms)
        Logger::instance().log(INFO, "POST request to static resource.");

        // Customize the response here
        response.setStatusCode(200);
        response.setHeader("Content-Type", "text/html");
        response.setBody("<html><body><h1>POST request received</h1></body></html>");
        // sendResponse(client_fd, response);
        return;
    }
}

void Server::handleDeleteRequest(const HTTPRequest& request) {
	std::string fullPath = _config.root + request.getPath();
	HTTPResponse response;
	if (access(fullPath.c_str(), F_OK) == -1) {
        Logger::instance().log(WARNING, "404 error (Not Found) sent on DELETE request for address: \n" + _config.root + request.getPath());
		response.beError(404);
	} else {
		if (remove(fullPath.c_str()) == 0) {
			response.setStatusCode(204);
			response.setHeader("Content-Type", "text/html");
			std::string body = "<html><body><h1>File deleted successfully</h1></body></html>";
			response.setHeader("Content-Length", to_string(body.size()));
			response.setBody(body);
            Logger::instance().log(INFO, "Successful DELETE on resource : " + fullPath);
			//sendResponse(client_fd, response);
		} else {
            Logger::instance().log(WARNING, "500 error (Internal Server Error) to DELETE: " + fullPath + ": remove() failed");
			response.beError(500);
		}
	}
}

void Server::serveStaticFile(int client_fd, const std::string& filePath,
                             HTTPResponse& response, const HTTPRequest& request) {
    struct stat pathStat;
    if (stat(filePath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
        // Vérifier s'il existe un fichier index
        Logger::instance().log(INFO, "Request File Path is a directory, searching for an index page...");
        std::string indexPath = filePath + "/" + _config.index;
        if (access(indexPath.c_str(), F_OK) != -1) {
            Logger::instance().log(INFO, "Found index page: " + indexPath);
            serveStaticFile(client_fd, indexPath, response, request);
        } else {
            // Vérifier la valeur de autoindex
            bool autoindex = _config.autoindex; // Valeur par défaut du serveur
            const Location* location = _config.findLocation(request.getPath());
            if (location && location->autoindex != -1) { // Si défini dans la location
                autoindex = (location->autoindex == 1);
            }

            if (autoindex) {
                // Générer le listing du répertoire
                Logger::instance().log(INFO, "Index page not found. Generating directory listing for: " + filePath);
                std::string directoryListing = generateDirectoryListing(filePath, request.getPath());

                response.setStatusCode(200);
                response.setHeader("Content-Type", "text/html");
                response.setBody(directoryListing);
                response.setHeader("Content-Length", to_string(directoryListing.size()));
                //sendResponse(client_fd, response);
            } else {
                // Si autoindex est désactivé, retourner une erreur 403 Forbidden
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
                // Vous pouvez ajouter d'autres types MIME si nécessaire
            }

            response.setHeader("Content-Type", contentType);
            response.setHeader("Content-Length", to_string(content.size()));
            response.setBody(content);
            Logger::instance().log(DEBUG, "Set-Cookie header: " + response.getStrHeader("Set-Cookie"));

            //sendResponse(client_fd, response);
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

    receiveRequest(client_fd, *connection.getRequest());

	if (connection.getRequest()->getErrorCode() != 0) {
        // Une erreur a été détectée dans receiveRequest
        HTTPResponse* errorResponse = new HTTPResponse();
        errorResponse->beError(connection.getRequest()->getErrorCode());
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
        connection.setExchangeOver(true);
        Logger::instance().log(INFO, "Response fully sent to client FD: " + to_string(client_fd));
    } else if (completed == -1) {
        close(client_fd);
        Logger::instance().log(ERROR, std::string("Error while writing to client fd :") + to_string(client_fd) + ". Closing Connection");
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

            // Ignorer les entrées spéciales "." et ".."
            if (name == "." || name == "..")
                continue;

            std::string fullPath = requestPath;
            if (!fullPath.empty() && fullPath.size() - 1 != '/')
                fullPath += "/";
            fullPath += name;

            // Ajouter un '/' à la fin si c'est un répertoire
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
