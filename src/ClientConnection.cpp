#include "ClientConnection.hpp"

Server* ClientConnection::getServer() {return _server;}
HTTPRequest* ClientConnection::getRequest() {return _request;}
HTTPResponse* ClientConnection::getResponse() {return _response;}

void	ClientConnection::setRequest(HTTPRequest* request) {this->_request = request;}
void	ClientConnection::setResponse(HTTPResponse* response) {this->_response = response;}

void 	ClientConnection::setRequestActivity(unsigned long time) {_request->setLastActivity(time);}



ClientConnection::ClientConnection(Server* server) : _server(server)
{
	_request = NULL;
	_response = NULL;
}

ClientConnection::~ClientConnection()
{
	delete _request;
	delete _response;
}
