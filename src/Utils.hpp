#ifndef UTILS_HPP
#define UTILS_HPP

#include <sstream>
#include <string>

#include <unistd.h>
#include <sys/time.h>


#define TIMEOUT_MS 5000

template <typename T>
std::string to_string(T value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

namespace serverSignal {
    extern int pipe_fd[2];
    void signal_handler(int signum);
}

enum FDType { FD_SERVER_SOCKET, FD_CLIENT_SOCKET, FD_CGI_INPUT, FD_CGI_OUTPUT, FD_UNKNOWN };

enum LoggerLevel { DEBUG, INFO, WARNING, ERROR };

unsigned long curr_time_ms();

#endif
