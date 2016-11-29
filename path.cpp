#include <sstream>
#include <string>

extern "C" {
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
}

bool read_link(std::string path, std::string &out)
{
	int fd = open(path.c_str(), O_PATH | O_NOFOLLOW);
	if(fd < 0)
		return false;
	struct stat stat;
	if(fstat(fd, &stat))
	{
		close(fd);
		return false;
	}
	if((stat.st_mode & S_IFMT) != S_IFLNK)
	{
		close(fd);
		return false;
	}
	size_t size = stat.st_size;
	if(!size)
		size++;
	char *buf = (char *)malloc(size + 1);
	ssize_t sz;
	while(1)
	{
		sz = readlinkat(fd, "", buf, size + 1);
		if(sz < 0)
		{
			close(fd);
			free(buf);
			return false;
		}
		if(sz > size)
		{
			size *= 2;
			buf = (char *)realloc(buf, size + 1);
		}
		else
			break;
	}
	close(fd);
	out.assign(buf, sz);
	free(buf);
	return true;
}

std::string canon_path_at(pid_t tid, int fd, std::string path)
{
	path = path.c_str();
	std::string real = "";
	if(*path.begin() != '/')
	{
		std::string proc;
		if(fd == AT_FDCWD)
			proc = ((std::ostringstream &)(std::ostringstream() << "/proc/" << tid << "/cwd")).str();
		else
			proc = ((std::ostringstream &)(std::ostringstream() << "/proc/" << tid << "/fd/" << fd)).str();
		if(!read_link(proc, real))
			return path;
	}

	std::stringstream s(path);
	std::string segment;
	while(std::getline(s, segment, '/'))
		if(segment != "" && segment != ".")
		{
			if(segment != "..")
				real += "/" + segment;
			else
				real.erase(real.rfind('/'), std::string::npos);
		}
	if(*path.rbegin() == '/')
		real += "/";
	return real;
}

bool in_dir(std::string dir, std::string path)
{
	return path == dir || path.rfind(dir, 0) == 0;
}
