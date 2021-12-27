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
#define ERR_EXIT(a) \
  do {              \
    perror(a);      \
    exit(1);        \
  } while (0)
#define BUF_LEN 1024

#define DIR_MODE (FILE_MODE | S_IXUSR | S_IXGRP | S_IXOTH)
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

class Server {
 public:  // TODO: modify access
  static const int MAX_FD = 1024;

  std::string response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n\r\n"
      "<!DOCTYPE html><html><head><title>Bye-bye baby bye-bye</title>"
      "<style>body { background-color: #111 }"
      "h1 { font-size:4cm; text-align: center; color: black;"
      " text-shadow: 0 0 2mm red}</style></head>"
      "<body><h1>Goodbye, world!</h1></body></html>\r\n";

  struct client {
    int fd;  // fd to talk with client
    char username[10];

    client() {
      fd = -1;
      bzero(username, sizeof(username));
    }
  };
  client clients[MAX_FD];

  fd_set master_RDset, working_RDset;
  char buf[BUF_LEN];

  void initServer() {
    int one = 1;
    struct sockaddr_in svr_addr;

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) err(1, "can't open socket");

    // TODO: Check out what this is for.
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));

    int port = 8080;
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = INADDR_ANY;
    svr_addr.sin_port = htons(port);

    if (bind(sock_fd, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) == -1) {
      close(sock_fd);
      err(1, "Can't bind");
    }

    listen(sock_fd, MAX_FD);
    serve(sock_fd);
  }
  void serve(int sock_fd) {
    clients[sock_fd].fd = sock_fd;

    FD_ZERO(&master_RDset);
    FD_SET(sock_fd, &master_RDset);

    struct sockaddr_in client_addr;
    int cli_fd;
    int clilen = sizeof(client_addr);

    client *clientP;  // Pointer to a specific client.

    while (1) {
      memcpy(&working_RDset, &master_RDset, sizeof(master_RDset));
      select(MAX_FD, &working_RDset, NULL, NULL, NULL);
      if (FD_ISSET(sock_fd, &working_RDset)) {
        if ((cli_fd = accept(sock_fd, (struct sockaddr *)&client_addr,
                             (socklen_t *)&clilen)) < 0) {
          ERR_EXIT("accept");
          continue;
        }
        FD_SET(cli_fd, &master_RDset);
        fprintf(stderr, "Accept! client fd: %d\n", cli_fd);
        clientP = &clients[cli_fd];
        clientP->fd = cli_fd;
        handle_write(cli_fd, response, true);
      }
    }
  }
  static void init_client(client *cli) {
    cli->fd = -1;
    bzero(cli->username, sizeof(cli->username));
  }

  static void free_client(client *cli) { init_client(cli); }

  void closeFD(client *cli) {
    fprintf(stderr, "closeFD: %d\n", cli->fd);
    FD_CLR(cli->fd, &master_RDset);   //?
    FD_CLR(cli->fd, &working_RDset);  //?
    close(cli->fd);
    free_client(cli);
  }

  int handle_read(int fd) {
    bzero(buf, BUF_LEN);
    int ret = read(fd, buf, sizeof(buf));
    if (ret <= 0 && fd > 0 && clients[fd].fd != -1) {
      fprintf(stderr, "handle_read: fd:%d closeFD\n", fd);
      closeFD(&clients[fd]);
    }
    return ret;
  }

  int handle_write(int fd, std::string str, bool isHTTP) {
    sprintf(buf, "%s", str.c_str());
    int writeLen = (isHTTP) ? strlen(buf) - 1 : strlen(buf);
    int ret = write(fd, buf, writeLen);
    if (ret != writeLen && fd > 0 && clients[fd].fd != -1) {
      fprintf(stderr, "handle_write: fd:%d closeFD\n", fd);
      closeFD(&clients[fd]);
    } else {
      fprintf(stderr, "handle_write: fd:%d str:%s\n", fd, str.c_str());
    }
    return ret;
  }
} server;

class Client {
 public:
  void initClient(char *ip, int port_num) {
    mkdir("client_dir", DIR_MODE);

    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port_num);
    addr_in.sin_addr.s_addr = inet_addr(ip);

    int server_fd;
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd >= 0) {
      int x = connect(server_fd, (struct sockaddr *)&addr_in, sizeof(addr_in));
      if (x == 0) {
        interactive(server_fd);
      } else
        close(server_fd);
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
            int x = send(server_fd, buf, n, 0);
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

  // server.initServer();

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
