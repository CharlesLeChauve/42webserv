
#include "Socket.hpp"

bool Socket::operator==(int fd) const {
	return (this->_socket_fd == fd);
}

Socket::Socket(int p_port) : _port(p_port) {
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET; // Specifie la famille d'add. (add IPV4 > AF_INET value)
	address.sin_port = htons(_port); // Port sur lequel mon serveur va ecouter. Ce port doit etre convertit en ordre reseau with htons().
	address.sin_addr.s_addr = INADDR_ANY; // Contient l'add. IP du serveur. INADDR_ANY : Serveur accepte les connexions sur toutes les interfaces reseau disponibles.
	socket_creation();
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

void    Socket::socket_binding() { // Check if bind() function is the same as connect()

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

	//int	add_size = sizeof(address);

	if (listen(_socket_fd, 10) == -1) {
		std::cout << "Failed to put socket in listening mode." << std::endl;
		return ;
	}
}

int     Socket::getSocket() const {
	return _socket_fd;
}

int     Socket::getPort() const {
	return _port;
}

// Not sending a ref. as we'll not modify values + not const. as we can't take the add. of a const ref.
sockaddr_in&	Socket::getAddress() {
	return address;
}

void    Socket::build_sockets() {
	socket_binding();
	socket_listening();
 }


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
