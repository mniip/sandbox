#include <sstream>
#include <fstream>

#include "config.h"
#include "wakeup.h"

static void process_line(std::string key, std::istringstream &iss)
{
	printf("%s\n", key.c_str());
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
