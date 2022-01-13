.PHONY: all clean run
CC=gcc
LDFLAGS=-lm
TARGET=sqlite server client 

all: $(TARGET)

sqlite: shell.c sqlite3.c
	gcc -lpthread -ldl -lm shell.c sqlite3.c -o sqlite3.o
server: server.cpp sqlite3.o
	g++ -g -Wall -std=c++17 -O2 sqlite3.o server.cpp -o server
client: client.cpp
	g++ -g -Wall -std=c++17 -O2 client.cpp -o client

clean:
	rm -f $(TARGET)

