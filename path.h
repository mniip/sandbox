#ifndef PATH_H
#define PATH_H

#include <string>

extern "C" {
#include <sys/types.h>
}

std::string canon_path_at(pid_t, int, std::string);
bool in_dir(std::string, std::string);

#endif
