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

#define FSROOT "/var/lib/xsbot/sandbox/root"
#define FSHOME FSROOT "/data"

char const *allowed_paths[] =
{
	FSROOT,
	"/usr/lib/tcc/libtcc1.a",
	"/proc/self",
	"/bin/dash",
	"/dev/null",
	"/dev/zero",
	"/dev/urandom",
	"/dev/full",
};

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
		for(int i = 0; i < sizeof allowed_paths / sizeof *allowed_paths; i++)
			if(in_dir(allowed_paths[i], canon))
				return true;
//		if(canon == "/")
//			return true;
		return false;
	}

	bool can_write_path(std::string path, int at, int &error)
	{
		std::string canon = canon_path_at(tid, at, path);
		if(in_dir(FSHOME "/http:", canon) || in_dir(FSHOME "/https:", canon))
		{
			error = -EEXIST;
			return false;
		}
		if(in_dir(FSHOME, canon))
			return true;
		return false;
	}

	bool can_create_thread(Sandbox<TracedThread> &s)
	{
		return s.threads.size() < 16;
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
		for(int i = 0; i < sizeof pastebins / sizeof *pastebins; i++)
			if(path.compare(0, strlen(pastebins[i]), pastebins[i]) == 0)
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
		for(int i = 0; i < sizeof pastebins / sizeof *pastebins; i++)
			if(path.compare(0, strlen(pastebins[i]), pastebins[i]) == 0)
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

	DIR *fds = opendir("/proc/self/fd/");
	if(fds)
	{
		struct dirent *dent;
		while(dent = readdir(fds))
			if(atoi(dent->d_name) > 2)
				close(atoi(dent->d_name));
		closedir(fds);
	}
	fflush(stdout);

	setenv("LD_LIBRARY_PATH", FSROOT "/lib:" FSROOT "/usr/lib", 1);
	setenv("PATH", FSROOT "/bin", 1);
	setenv("SHELL", FSROOT "/bin/sh", 1);
	chdir(FSHOME);
	struct rlimit limit;
	limit.rlim_max = limit.rlim_cur = 2047 * 1024 * 1024;
	setrlimit(RLIMIT_AS, &limit);
	limit.rlim_max = limit.rlim_cur = 0;
	setrlimit(RLIMIT_CORE, &limit);

	//limit.rlim_max = limit.rlim_cur = 5;
	//setrlimit(RLIMIT_CPU, &limit);
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
		main_process = sandbox.spawn_process(argv[3], argc - 3, argv + 3, set_limits);
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
