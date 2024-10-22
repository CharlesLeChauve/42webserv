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
	return "/images/" + to_string(num) + "_sorry.gif";
}

// Génération de la page d'erreur en utilisant les informations de configuration du serveur
std::string Server::generateErrorPage(int errorCode, const std::string& errorMessage) {
	std::stringstream page;
	page << "<html><head><title>Error " << errorCode << " - " << _config.serverName << "</title>";  // Utilisation du nom du serveur
	page << "<link rel=\"stylesheet\" href=\"/css/err_style.css\"></head>";
	page << "<body><h1>Error " << errorCode << ": " << errorMessage << "</h1>";
	page << "<img src=\"" << getSorryPath() << "\" alt=\"Error Image\">";
	page << "<div style=\"width:100%;height:0;padding-bottom:56%;position:relative;\">"
     << "<iframe src=\"https://giphy.com/embed/l0HlGkNWJbGSm24Te\" width=\"100%\" height=\"100%\" style=\"position:absolute\" frameBorder=\"0\" class=\"giphy-embed\" allowFullScreen></iframe>"
     << "</div><p><a href=\"https://giphy.com/gifs/southparkgifs-l0HlGkNWJbGSm24Te\">via GIPHY</a></p>";
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
	if (request.getMethod() == "GET") {
		handleGetOrPostRequest(client_fd, request);
	} else if (request.getMethod() == "POST") {
		handleGetOrPostRequest(client_fd, request);
	} else if (request.getMethod() == "DELETE") {
		handleDeleteRequest(client_fd, request);
	} else {
		sendErrorResponse(client_fd, 405);
	}
}

// Méthode pour gérer les requêtes GET et POST
void Server::handleGetOrPostRequest(int client_fd, const HTTPRequest& request) {
	std::string fullPath = _config.root + request.getPath();  // Utilisation du chemin racine de _config
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
	if (stat(filePath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode)) {
		// Le chemin est un répertoire, chercher un fichier index

		std::string indexPath;
		bool indexFound = false;

		// Rechercher dans les locations si un index est défini
		for (size_t i = 0; i < _config.locations.size(); ++i) {
			const Location& location = _config.locations[i];
			if (filePath.find(location.path) == 0) {  // Si la location correspond au chemin
				std::map<std::string, std::string>::const_iterator it = location.options.find("index");
				if (it != location.options.end()) {
					indexPath = filePath + "/" + it->second;  // Utiliser l'index défini dans la location
					indexFound = true;
					break;
				}
			}
		}

		// Si aucun index n'a été trouvé dans les locations, utiliser l'index par défaut du serveur
		if (!indexFound) {
			indexPath = filePath + "/" + _config.index;
		}

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
				write(client_fd, response.c_str(), response.size());
			} else {
				sendErrorResponse(client_fd, 404);  // Si l'index ne peut pas être lu
			}
		} else {
			sendErrorResponse(client_fd, 404);  // Aucun index trouvé
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
			write(client_fd, response.c_str(), response.size());
		} else {
			sendErrorResponse(client_fd, 404);  // Fichier non trouvé
		}
	}
}


// void Server::stockClientsSockets(Socket& socket) {
//     // Ajouter le socket du serveur à la liste des fds surveillés par poll
//     pollfd server_pollfd;
//     server_pollfd.fd = socket.getSocket();
//     server_pollfd.events = POLLIN; // Surveiller les connexions entrantes
//     fds.push_back(server_pollfd);

//     while (true) {
//         int fds_nb = poll(fds.data(), fds.size(), -1); // Attendre les événements
//         if (fds_nb == -1) {
//             std::cerr << "Error during poll" << std::endl;
//             return;
//         }

//         for (std::vector<pollfd>::iterator it = fds.begin(); it != fds.end();) {
//             if (it->revents & POLLIN) { // Prêt pour la lecture
//                 if (it->fd == socket.getSocket()) { // Nouvelle connexion entrante
//                     int client_fd = accept(socket.getSocket(), NULL, NULL);
//                     if (client_fd == -1) {
//                         std::cerr << "Failed to accept connection" << std::endl;
//                         continue;
//                     }

//                     // Ajouter le nouveau client à la liste de poll
//                     pollfd client_pollfd;
//                     client_pollfd.fd = client_fd;
//                     client_pollfd.events = POLLIN; // Surveiller les requêtes clients
//                     fds.push_back(client_pollfd);

//                     std::cout << "New client connected, fd: " << client_fd << std::endl;
//                 } else { // Gérer les requêtes des clients existants
//                     char buffer[1024];
//                     int bytes_received = recv(it->fd, buffer, sizeof(buffer), 0);
//                     if (bytes_received <= 0) {
//                         // Déconnexion du client ou erreur
//                         close(it->fd);
//                         it = fds.erase(it); // Supprimer le client de la liste de poll
//                         std::cout << "Client disconnected or error occurred" << std::endl;
//                         continue;
//                     } else {
//                         // Traiter la requête client
//                         std::cout << "Request received: " << buffer << std::endl;
//                         std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
//                         send(it->fd, response.c_str(), response.size(), 0);
//                     }
//                 }
//             }
//             ++it;
//         }
//     }
// }



// void Server::stockClientsSockets(Socket& socket) {
// 	int add_size = sizeof(socket.getAddress());

// 	int client_fd = accept(socket.getSocket(), (struct sockaddr *)&socket.getAddress(), (socklen_t*)&add_size);
// 	if (client_fd == -1) {
// 		std::cerr << "Failed to accept an incoming connection. errno: " << strerror(errno) << std::endl;
// 		return;
// 	}

// 	std::cout << "Connection accepted with client_fd: " << client_fd << std::endl;

// 	if (client_fd >= 0) {
// 		_clientsFd.push_back(client_fd);
// 	} else {
// 		std::cerr << "Invalid client file descriptor: " << client_fd << std::endl;
// 	}
// }


void    Server::stockClientsSockets(Socket& socket) {
    pollfd  server_pollfd;

    server_pollfd.fd = socket.getSocket();
    server_pollfd.events = POLLIN; // Surveiller les connexions entrantes. (POLLIN : lecture)
    fds.push_back(server_pollfd);

    while (true) {
        int fds_nb = poll(fds.data(), fds.size(), -1);
        if (fds_nb == -1) {
            std::cout << "Error while checking client requests" << std::endl;
            return ;
        }
        for (std::vector<pollfd>::iterator it = fds.begin(); it != fds.end();) {
            // Need to manage POLLERR - POLLHUP - POLLNVAL => Check if we use strerror(errno) for error log.
            // if (it->revents & POLLERR) { // signifie qu'une erreur non récupérable s'est produite.
            //     std::cout << "An error occured on this socket !" << std::endl;
            //     close (it->fd);
            //     it = fds.erase(it);
            //     break ;
            // }
            // else if (it->revents & POLLHUP) { // indique que le client a fermé la connexion proprement.
            //     std::cout << "Connection has been closed by client." << std::endl;
            //     close (it->fd);
            //     it = fds.erase(it);
            //     break ;
            // }
            // else if (it->revents & POLLNVAL) { // fd est invalide (ex., si tu utilises un socket fermé ou non valide dans poll())
            //     std::cout << "File descriptor is not valid." << std::endl;
            //     close(it->fd);
            //     it = fds.erase(it);
            //     break ;
            // }
            if (it->revents & POLLIN) { // Si le fd est prêt à être lu. > & : vérifie si le bit correspondant à POLLIN est activé dans revents
                if (it->fd == socket.getSocket()) { // Si c'est le socket serveur > Accept new connection.
					int add_size = sizeof(socket.getAddress());
                    int client_socket = accept(socket.getSocket(), (struct sockaddr*)&socket.getAddress(), (socklen_t*)&add_size);
                    if (client_socket == -1) {
                        std::cout << "Failed to accept client connection." << std::endl;
                        continue ;
                    }
                    pollfd  client_pollfd;

                    client_pollfd.fd = client_socket;
                    client_pollfd.events = POLLIN;
                    fds.push_back(client_pollfd);
                    std::cout << "New client connected." << std::endl;
                }
                // Means that the event is linked to a client request.
                else {
                    char    buffer[1024] = {0};
                    int bytes_rcv = recv(it->fd, buffer, 1024, 0);

                    if (bytes_rcv == 0) {
                        close (it->fd);
                        it = fds.erase(it); // Après avoir appelé erase(it), l'iterator est invalidé, donc il faut mettre à jour l'iterator pour qu'il pointe sur le bon élément suivant
                        std::cout << "Connection has been closed by the client." << std::endl;
                        continue ;
                    }
                    else if (bytes_rcv == -1) {
                        close (it->fd);
                        it = fds.erase(it); // Après avoir appelé erase(it), l'iterator est invalidé, donc il faut mettre à jour l'iterator pour qu'il pointe sur le bon élément suivant
                        std::cout << "Failed in receiving client request." << std::endl;
                        continue ;
                    }

                    std::cout << "Client request is : " << buffer << std::endl;
                    std::string response = "This is a fucking response waiting by another client !";

                    int bytes_sent = send(it->fd, response.c_str(), response.size(), 0);
                    if (bytes_sent == -1) {
                        std::cout << "Failed in sending response to client." << std::endl;
                        continue ;
                    }
                }
            }
			++it;
        }
    }
}
