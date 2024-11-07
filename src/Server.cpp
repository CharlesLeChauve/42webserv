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
    // Résoudre les chemins absolus
    char resolvedPath[PATH_MAX];
    char resolvedUploadPath[PATH_MAX];

    if (!realpath(path.c_str(), resolvedPath) || !realpath(uploadPath.c_str(), resolvedUploadPath)) {
        return false;
    }

    std::string pathStr(resolvedPath);
    std::string uploadPathStr(resolvedUploadPath);

    // Vérifier que le chemin commence par le chemin autorisé
    return pathStr.find(uploadPathStr) == 0;
}

void Server::handleFileUpload(const HTTPRequest& request, HTTPResponse& response, const std::string& boundary) {
    std::string requestBody = request.getBody();
    std::string boundaryMarker = "--" + boundary;
    std::string endBoundaryMarker = boundaryMarker + "--";
    std::string filename;
    size_t pos = 0;
    size_t endPos = 0;

    const Location* location = _config.findLocation(request.getPath());
    if (!location || location->uploadPath.empty()) {
        Logger::instance().log(ERROR, "Upload path not allowed for this location.");
        response.setStatusCode(403);
        response.setBody("Upload path not allowed.");
        return;
    }

    while (true) {
        pos = requestBody.find(boundaryMarker, endPos);
        if (pos == std::string::npos) {
            Logger::instance().log(DEBUG, "File fully received.");
            break;
        }
        pos += boundaryMarker.length();

        if (requestBody.substr(pos, 2) == "--") {
            break;
        }
        if (requestBody.substr(pos, 2) == "\r\n") {
            pos += 2;
        }

        // Extraire les en-têtes de la partie
        size_t headersEnd = requestBody.find("\r\n\r\n", pos);
        if (headersEnd == std::string::npos) {
            Logger::instance().log(WARNING, "Miss \\r\\n\\r\\n in request for Upload");
            response.setStatusCode(400);
            response.setBody("Bad Request: Miss \\r\\n\\r\\n in request for Upload");
            return;
        }
        std::string partHeaders = requestBody.substr(pos, headersEnd - pos);
        pos = headersEnd + 4; // Position du début du contenu

        // Trouver le prochain boundary
        endPos = requestBody.find(boundaryMarker, pos);
        if (endPos == std::string::npos) {
            Logger::instance().log(ERROR, "End Boundary Marker not found.");
            response.setStatusCode(400);
            response.setBody("Bad Request: End Boundary Marker not found.");
            return;
        }
        size_t contentEnd = endPos;

        // Retirer les éventuels \r\n avant le boundary
        if (requestBody.substr(contentEnd - 2, 2) == "\r\n") {
            contentEnd -= 2;
        }

        // Extraire le contenu de la partie
        std::string partContent = requestBody.substr(pos, contentEnd - pos);

        // Vérifier si c'est un fichier
        if (partHeaders.find("Content-Disposition") != std::string::npos &&
            partHeaders.find("filename=\"") != std::string::npos)
        {
            size_t filenamePos = partHeaders.find("filename=\"") + 10;
            size_t filenameEnd = partHeaders.find("\"", filenamePos);
            filename = partHeaders.substr(filenamePos, filenameEnd - filenamePos);

            std::string destPath = location->uploadPath + "/" + filename;

            if (!isPathAllowed(destPath, location->uploadPath)) {
                Logger::instance().log(ERROR, "Attempt to upload outside of allowed path.");
                response.setStatusCode(403);
                response.setBody("Attempt to upload outside of allowed path.");
                return;
            }

            try {
                // Enregistrer le fichier dans le chemin autorisé
                UploadHandler uploadHandler(destPath, partContent, this->_config);
            } catch (const std::exception& e) {
                Logger::instance().log(ERROR, std::string("Error while opening file: ") + e.what());
                response.setStatusCode(500);
                response.setBody("Internal Server Error: Erreur lors de l'upload du fichier.");
                return;
            }
        } else {
            Logger::instance().log(ERROR, std::string("Error while looking for the file in the request:") + partHeaders);
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
    response.setBody(script + "<head><meta charset=UTF-8></head>Fichier uploadé avec succès, Vous allez être redirigé vers la page d'Accueil.");
    Logger::instance().log(INFO, "Successfully uploaded file: " + filename + " at Address: " + location->uploadPath);
}


void Server::handleGetOrPostRequest(int client_fd, const HTTPRequest& request, HTTPResponse& response) {
    std::string fullPath = _config.root + request.getPath();

    // Log to verify the complete path
    Logger::instance().log(DEBUG, "handleGetOrPostRequest: fullPath =" + fullPath);
    //?? ici, changer le getPath() == pour regarder la config
	if (request.getMethod() == "POST" && request.getPath() == "/uploads" && request.hasHeader("Content-Type")) {
        std::string contentType = request.getStrHeader("Content-Type");
        if (contentType.find("multipart/form-data") != std::string::npos) {
		// Recherche du boundary dans le Content-Type avec find + check if end
		// Si ce n'est pas la fin extraire
			size_t	boundaryPos = contentType.find("boundary=");
			if (boundaryPos != std::string::npos) {
				std::string boundary = contentType.substr(boundaryPos + 9);
				handleFileUpload(request, response, boundary);
                std::string responseText = response.toString(); //?? Check for errors if it's better to use sendErrorResponse instead of toString().
                int bytes_written = write(client_fd, responseText.c_str(), responseText.length());  // Envoie la réponse
                if (bytes_written == -1) {
                    sendErrorResponse(client_fd, 500); // Internal server error
                    Logger::instance().log(WARNING, "500 error (Internal Server Error) to File Upload response for failing to send response");
                }
                return;
			}
		}
		else {
			sendErrorResponse(client_fd, 400); // Bad request
            Logger::instance().log(WARNING, "400 error (Bad Request) sent on request : \n" + request.toString());
        }
	}// Vérifier si le fichier a une extension CGI (.cgi, .sh, .php)
	else if (hasCgiExtension(fullPath)) {
        Logger::instance().log(DEBUG, "CGI extension detected for path: " + fullPath);
        if (access(fullPath.c_str(), F_OK) == -1) {
            Logger::instance().log(DEBUG, "CGI script not found: " + fullPath);
            sendErrorResponse(client_fd, 404);
        } else {
            CGIHandler cgiHandler;
            std::string cgiOutput = cgiHandler.executeCGI(fullPath, request);

            int bytes_written = write(client_fd, cgiOutput.c_str(), cgiOutput.length()); //?? CHECK ERROR : 0 / -1
            if (bytes_written == -1) {
                sendErrorResponse(client_fd, 500); // Internal server error
                Logger::instance().log(WARNING, "500 error (Internal Server Error) to CGI output response for failing to send response");
            }
        }
    } else {
        Logger::instance().log(DEBUG, "No CGI extension detected for path: " + fullPath + ". Serving as static file.");
        serveStaticFile(client_fd, fullPath, response);
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
							 HTTPResponse& response) {
	struct stat pathStat;
	if (stat(filePath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
		// Gérer les répertoires en cherchant un fichier index
        Logger::instance().log(INFO, "Request File Path is a dir, Searching for an index page...");
		std::string indexPath = filePath + "/" + _config.index;
		if (access(indexPath.c_str(), F_OK) != -1) {
            Logger::instance().log(INFO, "Found : " + indexPath);
			serveStaticFile(client_fd, indexPath, response);
		} else {
            Logger::instance().log(INFO, indexPath + " Not found, 404 error (Not Found) sent.");
			sendErrorResponse(client_fd, 404);
		}
	} else {
		std::ifstream file(filePath.c_str(), std::ios::binary);
		if (file) {
            Logger::instance().log(INFO, "Serving static file found at : " + filePath);
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
                //?? Pas de else ?
			}

			response.setHeader("Content-Type", contentType);
			response.setHeader("Content-Length", to_string(content.size()));
			response.setBody(content);
			std::cout << "Set-Cookie header : " << response.getStrHeader("Set-Cookie") << std::endl;

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

