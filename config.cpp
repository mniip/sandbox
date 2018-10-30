#include <sstream>
#include <fstream>
#include <map>

#include "config.h"
#include "wakeup.h"

std::string conf_program;
std::vector<std::string> conf_arg;
bool conf_closefds;
bool conf_clearenv;
std::vector<std::pair<std::string, std::string>> conf_setenv;
std::vector<std::string> conf_unsetenv;
std::vector<std::string> conf_see;
std::vector<std::string> conf_write;
int conf_maxthreads = 16;
bool conf_chdir;
std::string conf_chdirto;
std::vector<std::pair<int, rlim_t>> conf_rlimit;
std::vector<std::string> conf_http;
std::string conf_downloadpat = "download_XXXXXX";
std::string conf_sockdir = ".";
bool conf_wakeup;
int conf_timelimit = 1;

std::map<std::string, int> rlim_name = {
	{ "AS",         RLIMIT_AS         },
	{ "CORE",       RLIMIT_CORE       },
	{ "CPU",        RLIMIT_CPU        },
	{ "DATA",       RLIMIT_DATA       },
	{ "FSIZE",      RLIMIT_FSIZE      },
	{ "LOCKS",      RLIMIT_LOCKS      },
	{ "MEMLOCK",    RLIMIT_MEMLOCK    },
	{ "MSGQUEUE",   RLIMIT_MSGQUEUE   },
	{ "NICE",       RLIMIT_NICE       },
	{ "NOFILE",     RLIMIT_NOFILE     },
	{ "NPROC",      RLIMIT_NPROC      },
	{ "RSS",        RLIMIT_RSS        },
	{ "RTPRIO",     RLIMIT_RTPRIO     },
	{ "RTTIME",     RLIMIT_RTTIME     },
	{ "SIGPENDING", RLIMIT_SIGPENDING },
	{ "STACK",      RLIMIT_STACK      },
};

static void process_line(std::string key, std::istringstream &iss)
{
	if(key == "program")
	{
		iss >> conf_program;
	}
	else if(key == "arg")
	{
		std::string arg;
		if(iss >> arg)
			conf_arg.push_back(arg);
	}
	else if(key == "closefds")
	{
		conf_closefds = true;
	}
	else if(key == "clearenv")
	{
		conf_clearenv = true;
	}
	else if(key == "unsetenv")
	{
		std::string env;
		if(iss >> env)
			conf_unsetenv.push_back(env);
	}
	else if(key == "setenv")
	{
		std::string env, val;
		if(iss >> env >> val)
			conf_setenv.push_back(std::make_pair(env, val));
	}
	else if(key == "see")
	{
		std::string path;
		if(iss >> path)
			conf_see.push_back(path);
	}
	else if(key == "write")
	{
		std::string path;
		if(iss >> path)
			conf_write.push_back(path);
	}
	else if(key == "maxthreads")
	{
		int th;
		if(iss >> th)
			conf_maxthreads = th;
	}
	else if(key == "chdir")
	{
		if(iss >> conf_chdirto)
			conf_chdir = true;
	}
	else if(key == "rlimit")
	{
		std::string name;
		rlim_t value;
		if(iss >> name >> value)
			if(rlim_name.find(name) != rlim_name.end())
				conf_rlimit.push_back(std::make_pair(rlim_name[name], value));
	}
	else if(key == "http")
	{
		std::string url;
		if(iss >> url)
			conf_http.push_back(url);
	}
	else if(key == "downloadpat")
	{
		std::string pat;
		if(iss >> pat)
			conf_downloadpat = pat;
	}
	else if(key == "sockdir")
	{
		std::string dir;
		if(iss >> dir)
			conf_sockdir = dir;
	}
	else if(key == "wakeup")
	{
		conf_wakeup = true;
	}
	else if(key == "timelimit")
	{
		int limit;
		if(iss >> limit)
			conf_timelimit = limit;
	}
}

static void skip_block_until(std::ifstream &file, std::string until)
{
	std::string line;
	while(std::getline(file, line))
	{
		std::istringstream iss(line);
		std::string key;
		if(!(iss >> key)) continue;
		if(key == until) break;

		if(key == "ident")
			skip_block_until(file, "end");
	}
}

static void read_block_until(std::ifstream &file, std::string until)
{
	std::string line;
	while(std::getline(file, line))
	{
		std::istringstream iss(line);
		std::string key;
		if(!(iss >> key)) continue;
		if(key == until) break;
		
		if(key == "ident")
		{
			std::string id;
			if(!(iss >> id)) continue;
			if(id == ident)
				read_block_until(file, "end");
			else
				skip_block_until(file, "end");
		}
		else if(key != "#")
			process_line(key, iss);
	}
}

void read_conf(std::string filename)
{
	std::ifstream file(filename);
	read_block_until(file, "");
	file.close();
}
