.PHONY: all clean run
CC=gcc
CFLAGS=-std=c++17 -O2
LDFLAGS=-lm
TARGET=sqlite server client console 

all: $(TARGET)

sqlite: shell.c sqlite3.c
	gcc -lpthread -ldl -lm $^ -o $@
server: server.cpp sqlite3.o
	g++ -g -Wall ${CFLAGS} $^ -o $@
client: client.cpp
	g++ -g -Wall ${CFLAGS} $^ -o $@
console: console.cpp
	g++ -g -Wall ${CFLAGS} $^ -o $@

clean:
	rm -f $(TARGET)

