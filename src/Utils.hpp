#ifndef UTILS_HPP
#define UTILS_HPP

#include <sstream>
#include <string>

template <typename T>
std::string to_string(T value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

enum LoggerLevel { DEBUG, INFO, WARNING, ERROR };

#endif
