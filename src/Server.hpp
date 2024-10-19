#include <iostream>
#include <map>
#include <vector>
#include <poll.h>
#include <iostream>

#include "Socket.hpp"
#include "ServerConfig.hpp"

class Socket;

class Server
{
private:
	// ServerConfig& _config;
	// std::vector<Socket&> _sockets;
	std::vector<int> _clientsFd;
	std::vector<pollfd> fds;
public:
	// Server(ServerConfig& config);
	Server();
	~Server();

	void	stockClientsSockets(Socket& sockets);
	void	receiveAndSend();
};
