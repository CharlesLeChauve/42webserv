// ClientConnection.cpp
#include "ClientConnection.hpp"
#include "Server.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include <unistd.h>
#include <errno.h>

ClientConnection::ClientConnection(Server* server)
    : _server(server), _request(NULL), _response(NULL), _responseOffset(0), _isSending(false) {}

ClientConnection::~ClientConnection() {
    delete _request;
    delete _response;
}

Server* ClientConnection::getServer() { return _server; }
HTTPRequest* ClientConnection::getRequest() { return _request; }
HTTPResponse* ClientConnection::getResponse() { return _response; }
// CGIHandler* ClientConnection::getCgiHandler() { return _cgiHandler; }


        // if (connection.getCGIProcess())
        // {
            // CGIHandler cgiHandler;
            // std::string cgiOutput = cgiHandler.executeCGI(fullPath, request);

            // int bytes_written = write(client_fd, cgiOutput.c_str(), cgiOutput.length());
            // if (bytes_written == -1) {
            //     response.beError(500); // Internal Server Error
            //     Logger::instance().log(WARNING, "500 error (Internal Server Error): Failed to send CGI output response.");
            // }
        // }

// void ClientConnection::setCgiHandler(CGIHandler* cgiHandler) { this->_cgiHandler = cgiHandler; }
void ClientConnection::setRequest(HTTPRequest* request) { this->_request = request; }
void ClientConnection::setResponse(HTTPResponse* response) { this->_response = response; }
void ClientConnection::setRequestActivity(unsigned long time) { _request->setLastActivity(time); }

void ClientConnection::prepareResponse() {
    if (_response) {
        _responseBuffer = _response->toString();
        _responseOffset = 0;
        _isSending = true;
    }
}

bool ClientConnection::sendResponseChunk(int client_fd) {
    if (!_isSending) return false;

    const size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    size_t remaining = _responseBuffer.size() - _responseOffset;
    size_t bytesToSend = std::min(remaining, BUFFER_SIZE);

    // Copier les données dans le buffer fixe
    std::memcpy(buffer, _responseBuffer.c_str() + _responseOffset, bytesToSend);

    ssize_t bytesSent = write(client_fd, buffer, bytesToSend);

    if (bytesSent > 0) {
        _responseOffset += bytesSent;
        if (_responseOffset >= _responseBuffer.size()) {
            _isSending = false;
            return true; // Réponse entièrement envoyée
        }
    } else if (bytesSent == -1) {
        // Erreur lors de l'écriture
        _isSending = false;
        return true; // Considérer que la réponse est complète pour fermer la connexion
    }
    return false; // Réponse pas encore entièrement envoyée
}


bool ClientConnection::isResponseComplete() const {
    return !_isSending;
}

