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
        Logger::instance().log(ERROR,  "Invalid client FD before reading: " + to_string(client_fd));
        return "";
    }

    std::string request;
    char buffer[1024];
    int bytes_received;
    size_t content_length = 0;
    bool headers_received = false;

    while (true) {
        bytes_received = read(client_fd, buffer, sizeof(buffer));
        if (bytes_received == 0) {
            // Client closed the connection
            Logger::instance().log(WARNING, "Client closed the connection: FD " + to_string(client_fd));
            break;
        } else if (bytes_received < 0) {
            // Error reading from client
            Logger::instance().log(ERROR, "Error reading from client. Please check the connection.");
             //?? Handle different types error (status code).
            break;
        }

        Logger::instance().log(DEBUG, std::string("Receiving data..."));
        request.append(buffer, bytes_received);

        if (!headers_received) {
            // Check if we have received the full headers
            size_t header_end_pos = request.find("\r\n\r\n");
            if (header_end_pos != std::string::npos) {
                headers_received = true;

                // Parse headers to find Content-Length
                std::string headers = request.substr(0, header_end_pos);
                std::istringstream headers_stream(headers);
                std::string header_line;
                while (std::getline(headers_stream, header_line)) {
                    // Remove any trailing \r
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

                // If there is a body, calculate how many more bytes to read
                if (content_length > 0) {
                    size_t body_received = request.size() - (header_end_pos + 4);
                    if (body_received >= content_length) {
                        // We have received the entire request
                        break;
                    }
                } else {
                    // No Content-Length, so no body, we are done
                    break;
                }
            }
        } else {
            // We have already received headers, check if we have received the entire body
            size_t header_end_pos = request.find("\r\n\r\n");
            size_t body_received = request.size() - (header_end_pos + 4);
            if (body_received >= content_length) {
                // We have received the entire request
                break;
            }
        }
    }
    Logger::instance().log(INFO, "Full request read.");
    return request;
}

void Server::sendResponse(int client_fd, HTTPResponse response) {
	std::string responseString = response.toString();
	size_t bytes_written = write(client_fd, responseString.c_str(), responseString.size()); // Check error
    if (bytes_written == -1) {
        sendErrorResponse(client_fd, 500); // Internal server error
        Logger::instance().log(WARNING, "500 error (Internal Server Error) to sending response: " + bytes_written + ": failed to send");
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

bool Server::handleFileUpload(const HTTPRequest& request, HTTPResponse& response, const std::string& boundary) {
    std::string requestBody = request.getBody();
    std::string boundaryMarker = "--" + boundary;
    std::string endBoundaryMarker = boundaryMarker + "--";
    std::string filename;
    size_t pos = 0;
    size_t endPos = 0;

    while (true) {
        pos = requestBody.find(boundaryMarker, endPos);
        if (pos == std::string::npos) {
            Logger::instance().log(WARNING, "No Boundary Marker found in body for Upload.");
            response.setStatusCode(400);
            response.setBody("Error - No Boundary Marker found in body for upload");
            return false;
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
            response.setBody("Error - Miss \\r\\n\\r\\n in request for Upload");
            return false;
        }
        std::string partHeaders = requestBody.substr(pos, headersEnd - pos);
        pos = headersEnd + 4; // Position du début du contenu

        // Trouver le prochain boundary
        endPos = requestBody.find(boundaryMarker, pos);
        if (endPos == std::string::npos) {
            Logger::instance().log(ERROR, "End Boundary Marker not found.");
            response.setStatusCode(400);
            response.setBody("End Boundary Marker not found.");
            return false;
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
            try {
                //?? Ici, il faudra utiliser les éléments du conf pour savoir si c'est autorisé, et passer en adresse celle requise par la requete
                UploadHandler uploadHandler("www/uploads/" + filename, partContent, this->_config);
            } catch (const std::exception& e) {
                Logger::instance().log(ERROR, std::string("Error while opening file: ") + e.what());
                response.setStatusCode(500);
                response.setBody("Erreur lors de l'upload du fichier.");
                return false;
            }
        } else {
             //?? Else what ??
        }
        endPos = endPos + boundaryMarker.length();
    }
    response.setStatusCode(201);
	std::string script = "<script type=\"text/javascript\">"
                     "setTimeout(function() {"
                     "    window.location.href = 'index.html';"
                     "}, 3500);"
                     "</script>";
    response.setBody(script + "<head><meta charset=UTF-8></head>Fichier uploadé avec succès, Vous allez etre redirigé vers la page d'Accueil.");
    //?? Enlever www/uploads pour mettre le path requis
    Logger::instance().log(INFO, "Successfully uploaded file: " + filename + " at Address: " + "www/uploads");
    return true;
}

// Modification de la méthode handleGetOrPostRequest
void Server::handleGetOrPostRequest(int client_fd, const HTTPRequest& request, HTTPResponse& response) {
    std::string fullPath = _config.root + request.getPath();

    // Log to verify the complete path
    Logger::instance().log(DEBUG, "handleGetOrPostRequest: fullPath =" + fullPath);

	if (request.getMethod() == "POST" && request.getPath() == "/uploads" && request.hasHeader("Content-Type")) {
        std::string contentType = request.getStrHeader("Content-Type");
        if (contentType.find("multipart/form-data") != std::string::npos) {
		// Recherche du boundary dans le Content-Type avec find + check if end
		// Si ce n'est pas la fin extraire 
			size_t	boundaryPos = contentType.find("boundary=");
			if (boundaryPos != std::string::npos) {
				std::string boundary = contentType.substr(boundaryPos + 9);
				if (!handleFileUpload(request, response, boundary)) {
                    sendErrorResponse(client_fd, response.getStatusCode());
                    return;
                }
                std::string responseText = response.toString();  // Assure-toi que response.toString() génère la réponse complète
                size_t bytes_written = write(client_fd, responseText.c_str(), responseText.length());  // Envoie la réponse
                if (bytes_written == -1) {
                    sendErrorResponse(client_fd, 500); // Internal server error
                    Logger::instance().log(WARNING, "500 error (Internal Server Error) to File Upload response: " + bytes_written + ": failed to send");
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

            size_t bytes_written = write(client_fd, cgiOutput.c_str(), cgiOutput.length()); //?? CHECK ERROR : 0 / -1
            if (bytes_written == -1) {
                sendErrorResponse(client_fd, 500); // Internal server error
                Logger::instance().log(WARNING, "500 error (Internal Server Error) to CGI output response: " + bytes_written + ": failed to send");
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
    Logger::instance().log(INFO, "Acceptin new Connection on socket FD: " + to_string(server_fd));
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

