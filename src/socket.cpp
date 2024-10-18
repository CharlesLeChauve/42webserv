
#include "socket.hpp"

bool Socket::operator==(int fd) const {
	return (this->_socket_fd == fd);
}

Socket::Socket() : _socket_fd(0) {
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET; // Specifie la famille d'add. (add IPV4 > AF_INET value)
	address.sin_port = htons(8080); // Port sur lequel mon serveur va ecouter. Ce port doit etre convertit en ordre reseau with htons().
	address.sin_addr.s_addr = INADDR_ANY; // Contient l'add. IP du serveur. INADDR_ANY : Serveur accepte les connexions sur toutes les interfaces reseau disponibles.
} // Check initialization digit

Socket::~Socket()
{
}

void    Socket::socket_creation() {
	_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_socket_fd == -1) {
		std::cout << "Socket creation failed !" << std::endl;
		return ;
	}
	std::cout << "Socket successfully created !" << std::endl;
}

void    Socket::socket_binding() {

	int	add_size = sizeof(address);
 
	if(bind(_socket_fd, (struct sockaddr *)&address, add_size) == -1) { // Liaison du socket a l'addresse IP et au port qu'on a specifie dans la struct.
		std::cout << "Failed to link socket with IP address and port." << std::endl;
		return ;
	}
}

// void	Socket::close_sockets()
// {
// 	for (int i = 0; i < 10; i++) {
// 		close(new_sockets[i]);
// 	}
// }

void    Socket::socket_listening() {

	int	add_size = sizeof(address);

	if (listen(_socket_fd, 10) == -1) {
		std::cout << "Failed to put socket in listening mode." << std::endl;
		return ;
	}

	while (true) {
		int client_socket = accept(_socket_fd, (struct sockaddr *)&address, (socklen_t*)&add_size);
		if (client_socket == -1) {
			std::cout << "Failed to accept an incoming connection." << std::endl;
			continue ;
		}
		std::cout << "Connection accepted." << std::endl;

		char	buffer[1024] = {0};
		int		bytes_received = recv(client_socket, buffer, 1024, 0);
		if (bytes_received == 0) {
			std::cout << "Connection has been canceled by client." << std::endl;
			close(client_socket);
			continue ;
		}
		if (bytes_received == -1) {
			std::cout << "Failed in receiving request from client !" << std::endl;
			close(client_socket);
			continue ;
		}

		std::stringstream	line(buffer);
		std::string			received_line;
		while (std::getline(line, received_line)) {
			std::cout << "Received: " << received_line << std::endl;
			if (received_line.empty())
				continue ;
			
			std::string	response = "You're are receiving a bullshit message !";
			if (send(client_socket, response.c_str(), response.size(), 0) == -1) {
				std::cout << "Failed in sending a response !" << std::endl;
				close (client_socket);
				continue ;
			}
		}
		close(client_socket);
		std::cout << "Connection closed." << std::endl;
	}
}
// void    Socket::socket_listening() {

// 	int	add_size = sizeof(address);

// 	if (listen(_socket_fd, 10) == -1) {
// 		std::cout << "Failed to put socket in listening mode." << std::endl;
// 		return ;
// 	}
// 	for (int i = 0; i < 10; i++) {
// 		new_sockets[i] = accept(_socket_fd, (struct sockaddr *)&address, (socklen_t*)&add_size); // struct sockaddr* (le type générique pour les adresses réseau), c’est pourquoi tu fais un cast de struct sockaddr_in* vers struct sockaddr*.
// 		if (new_sockets[i] == -1) {
// 			std::cout << "Failed to accept an incoming connection." << std::endl;
// 			continue ;
// 		}
// 	}
// 	std::cout << "OKOKOK" << std::endl;
// 	std::stringstream	line;
// 	for (int i = 0; i < 10; i++) {
// 		char	buffer[1024] = {0};
// 		if (recv(new_sockets[i], buffer, 1024, 0) == -1) {
// 			std::cout << "Failed in receiving request !" << std::endl;
// 			return ;
// 		}
// 		std::istringstream line(buffer);
//         std::string received_line;
// 		while (std::getline(line, received_line)) { // Need to save what we received in a array ?
//             // Traitement de chaque ligne ici.
//             std::cout << "Received: " << received_line << std::endl;
// 			if (received_line.empty())
// 				continue ;
// 			std::string response = " You're are receiving a bullshit message !";
// 			if (send(new_sockets[i], response.c_str(), response.size(), 0) == -1) {
// 				std::cout << "Failed in sending a response !" << std::endl;
// 				close_sockets();
// 				return ;
// 			}
//         }
// 		close (new_sockets[i]);
// 	}
// }

/*
std::istringstream : C'est un flux d'entrée qui permet de traiter une chaîne de caractères comme un flux, ce qui facilite 
la lecture de données formatées. En utilisant buffer comme argument, tu crées un flux qui contient tout le texte reçu dans 
buffer.

line : C'est le nom de l'objet istringstream que tu crées. Cet objet te permettra d'extraire facilement des données ligne 
par ligne (ou sous d'autres formats) à partir du texte dans buffer.
*/

/*
Errors that need to be handle : 
Send : Si le nombre d'octets envoyés est inférieur à la taille de la réponse, cela pourrait signifier que l'envoi a été interrompu.
Recv : Need to handle 0 return (client connection has been closed)
Line : Que faire si la ligne est vide ? Cela pourrait être un cas que tu veux gérer.
Line : Need to save each line of the request ?
*/
