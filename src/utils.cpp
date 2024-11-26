#include "Utils.hpp"

namespace serverSignal {
    int pipe_fd[2]; // Définition de la variable

    void signal_handler(int signum) {
		(void)signum;
        char byte = 1;
        write(pipe_fd[1], &byte, sizeof(byte));
    }
}


unsigned long curr_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return static_cast<unsigned long>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}
