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
#include <vector>

#define ERR_EXIT(a) \
  do {              \
    perror(a);      \
    exit(1);        \
  } while (0)
#define BUF_LEN 2048
#define MAXFD 1024
#define SVR_PORT 8081
#define CRLF "\r\n" 
#define CONTENT_LEN "Content-Length: "
#define WEBKIT_FROM_BOUNDARY "------WebKitFormBoundary"
#define POST_RES_SUCCESS "HTTP/1.1 200 OK\r\n\r\n"
#define POST_RES_ERROR "HTTP/1.1 404 Not Found\r\n\r\n"

#define DIR_MODE (FILE_MODE | S_IXUSR | S_IXGRP | S_IXOTH)
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

enum {
  STATE_UNUSED,
  STATE_WAIT_REQ_FROM_BROWSER, 
  STATE_SEND_GET_REQ_TO_SVR, 
  STATE_WAIT_GET_RES_FROM_SVR, 
  STATE_SEND_GET_RES_TO_BROWSER,
  STATE_SEND_PUT_RES_TO_BROWSER,
  STATE_SEND_OTHER_POST_RES_TO_BROWSER
  };

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

class Client {
public:  // TODO: modify access
  int state = STATE_WAIT_REQ_FROM_BROWSER;
  std::string username = "default";
  bool hasUsername = false;
  HttpGet getRes;
  int browserfd;

  bool postNotFinished = false;

  static const int maxfd = MAXFD;
  int svrfd;
  int sockfd = -1;
  // browser browsers[maxfd];
  fd_set master_rfds, working_rfds, master_wfds, working_wfds;
  char buf[BUF_LEN];

  // TODO:
  bool loginflag = false;
  bool historyflag = false;

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
  
  void init(char *ip, int port_num) {
    /* To talk with server.cpp */
    mkdir("client_dir", DIR_MODE);

    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(port_num);
    addr_in.sin_addr.s_addr = inet_addr(ip);

    svrfd = socket(AF_INET, SOCK_STREAM, 0);
    if (svrfd >= 0) {
      int x = connect(svrfd, (struct sockaddr *)&addr_in, sizeof(addr_in));
      if (x != 0) {
        close(svrfd);
        fprintf(stderr, "No corresponding server.cpp.\n");
      }
    }

    /* To talk with browsers. */
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

  void work() {
    FD_ZERO(&master_rfds);
    FD_ZERO(&master_wfds);
  
    FD_SET(sockfd, &master_rfds);
    // browsers[sockfd].fd = sockfd;
    
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
        // browsers[clifd].start(clifd);
        // state = STATE_WAIT_REQ_FROM_BROWSER;
      }

      for (int fd = 0; fd < maxfd; fd++) {
        if (fd == sockfd) continue;
        
        if (FD_ISSET(fd, &working_rfds)) {
          if (state == STATE_WAIT_REQ_FROM_BROWSER && fd != svrfd) {
            // if (postNotFinished && fd != browserfd) continue;
            browserfd = fd;
            loginflag = false;
            historyflag=false;
            FD_SET(fd, &master_wfds);
            // TODO: What if buf is too small?
            std::stringstream ss;
            do {
              handleRead(fd);
              std::string req(buf);
              ss << req;
            } while (strlen(buf) == BUF_LEN && buf[BUF_LEN-1] != '\n');
            
            /* To browser */
            std::string command, target;
            ss >> command >> target;
            if (command == "GET") { 
              if (target[1] == '?') {
                target = target.substr(2); //rm "/?".
                std::size_t eqPos=target.find('=');
                if (eqPos == std::string::npos || eqPos == target.size() - 1) {
                  ERR_EXIT("GET TARGET WRONG FORMAT: no \"=\" or no string after \"=\"");
                } else {
                  std::string cmd = target.substr(0, eqPos);
                  std::string name = target.substr(eqPos+1);
                  if (cmd == "login") {
                    username = name;
                    loginflag = true;
                  } else if (cmd == "history") {
                    historyflag = true;
                  } else {
                    ERR_EXIT("GET TARGET WRONG FORMAT: <command>=<content>, command is wrong.");
                  }
                }
                
              } else {
                getRes = GetResponse(target);
              }
              state = STATE_SEND_GET_RES_TO_BROWSER;
            } else if (command == "POST") {
                // TODO
                fprintf(stderr, "========%s %s=========\n",command.c_str(), target.c_str());
                std::string tmp = ss.str();
                int contentLen = 0;
                int pos;
                std::string requestToSvr;
                std::string fileContentPutToSvr;
                /* Read Header*/
                while((pos = tmp.find('\n')) != std::string::npos) {
                    std::string line = tmp.substr(0, pos);
                    tmp = tmp.substr(pos + 1);
                    if (line == "\r") break;
                }
                /* Read Content */
                if (tmp.size() > 0 ) {
                    if ((pos = tmp.find(CRLF)) != std::string::npos) {
                        /* put request */
                        std::string friendnamefilename = tmp.substr(0, pos);
                        fileContentPutToSvr = tmp.substr(pos + strlen(CRLF));
                        
                        pos = friendnamefilename.find("=");
                        std::string friendname = friendnamefilename.substr(0, pos);
                        std::string filename = friendnamefilename.substr(pos+1);

                        requestToSvr = "put " + friendname + " " + filename;
                        // contentLen -= pos + strlen(CRLF);
                        state = STATE_SEND_PUT_RES_TO_BROWSER;
                    } else {
                        /* other post request */
                        // fprintf(stderr, "%s", tmp.c_str());
                        /*
                            say=friendname=content
                            add=friendname
                            delete=friendname
                        */
                        pos = tmp.find("=");
                        std::string cmd = tmp.substr(0, pos);
                        tmp = tmp.substr(pos + 1);
                        if (cmd == "say") {
                            pos = tmp.find("=");
                            std::string friendname = tmp.substr(0, pos);
                            std::string content = tmp.substr(pos + 1);
                            requestToSvr = "say " + username + " " + friendname + " " + content;
                        } else if (cmd == "add" || cmd == "delete") {
                            requestToSvr = cmd + " " + username + " " + tmp;
                        } else {
                            fprintf(stderr, "Wrong POST content which starts with cmd:%s\n", cmd.c_str());
                            handleWrite(fd, POST_RES_ERROR);
                            closeFD(fd);
                            continue;
                        }
                        state = STATE_SEND_OTHER_POST_RES_TO_BROWSER;
                        fprintf(stderr, "STATE_SEND_OTHER_POST_RES_TO_BROWSER %d\n", browserfd);
                    }  
                } else {
                    fprintf(stderr, "The POST content is empty.\n");
                    handleWrite(fd, POST_RES_ERROR);
                    closeFD(fd);
                }
                fprintf(stderr, "reqToSvr: %s\n", requestToSvr.c_str());
                fprintf(stderr, "fileContentPutToSvr: %s\n", fileContentPutToSvr.c_str());
                /*
                while((pos = tmp.find('\n')) != std::string::npos) {
                    std::string line = tmp.substr(0, pos);
                    tmp = tmp.substr(pos + 1);
                    if (line == "\r") break;
                    else if (line.find(CONTENT_LEN) != std::string::npos ) {
                        line = line.substr(strlen(CONTENT_LEN));
                        contentLen = atoi(line.c_str());
                        fprintf(stderr, "%s%d\n", CONTENT_LEN, contentLen);
                    }
                }
                if (tmp.size() > 0 ) {
                    if ((pos = tmp.find(CRLF)) != std::string::npos) {
                        std::string filename = tmp.substr(0, pos);
                        std::string filecontent = tmp.substr(pos + strlen(CRLF));
                        std::string requestToSvr = "put " + filename;
                        fprintf(stderr, "%s\n", requestToSvr.c_str());
                        fprintf(stderr, "%s", filecontent.c_str());
                        contentLen -= pos + strlen(CRLF);
                        fprintf(stderr, "contentLen: %d filecontent.size(): %lu\n", contentLen, filecontent.size());
                    } else {
                        fprintf(stderr, "%s", tmp.c_str());
                    }  
                    
                    handleWrite(fd, "HTTP/1.1 200 OK\r\n\r\n");
                    closeFD(fd);
                } else {
                    fprintf(stderr, "The POST content is on the next.\n");
                    postNotFinished = true;
                }
                */
            
            }
            else {
                fprintf(stderr, "Not GET nor POST\n");
                closeFD(fd);
            }
            
          }
          // TODO: Other states.
        } else if (FD_ISSET(fd, &working_wfds)) {
          if (fd != svrfd) {
            fprintf(stderr, "Writable fd: %d\n", fd);
          }
          if (state == STATE_SEND_GET_RES_TO_BROWSER && fd == browserfd) {
            std::string tmp;
            if (loginflag) {
              tmp = "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n[\"alice\", \"Bob\"]\r\n";
              handleWrite(fd, tmp);
              closeFD(fd);
              state = STATE_WAIT_REQ_FROM_BROWSER;
            }
            else if (historyflag) {
              tmp = "HTTP/1.1 200 OK\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n[{\"From\": \"Peter\",\"To\": \"Lisa\",\"Content\": \"A\"},{\"From\": \"Lisa\",\"To\": \"Peter\",\"Content\": \"B\"}, {\"From\": \"Peter\",\"To\": \"Lisa\",\"Content\": \"C\"	}]\r\n";
              handleWrite(fd, tmp);
              closeFD(fd);
              state = STATE_WAIT_REQ_FROM_BROWSER;
            } 
            else {
              if (getRes.isHeaderWritten) {
                if (getRes.targetFile && getline (getRes.targetFile, tmp)) {
                  handleWrite(fd, tmp);
                } else {
                  // End
                  handleWrite(fd, CRLF);
                  if (getRes.targetFile) {
                    getRes.targetFile.close();
                  }
                  closeFD(fd);
                  state = STATE_WAIT_REQ_FROM_BROWSER;
                }
              } else {
                handleWrite(fd, getRes.Header());
                getRes.isHeaderWritten = true;
              }
            }
            
          } 
          else if (state == STATE_SEND_PUT_RES_TO_BROWSER && fd == browserfd) {
              // TODO: return fail status.
              handleWrite(fd, POST_RES_SUCCESS);
              closeFD(fd);
              state = STATE_WAIT_REQ_FROM_BROWSER;
              fprintf(stderr, "STATE_WAIT_REQ_FROM_BROWSER");
          } else if (state == STATE_SEND_OTHER_POST_RES_TO_BROWSER&& fd == browserfd) {
              handleWrite(fd, POST_RES_SUCCESS);
              closeFD(fd);
              state = STATE_WAIT_REQ_FROM_BROWSER;
              fprintf(stderr, "STATE_WAIT_REQ_FROM_BROWSER");

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
  }

  int handleRead(int fd) {
    bzero(buf, BUF_LEN);
    int ret = read(fd, buf, sizeof(buf));
    if (ret <= 0 && fd > 0) {
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
    if (str=="") return ret;
    if (ret != writeLen && fd > 0) {
      fprintf(stderr, "handleWrite: fd:%d closeFD\n", fd);
      closeFD(fd);
    } else {
      fprintf(stderr, "========handleWrite: fd:%d buf_last_char:%d=========\n",fd, buf[strlen(buf) - 1]);
      fprintf(stderr, "%s", buf);
      fprintf(stderr, "=============================\n");
    }
    return ret;
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

  // clientOri.initClient(ip, port_num);

  client.init(ip, port_num);
  client.work();
}
