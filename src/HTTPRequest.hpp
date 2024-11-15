#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include "ServerConfig.hpp"
#include <string>
#include <map>


class HTTPRequest {
public:
	HTTPRequest();
	HTTPRequest(int def_max_body_size);
	~HTTPRequest();

	std::string getMethod() const;
	std::string getPath() const;
	std::string getQueryString() const;
	std::map<std::string, std::string> getHeaders() const;

	std::string getStrHeader(std::string header) const;
	bool hasHeader(std::string header) const;

	std::string getBody() const;
	std::string getHost() const;
	void trim(std::string& s) const;

	bool parse();
    std::string toString() const;
    std::string toStringHeaders() const;
	void parseRawRequest(const ServerConfig& config);

	std::string _rawRequest;

	bool getHeadersParsed() const;
    bool getRequestTooLarge() const;
    size_t getContentLength() const;
	size_t getBodyReceived() const;
	int	getMaxBodySize() const;
	std::string getRawRequest() const;
	unsigned long getLastActivity() const;


	void setBodyReceived(size_t size);
	void setRequestTooLarge(bool value);

	bool isComplete() const;
    void setComplete(bool value);
    bool getConnectionClosed() const;
    void setConnectionClosed(bool value);
	void setLastActivity(unsigned long timestamp);


private:
	std::string _method;
	std::string _path;
	std::string _queryString;
	std::string _body;
	std::map<std::string, std::string> _headers;
	bool _complete;
    bool _connectionClosed;

	int _maxBodySize;
	size_t _contentLength;
    size_t _bodyReceived;
    bool _headersParsed;
    bool _requestTooLarge;

	unsigned long _lastActivity;


	bool parseRequestLine(const std::string& line);
	void parseHeaders(const std::string& headers);
	void parseBody(const std::string& body);
	void parseQueryString();
};

#endif
