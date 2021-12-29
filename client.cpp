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
#include <string>
#include <sstream>
#include <fstream>

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

#define STATE_UNUSED 0
#define STATE_NO_USERNAME 1
#define STATE_RECV_REQ 2
#define STATE_SEND_GET_RES 3
#define STATE_SEND_POST_RES 4

class HttpGet {
public:
  bool isHeaderWritten;
  std::string statusCode;
  std::string contentType;
  std::ifstream targetFile;
  HttpGet() {}
  HttpGet(std::string target, std::string type="", std::string status="200 OK") {
    isHeaderWritten = false;
    targetFile.open(target);
    statusCode = status;
    setContentType(type);
  }
  
  void setContentType(std::string type) {
    if (type == ".html") {
      contentType = "text/html";
    } else if (type == ".ico") {
      contentType = "image/vnd.microsoft.icon";
    } else if (type == ".jpg" || type == ".jpeg") {
      contentType = "image/jpeg";
    } else if (type == ".css") {
      contentType = "text/css";
    } else if (type == ".scss") {
      contentType = "text/x-scss";
    } else if (type == ".js") {
      contentType = "text/javascript";
    } else {
      contentType = "text/plain";
    }
    // TODO:
  }
  std::string Header() {
    return "HTTP/1.1 " + statusCode + "\r\nContent-Type: " + contentType + "; charset=UTF-8\r\n\r\n";
  }
};

class Server {
public:  // TODO: modify access
  class client {
  public:
    int fd;
    int state;
    char username[10];
    HttpGet getRes;
    client() {
      reset();
    }
    void reset() {
      fd = -1;
      state = STATE_UNUSED;
      bzero(username, sizeof(username));
    }
    void start(int _fd) {
      fd = _fd;
      state = STATE_RECV_REQ;
    }
  };

  static const int maxfd = MAXFD;
  int sockfd;
  client clients[maxfd];
  fd_set master_rfds, working_rfds, master_wfds, working_wfds;
  char buf[BUF_LEN];
  
  HttpGet GetResponse(std::string target) {
    if (target == "/") {
      // Default: return index.html
      // target = "/index.html";
      target = "/index2.html";
    } 
    if (target.size() > 1){
      // TODO: Ask server.cpp
      // TODO: Json format
      target = target.substr(1);
      std::string type;
      std::size_t dotPos=target.find('.');
      if (dotPos != std::string::npos) {
        type = target.substr(dotPos);
        return HttpGet(target, type);
      }
    }
    return HttpGet(target);
  }
  
  void initServer() {
    int one = 1;
    struct sockaddr_in svr_addr;

    this->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) err(1, "can't open socket");

    // TODO: Check out what this is for.
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = INADDR_ANY;
    svr_addr.sin_port = htons(SVR_PORT);

    if (bind(sockfd, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) == -1) {
      close(sockfd);
      err(1, "Can't bind");
    }

    listen(sockfd, maxfd);
  }

  void serve() {
    clients[sockfd].fd = sockfd;

    FD_ZERO(&master_rfds);
    FD_ZERO(&master_wfds);
    FD_SET(sockfd, &master_rfds);

    struct sockaddr_in client_addr;
    int clifd;
    int clilen = sizeof(client_addr);

    while (1) {
      memcpy(&working_rfds, &master_rfds, sizeof(master_rfds));
      memcpy(&working_wfds, &master_wfds, sizeof(master_wfds));
      if (select(maxfd, &working_rfds, &working_wfds, NULL, NULL) < 0) {
        ERR_EXIT("select failed");
      }

      if (FD_ISSET(sockfd, &working_rfds)) {
        if ((clifd = accept(sockfd, (struct sockaddr *)&client_addr,
                             (socklen_t *)&clilen)) < 0) {
          ERR_EXIT("accept");
          continue;
        }
        FD_SET(clifd, &master_rfds);
        fprintf(stderr, "=============================\n");
        fprintf(stderr, "Accept! client fd: %d\n", clifd);
        fprintf(stderr, "=============================\n");
        clients[clifd].start(clifd);
      }

      for (int fd = 0; fd < maxfd; fd++) {
        if (fd == sockfd) continue;
        
        if (FD_ISSET(fd, &working_rfds)) {
          if (clients[fd].state == STATE_RECV_REQ) {
            // TODO: What if buf is too small?
            handleRead(fd);
            std::stringstream ss;
            std::string req(buf);
            ss << req;
            std::string command, target;
            ss >> command >> target;
            if (command == "GET") { 
              clients[fd].getRes = GetResponse(target);
              clients[fd].state = STATE_SEND_GET_RES;
              FD_SET(fd, &master_wfds);
            } else if (command == "POST") {
              fprintf(stderr, "========%s %s=========\n",command.c_str(), target.c_str());
            }
            else {
              fprintf(stderr, "========%s %s=========\n",command.c_str(), target.c_str());
            }
          }
          // TODO: Other states.
        } else if (FD_ISSET(fd, &working_wfds)) {
          if (clients[fd].state == STATE_SEND_GET_RES) {
            std::string tmp;
            if (clients[fd].getRes.isHeaderWritten) {
              if (clients[fd].getRes.targetFile && getline (clients[fd].getRes.targetFile, tmp)) {
                handleWrite(fd, tmp);
              } else {
                // End
                handleWrite(fd, CRLF);
                if (clients[fd].getRes.targetFile) {
                  clients[fd].getRes.targetFile.close();
                }
                closeFD(fd);
              }
            } else {
              handleWrite(fd, clients[fd].getRes.Header());
              clients[fd].getRes.isHeaderWritten = true;
            }
          } 
          // TODO: Other states.
        }     
      }
    }
  }

  void closeFD(int fd) {
    fprintf(stderr, "closeFD: %d\n", fd);
    FD_CLR(fd, &master_rfds);   //?
    FD_CLR(fd, &master_wfds);   //?
    close(fd);
    clients[fd].reset();
  }

  int handleRead(int fd) {
    bzero(buf, BUF_LEN);
    int ret = read(fd, buf, sizeof(buf));
    if (ret <= 0 && fd > 0 && clients[fd].fd != -1) {
      fprintf(stderr, "handleRead: fd:%d closeFD\n", fd);
      closeFD(fd);
    } else {
      fprintf(stderr, "========handleRead: fd:%d=========\n",fd);
      fprintf(stderr, "%s", buf);
      fprintf(stderr, "=============================\n");
    }
    return ret;
  }

  int handleWrite(int fd, std::string str) {
    sprintf(buf, "%s", str.c_str());
    // int writeLen = (isHTTP) ? strlen(buf) - 1 : strlen(buf);
    int writeLen = strlen(buf);
    int ret = write(fd, buf, writeLen);
    if (ret != writeLen && fd > 0 && clients[fd].fd != -1) {
      fprintf(stderr, "handleWrite: fd:%d closeFD\n", fd);
      closeFD(fd);
    } else {
      fprintf(stderr, "========handleWrite: fd:%d buf_last_char:%d=========\n",fd, buf[strlen(buf) - 1]);
      fprintf(stderr, "%s", buf);
      fprintf(stderr, "=============================\n");
    }
    return ret;
  }
} server;

class Client {
 public:
  int server_fd;

  void initClient(char *ip, int port_num) {
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
  }

  void interactive(int server_fd) {
    char username[16], command[128], filename[64],
        client_dir[64] = "./client_dir/", path[64];
    char buf[BUF_LEN];
    int n, file_fd;

    printf("input your username:");
    scanf("%s", username);

    do {
      n = send(server_fd, username, strlen(username), 0);
      recv(server_fd, buf, BUF_LEN, 0);
      if (buf[0] == '0') {
        printf("username is in used, please try another:");
        scanf("%s", username);
      }
    } while (buf[0] == '0');

    printf("connect successfully\n");

    while (1) {
      fprintf(stderr, "please enter your command:");
      char *line = NULL;
      size_t len = 0;
      ssize_t zb;
      while ((zb = getline(&line, &len, stdin)) <= 1) {
      }

      int command_argc = 0;
      char *pch;
      pch = strtok(line, " \n");
      while (pch != NULL) {
        if (command_argc == 0)
          strcpy(command, pch);
        else if (command_argc == 1)
          strcpy(filename, pch);
        command_argc++;
        pch = strtok(NULL, " \n");
      }

      if (strcmp(command, "ls") != 0 && strcmp(command, "put") != 0 &&
          strcmp(command, "get") != 0) {
        printf("Command not found\n");
        continue;
      }

      if ((strcmp(command, "ls") == 0 && command_argc != 1) ||
          (strcmp(command, "put") == 0 && command_argc != 2) ||
          (strcmp(command, "get") == 0 && command_argc != 2)) {
        printf("Command format error\n");
        continue;
      }

      if (command[0] == 'p' || command[0] == 'g') {
        strcpy(path, client_dir);
        strcpy(path + strlen(path), filename);
        command[3] = ' ';
        strcpy(command + 4, filename);
      }

      if (command[0] == 'p') {
        struct stat st;
        int valid;
        if ((valid = stat(path, &st)) == -1) {
          printf("The %s doesn’t exist\n", filename);
          continue;
        }
        command[strlen(command) + 1] = 0;
        command[strlen(command)] = ' ';
        sprintf(command + strlen(command), "%d\n", (int)st.st_size);
      }
      n = send(server_fd, command, strlen(command), 0);

      switch (command[0]) {
        case 'p':
          n = recv(server_fd, buf, 2, 0);

          file_fd = open(path, O_RDWR);
          while ((n = read(file_fd, buf, BUF_LEN)) > 0) {
            send(server_fd, buf, n, 0);
          }

          printf("put %s successfully\n", filename);
          close(file_fd);
          break;

        case 'g':
          int valid, file_len;
          n = recv(server_fd, buf, BUF_LEN, 0);
          sscanf(buf, "%d %d", &valid, &file_len);
          if (!valid) {
            printf("The %s doesn’t exist\n", filename);
            break;
          }

          sprintf(buf, "1\n");
          send(server_fd, buf, 2, 0);

          file_fd = open(path, O_CREAT | O_RDWR, FILE_MODE);
          while (file_len > 0 &&
                 (n = recv(server_fd, buf, std::min(file_len, BUF_LEN), 0)) >
                     0) {
            write(file_fd, buf, n);
            file_len -= n;
          }

          printf("get %s successfully\n", filename);
          close(file_fd);
          break;

        case 'l':
          int len;
          recv(server_fd, buf, BUF_LEN, 0);
          sscanf(buf, "%d", &len);

          sprintf(buf, "1\n");
          send(server_fd, buf, 2, 0);

          if (len > 0) {
            n = recv(server_fd, buf, len, 0);
            if (n > 0) {
              buf[n] = 0;
              printf("%s", buf);
              len -= n;
            }
          }
          break;
      }
    }
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

  server.initServer();
  server.serve();
}
