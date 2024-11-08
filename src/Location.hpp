#ifndef LOCATION_HPP
#define LOCATION_HPP

#include <string>
#include <map>
#include <vector>


struct Location {
	std::string path;
	std::map<std::string, std::string> options;
	std::vector<std::string> allowedMethods;
	int clientMaxBodySize;
	int returnCode;
	std::string returnUrl;
	std::string uploadPath;
	bool uploadOn;
	int autoindex;

	Location() : clientMaxBodySize(-1), returnCode(0), uploadOn(false), autoindex(-1) {}
};

#endif
