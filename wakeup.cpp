#include <thread>
#include <string>

#include "sandbox.h"

extern "C" {
#include <stdio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pthread.h>
}

char const *ident;
int in_pipe[2], out_pipe[2];

int old_client = -1;

std::thread *feed_in, *feed_out;

void feed_data(int from, int to)
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
		}
	}
	if(bytes_read < 0 && errno != EAGAIN)
	{
		printf("[Read error]\n");
		exit(0);
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
		kill(getppid(), SIGUSR1);
	unset_timer();

	int server = socket(AF_UNIX, SOCK_STREAM, 0);
	if(server < 0)
		panic_errno("socket");

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof addr);
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, ("socket_" + std::string(ident)).c_str(), sizeof addr.sun_path - 1);
	if(bind(server, (struct sockaddr *)&addr, sizeof addr))
		panic_errno("bind");

	if(listen(server, 1))
		panic_errno("listen");

	int client = accept(server, NULL, NULL);
	if(client < 0)
		panic_errno("accept");

	close(server);
	unlink(("socket_" + std::string(ident)).c_str());

	feed_in = new std::thread(feed_data, client, in_pipe[1]);
	feed_out = new std::thread(feed_data, out_pipe[0], client);

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
	strncpy(addr.sun_path, ("socket_" + std::string(ident)).c_str(), sizeof addr.sun_path - 1);
	if(connect(server, (struct sockaddr *)&addr, sizeof addr))
	{
		unlink(("socket_" + std::string(ident)).c_str());
		return;
	}

	printf("[Attached]\n");

	std::thread feed_in(feed_data, fileno(stdin), server);
	feed_data(server, fileno(stdout));
	exit(0);
}
