#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "sqlite3.h"

namespace fs = std::filesystem;

#define DIR_MODE (FILE_MODE | S_IXUSR | S_IXGRP | S_IXOTH)
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#define ERR_EXIT(a) \
  do {              \
    perror(a);      \
    exit(1);        \
  } while (0)

#define BUFLEN 2048
#define MAXFD 128

const fs::path server_dir{"server_dir"};

class Client {
 public:
  int connfd;
  sqlite3 *db, *users;
  char *zErrMsg = 0;
  char csql[BUFLEN];
  static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    int i;
    for (i = 0; i < argc; i++) {
      printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
  }
  void errorHandling(int rc, const char *msg) {
    if (rc != SQLITE_OK) {
      fprintf(stderr, "SQL error: %s\n", zErrMsg);
      sqlite3_free(zErrMsg);
    } else {
      fprintf(stderr, "Successfully %s.\n", msg);
    }
  }
  void openDatabase() {
    int rc;

    rc = sqlite3_open("chatroom.db", &db);
    if (rc) {
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      exit(0);
    } else {
      fprintf(stderr, "Successfully open database\n");
    }

    rc = sqlite3_open("username.db", &users);
    if (rc) {
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(users));
      exit(0);
    } else {
      fprintf(stderr, "Successfully open database\n");
    }
  }
  void closeDatabase() {
    sqlite3_close(db);
    sqlite3_close(users);
  }
  void createDatabase() {
    sprintf(csql,
            "CREATE TABLE CHATROOM("
            "USERNAME  VARCHAR(20) NOT NULL,"
            "FRIEND    VARCHAR(20) NOT NULL,"
            "HISTORY   TEXT,"
            "PRIMARY KEY (USERNAME, FRIEND)"
            ");");
    int rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "create database");

    sprintf(csql,
            "CREATE TABLE USERNAME("
            "USERNAME VARCHAR(20) NOT NULL,"
            "PRIMARY KEY (USERNAME)"
            ");");
    rc = sqlite3_exec(users, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "create username table");
  }
  void addFriend(char *username, char *friend_name) {
    sprintf(csql,
            "INSERT INTO CHATROOM "
            "VALUES ('%s', '%s', ''); ",
            username, friend_name);
    int rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "add friend");
  }
  void deleteFriend(char *username, char *friend_name) {
    sprintf(csql, "DELETE FROM CHATROOM WHERE USERNAME='%s' AND FRIEND='%s';",
            username, friend_name);
    int rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "delete friend");
  }
  static int listFriendCallback(void *connfd, int argc, char **argv,
                                char **azColName) {
    char buf[BUFLEN];
    for (int i = 0; i < argc; i++) {
      sprintf(buf + strlen(buf), "(%d) %s ", i + 1, argv[i] ? argv[i] : "NULL");
    }
    sprintf(buf + strlen(buf), "\n");
    send(*(int *)connfd, buf, strlen(buf), MSG_NOSIGNAL);
    return 0;
  }
  void listFriends(char *username) {
    sprintf(csql, "SELECT FRIEND FROM CHATROOM WHERE USERNAME='%s';", username);
    int rc = sqlite3_exec(db, csql, listFriendCallback, &connfd, &zErrMsg);
    errorHandling(rc, "list friends");
  }
  void addHistory(char *username, char *friend_name, char *history) {
    sprintf(csql,
            "UPDATE CHATROOM "
            "SET HISTORY=HISTORY||'%s' "
            "WHERE USERNAME='%s' AND FRIEND='%s'; ",
            history, username, friend_name);
    int rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
    sprintf(csql,
            "UPDATE CHATROOM "
            "SET HISTORY=HISTORY||'%s' "
            "WHERE USERNAME='%s' AND FRIEND='%s'; ",
            history, friend_name, username);
    rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "add history");
  }
  static int printHistoryCallback(void *connfd, int argc, char **argv,
                                  char **azColName) {
    char buf[BUFLEN];
    for (int i = 0; i < argc; i++) {
      sprintf(buf + strlen(buf), "%s\n", argv[i] ? argv[i] : "NULL");
    }
    send(*(int *)connfd, buf, strlen(buf), MSG_NOSIGNAL);
    return 0;
    // TODO: the history should not be longer than 512 characters
  }
  void printHistory(char *username, char *friend_name) {
    sprintf(csql,
            "SELECT HISTORY FROM CHATROOM WHERE USERNAME='%s' AND FRIEND='%s';",
            username, friend_name);
    int rc = sqlite3_exec(db, csql, printHistoryCallback, &connfd, &zErrMsg);
    errorHandling(rc, "print history");
  }
  void printDatabase() {
    sprintf(csql, "SELECT * FROM CHATROOM;");
    int rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "print database");
  }
  static int addUserCallback(void *connfd, int argc, char **argv,
                             char **azColName) {
    char buf[BUFLEN];
    if (argc >= 1)
      sprintf(buf, "0");
    else {
      sprintf(buf, "1");
    }
    send(*(int *)connfd, buf, strlen(buf), MSG_NOSIGNAL);
    return 0;
    // TODO: the history should not be longer than 512 characters
  }
  void addUser(char *username) {
    sprintf(csql, "SELECT * FROM USERNAME WHERE USERNAME='%s';", username);
    int rc = sqlite3_exec(users, csql, addUserCallback, &connfd, &zErrMsg);
    errorHandling(rc, "query username");
    sprintf(csql,
            "INSERT INTO USERNAME "
            "VALUES ('%s'); ",
            username);
    rc = sqlite3_exec(users, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "add username");
  }
};

void *handling_client(void *arg) {
  int connfd = *(int *)arg;
  Client client;
  client.connfd = connfd;
  int n;

  char command[BUFLEN], username[BUFLEN], friend_name[BUFLEN],
      something[BUFLEN], filename[BUFLEN], buf[BUFLEN];
  while (1) {
    n = recv(connfd, command, BUFLEN, 0);
    if (n <= 0) {
      close(connfd);
      pthread_exit((void *)1);
    }
    // TODO: process command

    char *pch;
    int file_fd;
    switch (command[0]) {
      case 'a':
        pch = strtok(command, " ");
        pch = strtok(NULL, " ");
        strcpy(username, pch);
        pch = strtok(NULL, " ");
        strcpy(friend_name, pch);
        client.addFriend(username, friend_name);
        break;
      case 'd':
        pch = strtok(command, " ");
        pch = strtok(NULL, " ");
        strcpy(username, pch);
        pch = strtok(NULL, " ");
        strcpy(friend_name, pch);
        client.deleteFriend(username, friend_name);
        break;
      case 'l':
        pch = strtok(command, " ");
        pch = strtok(NULL, " ");
        strcpy(username, pch);
        client.listFriends(username);
        break;
      case 'h':
        pch = strtok(command, " ");
        pch = strtok(NULL, " ");
        strcpy(username, pch);
        pch = strtok(NULL, " ");
        strcpy(friend_name, pch);
        client.printHistory(username, friend_name);
        break;
      case 's':
        pch = strtok(command, " ");
        pch = strtok(NULL, " ");
        strcpy(username, pch);
        pch = strtok(NULL, " ");
        strcpy(friend_name, pch);
        pch = strtok(NULL, " ");
        strcpy(something, pch);
        client.addHistory(username, friend_name, something);
        break;
      case 'j':
        pch = strtok(command, " ");
        pch = strtok(NULL, " ");
        strcpy(username, pch);
        client.addUser(username);
        break;
      case 'p':
        if (!fs::exists({server_dir / username})) {
          std::ofstream{server_dir / username};
        }
        char *pch;
        pch = strtok(command, " ");
        pch = strtok(NULL, " ");
        strcpy(username, pch);
        pch = strtok(NULL, " ");
        strcpy(filename, pch);
        send(connfd, "1", 1, MSG_NOSIGNAL);
        n = recv(connfd, buf, BUFLEN, 0);
        if (n <= 0) {
          close(connfd);
          pthread_exit((void *)1);
        }
        int filelen;
        sscanf(buf, "%*s %d", &filelen);
        send(connfd, "1", 1, MSG_NOSIGNAL);
        file_fd = open(fs::path({server_dir / username / filename}).string().c_str(), O_CREAT | O_RDWR, FILE_MODE);
        while(filelen > 0 && (n = recv(connfd, buf, BUFLEN, 0)) > 0) {
          write(file_fd, buf, n);
          filelen -= n;
        }
        close(file_fd);
        if (n <= 0) {
          close(connfd);
          pthread_exit((void *)1);
        }
        break;
      case 'g':
        struct stat st;
        stat(fs::path({server_dir / username / filename}).string().c_str(), &st);
        sprintf(buf, "%s %d\n", filename, (int)st.st_size);
        send(connfd, buf, strlen(buf), MSG_NOSIGNAL);
        n = recv(connfd, buf, BUFLEN, 0);
        if (n <= 0) {
          close(connfd);
          pthread_exit((void *)1);
        }
        file_fd = open(fs::path({server_dir / username / filename}).string().c_str(), O_RDWR);
        while((n = read(file_fd, buf, BUFLEN)) > 0) {
          send(connfd, buf, n, MSG_NOSIGNAL);
        }
        break;
    }
  }
}

#define MAX_CLIENT 128
pthread_t ntid[MAX_CLIENT];
void serve(int sockfd) {
  int maxfd = MAXFD;
  int ntid_cnt = 0;
  fd_set master_rfds, working_rfds;

  FD_ZERO(&master_rfds);
  FD_SET(sockfd, &master_rfds);

  while (1) {
    working_rfds = master_rfds;
    if (select(maxfd, &working_rfds, NULL, NULL, NULL) < 0) {
      fprintf(stderr, "select failed\n");
    }

    if (FD_ISSET(sockfd, &working_rfds)) {
      int connfd = accept(sockfd, NULL, NULL);
      pthread_create(&(ntid[ntid_cnt++]), NULL, handling_client, &(connfd));
    }
  }
  // pthread_join(ntid[?], NULL);
}

static int initServer(unsigned short port) {
  fs::create_directory(server_dir);

  struct sockaddr_in server_addr;
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) ERR_EXIT("socket");

  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
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

  Client root;
  root.openDatabase();
  root.createDatabase();

  int sockfd = initServer((unsigned short)atoi(argv[1]));
  serve(sockfd);
}
