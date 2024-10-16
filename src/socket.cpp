
#include "socket.hpp"

bool Socket::operator==(int fd) const {
	return (this->_socket_fd == fd);
}

Socket::Socket() : _socket_fd(0) {} // Check initialization digit

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

	struct sockaddr_in address;
	int				add_size = sizeof(address);
	const int		port = 8080;

	memset(&address, 0, add_size);
	address.sin_family = AF_INET; // Specifie la famille d'add. (add IPV4 > AF_INET value)
	address.sin_port = htons(port); // Port sur lequel mon serveur va ecouter. Ce port doit etre convertit en ordre reseau with htons().
	address.sin_addr.s_addr = INADDR_ANY; // Contient l'add. IP du serveur. INADDR_ANY : Serveur accepte les connexions sur toutes les interfaces reseau disponibles.
 
	if(bind(_socket_fd, (struct sockaddr *)&address, add_size) == -1) { // Liaison du socket a l'addresse IP et au port qu'on a specifie dans la struct.
		std::cout << "Failed to link socket with IP address and port." << std::endl;
		return ;
	}
	std::cout << "Socket lié à l'adresse et au port : " << port << std::endl;
}

void    Socket::socket_listening() {
	
}