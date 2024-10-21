
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
    int _port;
    struct sockaddr_in _server_address;

public:
    Client(int p_port);
    ~Client();

    void connect_to_server();
    void send_message(const std::string& message);
    void receive_message();
    void close_connection();
};

