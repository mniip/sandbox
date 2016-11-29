#ifndef ARCH_X86_64_H
#define ARCH_X86_64_H

#include "arch_base.h"

extern "C" {
#include <stdint.h>
#include <string.h>
}

struct regs_x86_64
{
	uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8, rax, rcx, rdx, rsi, rdi, orig_rax, rip, cs, eflags, rsp, ss, fs_base, gs_base, ds, es, fs, gs;
};

struct regs_x86_32
{
	uint32_t ebx, ecx, edx, esi, edi, ebp, eax, ds, es, fs, gs, orig_eax, eip, cs, eflags, esp, ss;
};

class SyscallInfo: public SyscallInfoBase<struct regs_x86_64, uint64_t>
{
public:
	bool is_compat_regset;
	bool is_compat_table;
	inline SyscallInfo(ThreadData &thread):
		SyscallInfoBase(thread, false),
		is_compat_regset(false),
		is_compat_table(false)
	{
		struct iovec iov = { .iov_base = (void *)&regs, .iov_len = sizeof regs };
		thread.ptrace_getregset(NT_PRSTATUS, iov);
		if(iov.iov_len == sizeof(regs_x86_32))
		{
			is_compat_regset = true;
			is_compat_table = true;
			struct regs_x86_32 tmp;
			memcpy(&tmp, &regs, sizeof tmp);

			regs.rbx = tmp.ebx;
			regs.rcx = tmp.ecx;
			regs.rdx = tmp.edx;
			regs.rsi = tmp.esi;
			regs.rdi = tmp.edi;
			regs.rbp = tmp.ebp;
			regs.rax = tmp.eax;
			regs.ds = tmp.ds;
			regs.es = tmp.es;
			regs.fs = tmp.fs;
			regs.gs = tmp.gs;
			regs.orig_rax = tmp.orig_eax;
			regs.rip = tmp.eip;
			regs.cs = tmp.cs;
			regs.eflags = tmp.eflags;
			regs.rsp = tmp.esp;
			regs.ss = tmp.ss;
		}
		else if(!regs.r11)
		{
			is_compat_table = true;
		}
	}

	virtual inline uint64_t &sysnum() { return regs.orig_rax; }
	virtual inline uint64_t &ret() { return regs.rax; }
	virtual inline uint64_t &arg(int arg)
	{
		if(is_compat_table)
			switch(arg)
			{
				case 1: return regs.rbx;
				case 2: return regs.rcx;
				case 3: return regs.rdx;
				case 4: return regs.rsi;
				case 5: return regs.rdi;
				case 6: return regs.rbp;
			}
		else
			switch(arg)
			{
				case 1: return regs.rdi;
				case 2: return regs.rsi;
				case 3: return regs.rdx;
				case 4: return regs.r10;
				case 5: return regs.r8;
				case 6: return regs.r9;
			}
	}

    bool save()
    {
		if(is_compat_regset)
		{
			struct regs_x86_32 tmp;
			tmp.ebx = regs.rbx;
			tmp.ecx = regs.rcx;
			tmp.edx = regs.rdx;
			tmp.esi = regs.rsi;
			tmp.edi = regs.rdi;
			tmp.ebp = regs.rbp;
			tmp.eax = regs.rax;
			tmp.ds = regs.ds;
			tmp.es = regs.es;
			tmp.fs = regs.fs;
			tmp.gs = regs.gs;
			tmp.orig_eax = regs.orig_rax;
			tmp.eip = regs.rip;
			tmp.cs = regs.cs;
			tmp.eflags = regs.eflags;
			tmp.esp = regs.rsp;
			tmp.ss = regs.ss;
			struct iovec iov = { .iov_base = (void *)&tmp, .iov_len = sizeof tmp };
			return thread.ptrace_setregset(NT_PRSTATUS, iov);
		}
		else
		{
			struct iovec iov = { .iov_base = (void *)&regs, .iov_len = sizeof regs };
			return thread.ptrace_setregset(NT_PRSTATUS, iov);
		}
    }
};

#endif
