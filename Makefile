.PHONY: all clean run
CC=gcc
CFLAGS=-std=c99 -O2
LDFLAGS=-lm
TARGET=sqlite server client 

all: $(TARGET)

sqlite: shell.c sqlite3.c
	gcc -lpthread -ldl -lm $^ -o $@
server: server.cpp sqlite3.o
	g++ -g -Wall -std=c++11 $^ -o $@
client: client.cpp
	g++ -g -Wall -std=c++11 $^ -o $@

clean:
	rm -f $(TARGET)

