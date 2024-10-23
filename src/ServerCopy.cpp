#include <fstream>
#include "ServerCopy.hpp"
#include "HTTPRequest.hpp"
#include "CGIHandler.hpp"
#include "ServerConfig.hpp"
#include <sys/stat.h>  // Pour utiliser la fonction stat

#include <time.h>

Server::Server(const ServerConfig& config) : _config(config) {
	// Affichage des informations de la configuration du serveur
	std::cout << "Server initialized with the following configuration:" << std::endl;

	// Afficher le nom du serveur
	std::cout << "Server Name: " << _config.serverName << std::endl;

	// Afficher l'hôte et les ports
	std::cout << "Host: " << _config.host << std::endl;
	std::cout << "Ports: ";
	for (size_t i = 0; i < _config.ports.size(); ++i) {
		std::cout << _config.ports[i];
		if (i != _config.ports.size() - 1)
			std::cout << ", ";
	}
	std::cout << std::endl;

	// Afficher le chemin racine et le fichier index
	std::cout << "Root directory: " << _config.root << std::endl;
	std::cout << "Index file: " << _config.index << std::endl;

	// Afficher les pages d'erreur
	std::cout << "Error Pages: " << std::endl;
	for (std::map<int, std::string>::const_iterator it = _config.errorPages.begin(); it != _config.errorPages.end(); ++it) {
		std::cout << "  Error Code " << it->first << ": " << it->second << std::endl;
	}

	// Afficher les locations et leurs options
	std::cout << "Locations: " << std::endl;
	for (size_t i = 0; i < _config.locations.size(); ++i) {
		const Location& location = _config.locations[i];
		std::cout << "  Location path: " << location.path << std::endl;
		for (std::map<std::string, std::string>::const_iterator it = location.options.begin(); it != location.options.end(); ++it) {
			std::cout << "    " << it->first << ": " << it->second << std::endl;
		}
	}

	// Vérifier si la configuration est valide
	if (!_config.isValid()) {
		std::cerr << "Server configuration is invalid." << std::endl;
	} else {
		std::cout << "Server configuration is valid." << std::endl;
	}
}


Server::~Server() {}

#include <fcntl.h>

void setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		std::cerr << "fcntl(F_GETFL) failed: " << strerror(errno) << std::endl;
		return;
	}

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cerr << "fcntl(F_SETFL) failed: " << strerror(errno) << std::endl;
	}
}


template <typename T>
std::string to_string(T value) {
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

std::string getSorryPath() {
	srand(time(NULL));
	int num = rand() % 6;
	num++;
	return "images/" + to_string(num) + "-sorry.gif";
}

// Génération de la page d'erreur en utilisant les informations de configuration du serveur
std::string Server::generateErrorPage(int errorCode, const std::string& errorMessage) {
	std::stringstream page;
	page << "<html><head><title>Error " << errorCode << " - " << _config.serverName << "</title>";  // Utilisation du nom du serveur
	page << "<link rel=\"stylesheet\" href=\"css/err_style.css\"></head>";
	page << "<body><h1>Error " << errorCode << ": " << errorMessage << "</h1>";
	page << "<img src=\"" << getSorryPath() << "\" alt=\"Error Image\">";
	page << "<p>The server encountered an issue processing your request.</p>";
	page << "<p>Server Root: " << _config.root << "</p>";  // Afficher le chemin racine configuré
	page << "</body></html>";
	return page.str();
}

// Utilisation de _config pour afficher les pages d'erreur personnalisées
std::string Server::getErrorMessage(int errorCode) {
	// Utilisation des pages d'erreur définies dans _config.errorPages
	std::map<int, std::string>::const_iterator it = _config.errorPages.find(errorCode);
	if (it != _config.errorPages.end()) {
		return it->second;  // Si une page d'erreur personnalisée existe
	}

	// Sinon, retourner des messages d'erreur par défaut
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

	std::string response = "HTTP/1.1 " + to_string(errorCode) + " " + errorMessage + "\r\n";
	response += "Content-Type: text/html\r\n";
	response += "Content-Length: " + to_string(errorPage.size()) + "\r\n\r\n";
	response += errorPage;

	write(client_fd, response.c_str(), response.size());
}

// Méthode pour gérer la requête HTTP en fonction de la méthode
void Server::handleHttpRequest(int client_fd, const HTTPRequest& request) {
	std::string fullPath = _config.root + request.getPath();  // Utilisation du chemin racine de _config
	std::cout << std::endl << " Test : " << _config.ports.at(0) << std::endl << std::endl;
	if (request.getMethod() == "GET" || request.getMethod() == "POST") {
		// Utilisez la même méthode pour gérer GET et POST
		handleGetOrPostRequest(client_fd, request);
	} else if (request.getMethod() == "DELETE") {
		handleDeleteRequest(client_fd, request);
	} else {
		sendErrorResponse(client_fd, 405);  // Méthode non autorisée
	}
}


// Méthode pour gérer les requêtes GET et POST
void Server::handleGetOrPostRequest(int client_fd, const HTTPRequest& request) {
	std::string fullPath = _config.root + request.getPath();  // Utilisation du chemin racine de _config
	//Changer ca pour détecter des vrais extrensions de fichiers...
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
	std::string fullPath = _config.root + request.getPath();  // Utilisation du chemin racine de _config
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
	struct stat pathStat;
	std::cout << "Filepath = " << filePath << std::endl;
	if (stat(filePath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
		// Le chemin est un répertoire, chercher un fichier index

		std::string indexPath;
		// bool indexFound = false;

		// Rechercher dans les locations si un index est défini
		if (filePath.substr(0, 4) == "www/") {
			indexPath = _config.root + "/" + _config.index;
			std::cout << "indexPath = " + indexPath << std::endl;
		}

		for (size_t i = 0; i < _config.locations.size(); ++i) {
			const Location& location = _config.locations[i];
			if (filePath.find(location.path) == 0) {  // Si la location correspond au chemin
				std::map<std::string, std::string>::const_iterator it = location.options.find("index");
				if (it != location.options.end()) {
					indexPath = filePath + "/" + it->second;  // Utiliser l'index défini dans la location
					// indexFound = true;
					break;
				}
			}
		}

		// Si aucun index n'a été trouvé dans les locations, utiliser l'index par défaut du serveur
		// if (!indexFound) {
		// 	indexPath = filePath + _config.index;
		// }

		// Vérifier si l'index existe
		if (access(indexPath.c_str(), F_OK) != -1) {
			// L'index existe, on le sert
			std::ifstream file(indexPath.c_str());
			if (file) {
				std::stringstream buffer;
				buffer << file.rdbuf();
				std::string content = buffer.str();

				std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
				response += "Content-Length: " + to_string(content.size()) + "\r\n\r\n";
				response += content;

				ssize_t bytes_written = 0;
				ssize_t total_bytes_written = 0;
				while (total_bytes_written < static_cast<ssize_t>(response.size())) {
					bytes_written = write(client_fd, response.c_str() + total_bytes_written, response.size() - total_bytes_written);
					if (bytes_written == -1) {
						std::cerr << "Error writing to client: " << strerror(errno) << std::endl;
						break;
					}
					total_bytes_written += bytes_written;
				}

				close(client_fd);  // Fermer la connexion après avoir servi le fichier
			} else {
				sendErrorResponse(client_fd, 404);  // Si l'index ne peut pas être lu
				close(client_fd);
			}
		} else {
			sendErrorResponse(client_fd, 404);  // Aucun index trouvé
			close(client_fd);
		}
	} else {
		// Le chemin n'est pas un répertoire, on essaie de servir le fichier directement
		std::ifstream file(filePath.c_str());
		if (file) {
			std::stringstream buffer;
			buffer << file.rdbuf();
			std::string content = buffer.str();

			std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
			response += "Content-Length: " + to_string(content.size()) + "\r\n\r\n";
			response += content;

			ssize_t bytes_written = 0;
			ssize_t total_bytes_written = 0;
			while (total_bytes_written < static_cast<ssize_t>(response.size())) {
				bytes_written = write(client_fd, response.c_str() + total_bytes_written, response.size() - total_bytes_written);
				if (bytes_written == -1) {
					std::cerr << "Error writing to client: " << strerror(errno) << std::endl;
					break;
				}
				total_bytes_written += bytes_written;
			}

			close(client_fd);  // Fermer la connexion après avoir servi le fichier
		} else {
			sendErrorResponse(client_fd, 404);  // Fichier non trouvé
			close(client_fd);
		}
	}
}



void Server::stockClientsSockets(std::vector<Socket*>& sockets) {
	std::vector<pollfd> poll_fds;

	// Ajouter tous les sockets des serveurs dans le tableau `poll_fds`
	for (size_t i = 0; i < sockets.size(); ++i) {
		Socket* socket = sockets[i];

		pollfd server_pollfd;
		server_pollfd.fd = socket->getSocket(); // Ajoute le FD du serveur
		server_pollfd.events = POLLIN;
		poll_fds.push_back(server_pollfd);

		std::cout << "Ajout du socket FD: " << socket->getSocket() << " pour surveiller les connexions sur le port: " << socket->getPort() << std::endl;
	}

	// Boucle principale de gestion des événements
	while (true) {
		int poll_result = poll(poll_fds.data(), poll_fds.size(), -1);  // Pas de timeout
		if (poll_result == -1) {
			if (errno == EINTR) {
				continue; // Réessayer si poll est interrompu par un signal
			}
			std::cerr << "poll failed: " << strerror(errno) << std::endl;
			break;
		}

		// Parcourir les résultats de poll pour chaque socket surveillé
		for (size_t i = 0; i < poll_fds.size(); ++i) {
			bool isServerSocket = false;
			for (size_t j = 0; j < sockets.size(); ++j) {
				if (sockets[j]->getSocket() == poll_fds[i].fd) {
					isServerSocket = true;
					break;
				}
			}

			if (poll_fds[i].revents & POLLIN) {
				if (isServerSocket) {
					Socket* socket = sockets[i];

					// Nouvelle connexion entrante
					int add_size = sizeof(socket->getAddress());
					int client_socket = accept(socket->getSocket(), (struct sockaddr*)&socket->getAddress(), (socklen_t*)&add_size);
					if (client_socket == -1) {
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							std::cerr << "No connections available to accept at the moment." << std::endl;
						} else {
							std::cerr << "Failed to accept client connection: " << strerror(errno) << std::endl;
						}
						continue;
					}

					// Ajouter le client connecté dans la liste des sockets surveillés
					pollfd client_pollfd;
					client_pollfd.fd = client_socket;
					client_pollfd.events = POLLIN;
					poll_fds.push_back(client_pollfd);
					std::cout << "New client connected on port: " << socket->getPort() << " (FD: " << client_socket << ")" << std::endl;
				}
				else {
					// Gérer les événements des clients déjà connectés
					char buffer[1024] = {0};
					ssize_t bytes_read = recv(poll_fds[i].fd, buffer, sizeof(buffer), 0);

					if (bytes_read == 0 || (poll_fds[i].revents & POLLHUP)) {
						// Déconnexion du client
						std::cout << "Client disconnected: " << poll_fds[i].fd << std::endl;
						close(poll_fds[i].fd);
						poll_fds.erase(poll_fds.begin() + i);  // Suppression du client
						--i; // Ajuster l'indice après suppression
						continue;
					} else if (bytes_read == -1) {
						std::cerr << "recv failed: " << strerror(errno) << std::endl;
						close(poll_fds[i].fd);
						poll_fds.erase(poll_fds.begin() + i);
						--i;
						continue;
					}

					std::cout << "Received data: " << buffer << std::endl;

					// Traiter la requête HTTP
					HTTPRequest request;
					if (request.parse(buffer)) {
						handleHttpRequest(poll_fds[i].fd, request);
					} else {
						sendErrorResponse(poll_fds[i].fd, 400);  // Requête malformée
					}

					close(poll_fds[i].fd);  // Fermer le client après avoir traité la requête
					poll_fds.erase(poll_fds.begin() + i);  // Suppression du client
					--i;  // Ajuster l'indice après suppression
				}
			}
		}
	}
}