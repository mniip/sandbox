#include <string>
#include <set>
#include <sstream>
#include <list>
#include <thread>

#include "sandbox.h"
#include "path.h"
#include "fs.h"
#include "wakeup.h"
#include "config.h"

extern "C" {
#include <stdio.h>
#include <fcntl.h>
#include <syscall.h>
#include <signal.h>
#include <dirent.h>

#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/wait.h>
}

char const *pastebins[] =
{
	"http://tcpst.net/",
	"https://tcpst.net/",
	"http://tcp.st/",
	"https://tcp.st/"
};

pid_t main_process;
class TracedThread;
Sandbox<TracedThread> sandbox;

class TracedThread: public FsThread<TracedThread>
{
public:
	TracedThread(pid_t tid):
		FsThread<TracedThread>(tid),
		pipe_fd(-1)
	{}

	bool can_see_path(std::string path, int at, int &error)
	{
		std::string canon = canon_path_at(tid, at, path);
		for(std::vector<std::string>::const_iterator i = conf_see.begin(), end = conf_see.end(); i != end; i++)
			if(in_dir(*i, canon))
				return true;
//		if(canon == "/")
//			return true;
		return false;
	}

	bool can_write_path(std::string path, int at, int &error)
	{
		std::string canon = canon_path_at(tid, at, path);
		std::string cwd = canon_path_at(tid, AT_FDCWD, "");
		if(in_dir(cwd + "/http:", canon) || in_dir(cwd + "/https:", canon))
		{
			error = -EEXIST;
			return false;
		}
		for(std::vector<std::string>::const_iterator i = conf_write.begin(), end = conf_write.end(); i != end; i++)
			if(in_dir(*i, canon))
				return true;
		return false;
	}

	bool can_create_thread(Sandbox<TracedThread> &s)
	{
		return s.threads.size() < conf_maxthreads;
	}

	void on_exit(Sandbox<TracedThread> &s, int status)
	{
		if(tid == main_process)
			if(WIFSIGNALED(status))
				printf("[%s]\n", strsignal(WTERMSIG(status)));
	}

	bool should_replace_stat(std::string path, struct stat &st, int at, int flags)
	{
		if(path.compare(0, strlen("http:/"), "http:/") == 0 && path.compare(0, strlen("http://"), "http://") != 0)
			path = "http://" + path.substr(strlen("http:/"));
		else if(path.compare(0, strlen("https:/"), "https:/") == 0 && path.compare(0, strlen("https://"), "https://") != 0)
			path = "https://" + path.substr(strlen("https:/"));
		for(std::vector<std::string>::const_iterator i = conf_http.begin(), end = conf_http.end(); i != end; i++)
			if(path.compare(0, i->size(), *i) == 0)
			{
				st.st_dev = 0;
				st.st_ino = 0;
				st.st_mode = 0755 | S_IFREG;
				st.st_nlink = 1;
				st.st_uid = 0;
				st.st_gid = 0;
				st.st_rdev = 0;
				st.st_size = 0;
				st.st_blksize = 0;
				st.st_blocks = 0;

				clock_gettime(CLOCK_REALTIME, &st.st_mtim);
				st.st_atim = st.st_ctim = st.st_mtim;

				return true;
			}
		return false;
	}

	int pipe_fd;

	bool should_replace_path(std::string path, std::string &newpath, int at, int flags)
	{
		if(path.compare(0, strlen("http:/"), "http:/") == 0 && path.compare(0, strlen("http://"), "http://") != 0)
			path = "http://" + path.substr(strlen("http:/"));
		else if(path.compare(0, strlen("https:/"), "https:/") == 0 && path.compare(0, strlen("https://"), "https://") != 0)
			path = "https://" + path.substr(strlen("https:/"));
		for(std::vector<std::string>::const_iterator i = conf_http.begin(), end = conf_http.end(); i != end; i++)
			if(path.compare(0, i->size(), *i) == 0)
			{
				if((flags & O_ACCMODE) != O_RDONLY)
				{
					newpath = "/dev/null";
					return true;
				}
				char filename[] = "download_XXXXXX";
				int fd = mkstemp(filename);
				if(fd == -1)
					return false;
				unlink(filename);

				pid_t pid = fork();
				if(pid == -1)
				{
					close(fd);
					return false;
				}

				if(pid == 0)
				{
					dup2(fd, 1);
					close(fd);
					execlp("curl", "curl", "--silent", "--globoff", "--", path.c_str(), NULL);
					exit(1);
				}

				while(1)
				{
					int status;
					if(waitpid(pid, &status, 0) == -1)
					{
						close(fd);
						return false;
					}
					if(WIFEXITED(status) || WIFSIGNALED(status))
						break;
				}

				pipe_fd = fd;
				std::stringstream ss;
				ss << "/proc/" << getpid() << "/fd/" << pipe_fd;

				newpath = ss.str();
				return true;
			}
		return false;
	}
	
	void after_replace_path(bool success)
	{
		if(pipe_fd != -1)
			close(pipe_fd);
		pipe_fd = -1;
	}
};

void set_limits()
{
	dup2(in_pipe[0], 0);
	dup2(out_pipe[1], 1);
	dup2(out_pipe[1], 2);
	close(in_pipe[0]);
	close(in_pipe[1]);
	close(out_pipe[0]);
	close(out_pipe[1]);

	if(conf_closefds)
	{
		DIR *fds = opendir("/proc/self/fd/");
		if(fds)
		{
			struct dirent *dent;
			while(dent = readdir(fds))
				if(atoi(dent->d_name) > 2)
					close(atoi(dent->d_name));
			closedir(fds);
		}
	}

	fflush(stdout);

	if(conf_clearenv)
		clearenv();
	for(std::vector<std::pair<std::string, std::string>>::const_iterator i = conf_setenv.begin(), end = conf_setenv.end(); i != end; i++)
		setenv(i->first.c_str(), i->second.c_str(), 1);
	for(std::vector<std::string>::const_iterator i = conf_unsetenv.begin(), end = conf_unsetenv.end(); i != end; i++)
		unsetenv(i->c_str());

	if(conf_chdir)
		chdir(conf_chdirto.c_str());

	struct rlimit limit;
	for(std::vector<std::pair<int, rlim_t>>::const_iterator i = conf_rlimit.begin(), end = conf_rlimit.end(); i != end; i++)
	{
		limit.rlim_max = limit.rlim_cur = i->second;
		setrlimit(i->first, &limit);
	}
}

void detach(int sig)
{
	exit(0);
}

void ignore(int sig)
{
}

int main(int argc, char **argv)
{
	if(argc < 3 || (std::string(argv[2]) == "kill" && argc < 4))
	{
		fprintf(stderr, "Usage: %s <config.conf> <ident> [prog [args...]]\n", argv[0]);
		fprintf(stderr, "       %s <config.conf> kill <ident>\n", argv[0]);
		abort();
	}

	ident = std::string(argv[2]) == "kill" ? argv[3] : argv[2];
	read_conf(argv[1]);

	if(std::string(argv[2]) == "kill")
	{
		kill_session();
		printf("Done\n");
		exit(EXIT_SUCCESS);
	}

	try_connect();

	signal(SIGUSR1, ignore);

	int pid = fork();
	if(pid < 0)
		panic_errno("fork");

	if(!pid)
	{
		if(pipe(in_pipe))
			panic_errno("pipe");
		if(pipe(out_pipe))
			panic_errno("pipe");
		if(argc >= 4)
		{
			main_process = sandbox.spawn_process(argv[3], argc - 3, argv + 3, set_limits);
		}
		else
		{
			std::vector<char *> av;
			for(std::vector<std::string>::iterator i = conf_arg.begin(), end = conf_arg.end(); i != end; i++)
				av.push_back(&(*i)[0]);
			main_process = sandbox.spawn_process(conf_program.c_str(), av.size(), av.data(), set_limits);
		}
		close(in_pipe[0]);
		close(out_pipe[1]);
		feed_in = new std::thread(feed_data, fileno(stdin), in_pipe[1], true);
		feed_out = new std::thread(feed_data, out_pipe[0], fileno(stdout), false);
		sandbox.event_loop();
		exit(0);
	}
	else
	{
		signal(SIGUSR1, detach);
		wait(NULL);
	}
}
