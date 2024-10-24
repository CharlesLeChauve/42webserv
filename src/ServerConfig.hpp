#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "Location.hpp"
#include <string>
#include <vector>
#include <map>

class ServerConfig {
public:
	std::vector<int> ports;
	std::vector<std::string> serverNames;
	std::string root;
	std::string index;
	std::map<int, std::string> errorPages;
	std::vector<Location> locations;
	std::string host;

	ServerConfig();
	ServerConfig(const ServerConfig& other);
	ServerConfig& operator=(const ServerConfig& other);
	~ServerConfig();

	bool isValid() const;
};

#endif
