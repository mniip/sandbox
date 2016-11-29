#ifndef FS_H
#define FS_H

#include <string>

#include "sandbox.h"
#include "path.h"
#include "arch.h"

extern "C" {
#include <stdio.h>
#include <fcntl.h>
#include <syscall.h>

#include <sys/wait.h>
}

template<typename T> class FsThread: public Thread<T>
{
public:
	FsThread<T>(pid_t tid):
		Thread<T>(tid),
		ret_override(false)
	{}

	bool ret_override;
	long ret_val;

	void block(SyscallInfo &i, long val)
	{
		i.sysnum() = -1;
		i.save();
		ret_override = true;
		ret_val = val;
	}

	virtual bool can_see_path(std::string path, int at = AT_FDCWD) { return true; }
	virtual bool can_write_path(std::string path, int at = AT_FDCWD) { return true; }
	virtual bool can_create_thread(Sandbox<T> &s) { return true; }

	virtual void on_syscall_entry(Sandbox<T> &s)
	{
		SyscallInfo i(*this);
		if(i.is_compat_table)
			return block(i, -ENOSYS);
		switch(i.sysnum())
		{
		case __NR_read:
		case __NR_write:
		case __NR_close:
		case __NR_fstat:
		case __NR_poll:
		case __NR_lseek:
		case __NR_mmap:
		case __NR_mprotect:
		case __NR_munmap:
		case __NR_brk:
		case __NR_rt_sigaction:
		case __NR_rt_sigprocmask:
		case __NR_rt_sigreturn:
		case __NR_ioctl:
		case __NR_pread64:
		case __NR_pwrite64:
		case __NR_readv:
		case __NR_writev:
		case __NR_pipe:
		case __NR_select:
		case __NR_sched_yield:
		case __NR_mremap:
		case __NR_msync:
		case __NR_mincore:
		case __NR_madvise:
		case __NR_dup:
		case __NR_dup2:
		case __NR_pause:
		case __NR_nanosleep:
		case __NR_getitimer:
		case __NR_alarm:
		case __NR_setitimer:
		case __NR_getpid:
		case __NR_sendfile:
		case __NR_exit:
		case __NR_wait4:
		case __NR_uname:
		case __NR_fcntl:
		case __NR_flock:
		case __NR_fsync:
		case __NR_fdatasync:
		case __NR_ftruncate:
		case __NR_getdents:
		case __NR_getcwd:
		case __NR_fchdir:
		case __NR_fchmod:
		case __NR_fchown:
		case __NR_umask:
		case __NR_gettimeofday:
		case __NR_getrlimit:
		case __NR_getrusage:
		case __NR_sysinfo:
		case __NR_times:
		case __NR_getuid:
		case __NR_getgid:
		case __NR_geteuid:
		case __NR_getegid:
		case __NR_setpgid:
		case __NR_getppid:
		case __NR_getpgrp:
		case __NR_getgroups:
		case __NR_getresuid:
		case __NR_getresgid:
		case __NR_getpgid:
		case __NR_getsid:
		case __NR_rt_sigpending:
		case __NR_rt_sigtimedwait:
		case __NR_rt_sigqueueinfo:
		case __NR_rt_sigsuspend:
		case __NR_sigaltstack:
		case __NR_utime:
		case __NR_personality:
		case __NR_fstatfs:
		case __NR_getpriority:
		case __NR_sched_getparam:
		case __NR_sched_getscheduler:
		case __NR_sched_get_priority_max:
		case __NR_sched_get_priority_min:
		case __NR_sched_rr_get_interval:
		case __NR_arch_prctl:
		case __NR_setrlimit:
		case __NR_gettid:
		case __NR_readahead:
		case __NR_time:
		case __NR_futex:
		case __NR_fgetxattr:
		case __NR_sched_getaffinity:
		case __NR_epoll_create:
		case __NR_epoll_ctl_old:
		case __NR_epoll_wait_old:
		case __NR_restart_syscall:
		case __NR_set_tid_address:
		case __NR_fadvise64:
		case __NR_timer_create:
		case __NR_timer_settime:
		case __NR_timer_gettime:
		case __NR_timer_getoverrun:
		case __NR_timer_delete:
		case __NR_clock_gettime:
		case __NR_exit_group:
		case __NR_epoll_wait:
		case __NR_epoll_ctl:
		case __NR_set_robust_list:
		case __NR_get_robust_list:
		case __NR_epoll_pwait:
		case __NR_eventfd:
		case __NR_eventfd2:
		case __NR_epoll_create1:
		case __NR_splice:
		case __NR_tee:
		case __NR_dup3:
		case __NR_pipe2:
		case __NR_preadv:
		case __NR_pwritev:
		//case __NR_mlock2:
		//case __NR_preadv2:
		//case __NR_pwritev2:
			return;
		case __NR_open:
			{
				std::string path = i.fetch_cstr((void *)i.arg(1));
				if(*path.rbegin())
					return block(i, -EBADF);
				if(!can_see_path(path))
					return block(i, -ENOENT);
				if((i.arg(2) & O_ACCMODE) != O_RDONLY && !can_write_path(path))
					return block(i, -EPERM);
				return;
			}
		case __NR_openat:
			{
				int at = i.arg(1);
				std::string path = i.fetch_cstr((void *)i.arg(2));
				if(*path.rbegin())
					return block(i, -EBADF);
				if(!can_see_path(path, at))
					return block(i, -ENOENT);
				if((i.arg(2) & O_ACCMODE) != O_RDONLY && !can_write_path(path, at))
					return block(i, -EPERM);
				return;
			}
		case __NR_execve:
		case __NR_stat:
		case __NR_lstat:
		case __NR_access:
		case __NR_chdir:
		case __NR_readlink:
		case __NR_getxattr:
		case __NR_lgetxattr:
		case __NR_statfs:
		case __NR_utimes:
			{
				std::string path = i.fetch_cstr((void *)i.arg(1));
				if(*path.rbegin())
					return block(i, -EBADF);
				if(!can_see_path(path))
					return block(i, -ENOENT);
				return;
			}
		case __NR_futimesat:
		case __NR_newfstatat:
		case __NR_readlinkat:
		case __NR_faccessat:
		//case __NR_execveat:
			{
				int at = i.arg(1);
				std::string path = i.fetch_cstr((void *)i.arg(2));
				if(*path.rbegin())
					return block(i, -EBADF);
				if(!can_see_path(path, at))
					return block(i, -ENOENT);
				return;
			}
		case __NR_truncate:
		case __NR_mkdir:
		case __NR_rmdir:
		case __NR_creat:
		case __NR_unlink:
		case __NR_chmod:
		case __NR_chown:
		case __NR_lchown:
			{
				std::string path = i.fetch_cstr((void *)i.arg(1));
				if(*path.rbegin())
					return block(i, -EBADF);
				if(!can_write_path(path))
					return block(i, -EPERM);
				return;
			}
		case __NR_mkdirat:
		case __NR_fchownat:
		case __NR_unlinkat:
		case __NR_fchmodat:
			{
				int at = i.arg(1);
				std::string path = i.fetch_cstr((void *)i.arg(2));
				if(*path.rbegin())
					return block(i, -EBADF);
				if(!can_write_path(path, at))
					return block(i, -EPERM);
				return;
			}
		case __NR_link:
		case __NR_symlink:
			{
				std::string path1 = i.fetch_cstr((void *)i.arg(1));
				if(*path1.rbegin())
					return block(i, -EBADF);
				if(!can_see_path(path1))
					return block(i, -ENOENT);
				std::string path2 = i.fetch_cstr((void *)i.arg(2));
				if(*path2.rbegin())
					return block(i, -EBADF);
				if(!can_write_path(path2))
					return block(i, -EPERM);
				return;
			}
		case __NR_rename:
			{
				std::string path1 = i.fetch_cstr((void *)i.arg(1));
				if(*path1.rbegin())
					return block(i, -EBADF);
				if(!can_write_path(path1))
					return block(i, -EPERM);
				std::string path2 = i.fetch_cstr((void *)i.arg(2));
				if(*path2.rbegin())
					return block(i, -EBADF);
				if(!can_write_path(path2))
					return block(i, -EPERM);
				return;
			}
		case __NR_linkat:
		case __NR_symlinkat:
			{
				int at1 = i.arg(1);
				std::string path1 = i.fetch_cstr((void *)i.arg(2));
				if(*path1.rbegin())
					return block(i, -EBADF);
				if(!can_see_path(path1, at1))
					return block(i, -ENOENT);
				int at2 = i.arg(3);
				std::string path2 = i.fetch_cstr((void *)i.arg(4));
				if(*path2.rbegin())
					return block(i, -EBADF);
				if(!can_write_path(path2, at2))
					return block(i, -EPERM);
				return;
			}
		case __NR_renameat:
		case __NR_renameat2:
			{
				int at1 = i.arg(1);
				std::string path1 = i.fetch_cstr((void *)i.arg(2));
				if(*path1.rbegin())
					return block(i, -EBADF);
				if(!can_see_path(path1, at1))
					return block(i, -EPERM);
				int at2 = i.arg(3);
				std::string path2 = i.fetch_cstr((void *)i.arg(4));
				if(*path2.rbegin())
					return block(i, -EBADF);
				if(!can_write_path(path2, at2))
					return block(i, -EPERM);
				return;
			}
		case __NR_clone:
		case __NR_fork:
		case __NR_vfork:
			if(!can_create_thread(s))
				block(i, -EPERM);
			return;
		//case __NR_uselib:
		//case __NR_ustat:
		//case __NR_sysfs:
		//case __NR_setpriority:
		//case __NR_sched_setparam:
		//case __NR_sched_setscheduler:
		//case __NR_vhangup:
		//case __NR_modify_ldt:
		//case __NR_pivot_root:
		//case __NR__sysctl:
		//case __NR_prctl:
		//case __NR_adjtimex:
		//case __NR_chroot:
		//case __NR_sync:
		//case __NR_acct:
		//case __NR_settimeofday:
		//case __NR_mount:
		//case __NR_umount2:
		//case __NR_swapon:
		//case __NR_swapoff:
		//case __NR_reboot:
		//case __NR_sethostname:
		//case __NR_setdomainname:
		//case __NR_iopl:
		//case __NR_ioperm:
		//case __NR_create_module:
		//case __NR_init_module:
		//case __NR_delete_module:
		//case __NR_get_kernel_syms:
		//case __NR_query_module:
		//case __NR_quotactl:
		//case __NR_nfsservctl:
		//case __NR_getpmsg:
		//case __NR_putpmsg:
		//case __NR_afs_syscall:
		//case __NR_tuxcall:
		//case __NR_security:
		//case __NR_setxattr:
		//case __NR_lsetxattr:
		//case __NR_fsetxattr:
		//case __NR_listxattr:
		//case __NR_llistxattr:
		//case __NR_flistxattr:
		//case __NR_removexattr:
		//case __NR_lremovexattr:
		//case __NR_fremovexattr:
		//case __NR_sched_setaffinity:
		//case __NR_set_thread_area:
		//case __NR_io_setup:
		//case __NR_io_destroy:
		//case __NR_io_getevents:
		//case __NR_io_submit:
		//case __NR_io_cancel:
		//case __NR_get_thread_area:
		//case __NR_lookup_dcookie:
		//case __NR_remap_file_pages:
		//case __NR_getdents64:
		//case __NR_semtimedop:
		//case __NR_clock_settime:
		//case __NR_clock_getres:
		//case __NR_clock_nanosleep:
		//case __NR_vserver:
		//case __NR_mbind:
		//case __NR_set_mempolicy:
		//case __NR_get_mempolicy:
		//case __NR_mq_open:
		//case __NR_mq_unlink:
		//case __NR_mq_timedsend:
		//case __NR_mq_timedreceive:
		//case __NR_mq_notify:
		//case __NR_mq_getsetattr:
		//case __NR_kexec_load:
		//case __NR_waitid:
		//case __NR_add_key:
		//case __NR_request_key:
		//case __NR_keyctl:
		//case __NR_ioprio_set:
		//case __NR_ioprio_get:
		//case __NR_inotify_init:
		//case __NR_inotify_add_watch:
		//case __NR_inotify_rm_watch:
		//case __NR_migrate_pages:
		//case __NR_pselect6:
		//case __NR_ppoll:
		//case __NR_unshare:
		//case __NR_sync_file_range:
		//case __NR_vmsplice:
		//case __NR_move_pages:
		//case __NR_utimensat:
		//case __NR_signalfd:
		//case __NR_timerfd_create:
		//case __NR_fallocate:
		//case __NR_timerfd_settime:
		//case __NR_timerfd_gettime:
		//case __NR_accept4:
		//case __NR_signalfd4:
		//case __NR_inotify_init1:
		//case __NR_rt_tgsigqueueinfo:
		//case __NR_perf_event_open:
		//case __NR_recvmmsg:
		//case __NR_fanotify_init:
		//case __NR_fanotify_mark:
		//case __NR_prlimit64:
		//case __NR_name_to_handle_at:
		//case __NR_open_by_handle_at:
		//case __NR_clock_adjtime:
		//case __NR_syncfs:
		//case __NR_sendmmsg:
		//case __NR_setns:
		//case __NR_getcpu:
		//case __NR_process_vm_readv:
		//case __NR_process_vm_writev:
		//case __NR_kcmp:
		//case __NR_finit_module:
		//case __NR_sched_setattr:
		//case __NR_sched_getattr:
		//case __NR_seccomp:
		//case __NR_getrandom:
		//case __NR_memfd_create:
		//case __NR_kexec_file_load:
		//case __NR_bpf:
		//case __NR_userfaultfd:
		//case __NR_membarrier:
		//case __NR_copy_file_range:
		case __NR_shmget:
		case __NR_shmat:
		case __NR_shmctl:
		case __NR_socket:
		case __NR_connect:
		case __NR_accept:
		case __NR_sendto:
		case __NR_recvfrom:
		case __NR_sendmsg:
		case __NR_recvmsg:
		case __NR_shutdown:
		case __NR_bind:
		case __NR_listen:
		case __NR_getsockname:
		case __NR_getpeername:
		case __NR_socketpair:
		case __NR_setsockopt:
		case __NR_getsockopt:
		case __NR_semget:
		case __NR_semop:
		case __NR_semctl:
		case __NR_shmdt:
		case __NR_msgget:
		case __NR_msgsnd:
		case __NR_msgrcv:
		case __NR_msgctl:
		case __NR_ptrace:
		case __NR_syslog:
		case __NR_setuid:
		case __NR_setgid:
		case __NR_setsid:
		case __NR_setreuid:
		case __NR_setregid:
		case __NR_setgroups:
		case __NR_setresuid:
		case __NR_setresgid:
		case __NR_setfsuid:
		case __NR_setfsgid:
		case __NR_capget:
		case __NR_capset:
		case __NR_mknod:
		case __NR_mlock:
		case __NR_munlock:
		case __NR_mlockall:
		case __NR_munlockall:
		case __NR_mknodat:
			block(i, -EPERM);
			return;
		case __NR_kill:
		case __NR_tkill:
			if(s.find_tid(i.arg(1)) == s.threads.end())
				return block(i, -EPERM);
			else
				return;
		case __NR_tgkill:
			if((i.arg(1) != -1 && s.find_tid(i.arg(1)) == s.threads.end()) || s.find_tid(i.arg(2)) == s.threads.end())
				return block(i, -EPERM);
			else
				return;
		default:
			printf("Unknown syscall %d\n", i.sysnum());
			block(i, -ENOSYS);
			return;
		}
	}

	virtual void on_syscall_exit(Sandbox<T> &s)
	{
		SyscallInfo i(*this);
		if(ret_override)
		{
			i.ret() = ret_val;
			i.save();
			ret_override = false;
		}
	}
};

#endif
