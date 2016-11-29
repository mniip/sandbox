ARCH=$(shell uname -m)

CXX= g++
CFLAGS= -O0 -ggdb -std=c++11
CPPFLAGS=
LDFLAGS= -lpthread

OBJS= main.o sandbox.o path.o

all: sandbox

clean:
	rm -f sandbox $(OBJS) arch.h

sandbox: $(OBJS)
	$(CXX) -o $@ $+ $(CFLAGS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

main.o: arch.h sandbox.h sandbox.impl fs.h

arch.h:
	echo '#define ARCH $(ARCH)' > $@
	echo '#include "arch/$(ARCH).h"' >> $@

.PHONY: all clean
