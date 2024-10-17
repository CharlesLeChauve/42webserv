#ifndef LOCATION_HPP
#define LOCATION_HPP

#include <string>
#include <map>

struct Location {
	std::string path;
	std::map<std::string, std::string> options;
};

#endif
