#pragma once

#include "Logger.hpp"
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

public:
	SessionManager(std::string session_id);
	SessionManager();
	~SessionManager();

	void setData(const std::string& key, const std::string& value, bool append = false);
	std::string getData(const std::string& key) const;
	void	persistSession();
	void	loadSession();
	std::string	curr_time();


	std::string generate_session_id();
	std::string generateUUID();
	const std::string& getSessionId() const;
	bool getFirstCon() const;	
};

