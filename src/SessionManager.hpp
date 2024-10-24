#pragma once
#include <string>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <sys/time.h>
#include <map>
#include <iostream>

class SessionManager
{
private:
	std::string _session_id;
	bool			_first_con;

public:
	SessionManager(std::string session_id);
	SessionManager();
	~SessionManager();

	std::string generate_session_id();
	const std::string& getSessionId() const;
	bool getFirstCon() const;
};

