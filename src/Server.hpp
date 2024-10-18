#include <iostream>
#include <map>
#include <vector>

#include "Socket.hpp"
#include "ServerConfig.hpp"

class Socket;

class Server
{
private:
	// ServerConfig& _config;
	// std::vector<Socket&> _sockets;
	std::vector<int> _clientsFd;
public:
	// Server(ServerConfig& config);
	Server();
	~Server();

	void	stockClientsSockets(Socket& sockets);
	void	receiveAndSend();
};
