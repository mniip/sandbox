CXX= g++ -std=c++98
CFLAGS= -O0 -ggdb
CPPFLAGS= 
LDFLAGS= 

OBJS= main.o sandbox.o

all: sandbox

sandbox: $(OBJS)
	$(CXX) -o $@ $+ $(CFLAGS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(CFLAGS) $(CPPFLAGS) 

.PHONY: all
