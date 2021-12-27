#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#define ERR_EXIT(a) \
  do {              \
    perror(a);      \
    exit(1);        \
  } while (0)

#define BUFLEN 512
#define MAXFD 128

#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define DIR_MODE (FILE_MODE | S_IXUSR | S_IXGRP | S_IXOTH)

#define STATE_RECV_USERNAME 1
#define STATE_SEND_USERNAME_VALID 2
#define STATE_RECV_COMMAND 3
#define STATE_RCSN_FILESIZE 4
#define STATE_CHECK_FILESIZE 5
#define STATE_RCSN_FILE 6

#define PUT 0
#define GET 1
#define LS 2

int state[2048], commands[2048], filelen[2048], file_fd[2048];
char filename[2048][64], username[2048][16];
off_t offset[2048];
std::vector<std::string> ls[2048];

void dopath(int connfd) {
  struct dirent *dirp;
  DIR *dp;
  if ((dp = opendir("./server_dir/")) == NULL) {
    fprintf(stderr, "can't open ./server_dir/\n");
  }
  ls[connfd].clear();
  while ((dirp = readdir(dp)) != NULL) {
    if (strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
      continue;
    ls[connfd].push_back(std::string(dirp->d_name, strlen(dirp->d_name)));
  }
  closedir(dp);
  filelen[connfd] = 0;
  sort(ls[connfd].begin(), ls[connfd].end());
  return;
}

void serve(int sockfd) {
  int n;
  char buf[BUFLEN], server_dir[64] = "./server_dir/", path[64];

  memset(state, 0, sizeof(state));

  int maxfd = MAXFD;
  fd_set master_rfds, working_rfds, master_wfds, working_wfds;

  FD_ZERO(&master_rfds);
  FD_ZERO(&master_wfds);
  FD_SET(sockfd, &master_rfds);

  while (1) {
    working_rfds = master_rfds;
    working_wfds = master_wfds;
    if (select(maxfd, &working_rfds, &working_wfds, NULL, NULL) < 0) {
      fprintf(stderr, "select failed\n");
    }

    if (FD_ISSET(sockfd, &working_rfds)) {
      int connfd = accept(sockfd, NULL, NULL);
      FD_SET(connfd, &master_rfds);
      state[connfd] = STATE_RECV_USERNAME;
    }

    for (int connfd = 0; connfd < maxfd; connfd++) {
      if (connfd == sockfd) continue;
      if (FD_ISSET(connfd, &working_rfds)) {
        if (state[connfd] == STATE_RECV_USERNAME) {  // recv username
          // read username
          if ((n = recv(connfd, username[connfd], BUFLEN, 0)) <= 0) {
            username[connfd][0] = 0;
            continue;
          }
          username[connfd][n] = 0;

          state[connfd] = STATE_SEND_USERNAME_VALID;
          FD_CLR(connfd, &master_rfds);
          FD_SET(connfd, &master_wfds);
        } else if (state[connfd] == STATE_RECV_COMMAND) {  // recv command
          // read command: put / get / ls
          if ((n = recv(connfd, buf, BUFLEN, 0)) <= 0) {
            username[connfd][0] = 0;
            continue;
          }
          buf[n] = 0;

          switch (buf[0]) {
            case 'p':
              commands[connfd] = PUT;
              state[connfd] = STATE_CHECK_FILESIZE;
              break;
            case 'g':
              commands[connfd] = GET;
              state[connfd] = STATE_RCSN_FILESIZE;
              break;
            case 'l':
              commands[connfd] = LS;
              state[connfd] = STATE_RCSN_FILESIZE;
              break;
          }

          if (commands[connfd] == PUT || commands[connfd] == GET) {
            char *pch;
            pch = strtok(buf, " ");
            pch = strtok(NULL, " ");
            strcpy(filename[connfd], pch);

            if (commands[connfd] == PUT) {
              pch = strtok(NULL, " ");
              sscanf(pch, "%d", &filelen[connfd]);
            }

            offset[connfd] = 0;
            file_fd[connfd] = -1;
          }

          FD_CLR(connfd, &master_rfds);
          FD_SET(connfd, &master_wfds);

        } else if (state[connfd] == STATE_CHECK_FILESIZE) {
          if((n = recv(connfd, buf, 2, 0)) <= 0){
            username[connfd][0] = 0;
            continue;
          }
          FD_CLR(connfd, &master_rfds);
          FD_SET(connfd, &master_wfds);
          state[connfd] = STATE_RCSN_FILE;
        } else if (state[connfd] == STATE_RCSN_FILE) {
          if (file_fd[connfd] == -1) {
            strcpy(path, server_dir);
            strcpy(path + strlen(path), filename[connfd]);
            file_fd[connfd] = open(path, O_RDWR | O_CREAT | O_TRUNC, FILE_MODE);
          }

          if (filelen[connfd] > 0) {
            if((n = recv(connfd, buf, std::min(filelen[connfd], BUFLEN), 0)) <= 0){
              username[connfd][0] = 0;
              continue;
            }
            pwrite(file_fd[connfd], buf, n, offset[connfd]);
            filelen[connfd] -= n;
            offset[connfd] += n;
          }

          if (filelen[connfd] <= 0 || n <= 0) {
            close(file_fd[connfd]);
            file_fd[connfd] = -1;
            offset[connfd] = 0;
            state[connfd] = STATE_RECV_COMMAND;
          }
        }
      } else if (FD_ISSET(connfd, &working_wfds)) {
        if (state[connfd] == STATE_SEND_USERNAME_VALID) {
          int valid = 1;
          for (int iter_fd = 0; iter_fd < maxfd; iter_fd++) {
            if (iter_fd != connfd &&
                strcmp(username[iter_fd], username[connfd]) == 0) {
              username[connfd][0] = 0;
              valid = 0;
            }
          }
          sprintf(buf, "%d\n", valid);
          send(connfd, buf, 1, MSG_NOSIGNAL);

          state[connfd] = valid ? STATE_RECV_COMMAND : STATE_RECV_USERNAME;
          FD_CLR(connfd, &master_wfds);
          FD_SET(connfd, &master_rfds);
        } else if (state[connfd] == STATE_RCSN_FILESIZE) {
          if (commands[connfd] == GET) {
            struct stat st;
            strcpy(path, server_dir);
            strcpy(path + strlen(path), filename[connfd]);
            int valid;
            if ((valid = stat(path, &st)) == -1) {
              sprintf(buf, "0\n");
              send(connfd, buf, strlen(buf), MSG_NOSIGNAL);
              state[connfd] = STATE_RECV_COMMAND;
            } else {
              sprintf(buf, "1 %d\n", (int)st.st_size);
              send(connfd, buf, strlen(buf), MSG_NOSIGNAL);
              state[connfd] = STATE_CHECK_FILESIZE;
              filelen[connfd] = (int)st.st_size;
            }
          } else if (commands[connfd] == LS) {
            dopath(connfd);
            int sum = 0;
            for (std::string s : ls[connfd]) sum += s.length();
            sprintf(buf, "%lu\n", sum + ls[connfd].size());
            n = send(connfd, buf, strlen(buf), MSG_NOSIGNAL);
            state[connfd] = STATE_CHECK_FILESIZE;
          }
          FD_CLR(connfd, &master_wfds);
          FD_SET(connfd, &master_rfds);
        } else if (state[connfd] == STATE_CHECK_FILESIZE) {
          sprintf(buf, "1\n");
          n = send(connfd, buf, 2, MSG_NOSIGNAL);
          state[connfd] = STATE_RCSN_FILE;
          FD_CLR(connfd, &master_wfds);
          FD_SET(connfd, &master_rfds);
        } else if (state[connfd] == STATE_RCSN_FILE) {
          if (commands[connfd] == LS) {
            int l = 0;
            for (int i = filelen[connfd]; i < ls[connfd].size(); i++) {
              std::string s = ls[connfd][i];
              if (l + s.length() + 1 > BUFLEN) {
                send(connfd, buf, l, MSG_NOSIGNAL);
                filelen[connfd] = i;
                l = 0;
              }
              strcpy(buf + l, s.c_str());
              l += s.length();
              buf[l++] = '\n';
            }
            if (l) {
              n = send(connfd, buf, l, MSG_NOSIGNAL);
            }
            state[connfd] = STATE_RECV_COMMAND;
            FD_CLR(connfd, &master_wfds);
            FD_SET(connfd, &master_rfds);
          } else if (commands[connfd] == GET) {
            // send get file
            if (file_fd[connfd] == -1) {
              strcpy(path, server_dir);
              strcpy(path + strlen(path), filename[connfd]);
              file_fd[connfd] = open(path, O_RDWR);
            }

            n = pread(file_fd[connfd], buf, std::min(BUFLEN, filelen[connfd]),
                      offset[connfd]);
            if (n > 0) {
              int x = send(connfd, buf, n, MSG_NOSIGNAL);
              filelen[connfd] -= n;
              offset[connfd] += n;
            }

            if (n <= 0) {
              close(file_fd[connfd]);
              file_fd[connfd] = -1;
              offset[connfd] = 0;
              state[connfd] = STATE_RECV_COMMAND;
              FD_CLR(connfd, &master_wfds);
              FD_SET(connfd, &master_rfds);
            }
          }
        }
      }
    }
  }
}


static int initServer(unsigned short port) {
  struct sockaddr_in server_addr;
  int sockfd = socket(AF_INET , SOCK_STREAM , 0);
  if (sockfd < 0) ERR_EXIT("socket");

  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr*)& server_addr, sizeof(server_addr)) < 0) {
      ERR_EXIT("bind");
  }
  if (listen(sockfd, MAXFD) < 0) {
      ERR_EXIT("listen");
  }
  return sockfd;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "usage: ./server [port]\n");
    exit(1);
  }

  mkdir("server_dir", DIR_MODE);

  int sockfd = initServer((unsigned short) atoi(argv[1]));
  serve(sockfd); 
}
