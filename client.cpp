#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
using namespace std;

#define BUFLEN 512

#define DIR_MODE (FILE_MODE | S_IXUSR | S_IXGRP | S_IXOTH)
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

void interactive(int sockfd) {
  char username[16], command[128], filename[64],
      client_dir[64] = "./client_dir/", path[64];
  char buf[BUFLEN];
  int n, file_fd;

  printf("input your username:");
  scanf("%s", username);

  do {
    n = send(sockfd, username, strlen(username), 0);
    fprintf(stderr, "send %d bytes\n", n);
    recv(sockfd, buf, BUFLEN, 0);
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
    while((zb = getline(&line, &len, stdin)) <= 1) {}

    fprintf(stderr, "receive user command: %s\n", line);
    
    int command_argc = 0;
    char *pch;
    pch = strtok (line, " \n");
    while (pch != NULL)
    {
      fprintf(stderr, "pch = %s\n", pch);
      if(command_argc == 0) strcpy(command, pch);
      else if(command_argc == 1) strcpy(filename, pch);
      command_argc++;
      pch = strtok (NULL, " \n");
    }

    if (strcmp(command, "ls") != 0 && strcmp(command, "put") != 0 &&
        strcmp(command, "get") != 0) {
      printf("Command not found\n");
      continue;
    }

    if((strcmp(command, "ls") == 0 && command_argc != 1) || 
       (strcmp(command, "put") == 0 && command_argc != 2) ||
       (strcmp(command, "get") == 0 && command_argc != 2)){
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
      fprintf(stderr, "command is %s\n", command);
    }
    n = send(sockfd, command, strlen(command), 0);
    fprintf(stderr, "send %d bytes\n", n);

    switch (command[0]) {
      case 'p':
        n = recv(sockfd, buf, 2, 0);
        fprintf(stderr, "recv %d bytes\n", n);

        file_fd = open(path, O_RDWR);
        while ((n = read(file_fd, buf, BUFLEN)) > 0) {
          int x = send(sockfd, buf, n, 0);
          fprintf(stderr, "send %d bytes\n", x);
        }

        printf("put %s successfully\n", filename);
        close(file_fd);
        break;

      case 'g':
        int valid, file_len;
        n = recv(sockfd, buf, BUFLEN, 0);
        sscanf(buf, "%d %d", &valid, &file_len);
        if (!valid) {
          printf("The %s doesn’t exist\n", filename);
          break;
        }
        fprintf(stderr, "receive %d bytes, file len = %d\n", n, file_len);

        sprintf(buf, "1\n");
        send(sockfd, buf, 2, 0);

        fprintf(stderr, "open file at %s\n", path);
        file_fd = open(path, O_CREAT | O_RDWR, FILE_MODE);
        while (file_len > 0 &&
               (n = recv(sockfd, buf, min(file_len, BUFLEN), 0)) > 0) {
          fprintf(stderr, "recv %d bytes\n", n);
          write(file_fd, buf, n);
          file_len -= n;
          fprintf(stderr, "file len = %d\n", file_len);
        }

        printf("get %s successfully\n", filename);
        close(file_fd);
        break;

      case 'l':
        int len;
        recv(sockfd, buf, BUFLEN, 0);
        sscanf(buf, "%d", &len);

        sprintf(buf, "1\n");
        send(sockfd, buf, 2, 0);

        if (len > 0) {
          n = recv(sockfd, buf, len, 0);
          fprintf(stderr, "recv %d bytes\n", n);
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
int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: ./client [ip:port]\n");
    exit(1);
  }

  mkdir("client_dir", DIR_MODE);

  char ip[32], port[32];
  char *pch;
  pch = strtok(argv[1], ":");
  strcpy(ip, pch);
  pch = strtok(NULL, ":");
  strcpy(port, pch);

  uint16_t port_num = 0;
  int port_l = strlen(port);
  for (int i = 0; i < port_l; i++) port_num = port_num * 10 + port[i] - '0';
  fprintf(stderr, "port num = %d\n", port_num);

  struct sockaddr_in addr_in;
  addr_in.sin_family = AF_INET;
  addr_in.sin_port = htons(port_num);
  addr_in.sin_addr.s_addr = inet_addr(ip);

  int sockfd;
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  fprintf(stderr, "sockfd = %d\n", sockfd);
  if (sockfd >= 0) {
    int x = connect(sockfd, (struct sockaddr *)&addr_in, sizeof(addr_in));
    if (x == 0) {
      interactive(sockfd);
    } else
      close(sockfd);
  }

  fprintf(stderr, "connection refused.\n");
}
