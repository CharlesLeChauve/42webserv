#include "Utils.hpp"

namespace serverSignal {
    int pipe_fd[2]; // Définition de la variable

    void signal_handler(int signum) {
		(void)signum;
        char byte = 1;
        write(pipe_fd[1], &byte, sizeof(byte));
    }
}
