
#pragma once

#include <iostream>
#include <sstream>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <cerrno>

class Socket
{
private:
    int _socket_fd;
    int _port;
    struct sockaddr_in address;

public:
    Socket(const std::string& host, int port);
    ~Socket();

    void    socket_creation();
    void    socket_binding();
    void    socket_listening();

    bool    operator==(int fd) const;
    int     getSocket() const;
    int     getPort() const;
    sockaddr_in& getAddress() ;

    void    build_sockets();
    void    close_sockets();
};
