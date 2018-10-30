#include <thread>
#include <string>

#include "sandbox.h"
#include "config.h"

extern "C" {
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <pthread.h>
}

char const *ident;
int in_pipe[2], out_pipe[2];

int old_client = -1;

std::thread *feed_in, *feed_out;

bool startup_over = false;
int written = 0;

bool close_input = false;

void feed_data(int from, int to, bool track, bool closeeof)
{
	const size_t BLOCK_SIZE = 4096;
	char buffer[BLOCK_SIZE];
	ssize_t bytes_read;
	while(0 < (bytes_read = read(from, buffer, BLOCK_SIZE)))
	{
		size_t done = 0;
		while(done < bytes_read)
		{
			ssize_t bytes_wrote = write(to, buffer + done, bytes_read - done);
			if(bytes_wrote <= 0)
			{
				printf("[Write error]\n");
				exit(0);
			}
			done += bytes_wrote;
			if(track)
				written += bytes_wrote;
		}
	}
	if(bytes_read < 0 && errno != EAGAIN)
	{
		printf("[Read error]\n");
		exit(0);
	}
	if(closeeof)
	{
		close_input = true;
		if(startup_over)
			close(to);
	}
}

void time_limit()
{
    sleep(5);
    fprintf(old_client == -1 ? stdout : fdopen(old_client, "w"), "[Timed out]\n");
    exit(0);
}

std::thread *timer;

void set_timer()
{
	timer = new std::thread(time_limit);
}

void unset_timer()
{
	pthread_cancel(timer->native_handle());
	timer->join();
	delete timer;
}

void do_wakeup()
{
	pthread_cancel(feed_in->native_handle());
	feed_in->join();
	fcntl(out_pipe[0], F_SETFL, fcntl(out_pipe[0], F_GETFL) | O_NONBLOCK);
	pthread_kill(feed_out->native_handle(), SIGUSR1);
	feed_out->join();
	fcntl(out_pipe[0], F_SETFL, fcntl(out_pipe[0], F_GETFL) & ~O_NONBLOCK);
	delete feed_in;
	delete feed_out;
	if(old_client != -1)
		close(old_client);
	else
	{
		kill(getppid(), SIGUSR1);
		close(fileno(stdin));
		close(fileno(stdout));
		close(fileno(stderr));
	}
	unset_timer();

	int server = socket(AF_UNIX, SOCK_STREAM, 0);
	if(server < 0)
		panic_errno("socket");

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, (conf_sockdir + "/socket_" + ident).c_str(), sizeof addr.sun_path - 1);
	if(bind(server, (struct sockaddr *)&addr, sizeof addr))
		panic_errno("bind");

	if(listen(server, 1))
		panic_errno("listen");

	int client = accept(server, NULL, NULL);
	if(client < 0)
		panic_errno("accept");

	close(server);
	unlink((conf_sockdir + "/socket_" + ident).c_str());

	feed_in = new std::thread(feed_data, client, in_pipe[1], false, !conf_wakeup);
	feed_out = new std::thread(feed_data, out_pipe[0], client, false, false);

	old_client = client;
	set_timer();
}

void try_connect()
{
	int server = socket(AF_UNIX, SOCK_STREAM, 0);
	if(server < 0)
		panic_errno("socket");
	
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, (conf_sockdir + "/socket_" + ident).c_str(), sizeof addr.sun_path - 1);
	if(connect(server, (struct sockaddr *)&addr, sizeof addr))
	{
		unlink((conf_sockdir + "/socket_" + ident).c_str());
		return;
	}

	std::thread feed_in(feed_data, fileno(stdin), server, false, !conf_wakeup);
	feed_data(server, fileno(stdout), false, false);
	exit(0);
}

void kill_session()
{
	int server = socket(AF_UNIX, SOCK_STREAM, 0);
	if(server < 0)
		panic_errno("socket");
	
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, (conf_sockdir + "/socket_" + ident).c_str(), sizeof addr.sun_path - 1);
	if(connect(server, (struct sockaddr *)&addr, sizeof addr))
	{
		if(errno == ENOENT)
			return;
		int err = errno;
		unlink(("socket_" + std::string(ident)).c_str());
		errno = err;
		panic_errno("connect");
	}

	unlink((conf_sockdir + "/socket_" + ident).c_str());

	struct ucred creds;
	socklen_t len = sizeof creds;
	if(getsockopt(server, SOL_SOCKET, SO_PEERCRED, &creds, &len))
		panic_errno("getsockopt SO_PEERCRED");
	if(kill(creds.pid, SIGTERM))
		panic_errno("kill");
}

void check_startup()
{
	if(!startup_over)
	{
		int buffer;
		if(ioctl(in_pipe[1], FIONREAD, &buffer))
			panic_errno("FIONREAD");
		if(buffer - written < 0)
		{
			if(close_input)
				close(in_pipe[1]);
			startup_over = true;
			set_timer();
		}
	}
}
