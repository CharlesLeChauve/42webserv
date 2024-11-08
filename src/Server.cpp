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

std::string Server::receiveRequest(int client_fd) {
    if (client_fd <= 0) {
        Logger::instance().log(ERROR, "Invalid client FD before reading: " + to_string(client_fd));
        return "";
    }

    std::string request;
    char buffer[1024];
    int bytes_received;
    size_t content_length = 0;
    bool headers_received = false;
    size_t body_received = 0;
    this->requestTooLarge = false;

    // Par défaut, utiliser la limite définie dans ServerConfig
    int max_body_size = _config.clientMaxBodySize;

    if (max_body_size <= 0) {
        // Si aucune limite définie, on utilise une limite très élevée par défaut
        max_body_size = std::numeric_limits<int>::max();
    }

    std::string request_line;
    std::string path;

    while (true) {
        bytes_received = read(client_fd, buffer, sizeof(buffer));
        if (bytes_received == 0) {
            Logger::instance().log(WARNING, "Client closed the connection: FD " + to_string(client_fd));
            break;
        } else if (bytes_received < 0) {
            Logger::instance().log(ERROR, "Error reading from client. Please check the connection.");
            break;
        }

        request.append(buffer, bytes_received);

        if (!headers_received) {
            size_t header_end_pos = request.find("\r\n\r\n");
            if (header_end_pos != std::string::npos) {
                headers_received = true;

                // Extraire la ligne de requête pour obtenir le chemin
                size_t line_end_pos = request.find("\r\n");
                if (line_end_pos != std::string::npos) {
                    request_line = request.substr(0, line_end_pos);
                    std::istringstream iss(request_line);
                    std::string method;
                    iss >> method >> path;

                    // Extraire la Location correspondante et sa limite `client_max_body_size`
                    const Location* location = _config.findLocation(path);
                    if (location && location->clientMaxBodySize != -1) {
                        max_body_size = location->clientMaxBodySize;
                    }
                }

                // Parse headers to find Content-Length
                std::string headers = request.substr(0, header_end_pos);
                std::istringstream headers_stream(headers);
                std::string header_line;
                while (std::getline(headers_stream, header_line)) {
                    if (!header_line.empty() && header_line[header_line.size() - 1] == '\r') {
                        header_line.erase(header_line.size() - 1);
                    }
                    size_t colon_pos = header_line.find(":");
                    if (colon_pos != std::string::npos) {
                        std::string header_name = header_line.substr(0, colon_pos);
                        std::string header_value = header_line.substr(colon_pos + 1);
                        // Trim whitespace
                        header_name.erase(0, header_name.find_first_not_of(" \t"));
                        header_name.erase(header_name.find_last_not_of(" \t") + 1);
                        header_value.erase(0, header_value.find_first_not_of(" \t"));
                        header_value.erase(header_value.find_last_not_of(" \t") + 1);

                        if (header_name == "Content-Length") {
                            content_length = static_cast<size_t>(atoi(header_value.c_str()));
                            break;
                        }
                    }
                }

                // Log pour vérifier la taille de `Content-Length` et `max_body_size`
                Logger::instance().log(DEBUG, "Content-Length detected: " + to_string(content_length));
                Logger::instance().log(DEBUG, "Configured client_max_body_size: " + to_string(max_body_size));

                // Vérifier si Content-Length dépasse la limite
                if (content_length > static_cast<size_t>(max_body_size)) {
                    Logger::instance().log(WARNING, "Content-Length exceeds the configured maximum.");
                    requestTooLarge = true;
                    break;
                }

                // Si pas de corps, on peut arrêter la lecture
                if (content_length == 0) {
                    break;
                }
            }
        } else {
            // Nous avons déjà reçu les en-têtes
            body_received = request.size() - request.find("\r\n\r\n") - 4;

            // Vérifier si la taille du corps dépasse la limite
            if (body_received > static_cast<size_t>(max_body_size)) {
                Logger::instance().log(WARNING, "Request body size exceeds the configured maximum.");
                requestTooLarge = true;
                break;
            }

            // Vérifier si nous avons reçu tout le corps
            if (body_received >= content_length) {
                break;
            }
        }
    }

    if (requestTooLarge) {
        // Enregistrer que la requête est trop grande pour la traiter
        this->requestTooLarge = true;
        return "";
    }

    Logger::instance().log(INFO, "Full request read.");
    return request;
}

void Server::sendResponse(int client_fd, HTTPResponse response) {
	std::string responseString = response.toString();
	int bytes_written = write(client_fd, responseString.c_str(), responseString.size()); // Check error
    if (bytes_written == -1) {
        sendErrorResponse(client_fd, 500); // Internal server error
        Logger::instance().log(WARNING, "500 error (Internal Server Error) for failing to send response");
    }
    Logger::instance().log(WARNING, "Response sent to client; No verif on write : \n" + response.toStringHeaders());
}

void Server::handleHttpRequest(int client_fd, const HTTPRequest& request, HTTPResponse& response) {
    const Location* location = _config.findLocation(request.getPath());

    if (location && !location->allowedMethods.empty()) {
        if (std::find(location->allowedMethods.begin(), location->allowedMethods.end(), request.getMethod()) == location->allowedMethods.end()) {
            sendErrorResponse(client_fd, 405); // Méthode non autorisée
            Logger::instance().log(WARNING, "405 error (Forbidden) sent on request : \n" + request.toString());
            return;
        }
    }

	if (location && location->returnCode != 0) {
        response.setStatusCode(location->returnCode);
        response.setHeader("Location", location->returnUrl);
        sendResponse(client_fd, response);
        Logger::instance().log(INFO, "Redirecting " + request.getPath() + " to " + location->returnUrl);
        return;
    }

    // Vérification de l'hôte et du port (code existant)
    std::string hostHeader = request.getHost();
    if (!hostHeader.empty()) {
        size_t colonPos = hostHeader.find(':');
        std::string hostName = (colonPos != std::string::npos) ? hostHeader.substr(0, colonPos) : hostHeader;
        std::string portStr = (colonPos != std::string::npos) ? hostHeader.substr(colonPos + 1) : "";

        if (!portStr.empty()) {
            int port = std::atoi(portStr.c_str());
            if (port != _config.ports.at(0)) {
                sendErrorResponse(client_fd, 404); // Not Found
                Logger::instance().log(WARNING, "404 error (Not Found) sent on request : \n" + request.toString());
                return;
            }
        }
    } else {
        sendErrorResponse(client_fd, 400); // Mauvaise requête
        Logger::instance().log(WARNING, "400 error (Bad Request) sent on request : \n" + request.toString());
        return;
    }

    // Traitement de la requête selon la méthode
    if (request.getMethod() == "GET" || request.getMethod() == "POST") {
        handleGetOrPostRequest(client_fd, request, response);
    } else if (request.getMethod() == "DELETE") {
        handleDeleteRequest(client_fd, request);
    } else {
        sendErrorResponse(client_fd, 501); // Not implemented method
        Logger::instance().log(WARNING, "501 error (Not Implemented) sent on request : \n" + request.toString());
    }
}

bool Server::hasCgiExtension(const std::string& path) const {
    // Logger le message initial
    std::ostringstream oss;
    oss << "CGI Extensions for this server:" << std::endl;
    for (size_t i = 0; i < _config.cgiExtensions.size(); ++i) {
        oss << " - \"" << _config.cgiExtensions[i] << "\"";
    }

    for (size_t i = 0; i < _config.cgiExtensions.size(); ++i) {
        if (endsWith(path, _config.cgiExtensions[i])) {
            oss << "hasCgiExtension: Matched extension " << _config.cgiExtensions[i]
                << " for path " << path;
            return true;
        }
    }

    oss << "hasCgiExtension: No matching CGI extension for path " << path << std::endl;
    Logger::instance().log(DEBUG, oss.str());
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
    std::string requestBody = request.getBody();
    std::string boundaryMarker = "--" + boundary;
    std::string endBoundaryMarker = boundaryMarker + "--";
    std::string filename;
    size_t pos = 0;
    size_t endPos = 0;

    const Location* location = _config.findLocation(request.getPath());
    if (!location || !location->uploadOn) {
        Logger::instance().log(ERROR, "Upload not allowed for this location.");
        response.setStatusCode(403);
        response.setBody("Upload not allowed.");
        return;
    }
    if (location->uploadPath.empty()) {
        Logger::instance().log(ERROR, "Upload path not specified for this location.");
        response.setStatusCode(403);
        response.setBody("Upload path not specified.");
        return;
    }

    // Prepend _config.root to uploadPath if it's a relative path
    std::string uploadDir = location->uploadPath;
    if (!uploadDir.empty() && uploadDir[0] != '/') {
        uploadDir = _config.root + "/" + uploadDir;
    }

    // Ensure the upload directory exists
    struct stat st;
    if (stat(uploadDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        Logger::instance().log(ERROR, "Upload directory does not exist or is not a directory: " + uploadDir);
        response.setStatusCode(500);
        response.setBody("Internal Server Error: Upload directory does not exist.");
        return;
    }

    while (true) {
        pos = requestBody.find(boundaryMarker, endPos);
        if (pos == std::string::npos) {
            Logger::instance().log(DEBUG, "No more parts to process.");
            break;
        }
        pos += boundaryMarker.length();

        if (requestBody.substr(pos, 2) == "--") {
            Logger::instance().log(DEBUG, "End of multipart data.");
            break;
        }
        if (requestBody.substr(pos, 2) == "\r\n") {
            pos += 2;
        }

        // Extract headers of the part
        size_t headersEnd = requestBody.find("\r\n\r\n", pos);
        if (headersEnd == std::string::npos) {
            Logger::instance().log(WARNING, "Missing \\r\\n\\r\\n in request for Upload");
            response.setStatusCode(400);
            response.setBody("Bad Request: Missing headers in request for Upload");
            return;
        }
        std::string partHeaders = requestBody.substr(pos, headersEnd - pos);
        pos = headersEnd + 4; // Position of the beginning of the content

        // Find the next boundary
        endPos = requestBody.find(boundaryMarker, pos);
        if (endPos == std::string::npos) {
            Logger::instance().log(ERROR, "End Boundary Marker not found.");
            response.setStatusCode(400);
            response.setBody("Bad Request: End Boundary Marker not found.");
            return;
        }
        size_t contentEnd = endPos;

        // Remove any \r\n before the boundary
        if (requestBody.substr(contentEnd - 2, 2) == "\r\n") {
            contentEnd -= 2;
        }

        // Extract the content of the part
        std::string partContent = requestBody.substr(pos, contentEnd - pos);

        // Check if it's a file
        if (partHeaders.find("Content-Disposition") != std::string::npos &&
            partHeaders.find("filename=\"") != std::string::npos)
        {
            size_t filenamePos = partHeaders.find("filename=\"") + 10;
            size_t filenameEnd = partHeaders.find("\"", filenamePos);
            filename = partHeaders.substr(filenamePos, filenameEnd - filenamePos);

            // Sanitize filename to prevent directory traversal attacks
            filename = sanitizeFilename(filename);

            // Construct the destination path
            std::string destPath = uploadDir + "/" + filename;

            if (!isPathAllowed(destPath, uploadDir)) {
                Logger::instance().log(ERROR, "Attempt to upload outside of allowed path.");
                response.setStatusCode(403);
                response.setBody("Attempt to upload outside of allowed path.");
                return;
            }

            try {
                // Save the file to the authorized path
                UploadHandler uploadHandler(destPath, partContent, _config);
            } catch (const std::exception& e) {
                Logger::instance().log(ERROR, std::string("Error while saving file: ") + e.what());
                response.setStatusCode(500);
                response.setBody("Internal Server Error: Error during file upload.");
                return;
            }
        } else {
            Logger::instance().log(ERROR, std::string("Error while parsing the file in the request:") + partHeaders);
            response.setStatusCode(400);
            response.setBody("Bad Request: File not found.");
            return;
        }
        endPos = endPos + boundaryMarker.length();
    }
    response.setStatusCode(201);
    std::string script = "<script type=\"text/javascript\">"
                         "setTimeout(function() {"
                         "    window.location.href = 'index.html';"
                         "}, 3500);"
                         "</script>";
    response.setBody(script + "<html><body><h1>File successfully uploaded, you'll be redirected on HomePage</h1></body></html>");
    Logger::instance().log(INFO, "Successfully uploaded file: " + filename + " to " + uploadDir);
}



void Server::handleGetOrPostRequest(int client_fd, const HTTPRequest& request, HTTPResponse& response) {
    std::string fullPath = _config.root + request.getPath();

    // Log pour vérifier le chemin complet
    Logger::instance().log(DEBUG, "handleGetOrPostRequest: fullPath = " + fullPath);

    // Trouver la Location correspondante
    const Location* location = _config.findLocation(request.getPath());

    if (request.getMethod() == "POST") {
        bool isFileUpload = false;
        std::string contentType;
        if (request.hasHeader("Content-Type")) {
            contentType = request.getStrHeader("Content-Type");
            if (contentType.find("multipart/form-data") != std::string::npos) {
                isFileUpload = true;
            }
        }

        if (isFileUpload) {
            // Gestion de l'upload de fichier
            if (location && location->uploadOn) {
                size_t boundaryPos = contentType.find("boundary=");
                if (boundaryPos != std::string::npos) {
                    std::string boundary = contentType.substr(boundaryPos + 9);
                    handleFileUpload(request, response, boundary);

                    // Envoyer la réponse
                    sendResponse(client_fd, response);
                    return;
                } else {
                    // Boundary manquant dans Content-Type
                    sendErrorResponse(client_fd, 400); // Bad Request
                    Logger::instance().log(WARNING, "400 error (Bad Request): Missing boundary in Content-Type header.");
                    return;
                }
            } else {
                // Upload non autorisé dans cette location
                sendErrorResponse(client_fd, 403); // Forbidden
                Logger::instance().log(WARNING, "403 error (Forbidden): Upload not allowed for this location.");
                return;
            }
        } else {
            // Traiter les autres requêtes POST (par exemple, les formulaires)
            // Vérifier si le fichier a une extension CGI
            if (hasCgiExtension(fullPath)) {
                Logger::instance().log(DEBUG, "CGI extension detected for path: " + fullPath);
                if (access(fullPath.c_str(), F_OK) == -1) {
                    Logger::instance().log(DEBUG, "CGI script not found: " + fullPath);
                    sendErrorResponse(client_fd, 404); // Not Found
                } else {
                    CGIHandler cgiHandler;
                    std::string cgiOutput = cgiHandler.executeCGI(fullPath, request);

                    int bytes_written = write(client_fd, cgiOutput.c_str(), cgiOutput.length());
                    if (bytes_written == -1) {
                        sendErrorResponse(client_fd, 500); // Internal Server Error
                        Logger::instance().log(WARNING, "500 error (Internal Server Error): Failed to send CGI output response.");
                    }
                    return;
                }
            } else {
                // Si ce n'est pas un script CGI, vous pouvez renvoyer une erreur ou servir un fichier statique
                Logger::instance().log(WARNING, "POST request to non-CGI resource.");
                sendErrorResponse(client_fd, 405); // Method Not Allowed
                return;
            }
        }
    } else if (request.getMethod() == "GET") {
        // Vérifier si le fichier a une extension CGI
        if (hasCgiExtension(fullPath)) {
            Logger::instance().log(DEBUG, "CGI extension detected for path: " + fullPath);
            if (access(fullPath.c_str(), F_OK) == -1) {
                Logger::instance().log(DEBUG, "CGI script not found: " + fullPath);
                sendErrorResponse(client_fd, 404); // Not Found
            } else {
                CGIHandler cgiHandler;
                std::string cgiOutput = cgiHandler.executeCGI(fullPath, request);

                int bytes_written = write(client_fd, cgiOutput.c_str(), cgiOutput.length());
                if (bytes_written == -1) {
                    sendErrorResponse(client_fd, 500); // Internal Server Error
                    Logger::instance().log(WARNING, "500 error (Internal Server Error): Failed to send CGI output response.");
                }
                return;
            }
        } else {
            // Servir le fichier statique
            Logger::instance().log(DEBUG, "No CGI extension detected for path: " + fullPath + ". Serving as static file.");
            serveStaticFile(client_fd, fullPath, response, request);
        }
    } else {
        // Méthode non supportée
        sendErrorResponse(client_fd, 501); // Not Implemented
        Logger::instance().log(WARNING, "501 error (Not Implemented) sent on request: \n" + request.toString());
    }
}





void Server::handleDeleteRequest(int client_fd, const HTTPRequest& request) {
	std::string fullPath = _config.root + request.getPath();
	HTTPResponse response;
	if (access(fullPath.c_str(), F_OK) == -1) {
        Logger::instance().log(WARNING, "404 error (Not Found) sent on DELETE request for address: \n" + _config.root + request.getPath());
		sendErrorResponse(client_fd, 404);
	} else {
		if (remove(fullPath.c_str()) == 0) {
			response.setStatusCode(200);
			response.setReasonPhrase("OK");
			response.setHeader("Content-Type", "text/html");
			std::string body = "<html><body><h1>File deleted successfully</h1></body></html>";
			response.setHeader("Content-Length", to_string(body.size()));
			response.setBody(body);
            Logger::instance().log(INFO, "Successful DELETE on resource : " + fullPath);
			sendResponse(client_fd, response);
		} else {
            Logger::instance().log(WARNING, "500 error (Internal Server Error) to DELETE: " + fullPath + ": remove() failed");
			sendErrorResponse(client_fd, 500);
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
                sendResponse(client_fd, response);
            } else {
                // Si autoindex est désactivé, retourner une erreur 403 Forbidden
                Logger::instance().log(INFO, "Index page not found and autoindex is off. Sending 403 Forbidden.");
                sendErrorResponse(client_fd, 403);
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

            sendResponse(client_fd, response);
        } else {
            Logger::instance().log(WARNING, "Requested file not found: " + filePath + "; 404 error sent");
            sendErrorResponse(client_fd, 404);
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

	return client_fd;
}

void Server::handleClient(int client_fd) {
	if (client_fd <= 0) {
        Logger::instance().log(ERROR, "Invalid client FD: " + to_string(client_fd));
		return;
	}
	std::string requestString = receiveRequest(client_fd);
	if (requestString.empty()) {
        Logger::instance().log(ERROR, "Error receiving request.");
        sendErrorResponse(client_fd, 400); // Request is empty.
		return;
	}

	HTTPRequest request;
	HTTPResponse response;

	if (!request.parse(requestString)) {
        Logger::instance().log(ERROR, "Failed to parse client request on fd" + to_string(client_fd) + "; 400 error (Bad Request) sent");
		sendErrorResponse(client_fd, 400);  // Mauvaise requête
		return;
	}

	SessionManager session(request.getStrHeader("Cookie"));
	if (session.getFirstCon())
		response.setHeader("Set-Cookie", session.getSessionId() + "; Path=/; HttpOnly");

    Logger::instance().log(INFO, "Parsing OK, starting to handle request for client fd: " + to_string(client_fd));
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
        Logger::instance().log(INFO, "Error page found in config file, searching for : " + errorPagePath);
		if (errorFile) {
			std::stringstream buffer;
			buffer << errorFile.rdbuf();
			errorContent = buffer.str();
		} else {
            Logger::instance().log(WARNING, "Failed to open custom error page : " + errorPagePath + "; Serving default");
			// Laisser errorContent vide pour utiliser la page d'erreur par défaut
		}
	}

	// Passer errorContent à beError, qui utilisera la page par défaut si errorContent
	// est vide
	response.beError(errorCode, errorContent);
	sendResponse(client_fd, response);
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
            if (fullPath.back() != '/')
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
