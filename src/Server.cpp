
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

// void    Server::stockClientsSockets(Socket& socket) {
//     pollfd  server_pollfd;

//     server_pollfd.fd = socket.getSocket();
//     server_pollfd.events = POLLIN; // Surveiller les connexions entrantes. (POLLIN : lecture)
//     fds.push_back(server_pollfd);

//     while (true) {
//         int fds_nb = poll(fds.data(), fds.size(), -1);
//         if (fds_nb == -1) {
//             std::cout << "Error while checking client requests" << std::endl;
//             return ;
//         }
//         for (std::vector<pollfd>::iterator it = fds.begin(); it != fds.end(); ++it) {
//             if (it->revents & POLLIN) { // Si le fd est prêt à être lu. > & : vérifie si le bit correspondant à POLLIN est activé dans revents
//                 if (it->fd == socket.getSocket()) { // Si c'est le socket serveur > Accept new connection.
//                     socklen_t   addr_len = sizeof(socket.getAddress());
//                     int client_socket = accept(socket.getSocket(), (struct sockaddr*)&server.getAddress(), addr_len);
//                     if (client_socket == -1) {
//                         std::cout << "Failed to accept client connection." << std::endl;
//                         continue ;
//                     }
//                     pollfd  client_pollfd;

//                     client_pollfd.fd = client_socket;
//                     client_pollfd.events = POLLIN;
//                     fds.push_back(client_pollfd);
//                     std::cout << "New client connected." << std::endl;
//                 }
//                 // Means that the event is linked to a client request.
//                 else {
//                     char    buffer[1024] = {0};
//                     int bytes_rcv = recv(it->fd, buffer, 1024, 0);

//                     if (bytes_rcv <= 0) {
//                         close (it->fd);
//                         it = fds.erase(it); // Après avoir appelé erase(it), l'iterator est invalidé, donc il faut mettre à jour l'iterator pour qu'il pointe sur le bon élément suivant
//                         std::cout << "Connection has been closed by the client." << std::endl;
//                         continue ;
//                     }

//                     std::cout << "Client request is : " << buffer << std::endl;
//                     std::string response = "This is a fucking response waiting by another client !";

//                     int bytes_sent = send(it->fd, response.c_str(), response.size(), 0);
//                     if (bytes_sent == -1) {
//                         std::cout << "Failed in sending response to client." << std::endl;
//                         continue ;
//                     }
//                 }
//                 // Need to manage POLLERR - POLLHUP - POLLNVAL
//             }
//         }
//     }
// }