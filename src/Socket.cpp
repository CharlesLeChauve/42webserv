#include "Socket.hpp"
#include <fcntl.h>
#include <cstring>
#include <iostream>

bool Socket::operator==(int fd) const {
	return (this->_socket_fd == fd);
}

Socket::Socket(int p_port) : _socket_fd(-1), _port(p_port) {
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(_port);
	address.sin_addr.s_addr = INADDR_ANY;
	socket_creation();
}

Socket::~Socket() {
	if (_socket_fd != -1) {
		std::cout << "Fermeture du socket FD: " << _socket_fd << std::endl;
		close(_socket_fd);
		_socket_fd = -1;
	}
}



void Socket::socket_creation() {
	_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_socket_fd == -1) {
		std::cerr << "Socket creation failed: " << strerror(errno) << std::endl;
		return;
	}
	std::cout << "Socket successfully created with FD: " << _socket_fd << " for port: " << _port << std::endl;

	// Rendre le socket non bloquant
	int flags = fcntl(_socket_fd, F_GETFL, 0);
	if (flags == -1) {
		std::cerr << "fcntl(F_GETFL) failed for FD: " << _socket_fd << " Error: " << strerror(errno) << std::endl;
		close(_socket_fd);
		_socket_fd = -1;
		return;
	}

	if (fcntl(_socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cerr << "fcntl(F_SETFL) failed for FD: " << _socket_fd << " Error: " << strerror(errno) << std::endl;
		close(_socket_fd);
		_socket_fd = -1;
		return;
	}
	std::cout << "Socket FD: " << _socket_fd << " set to non-blocking mode." << std::endl;
}


void Socket::socket_binding() {
	int add_size = sizeof(address);

	if (bind(_socket_fd, (struct sockaddr *)&address, add_size) == -1) {
		std::cerr << "Failed to bind socket to IP address and port: " << strerror(errno) << std::endl;
		close(_socket_fd);  // Fermer le socket en cas d'erreur
		_socket_fd = -1;
		return;
	}
	std::cout << "Socket successfully bound to port " << _port << std::endl;
}


void Socket::socket_listening() {
	if (listen(_socket_fd, 10) == -1) {
		std::cerr << "Failed to put socket in listening mode: " << strerror(errno) << std::endl;
		close(_socket_fd);  // Fermer le socket en cas d'erreur
		_socket_fd = -1;
		return;
	}
	std::cout << "Socket is now listening on port " << _port << std::endl;
}

int Socket::getSocket() const {
	return _socket_fd;
}

int Socket::getPort() const {
	return _port;
}

sockaddr_in& Socket::getAddress() {
	return address;
}

void Socket::build_sockets() {
	socket_binding();
	if (_socket_fd != -1) {
		socket_listening();
	}
}
