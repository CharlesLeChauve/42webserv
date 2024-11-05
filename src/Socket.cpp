#include "Socket.hpp"
#include "Logger.hpp"
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
		// std::cout << "Fermeture du socket FD: " << _socket_fd << std::endl;
		close(_socket_fd);
		_socket_fd = -1;
	}
}

void Socket::socket_creation() {
	_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (address.sin_family != AF_INET) {
		Logger::instance().log(WARNING, "Erreur: mauvaise famille d'adresses pour le socket: " + to_string(address.sin_family));
	}

	if (_socket_fd == -1) {
		Logger::instance().log(ERROR, std::string("Socket creation failed: ") + strerror(errno));
		return;
	}
	// std::cout << "Socket successfully created with FD: " << _socket_fd << " for port: " << _port << std::endl;

	// Rendre le socket non bloquant
	// int flags = fcntl(_socket_fd, F_GETFL, 0);
	// if (flags == -1) {
	// 	std::cerr << "fcntl(F_GETFL) failed for FD: " << _socket_fd << " Error: " << strerror(errno) << std::endl;
	// 	close(_socket_fd);
	// 	_socket_fd = -1;
	// 	return;
	// }

	// if (fcntl(_socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
	// 	std::cerr << "fcntl(F_SETFL) failed for FD: " << _socket_fd << " Error: " << strerror(errno) << std::endl;
	// 	close(_socket_fd);
	// 	_socket_fd = -1;
	// 	return;
	// }
	// std::cout << "Socket FD: " << _socket_fd << " set to non-blocking mode." << std::endl;
}


void Socket::socket_binding() {
	int add_size = sizeof(address);

	// Set socket options to allow reuse of the address and port
	int opt = 1;
	if (setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		Logger::instance().log(ERROR, std::string("Failed to set socket options: ") + strerror(errno));
		close(_socket_fd);
		_socket_fd = -1;
		return;
	}

	if (bind(_socket_fd, (struct sockaddr *)&address, add_size) == -1) {
		Logger::instance().log(ERROR, std::string("Failed to bind socket to IP address and port: " ) + strerror(errno));
		close(_socket_fd);
		_socket_fd = -1;
		return;
	}
	Logger::instance().log(INFO, "Socket " + to_string(_socket_fd) + " successfully bound to port " + to_string(_port));
}

void Socket::socket_listening() {
	int ret = listen(_socket_fd, SOMAXCONN);
	Logger::instance().log(DEBUG, "listen() returned: " + to_string(ret));
	if (ret == -1) {
		Logger::instance().log(ERROR, std::string("Failed to put socket in listening mode: ") + strerror(errno));
		close(_socket_fd);
		_socket_fd = -1;
		return;
	}
	Logger::instance().log(INFO, "Socket " + to_string(_socket_fd) + " is now listening on port " + to_string(_port));
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
