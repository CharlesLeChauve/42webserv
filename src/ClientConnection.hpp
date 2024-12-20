// ClientConnection.hpp
#ifndef CLIENTCONNECTION_HPP
#define CLIENTCONNECTION_HPP

#include <string>

class Server;
class HTTPRequest;
class HTTPResponse;
class CGIHandler;

class ClientConnection {
private:
    Server* _server;
    HTTPRequest* _request;
    HTTPResponse* _response;
    CGIHandler* _cgiHandler;

    std::string _responseBuffer;
    size_t _responseOffset;
    bool _isSending;
    bool _exchangeOver;
    bool _used;


public:
    ClientConnection(Server* server);
    ~ClientConnection();

    Server* getServer() const;
    HTTPRequest* getRequest() const;
    HTTPResponse* getResponse() const;
    CGIHandler* getCgiHandler() const;
    bool getExchangeOver() const;   
    bool getUsed() const;

    void setExchangeOver(bool value);
    void setCgiHandler(CGIHandler* cgiHandler);
    void setRequest(HTTPRequest* request);
    void setResponse(HTTPResponse* response);
    void setRequestActivity(unsigned long time);

    void prepareResponse();
    int sendResponseChunk(int client_fd);
    bool isResponseComplete() const;
    void resetConnection();

};

#endif // CLIENTCONNECTION_HPP
