// ClientConnection.cpp
#include "ClientConnection.hpp"

#include "CGIHandler.hpp"
#include "Server.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"

#include <unistd.h>
#include <errno.h>

ClientConnection::ClientConnection(Server* server)
    : _server(server), _request(NULL), _response(NULL), _cgiHandler(NULL), _responseOffset(0), _isSending(false), _exchangeOver(false), _used(false) {}

ClientConnection::~ClientConnection() {
    delete _request;
    delete _response;
}

Server* ClientConnection::getServer() const { return _server; }
HTTPRequest* ClientConnection::getRequest() const { return _request; }
HTTPResponse* ClientConnection::getResponse() const { return _response; }
CGIHandler* ClientConnection::getCgiHandler() const { return _cgiHandler; }
bool ClientConnection::getExchangeOver() const { return _exchangeOver; }
bool ClientConnection::getUsed() const { return _used; }

void ClientConnection::setExchangeOver(bool value) { _exchangeOver = value; }
void ClientConnection::setCgiHandler(CGIHandler* cgiHandler) { this->_cgiHandler = cgiHandler; }
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

int ClientConnection::sendResponseChunk(int client_fd) {
    if (!_isSending) return false;

    const size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];

    size_t remaining = _responseBuffer.size() - _responseOffset;
    size_t bytesToSend = std::min(remaining, BUFFER_SIZE);

    std::memcpy(buffer, _responseBuffer.c_str() + _responseOffset, bytesToSend);

    ssize_t bytesSent = write(client_fd, buffer, bytesToSend);

    if (bytesSent > 0) {
        _responseOffset += bytesSent;
        if (_responseOffset >= _responseBuffer.size()) {
            _isSending = false;
            return 0; // Response fully sent
        }
    } else if (bytesSent == -1) {
        _isSending = false;
        return -1;
    }
    return 1; // Response not fully sent
}

void ClientConnection::resetConnection() {
    if (_request) {
        delete _request;
        _request = NULL;
    }
    if (_response) {
        delete _response;
        _response = NULL;
    }
    if (_cgiHandler) {
        delete _cgiHandler;
        _cgiHandler = NULL;
    }
    _responseOffset = 0;
    _isSending = false;
    _exchangeOver = false;
    _used = true;
}


bool ClientConnection::isResponseComplete() const {
    return !_isSending;
}

