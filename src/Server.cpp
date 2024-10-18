
#include "Server.hpp"

Server::Server() {}

// Server::Server(ServerConfig& config)
// {
// 	std::vector<int>::iterator it = config.ports.begin();
// 	for (int i = 0; it != config.ports.end(); i++, it++)
// 	{
// 		_sockets.emplace_back(Socket(*it));
// 	}
// }

Server::~Server()
{
}


// Passer une ref. a un obj. Socket existant pour recuperer le "main" socket
void	Server::stockClientsSockets(Socket& sockets) {

    int add_size = sizeof(sockets.getAddress());
    // Can't take the add. of a temporary copy (here sockaddr_in ref).
    int client_fd = accept(sockets.getSocket(), (struct sockaddr *)&sockets.getAddress(), (socklen_t*)&add_size);
    if (client_fd == -1) {
		std::cout << "Failed to accept an incoming connection." << std::endl;
		return ;
	}
	std::cout << "Connection accepted." << std::endl;
    _clientsFd.push_back(client_fd); // Add our client_fd after checking if connection is accepted.
}

// Refaire fonction to send and rcv with write & read.

void	Server::receiveAndSend() {
    char    buffer[1024] = {0};

    for (std::vector<int>::iterator it = _clientsFd.begin(); it != _clientsFd.end(); ++it) {
        int bytes_received = read(*it, buffer, 1024);
        if (bytes_received == -1) {
            std::cout << "Failed in receiving request from client !" << std::endl;
            close(*it);
            return ;
        }
        if (bytes_received == 0) {
            std::cout << "There is nothing in this client request !" << std::endl;
            close(*it);
            continue ;
        }
        std::cout << "We received : " << buffer << std::endl;

        std::string response = "You're are receiving a bullshit message !";
        int bytes_sent = write(*it, response.c_str(), response.length());
        if (bytes_sent == -1) {
            std::cout << "Failed in sending a response !" << std::endl;
            close(*it);
            return ; // Let's see if we quit or continue.
        }
        close(*it);
        std::cout << "Connection closed." << std::endl;
    }
}