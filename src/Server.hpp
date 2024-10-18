#include <iostream>
#include <map>
#include <vector>

class Socket;
#include "ServerConfig.hpp"

class Server
{
private:
	ServerConfig& _config;
	std::vector<Socket&> _sockets;
	std::vector<int> _clientsFd;
public:
	Server(ServerConfig& config);
	~Server();
};

Server::Server(ServerConfig& config)
{
	std::vector<int>::iterator it = config.ports.begin();
	for (int i = 0; it != config.ports.end(); i++, it++)
	{
		_sockets.emplace_back(Socket(*it));
	}
}

Server::~Server()
{
}
