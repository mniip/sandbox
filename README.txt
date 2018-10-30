A ptrace-based syscall filtering sandbox
========================================

This is a configurable ptrace jail that supports sandboxing multiple threads,
filtering access to subtrees of the filesystem, suspending processes, and
pretending that HTTP links are files. This seemmingly absurd set of features
is ideal for sandboxing a programming language interpreter process for an
interactive evaluation bot on IRC or the like.

In general, you'll want an environment analogous to a chroot (except we don't
do chroots), with all the executables and shared libraries inside, and,
possibly a writeable directory in there as well. The sandbox doesn't enforce
any file size limits so it would be appropriate to place the writeable
directory on a loop mountpoint, or assign a filesystem quota, or use a
filesystem-specific solution that accomplishes the same.

To run a process inside the sandbox, run:

    sandbox <config.conf> <ident> [program [args...]]

The <ident> is an identifier that is used to distinguish suspended processes,
and to allow identifier-specific configuration.

To kill a suspended process by its identifier, run:

    sandbox <config.conf> kill <ident>

The configuration file syntax is as follows: each line is a keyword followed
by optional space-separated arguments whose meaning depends on the keyword.
A line beginning with # followed by a space is a comment.

A line "ident <name>" starts a block of configuration specific to the
identifier <ident>, which spans until the line "end". Other keywords include:


program <path>
    Specify a default executable path in case sandbox is called without
    specifying the executable.

arg <word>
    Append an argument to the default commandline. Note that the first such
    keyword actually sets argv[0].

closefds
    Close all file descriptors other than 0, 1, 2.

setenv <var> <value>
    Set an environment variable for the sandboxed process.

clearenv
    Clear all environment variables at the start.

unsetenv <var>
    Remove a single environment variable.

see <path>
    Specify a subtree of the filesystem which the sandboxed process is allowed
    to see. This includes opening for reading, stat'ing, readlink'ing,
    readdir'ing, execve'ing, and so on.

    Note that this doesn't magically lift restrictions imposed by the kernel:
    the sandboxed process still won't be able to see files/directories hidden
    by permission bits affecting the process's UID/GID.

write <path>
    Specify a subtree of the filesystem which the sandboxed process is allowed
    to write to. This includes opening for writing, mkdir'ing, rename'ing, and
    so on.

maxthreads <number>
    Specify the maximum number of threads or processes in a sandbox.

chdir <path>
    Change directory for the sandboxed process to the specified one.

rlimit <lim> <value>
    Set a rlimit on the sandboxed process. For more information consult
    getrlimit(3). Example: "rlimit NOFILE 32". You might wish to limit the
    amount of virtual memory available to the process (AS) with this.

timelimit <limit>
    Specify a time limit in seconds (wall clock time) after which the sandboxed
    process will be forcibly and abruptly terminated.

wakeup
    Allow the process to issue a special syscall that will tell the sandbox
    that the process is done interpreting the current command, and is waiting
    for the next. The sandbox will then fork into the background, and then
    a later invocation with the same identifier will talk to the existing
    process instead of creating a new one.

    In a REPL, you'll want to place the syscall at the end of the Loop, after
    the Read, Eval, Print steps. You also want to make sure that an invocation
    of the sandbox receives exactly as much input as the Read step expects,
    otherwise it might time out waiting for input, or have extra input queued
    up.

    With this flag enabled, an EOF on the sandbox's stdin will not cause the
    sandboxed process to receive an EOF, in assurance that the sandboxed
    process will suspend after consuming the input.

sockdir <path>
    Specify a directory for UNIX sockets which are used to talk to suspended
    processes.

http <url>
    Pretend that any file whose name begins with <url> actually exists. When
    the sandboxed process tries to open such a file, it will be downloaded and
    fed into the process.

downloadpat <pattern>
    Specify location and name of temporary files used for downloads. This will
    be fed to mkstemp(3) and should conform to that function's expectations.
