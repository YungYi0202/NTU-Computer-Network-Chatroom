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
#include <iostream>
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

void closeFD(int fd) {
  fprintf(stderr, "closeFD: %d\n", fd);
  close(fd);
}

int handleRecv(int connfd, char *buf) {
  bzero(buf, BUF_LEN);
  int ret = recv(connfd, buf, BUF_LEN, 0);
  if (ret <= 0) {
    closeFD(connfd);
    pthread_exit((void *)1);
  } else {
    fprintf(stderr, "========handleRecv: connfd:%d=========\n", connfd);
    fprintf(stderr, "%s", buf);
    fprintf(stderr, "=============================\n");
  }
  return ret;
}

int _handleSend(int connfd, char *buf) {
  int writeLen = strlen(buf);
  int ret = send(connfd, buf, writeLen, MSG_NOSIGNAL);
  if (ret != writeLen) {
    closeFD(connfd);
    return ret;
  } else {
    fprintf(stderr,
            "========handleSend: connfd:%d buf_last_char:%d=========\n",
            connfd, buf[strlen(buf) - 1]);
    fprintf(stderr, "%s", buf);
    fprintf(stderr, "=============================\n");
  }
  return ret;
}

int handleSend(int connfd, char *buf, std::string str = "") {
  int ret, writeLen;
  std::string tmp;
  if (str == "") {
    ret = _handleSend(connfd, buf);
  } else {
    while (str.size()) {
      bzero(buf, BUF_LEN);
      writeLen = (str.size() < BUF_LEN) ? str.size() : BUF_LEN;
      tmp = str.substr(0, writeLen);
      str = str.substr(writeLen);
      sprintf(buf, "%s", tmp.c_str());
      ret = _handleSend(connfd, buf);
    }
  }
  return ret;
}

class Client {
 public:
  int server_fd;
  char buf[BUF_LEN], path[BUF_LEN];

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

  void listFriend(const char *username) {
    handleRecv(server_fd, buf);
    int filelen;
    sscanf(buf, "%d", &filelen);
    fprintf(stderr, "list friend len = %d\n", filelen);
    handleSend(server_fd, buf, "1");
    int n;
    while (filelen > 0 && (n = handleRecv(server_fd, buf)) > 0) {
      printf("%s", buf);
      filelen -= n;
      fprintf(stderr, "filelen = %d, n = %d\n", filelen, n);
    }
    fprintf(stderr, "filelen = %d\n", filelen);
  }

  void history(const char *username, const char *friend_name) {
    handleRecv(server_fd, buf);
    int filelen;
    sscanf(buf, "%d", &filelen);
    handleSend(server_fd, buf, "1");
    int n;
    while(filelen && (n = handleRecv(server_fd, buf)) > 0) {
      printf("%s", buf);
      filelen -= n;
    }
  }

  void put(const char *username, const char *filename) {
    // incomplete
  }

  void get(const char *username, const char *filename) {
    // TODO: will not overwrite the whole file, hence, if new file len > past file len, then the content
    // over new file len will remain the same
    handleRecv(server_fd, buf);
    int filelen;
    sscanf(buf, "%d", &filelen);
    fprintf(stderr, "filelen = %d\n", filelen);
    handleSend(server_fd, buf, "1");
    sprintf(path, "client_dir/%s", filename);
    int n;
    int file_fd = open(path, O_CREAT | O_RDWR, FILE_MODE);
    while(filelen > 0 && (n = handleRecv(server_fd, buf)) > 0) {
      write(file_fd, buf, n);
      filelen -= n;
    }
    close(file_fd);
  }

} client;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: ./client [ip:port]\n");
    exit(1);
  }

  char ip[32], port[32], buf[BUF_LEN];
  char *pch;
  pch = strtok(argv[1], ":");
  strcpy(ip, pch);
  pch = strtok(NULL, ":");
  strcpy(port, pch);

  uint16_t port_num = 0;
  int port_l = strlen(port);
  for (int i = 0; i < port_l; i++) port_num = port_num * 10 + port[i] - '0';

  client.initClient(ip, port_num);
  std::string s, command, username, friend_name, something, filename;
  while (getline(std::cin, s)) {
    std::cerr << "recv command: " << s << std::endl;
    handleSend(client.server_fd, buf, s);

    std::stringstream ss(s);
    ss >> command;
    switch (command[0]) {
      case 'l':
        ss >> username;
        std::cerr << "username: " << username << std::endl;
        client.listFriend(username.c_str());
        break;
      case 'h':
        ss >> username >> friend_name;
        client.history(username.c_str(), friend_name.c_str());
        break;
      case 'p':
        ss >> username >> filename;
        client.put(username.c_str(), filename.c_str());
        break;
      case 'g':
        ss >> username >> filename;
        client.get(username.c_str(), filename.c_str());
        break;
    }
    std::cout << "please input your command:" << std::endl;
  }
}
