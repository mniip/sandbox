#ifndef SANDBOX_H_
#define SANDBOX_H_

#include <list>
#include <string>
#include <stdexcept>

extern "C" {
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include <sys/wait.h>
#include <sys/signal.h>
}

class ThreadGone: public std::runtime_error
{
public:
	pid_t tid;

	ThreadGone(pid_t tid_):
		runtime_error("Thread is gone"), tid(tid_)
	{}
};

class ThreadData
{
public:
	pid_t tid;

	bool had_syscall_entry;
	bool set_opts;

	bool suppress_stop;
	bool unknown;

	inline ThreadData(pid_t tid_):
		tid(tid_),
		set_opts(false),
		had_syscall_entry(false),
		suppress_stop(false),
		unknown(false)
	{}

	long ptrace(int, void *, void *);
	bool ptrace_setoptions(int);
	bool ptrace_geteventmsg(unsigned long &);
	bool ptrace_resume(int, int signal = 0);
	bool ptrace_getsiginfo(siginfo_t &);

	void set_options();
};

template<typename T> class Sandbox;

template<typename T> class Thread: public ThreadData
{
public:
	inline Thread(pid_t tid): ThreadData(tid) {}

	virtual inline void on_syscall_entry(Sandbox<T>) {};
	virtual inline void on_syscall_exit(Sandbox<T>) {};
	virtual inline void on_fork(Sandbox<T>, pid_t) {};
	virtual inline void on_vfork(Sandbox<T>, pid_t) {};
	virtual inline void on_clone(Sandbox<T>, pid_t) {};
	virtual inline void on_exec(Sandbox<T>) {};
	virtual inline void on_exit(Sandbox<T>, int) {};
	virtual inline int on_kill(Sandbox<T>, int status) { return WSTOPSIG(status); };
};

template<typename T> class Sandbox
{
public:
	std::list<T> threads;

	typename std::list<T>::iterator thread_add(pid_t);
	typename std::list<T>::iterator find_tid(pid_t);

	void event_loop();
	pid_t spawn_process(char const *, int, char const *const *);
	static bool ptrace_traceme();
};

void panic(std::string);
void panic_errno(std::string);

#include "sandbox.impl"
#endif
