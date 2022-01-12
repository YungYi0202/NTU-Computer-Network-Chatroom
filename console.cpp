#include <arpa/inet.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#define ERR_EXIT(a) \
  do {              \
    perror(a);      \
    exit(1);        \
  } while (0)
#define BUF_LEN 2048
#define MAXFD 1024
#define SVR_PORT 8080
#define CRLF "\r\n"

#define DIR_MODE (FILE_MODE | S_IXUSR | S_IXGRP | S_IXOTH)
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

class Client {
 public:
  int server_fd;
  char buf[BUF_LEN];

  int initClient(char *ip, int port_num) {
    mkdir("client_dir", DIR_MODE);

    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port_num);
    addr_in.sin_addr.s_addr = inet_addr(ip);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd >= 0) {
      int x = connect(server_fd, (struct sockaddr *)&addr_in, sizeof(addr_in));
      if (x != 0) {
        close(server_fd);
      }
    }

    return server_fd;
  }

  void addFriend(char *username, char *friend_name) {
    sprintf(buf, "add %s %s", username, friend_name);
    int n;
    n = send(server_fd, buf, strlen(buf), 0);
  }

  void deleteFriend(char *username, char *friend_name) {
    sprintf(buf, "delete %s %s", username, friend_name);
    int n;
    n = send(server_fd, buf, strlen(buf), 0);
  }

  void listFriend(char *username) {
    sprintf(buf, "ls %s", username);
    int n;
    n = send(server_fd, buf, strlen(buf), 0);
    recv(server_fd, buf, BUF_LEN, 0);
  }

  void history(char *username, char *friend_name) {
    sprintf(buf, "history %s %s", username, friend_name);
    int n;
    n = send(server_fd, buf, strlen(buf), 0);
    recv(server_fd, buf, BUF_LEN, 0);
  }

  void say(char *username, char *friend_name, char *something) {
    sprintf(buf, "say %s %s %s", username, friend_name, something);
    int n;
    n = send(server_fd, buf, strlen(buf), 0);
    recv(server_fd, buf, BUF_LEN, 0);
  }

  void put(char *username, char* filename) {
    sprintf(buf, "put %s %s", username, filename);
    
  }

} client;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: ./client [ip:port]\n");
    exit(1);
  }

  char ip[32], port[32];
  char *pch;
  pch = strtok(argv[1], ":");
  strcpy(ip, pch);
  pch = strtok(NULL, ":");
  strcpy(port, pch);

  uint16_t port_num = 0;
  int port_l = strlen(port);
  for (int i = 0; i < port_l; i++) port_num = port_num * 10 + port[i] - '0';

  client.initClient(ip, port_num);
}
