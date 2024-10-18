
#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <iostream>

class Client {
private:
    int _client_socket;
    struct sockaddr_in _server_address;

public:
    Client();
    ~Client();

    void connect_to_server();
    void send_message(const std::string& message);
    void receive_message();
    void close_connection();
};

