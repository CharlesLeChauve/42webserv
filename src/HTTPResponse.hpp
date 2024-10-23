#ifndef HTTPRESPONSE_HPP
#define HTTPRESPONSE_HPP

#include <string>
#include <map>

class HTTPResponse {
public:
	HTTPResponse();
	~HTTPResponse();

	void setStatusCode(int code);
	void setReasonPhrase(const std::string& reason);
	void setHeader(const std::string& key, const std::string& value);
	void setBody(const std::string& body);

	int getStatusCode() const;
	std::string getReasonPhrase() const;
	std::map<std::string, std::string> getHeaders() const;
	std::string getBody() const;

	std::string toString() const;

private:
	int _statusCode;
	std::string _reasonPhrase;
	std::map<std::string, std::string> _headers;
	std::string _body;
};

#endif
