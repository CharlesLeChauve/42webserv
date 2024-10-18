
#pragma once 

#include <sys/socket.h> // Fonction socket() + types AF_INET & SOCK_STREAM
#include <netinet/in.h> // Struct. et constantes liees aux add. reseau (sockaddr_in + htons() pour convertir les ports au bon format)
#include <arpa/inet.h> // Fonctions pour convertir les add. IP (inet_addr() et inet_ntoa())
#include <unistd.h> // close()
#include <string.h> // memset()
#include <iostream>
#include <sstream>

class Socket
{
private:
    int _socket_fd;
    struct sockaddr_in address;
    // int new_sockets[10]; // Need to use vector later ?

public:
    Socket();
    ~Socket();

    // Socket creation
    void    socket_creation();
    // Binding
    void    socket_binding();
    // Listening
    void    socket_listening();
    // Clients connection

    // Sending & receiving messages

    bool operator==(int fd) const;

    void    close_sockets();
};
