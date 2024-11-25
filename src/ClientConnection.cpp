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

    const char* bufferPtr = _responseBuffer.c_str() + _responseOffset;
    size_t remaining = _responseBuffer.size() - _responseOffset;

    ssize_t bytesSent = write(client_fd, bufferPtr, remaining);
    if (bytesSent > 0) {
        _responseOffset += bytesSent;
        if (_responseOffset >= _responseBuffer.size()) {
            _isSending = false;
            return true; // Response fully sent
        }
    } else if (bytesSent == -1) {
        // Error during write
        _isSending = false;
        return true; // Treat as if the response is complete to close the connection
    }
    return false; // Response not yet fully sent
}

bool ClientConnection::isResponseComplete() const {
    return !_isSending;
}

