#pragma once

#include "Logger.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include <string>
#include <cstring>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <sys/time.h>
#include <sys/stat.h>
#include <map>
#include <iostream>
#include <iomanip>
#include <fstream>


class SessionManager
{
private:
	std::string		_session_id;
	bool			_first_con;
	std::map<std::string, std::string> _session_data;
	void setData(const std::string& key, const std::string& value, bool append = false);
	void manageUserSession(HTTPRequest* request, HTTPResponse* response, int client_fd, SessionManager& session);
	void	persistSession();
	void	loadSession();

public:
	SessionManager(std::string session_id);
	SessionManager();
	~SessionManager();

	std::string getData(const std::string& key) const;
	std::string	curr_time();
	void	getManager(HTTPRequest* request, HTTPResponse* response, int client_fd, SessionManager& session);


	std::string generate_session_id();
	std::string generateUUID();
	const std::string& getSessionId() const;
	bool getFirstCon() const;	
};

