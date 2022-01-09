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

#include "sqlite3.h"

#define ERR_EXIT(a) \
  do {              \
    perror(a);      \
    exit(1);        \
  } while (0)

#define BUFLEN 512
#define MAXFD 128

class Database {
 public:
  sqlite3 *db;
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
  }
  void closeDatabase() { sqlite3_close(db); }
  void createDatabase() {
    sprintf(csql,
            "CREATE TABLE CHATROOM("
            "USERNAME  VARCHAR(20) NOT NULL,"
            "FRIEND    VARCHAR(20) NOT NULL,"
            "HISTORY   TEXT,"
            "PRIMARY KEY (USERNAME, FRIEND)"
            ");");
    int rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "create table");
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
  void listFriends(char *username) {
    sprintf(csql, "SELECT FRIEND FROM CHATROOM WHERE USERNAME='%s';", username);
    int rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
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
  void printDatabase() {
    sprintf(csql, "SELECT * FROM CHATROOM;");
    int rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "print database");
  }
} database;

void serve(int sockfd) {
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
    }
  }
}

static int initServer(unsigned short port) {
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

  database.openDatabase();
  database.createDatabase();
  database.closeDatabase();

  int sockfd = initServer((unsigned short)atoi(argv[1]));
  serve(sockfd);
}
