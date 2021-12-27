all: client server
client:
	g++ -std=c++17 client.cpp -o client
server:
	g++ -std=c++17 server.cpp -o server
