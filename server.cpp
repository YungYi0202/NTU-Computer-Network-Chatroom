#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
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
#include <sstream>
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

std::string server_dir = "server_dir";

void closeFD(int fd) {
  fprintf(stderr, "closeFD: %d\n", fd);
  close(fd);
}

int handleRecv(int connfd, char *buf) {
  bzero(buf, BUFLEN);
  int ret = recv(connfd, buf, BUFLEN, 0);
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

int handleRead(std::string path, std::string *file_content) {
  std::ifstream targetFile;
  std::string line;
  targetFile.open(path);
  if (targetFile.is_open()) {
    while (getline(targetFile, line)) {
      *file_content = *file_content + line + '\n';
      std::cerr << *file_content << std::endl;
    }
  }
  file_content->pop_back();
  return file_content->length();
}

int _handleSend(int connfd, char *buf) {
  int writeLen = strlen(buf);
  int ret = send(connfd, buf, writeLen, MSG_NOSIGNAL);
  if (ret != writeLen) {
    closeFD(connfd);
    return ret;
  } else {
    fprintf(stderr, "========handleSend: connfd:%d buf_last_char:%d=========\n",
            connfd, buf[strlen(buf) - 1]);
    fprintf(stderr, "%s", buf);
    fprintf(stderr, "=============================\n");
  }
  return ret;
}

int handleSend(int connfd, char *buf, std::string str = "") {
  std::cerr << "str = " << str << std::endl;
  int ret, writeLen;
  std::string tmp;
  if (str == "") {
    ret = _handleSend(connfd, buf);
  } else {
    while (str.size()) {
      bzero(buf, BUFLEN);
      writeLen = (str.size() < BUFLEN) ? str.size() : BUFLEN;
      tmp = str.substr(0, writeLen);
      str = str.substr(writeLen);
      // fprintf(stderr, "original buf = %s\n", buf);
      std::cerr << "tmp=" << tmp << std::endl;
      sprintf(buf, "%s", tmp.c_str());
      // fprintf(stderr, "buf = %s\n", buf);
      ret = _handleSend(connfd, buf);
    }
  }
  return ret;
}

int _handleWrite(int file_fd, char *buf) {
  int writeLen = strlen(buf);
  int ret = write(file_fd, buf, writeLen);
  if (ret != writeLen) {
    closeFD(file_fd);
    return ret;
  } else {
    fprintf(stderr,
            "========handleWrite: file_fd:%d buf_last_char:%d=========\n",
            file_fd, buf[strlen(buf) - 1]);
    fprintf(stderr, "%s", buf);
    fprintf(stderr, "=============================\n");
  }
  return ret;
}

int handleWrite(int file_fd, char *buf, std::string str = "") {
  int ret, writeLen;
  std::string tmp;
  if (str == "") {
    ret = _handleWrite(file_fd, buf);
  } else {
    while (str.size()) {
      bzero(buf, BUFLEN);
      writeLen = (str.size() < BUFLEN) ? str.size() : BUFLEN;
      tmp = str.substr(0, writeLen);
      str = str.substr(writeLen);
      sprintf(buf, "%s", tmp.c_str());
      ret = _handleWrite(file_fd, buf);
    }
  }
  return ret;
}

struct historyData {
  std::string From;
  std::string To;
  std::string Content;
};

class Client {
 public:
  int connfd;
  sqlite3 *db, *users;
  char *zErrMsg = 0;
  char csql[BUFLEN];
  std::string history;
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
      fflush(stderr);
      sqlite3_free(zErrMsg);
    } else {
      fprintf(stderr, "Successfully %s.\n", msg);
      fflush(stderr);
    }
  }
  void openDatabase() {
    int rc;

    rc = sqlite3_open("chatroom.db", &db);
    if (rc) {
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
      fflush(stderr);
      exit(0);
    } else {
      fprintf(stderr, "Successfully open database\n");
      fflush(stderr);
    }

    rc = sqlite3_open("username.db", &users);
    if (rc) {
      fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(users));
      fflush(stderr);
      exit(0);
    } else {
      fprintf(stderr, "Successfully open database\n");
      fflush(stderr);
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
  void addFriend(const char *username, const char *friend_name) {
    sprintf(csql,
            "INSERT INTO CHATROOM "
            "VALUES ('%s', '%s', ''); ",
            username, friend_name);
    int rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "add friend");
  }
  void deleteFriend(const char *username, const char *friend_name) {
    sprintf(csql, "DELETE FROM CHATROOM WHERE USERNAME='%s' AND FRIEND='%s';",
            username, friend_name);
    int rc = sqlite3_exec(db, csql, callback, 0, &zErrMsg);
    errorHandling(rc, "delete friend");
  }
  void listFriends(const char *username) {
    sqlite3_stmt *stmt;
    fprintf(stderr, "list %s friends\n", username);
    fflush(stderr);
    sprintf(csql, "SELECT FRIEND FROM CHATROOM WHERE USERNAME='%s';", username);
    int rc = sqlite3_prepare_v2(db, csql, -1, &stmt,
                                NULL);  // -1: read to first null byte
    if (rc != SQLITE_OK) {
      fprintf(stderr, "Can't list friends: %s\n", sqlite3_errmsg(db));
      fflush(stderr);
      return;
    }
    sqlite3_bind_int(stmt, 1, 1);

    char friends[MAXFD][32];
    int idx = 0, filelen = 0;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      const unsigned char *content = sqlite3_column_text(stmt, 0);
      fprintf(stderr, "%s", content);
      fflush(stderr);
      sprintf(friends[idx], "%s", content);
      filelen += strlen(friends[idx]);
      fprintf(stderr, "%d\n", filelen);
      fflush(stderr);
      idx++;
    }

    char buf[BUFLEN];
    sprintf(buf, "%d", filelen + 2 * (idx) + 2 * (idx - 1) + 2);
    handleSend(connfd, buf);
    handleRecv(connfd, buf);

    std::string sbuf = "[";
    for (int i = 0; i < idx; i++) {
      if (i) sbuf += ", ";
      sbuf += "\"" + std::string(friends[i]) + "\"";
    }
    sbuf += "]";
    handleSend(connfd, buf, sbuf);

    if (rc != SQLITE_DONE) {
      fprintf(stderr, "error: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
  }
  void addHistory(const char *username, const char *friend_name,
                  const char *history) {
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
  void printHistory(const char *username, const char *friend_name) {
    sqlite3_stmt *stmt;
    sprintf(csql,
            "SELECT HISTORY FROM CHATROOM WHERE USERNAME='%s' AND FRIEND='%s';",
            username, friend_name);
    int rc = sqlite3_prepare_v2(db, csql, -1, &stmt,
                                NULL);  // -1: read to first null byte
    if (rc != SQLITE_OK) {
      fprintf(stderr, "Can't print history: %s\n", sqlite3_errmsg(db));
      return;
    }
    sqlite3_bind_int(stmt, 1, 1);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
      const unsigned char *content = sqlite3_column_text(stmt, 0);
      history = std::string(
          (const char *)content);  // history length no more than 1e6
    }
    processHistory();
  }
  std::vector<historyData> data;
  void processHistory() {
    data.clear();
    std::stringstream ss(history);
    std::string line;
    while (getline(ss, line)) {
      std::stringstream sline(line);
      std::string from, to, something;
      sline >> from >> to;
      getline(sline, something);
      data.push_back({from, to, something});
    }
    std::string json = "[\n";
    for (int i = 0; i < data.size(); i++) {
      historyData hd = data[i];
      if (i) json += ",\n";
      json += "{\n";
      json += "\"From\": \"" + hd.From + "\",\n";
      json += "\"To\": \"" + hd.To + "\",\n";
      json += "\"Content\": \"" + hd.Content + "\"\n";
      json += "}";
    }
    json += "\n]";

    char buf[BUFLEN];
    handleSend(connfd, buf, std::to_string(json.length()));
    handleRecv(connfd, buf);
    handleSend(connfd, buf, json);
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
  void addUser(const char *username) {
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
  void say(std::string username, std::string friend_name,
           std::string something) {
    std::string record = username + " " + friend_name + " " + something + "\n";
    std::cerr << "record = " << record << std::endl;
    addHistory(username.c_str(), friend_name.c_str(), record.c_str());
  }
};

void *handling_client(void *arg) {
  int connfd = *(int *)arg;
  Client client;
  client.connfd = connfd;
  client.openDatabase();
  int n;

  char c_command[BUFLEN], buf[BUFLEN];
  std::string username, friend_name, something, filename, command;
  while (1) {
    handleRecv(connfd, c_command);
    std::string commands(c_command);
    std::stringstream commandss(commands);

    switch (c_command[0]) {
      case 'a':
        commandss >> command >> username >> friend_name;
        client.addFriend(username.c_str(), friend_name.c_str());
        break;
      case 'd':
        commandss >> command >> username >> friend_name;
        client.deleteFriend(username.c_str(), friend_name.c_str());
        break;
      case 'l':
        commandss >> command >> username;
        client.listFriends(username.c_str());
        break;
      case 'h':
        commandss >> command >> username >> friend_name;
        client.printHistory(username.c_str(), friend_name.c_str());
        break;
      case 's':
        commandss >> command >> username >> friend_name;
        getline(commandss, something);
        std::cerr << "something = " << something << std::endl;
        client.say(username, friend_name, something);
        break;
      case 'j':
        commandss >> command >> username;
        client.addUser(username.c_str());
        break;
      case 'p': {
        commandss >> command >> username >> filename;
        const fs::path dir = server_dir + "/" + username;
        if (!fs::exists(dir)) {
          std::cerr << "dir = " << dir << std::endl;
          fs::create_directory(dir);
        }
        handleSend(connfd, buf, "1");
        handleRecv(connfd, buf);
        int filelen;
        sscanf(buf, "%d", &filelen);
        handleSend(connfd, buf, "1");
        char c_path[128];
        sprintf(c_path, "server_dir/%s/%s", username.c_str(), filename.c_str());
        int file_fd = open(c_path, O_CREAT | O_RDWR, FILE_MODE);
        while (filelen > 0 && (n = handleRecv(connfd, buf)) > 0) {
          write(file_fd, buf, n);
          filelen -= n;
        }
        close(file_fd);
        break;
      }
      case 'g':
        commandss >> command >> username >> filename;
        std::string path =
            std::string(server_dir) + "/" + username + "/" + filename;
        std::string file_content;
        handleRead(path, &file_content);
        handleSend(connfd, buf, std::to_string(file_content.length()));
        handleRecv(connfd, buf);
        handleSend(connfd, buf, file_content);
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
      fflush(stderr);
    }

    if (FD_ISSET(sockfd, &working_rfds)) {
      int connfd = accept(sockfd, NULL, NULL);
      pthread_create(&(ntid[ntid_cnt++]), NULL, handling_client, &(connfd));
      // handling_client(&(connfd));
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