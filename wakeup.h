#ifndef WAKEUP_H
#define WAKEUP_H

#include <thread>

#define WAKE_MAGIC 0x77616b65

extern char const *ident;
extern int in_pipe[2], out_pipe[2];

extern std::thread *feed_in, *feed_out;

void check_startup();
void do_wakeup();
void feed_data(int, int, bool, bool);
void try_connect();
void kill_session();
void set_timer();

#endif
