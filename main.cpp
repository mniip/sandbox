#include "sandbox.h"

extern "C" {
#include <stdio.h>
#include <sys/wait.h>
}

class TracedThread: public Thread<TracedThread>
{
public:
	TracedThread(pid_t tid): Thread<TracedThread>(tid) {}

	virtual void on_exit(Sandbox<TracedThread> s, int status)
	{
		printf("pid=%d exited with code %d\n", tid, WEXITSTATUS(status));
	}
};

int main(int argc, char **argv)
{
	Sandbox<TracedThread> s;
	s.spawn_process(argv[1], argc - 1, argv + 1);
	s.event_loop();
}
