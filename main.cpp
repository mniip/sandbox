#include <string>
#include <list>
#include <thread>

#include "sandbox.h"
#include "path.h"
#include "fs.h"

extern "C" {
#include <stdio.h>
#include <fcntl.h>
#include <syscall.h>
#include <signal.h>

#include <sys/resource.h>
#include <sys/wait.h>
}

char const *allowed_paths[] =
{
	"/var/lib/xsbot/sandbox/root",
	"/usr/lib/tcc/libtcc1.a",
	"/proc/self",
	"/bin/sh",
	"/dev/null",
	"/dev/zero",
	"/dev/urandom",
	"/dev/full",
};

pid_t main_process;

class TracedThread: public FsThread<TracedThread>
{
public:
	TracedThread(pid_t tid):
		FsThread<TracedThread>(tid)
	{}

	bool can_see_path(std::string path, int at)
	{
		std::string canon = canon_path_at(tid, at, path);
		for(int i = 0; i < sizeof allowed_paths / sizeof *allowed_paths; i++)
			if(in_dir(allowed_paths[i], canon))
				return true;
//		if(canon == "/")
//			return true;
		return false;
	}

	bool can_write_path(std::string path, int at)
	{
		std::string canon = canon_path_at(tid, at, path);
		if(in_dir("/var/lib/xsbot/sandbox/root/data", canon))
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
};

void set_limits()
{
	setenv("LD_LIBRARY_PATH", "/var/lib/xsbot/sandbox/root/lib:/var/lib/xsbot/sandbox/usr/lib", 1);
	setenv("PATH", "/var/lib/xsbot/sandbox/root/bin", 1);
	setenv("SHELL", "/var/lib/xsbot/sandbox/root/bin/sh", 1);
	chdir("/var/lib/xsbot/sandbox/root/data");
	struct rlimit limit;
	limit.rlim_max = limit.rlim_cur = 1024 * 1024 * 1024;
	setrlimit(RLIMIT_AS, &limit);
	limit.rlim_max = limit.rlim_cur = 0;
	setrlimit(RLIMIT_CORE, &limit);
	limit.rlim_max = limit.rlim_cur = 5;
	setrlimit(RLIMIT_CPU, &limit);
}

void time_limit(Sandbox<TracedThread> *s)
{
	sleep(5);
	printf("[Timed out]\n");
	exit(0);
}

int main(int argc, char **argv)
{
	Sandbox<TracedThread> s;
	main_process = s.spawn_process(argv[1], argc - 1, argv + 1, set_limits);
	std::thread limit(time_limit, &s);
	s.event_loop();
	exit(0);
}


