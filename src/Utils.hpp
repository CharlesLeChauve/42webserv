#ifndef UTILS_HPP
#define UTILS_HPP

#include <sstream>
#include <string>
#include <unistd.h>

#define TIMEOUT_MS 30000

template <typename T>
std::string to_string(T value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

namespace serverSignal {
    extern int pipe_fd[2]; // Déclaration de la variable
    void signal_handler(int signum);
}

enum LoggerLevel { DEBUG, INFO, WARNING, ERROR };

#endif
