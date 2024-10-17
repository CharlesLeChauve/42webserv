#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "ServerConfig.hpp"
#include <vector>
#include <string>
#include <map>
#include <fstream>

class ConfigParser {
public:
	ConfigParser();
	~ConfigParser();

	bool parseConfigFile(const std::string &filename);

	const std::vector<ServerConfig>& getServerConfigs() const;

private:
	std::vector<ServerConfig> _serverConfigs;

	bool processServerDirective(std::ifstream &file, const std::string &line, ServerConfig &serverConfig);

	bool processLocationBlock(std::ifstream &file, std::map<std::string, std::string> &options);

	bool validateDirectiveValue(const std::string &directive, const std::string &value);

	void trim(std::string &s);
};

#endif
