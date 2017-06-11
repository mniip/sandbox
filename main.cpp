#include <string>
#include <list>
#include <thread>

#include "sandbox.h"
#include "path.h"
#include "fs.h"
#include "wakeup.h"

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
	"/bin/dash",
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
	dup2(in_pipe[0], 0);
	dup2(out_pipe[1], 1);
	dup2(out_pipe[1], 2);
	close(in_pipe[0]);
	close(in_pipe[1]);
	close(out_pipe[0]);
	close(out_pipe[1]);

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

void detach(int sig)
{
	exit(0);
}

void ignore(int sig)
{
}

int main(int argc, char **argv)
{
	ident = argv[1];
	try_connect();

	signal(SIGUSR1, ignore);

	int pid = fork();
	if(pid < 0)
		panic_errno("fork");

	if(!pid)
	{
		Sandbox<TracedThread> s;
		if(pipe(in_pipe))
			panic_errno("pipe");
		if(pipe(out_pipe))
			panic_errno("pipe");
		main_process = s.spawn_process(argv[2], argc - 2, argv + 2, set_limits);
		close(in_pipe[0]);
		close(out_pipe[1]);
		feed_in = new std::thread(feed_data, fileno(stdin), in_pipe[1]);
		feed_out = new std::thread(feed_data, out_pipe[0], fileno(stdout));
		set_timer();
		s.event_loop();
		exit(0);
	}
	else
	{
		signal(SIGUSR1, detach);
		wait(NULL);
	}
}
