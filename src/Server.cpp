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
	return "images/" + to_string(num) + "-sorry.gif";
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
            break;
        }

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

    return request;
}

void Server::sendResponse(int client_fd, HTTPResponse response) {
	std::string responseString = response.toString();
	write(client_fd, responseString.c_str(), responseString.size()); // Check error : 0 / -1
}

void Server::handleHttpRequest(int client_fd, const HTTPRequest& request, HTTPResponse& response) {
    const Location* location = _config.findLocation(request.getPath());

    if (location && !location->allowedMethods.empty()) {
        if (std::find(location->allowedMethods.begin(), location->allowedMethods.end(), request.getMethod()) == location->allowedMethods.end()) {
            sendErrorResponse(client_fd, 405); // Méthode non autorisée
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
                return;
            }
        }
    } else {
        sendErrorResponse(client_fd, 400); // Mauvaise requête
        return;
    }

    // Traitement de la requête selon la méthode
    if (request.getMethod() == "GET" || request.getMethod() == "POST") {
        handleGetOrPostRequest(client_fd, request, response);
    } else if (request.getMethod() == "DELETE") {
        handleDeleteRequest(client_fd, request);
    } else {
        sendErrorResponse(client_fd, 405); // Méthode non autorisée
    }
}

bool Server::hasCgiExtension(const std::string& path) const {
    // Logger le message initial
    std::ostringstream oss;
    Logger::instance().log(DEBUG, "CGI Extensions for this server:");
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

    // Logger le fait qu'aucune extension correspondante n'a été trouvée
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

void Server::handleFileUpload(const HTTPRequest& request, HTTPResponse& response, const std::string& boundary) {
    std::string requestBody = request.getBody();
    std::string boundaryMarker = "--" + boundary;
    std::string endBoundaryMarker = boundaryMarker + "--";
    size_t pos = 0;
    size_t endPos = 0;

    while (true) {
        pos = requestBody.find(boundaryMarker, endPos);
        if (pos == std::string::npos) {
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
            break;
        }
        std::string partHeaders = requestBody.substr(pos, headersEnd - pos);
        pos = headersEnd + 4; // Position du début du contenu

        // Trouver le prochain boundary
        endPos = requestBody.find(boundaryMarker, pos);
        if (endPos == std::string::npos) {
            Logger::instance().log(ERROR, "Boundary de fin introuvable.");
            break;
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
            std::string filename = partHeaders.substr(filenamePos, filenameEnd - filenamePos);
            try {
                UploadHandler uploadHandler("www/uploads/" + filename, partContent, this->_config);
            } catch (const std::exception& e) {
                Logger::instance().log(ERROR, std::string("Erreur lors de l'upload du fichier : ") + e.what());
                response.setStatusCode(500);
                response.setBody("Erreur lors de l'upload du fichier.");
                return;
            }
        } else {
             
        }
        endPos = endPos + boundaryMarker.length();
    }
    response.setStatusCode(201);
	std::string script = "<script type=\"text/javascript\">"
                     "setTimeout(function() {"
                     "    window.location.href = 'index.html';"
                     "}, 3500);"
                     "</script>";
    response.setBody(script + "Fichier uploadé avec succès, Vous allez etre redirigé vers la page d'Accueil.");
}

// Modification de la méthode handleGetOrPostRequest
void Server::handleGetOrPostRequest(int client_fd, const HTTPRequest& request, HTTPResponse& response) {
    std::string fullPath = _config.root + request.getPath();

    // Log to verify the complete path
    Logger::instance().log(DEBUG, "handleGetOrPostRequest: fullPath =" + fullPath);

    // Vérifier si le fichier a une extension CGI (.cgi, .sh, .php)
	if (request.getMethod() == "POST" && request.getPath() == "/uploads" && request.hasHeader("Content-Type")) {
        std::string contentType = request.getStrHeader("Content-Type");
        if (contentType.find("multipart/form-data") != std::string::npos) {
		// Recherche du boundary dans le Content-Type avec find + check if end
		// Si ce n'est pas la fin extraire 
			size_t	boundaryPos = contentType.find("boundary=");
			if (boundaryPos != std::string::npos) {
				std::string boundary = contentType.substr(boundaryPos + 9);
				handleFileUpload(request, response, boundary);
                std::string responseText = response.toString();  // Assure-toi que response.toString() génère la réponse complète
                write(client_fd, responseText.c_str(), responseText.length());  // Envoie la réponse
                return;
			}
		}
		else
			sendErrorResponse(client_fd, 400); // Bad request
	}
	else if (hasCgiExtension(fullPath)) {
        Logger::instance().log(DEBUG, "CGI extension detected for path: " + fullPath);
        if (access(fullPath.c_str(), F_OK) == -1) {
            Logger::instance().log(DEBUG, "CGI script not found: " + fullPath);
            sendErrorResponse(client_fd, 404);
        } else {
            CGIHandler cgiHandler;
            std::string cgiOutput = cgiHandler.executeCGI(fullPath, request);

            write(client_fd, cgiOutput.c_str(), cgiOutput.length()); //CHECK ERROR : 0 / -1
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

