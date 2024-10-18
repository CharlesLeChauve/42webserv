
#include "Client.hpp"

Client::Client() : _client_socket(0) {
    _client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (_client_socket == -1) {
        std::cout << "Failed to create client socket." << std::endl;
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&_server_address, 0, sizeof(_server_address));
    _server_address.sin_family = AF_INET;
    _server_address.sin_port = htons(8080); // Same port as server
    _server_address.sin_addr.s_addr = inet_addr("127.0.0.1"); // Server IP (localhost)
}

Client::~Client() {
    close(_client_socket);
}

void Client::connect_to_server() {
    if (connect(_client_socket, (struct sockaddr*)&_server_address, sizeof(_server_address)) == -1) {
        std::cout << "Connection to server failed." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Connected to server!" << std::endl;
}

void Client::send_message(const std::string& message) {
    if (send(_client_socket, message.c_str(), message.size(), 0) == -1) {
        std::cout << "Failed to send message to server." << std::endl;
        close_connection();
        exit(EXIT_FAILURE);
    }
    std::cout << "Message sent to server: " << message << std::endl;
}

void Client::receive_message() {
    char buffer[1024] = {0};
    if (recv(_client_socket, buffer, 1024, 0) == -1) {
        std::cout << "Failed to receive response from server." << std::endl;
        close_connection();
        exit(EXIT_FAILURE);
    }
    std::cout << "Response from server: " << buffer << std::endl;
}

void Client::close_connection() {
    close(_client_socket);
    std::cout << "Connection closed." << std::endl;
}

int main() {
    Client client;
    client.connect_to_server();
    
    std::string message = "Hello from client!";
    client.send_message(message);
    client.receive_message();
    
    client.close_connection();
    return 0;
}