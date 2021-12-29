.PHONY: all clean run
CC=gcc
CFLAGS=-std=c99 -O2
LDFLAGS=-lm
TARGET=server client 

all: $(TARGET)

server: server.cpp
	g++ -g -Wall -std=c++11 -l sqlite3 $^ -o $@
client: client.cpp
	g++ -g -Wall -std=c++11 -l sqlite3 $^ -o $@

clean:
	rm -f $(TARGET)

