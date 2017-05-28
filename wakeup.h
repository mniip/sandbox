#ifndef WAKEUP_H
#define WAKEUP_H

#include <thread>

#define WAKE_MAGIC 0x77616b65

extern char const *ident;
extern int in_pipe[2], out_pipe[2];

extern std::thread *feed_in, *feed_out;

void do_wakeup();
void feed_data(int, int);
void try_connect();
void set_timer();

#endif
