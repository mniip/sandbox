#include <list>
#include <string>
#include <stdexcept>

#include "sandbox.h"

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/uio.h>
}

void panic(std::string s)
{
	throw std::runtime_error(s);
}

void panic_errno(std::string s)
{
	panic(s + ": " + strerror(errno));
}

long ThreadData::ptrace(int request, void *addr, void *data)
{
	errno = 0;
	long result = ::ptrace((enum __ptrace_request)request, tid, addr, data);
	if(errno == ESRCH)
		throw ThreadGone(tid);
	return result;
}

bool ThreadData::ptrace_setoptions(int options)
{
	return !ptrace(PTRACE_SETOPTIONS, NULL, (void *)(intptr_t)options);
}

bool ThreadData::ptrace_geteventmsg(unsigned long &msg)
{
	return !ptrace(PTRACE_GETEVENTMSG, NULL, &msg);
}

bool ThreadData::ptrace_resume(int resume, int signal)
{
	if(resume != PTRACE_SYSCALL)
		had_syscall_entry = false;
	return !ptrace(resume, NULL, (void *)(intptr_t)signal);
}

bool ThreadData::ptrace_getsiginfo(siginfo_t &info)
{
	return !ptrace(PTRACE_GETSIGINFO, NULL, &info);
}

bool ThreadData::ptrace_peekdata(void *addr, long &word)
{
	errno = 0;
	word = ptrace(PTRACE_PEEKDATA, addr, NULL);
	return errno == 0;
}

bool ThreadData::ptrace_pokedata(void *addr, long word)
{
	errno = 0;
	return !ptrace(PTRACE_POKEDATA, addr, (void *)(intptr_t)word);
}

bool ThreadData::ptrace_getregset(int set, struct iovec &iov)
{
	return !ptrace(PTRACE_GETREGSET, (void *)(intptr_t)set, (void *)&iov);
}

bool ThreadData::ptrace_setregset(int set, struct iovec &iov)
{
	return !ptrace(PTRACE_SETREGSET, (void *)(intptr_t)set, (void *)&iov);
}

void ThreadData::set_options()
{
	if(!set_opts)
	{
		if(!ptrace_setoptions(PTRACE_O_EXITKILL | PTRACE_O_TRACECLONE | PTRACE_O_TRACEEXEC | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK))
			panic_errno("ptrace SETOPTIONS");
		set_opts = true;
	}
}
