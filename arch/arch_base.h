#ifndef ARCH_BASE_H
#define ARCH_BASE_H

#include "../sandbox.h"

extern "C" {
#include <stdint.h>

#include <sys/uio.h>

#include <linux/elf.h>
}

template<typename T, typename R> class SyscallInfoBase
{
protected:
	inline SyscallInfoBase(ThreadData &thread_, bool):
		thread(thread_)
	{}

public:
	T regs;

	ThreadData &thread;

	inline SyscallInfoBase(ThreadData &thread_):
		thread(thread_)
	{
		struct iovec iov = { .iov_base = (void *)&regs, .iov_len = sizeof regs };
		thread.ptrace_getregset(NT_PRSTATUS, iov);
	}

	bool save()
	{
		struct iovec iov = { .iov_base = (void *)&regs, .iov_len = sizeof regs };
		return thread.ptrace_setregset(NT_PRSTATUS, iov);
	}

	virtual R &sysnum() = 0;
	virtual R &ret() = 0;
	virtual R &arg(int) = 0;

	uint8_t fetch_byte(void *addr)
	{
		long word = 0;
		thread.ptrace_peekdata((void *)((uintptr_t)addr & ~(sizeof word - 1)), word);
		return ((uint8_t *)&word)[(uintptr_t)addr & (sizeof word - 1)];
	}

	size_t fetch(void *addr, void *buf, size_t len)
	{
		long word;
		void *read_addr = (void *)((uintptr_t)addr & ~(sizeof word - 1));
		int off = (uintptr_t)addr & (sizeof word - 1);

		size_t count = 0;

		if(!thread.ptrace_peekdata(read_addr, word))
			return count;

		while(count < len)
		{
			uint8_t b = ((uint8_t *)&word)[off];
			((uint8_t *)buf)[count] = b;
			count++;
			if(++off == sizeof word)
			{
				off = 0;
				read_addr = (void *)((long *)read_addr + 1);
				if(!thread.ptrace_peekdata(read_addr, word))
					return count;
			}
		}
		return count;
	}

	std::string fetch_cstr(void *addr)
	{
		long word;
		void *read_addr = (void *)((uintptr_t)addr & ~(sizeof word - 1));
		int off = (uintptr_t)addr & (sizeof word - 1);
		
		std::string r;

		if(!thread.ptrace_peekdata(read_addr, word))
			return r;

		uint8_t b;
		do
		{
			b = ((uint8_t *)&word)[off];
			r.push_back(b);
			if(++off == sizeof word)
			{
				off = 0;
				read_addr = (void *)((long *)read_addr + 1);
				if(!thread.ptrace_peekdata(read_addr, word))
					return r;
			}
		}
		while(b);
		return r;
	}
};

#endif
