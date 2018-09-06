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
	virtual R &stack() = 0;

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

	bool fetch_cstr(void *addr, std::string &r)
	{
		long word;
		void *read_addr = (void *)((uintptr_t)addr & ~(sizeof word - 1));
		int off = (uintptr_t)addr & (sizeof word - 1);

		r = "";

		if(!thread.ptrace_peekdata(read_addr, word))
			return false;

		uint8_t b;
		do
		{
			b = ((uint8_t *)&word)[off];
			if(b)
				r.push_back(b);
			if(++off == sizeof word)
			{
				off = 0;
				read_addr = (void *)((long *)read_addr + 1);
				if(!thread.ptrace_peekdata(read_addr, word))
					return false;
			}
		}
		while(b);
		return true;
	}

	std::vector<uint8_t> fetch_array(void *addr, size_t length)
	{
		long word;
		void *read_addr = (void *)((uintptr_t)addr & ~(sizeof word - 1));
		int off = (uintptr_t)addr & (sizeof word - 1);
		
		std::vector<uint8_t> r;
		r.reserve(length);

		if(!thread.ptrace_peekdata(read_addr, word))
			return r;

		uint8_t b;
		for(size_t i = 0; i < length; i++)
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
		return r;
	}

	bool emplace_array(void *addr, std::vector<uint8_t> array)
	{
		long word;
		void *write_addr = (void *)((uintptr_t)addr & ~(sizeof word - 1));
		int off = (uintptr_t)addr & (sizeof word - 1);

		if(off)
			if(!thread.ptrace_peekdata(write_addr, word))
				return false;

		size_t i = 0;
		while(write_addr < (char *)addr + array.size())
		{
			((uint8_t *)&word)[off] = array[i++];
			if(++off == sizeof word)
			{
				off = 0;
				if(!thread.ptrace_pokedata(write_addr, word))
					return false;
				write_addr = (void *)((long *)write_addr + 1);
				if(write_addr < (char *)addr + array.size() && write_addr > (char *)addr + array.size() - 8)
					if(!thread.ptrace_peekdata(write_addr, word))
						return false;
			}
		}
		return true;
	}
};

#endif
