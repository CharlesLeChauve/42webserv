// ClientConnection.hpp
#ifndef CLIENTCONNECTION_HPP
#define CLIENTCONNECTION_HPP

#include <string>

// Forward declarations
class Server;
class HTTPRequest;
class HTTPResponse;

class ClientConnection {
private:
    Server* _server;
    HTTPRequest* _request;
    HTTPResponse* _response;

    // Attributes for managing response sending
    std::string _responseBuffer;
    size_t _responseOffset;
    bool _isSending;

public:
    ClientConnection(Server* server);
    ~ClientConnection();

    Server* getServer();
    HTTPRequest* getRequest();
    HTTPResponse* getResponse();
    void setRequest(HTTPRequest* request);
    void setResponse(HTTPResponse* response);
    void setRequestActivity(unsigned long time);

    // Methods to manage sending the response
    void prepareResponse();
    bool sendResponseChunk(int client_fd);
    bool isResponseComplete() const;
};

#endif // CLIENTCONNECTION_HPP
