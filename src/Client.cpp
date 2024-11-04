
#include "Client.hpp"
#include "Logger.hpp"

Client::Client(int p_port) : _client_socket(0), _port(p_port) {
    _client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_client_socket == -1) {
        Logger::instance().log(ERROR, "Failed to create client socket.");
		exit(EXIT_FAILURE) ;
    }

    memset(&_server_address, 0, sizeof(_server_address));
    _server_address.sin_family = AF_INET;
    _server_address.sin_port = htons(_port);
    _server_address.sin_addr.s_addr = inet_addr("127.0.0.1"); // Server IP (localhost) - INADD_ANY
}

Client::~Client() {
    close(_client_socket);
}

void Client::connect_to_server() {
    int connect = connect(_client_socket, (struct sockaddr*)&_server_address, sizeof(_server_address));
    if (connect == -1) {
        Logger::instance().log(ERROR, "Connection to server failed.");
        exit(EXIT_FAILURE);
    }
    Logger::instance().log(INFO, "Connection to server." )
}

void Client::send_message(const std::string& message) {
    int bytes_sent = send(_client_socket, message.c_str(), message.size(), 0); // CHECK ERROR : 0
    if (bytes_sent == -1) {
        Logger::instance().log(ERROR, "Failed to send message to server.")
        close_connection();
        exit(EXIT_FAILURE);
    }
    std::cout << "Message sent to server : " << message << std::endl;
}

void Client::receive_message() {
    char    buffer[1024] = {0};
    int bytes_rcv = recv(_client_socket, buffer, 1024, 0); // CHECK ERROR : 0
    if (bytes_rcv == -1) {
        std::cout << "Failed to receive response from server." << std::endl;
        close_connection();
        exit(EXIT_FAILURE);
    }
    std::cout << "Response from server : " << buffer << std::endl;
}

void Client::close_connection() {
    close(_client_socket);
    std::cout << "Connection closed." << std::endl;
}

int main() {
    Client client(8080);
    client.connect_to_server();
    
    std::string message = "Hello from client!";
    client.send_message(message);
    client.receive_message();
    
    client.close_connection();
    return 0;
}