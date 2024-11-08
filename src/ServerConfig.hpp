// ServerConfig.hpp
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
    int clientMaxBodySize;
    bool autoindex;

    // Ajout d'un vecteur pour les extensions CGI
    std::vector<std::string> cgiExtensions;

    ServerConfig();
    ServerConfig(const ServerConfig& other);
    ServerConfig& operator=(const ServerConfig& other);
    ~ServerConfig();

    const Location* findLocation(const std::string& path) const;
    bool isValid() const;
};

#endif
