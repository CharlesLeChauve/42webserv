#ifndef LOCATION_HPP
#define LOCATION_HPP

#include <string>
#include <map>
#include <vector>


struct Location {
	std::string path;
	std::map<std::string, std::string> options;
	std::vector<std::string> allowedMethods;
};

#endif
