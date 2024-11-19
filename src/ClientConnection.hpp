#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include "Server.hpp"


class ClientConnection
{
private:
	Server *_server;
	HTTPRequest *_request;
	HTTPResponse *_response;
public:
	ClientConnection(Server* server);
	~ClientConnection();

	Server* getServer();
	HTTPRequest* getRequest();
	HTTPResponse* getResponse();
	void setRequest(HTTPRequest* request);	
	void setResponse(HTTPResponse* response);
	void setRequestActivity(unsigned long time);
};
