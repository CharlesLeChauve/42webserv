// ConfigParser.hpp
#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "ServerConfig.hpp"
#include "Logger.hpp"
#include <vector>
#include <string>
#include <map>
#include <fstream>
#include <stdexcept>

class ConfigParserException : public std::exception {
public:
    ConfigParserException(const std::string& message) : _message(message) {}
    virtual ~ConfigParserException() throw() {}
    virtual const char* what() const throw() {
        return _message.c_str();
    }
private:
    std::string _message;
};

class ConfigParser {
public:
    ConfigParser();
    ~ConfigParser();

    void parseConfigFile(const std::string &filename);

    const std::vector<ServerConfig>& getServerConfigs() const;

private:
    std::vector<ServerConfig> _serverConfigs;

    void processServerDirective(std::ifstream &file, const std::string &line, ServerConfig &serverConfig);

    void processLocationBlock(std::ifstream &file, const std::string& locationPath, ServerConfig& serverConfig);

    void validateDirectiveValue(const std::string &directive, const std::string &value);

    void trim(std::string &s);
};

#endif
