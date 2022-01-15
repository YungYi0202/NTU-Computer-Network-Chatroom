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
#include <vector>
#include <sstream>
#include <string>

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
#define POST_RES_SUCCESS "HTTP/1.1 200 OK\r\n\r\n"
#define POST_RES_ERROR "HTTP/1.1 404 Not Found\r\n\r\n"
#define DEFAULTUSERNAME "default"


#define DIR_MODE (FILE_MODE | S_IXUSR | S_IXGRP | S_IXOTH)
#define FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

#define ACK "1"
enum {
  STATE_UNUSED,
  STATE_WAIT_REQ_FROM_BROWSER, 
  STATE_SEND_GET_REQ_TO_SVR, 
  STATE_SEND_GET_LEN_ACK_TO_SVR, 
  STATE_WAIT_GET_LEN_FROM_SVR, 
  STATE_WAIT_GET_RES_FROM_SVR, 
  STATE_SEND_GET_RES_TO_BROWSER,
  
  STATE_SEND_PUT_REQ_TO_SVR, 
  STATE_WAIT_PUT_ACK_FROM_SVR,
  STATE_SEND_PUT_LEN_TO_SVR,
  STATE_WAIT_PUT_LEN_ACK_FROM_SVR, 
  STATE_SEND_PUT_CONTENT_TO_SVR,
  STATE_SEND_PUT_RES_TO_BROWSER,

  STATE_SEND_OTHER_POST_REQ_TO_SVR,
  STATE_SEND_OTHER_POST_RES_TO_BROWSER
};

std::string contentType(std::string target) {
    std::string type;
    std::size_t dotPos=target.find('.');
    if (dotPos != std::string::npos) {
      type = target.substr(dotPos);
      if (type == ".html") return "text/html";
      if (type == ".ico") return "image/vnd.microsoft.icon";
      if (type == ".jpg" || type == ".jpeg") return "image/jpeg";
      if (type == ".css") return "text/css";
      if (type == ".scss") return "text/x-scss";
      if (type == ".js") return "text/javascript";
      // TODO:Add other type
    }  
    return "text/plain";
}

std::string httpGetHeader(std::string contentType, std::string statusCode = "200 OK") {
  return "HTTP/1.1 " + statusCode + "\r\nContent-Type: " + contentType + "; charset=UTF-8\r\n\r\n";
}



class Client {
public:  // TODO: modify access
  int state = STATE_WAIT_REQ_FROM_BROWSER;
  std::string username = DEFAULTUSERNAME;
  // bool hasUsername = false;
  std::string requestToSvr;
  std::string fileContentPutToSvr;
  int responseLenFromSvr = 0;
  std::string responseTypeToBrowser;
  bool headerSent = false;

  static const int maxfd = MAXFD;
  int svrfd;
  int browserfd;
  int sockfd;
  // browser browsers[maxfd];
  fd_set master_rfds, working_rfds, master_wfds, working_wfds;
  char buf[BUF_LEN];
  
  void init(char *ip, int svr_port_num, int cli_port_num) {
    /* To talk with server.cpp */
    mkdir("client_dir", DIR_MODE);

    struct sockaddr_in addr_in;
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(svr_port_num);
    addr_in.sin_addr.s_addr = inet_addr(ip);

    svrfd = socket(AF_INET, SOCK_STREAM, 0);
    if (svrfd >= 0) {
      int x = connect(svrfd, (struct sockaddr *)&addr_in, sizeof(addr_in));
      if (x != 0) {
        close(svrfd);
        fprintf(stderr, "No corresponding server.cpp.\n");
      } else {
        fprintf(stderr, "Server fd:%d.\n", svrfd);

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
    svr_addr.sin_port = htons(cli_port_num);

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
    FD_SET(svrfd, &master_wfds);
    FD_SET(svrfd, &master_rfds);
    
    struct sockaddr_in client_addr;
    int clifd;
    int clilen = sizeof(client_addr);
    
    while (1) {
      memcpy(&working_rfds, &master_rfds, sizeof(master_rfds));
      memcpy(&working_wfds, &master_wfds, sizeof(master_wfds));
      if (select(maxfd, &working_rfds, &working_wfds, NULL, NULL) < 0) {
        ERR_EXIT("select failed");
      }
      // fprintf(stderr, "select");
      
      if (FD_ISSET(sockfd, &working_rfds)) {
        if ((clifd = accept(sockfd, (struct sockaddr *)&client_addr, (socklen_t *)&clilen)) < 0) {
          ERR_EXIT("accept");
          continue;
        }
        FD_SET(clifd, &master_rfds);
        fprintf(stderr, "=============================\n");
        fprintf(stderr, "Accept! client fd: %d\n", clifd);
        fprintf(stderr, "=============================\n");
      }

      for (int fd = 0; fd < maxfd; fd++) {
        if (fd == sockfd) continue;

        if (FD_ISSET(fd, &working_rfds)) {
          /** Read **/
          if (state == STATE_WAIT_REQ_FROM_BROWSER && fd != svrfd) {
            browserfd = fd;
            std::stringstream ss;
            do {
              handleRead(fd);
              std::string req(buf);
              ss << req;
              fprintf(stderr, "strlen(buf): %lu\n",strlen(buf));
            } while (strlen(buf) == BUF_LEN && buf[BUF_LEN-1] != '\n');
            FD_SET(fd, &master_wfds);

            /* To browser */
            std::string command, target;
            ss >> command >> target;
            fprintf(stderr, "command: %s target:%s\n",command.c_str(), target.c_str());
            if (command == "GET") { 
              handleGetReqFromBrowser(target);
              state = STATE_SEND_GET_REQ_TO_SVR;
              // Directly return the header
              handleWrite(fd, httpGetHeader(responseTypeToBrowser));
              fprintf(stderr, "check svrfd writable: %d %d\n", FD_ISSET(svrfd, &working_wfds), FD_ISSET(svrfd, &master_wfds));
            } else if (command == "POST") {
                std::string tmp = ss.str();
                // int contentLen = 0;
                int pos;
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

                        requestToSvr = "put " +  username + " " + friendname + " " + filename;
                        // contentLen -= pos + strlen(CRLF);
                        state = STATE_SEND_PUT_REQ_TO_SVR;
                    } else {
                        /* other post request */
                        fprintf(stderr, "%s", tmp.c_str());
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
                        state = STATE_SEND_OTHER_POST_REQ_TO_SVR;
                    }  
                } else {
                    fprintf(stderr, "The POST content is empty.\n");
                    handleWrite(fd, POST_RES_ERROR);
                    closeFD(fd);
                }
            }
            else {
              fprintf(stderr, "Not GET nor POST\n");
              handleWrite(fd, POST_RES_ERROR);
              closeFD(fd);
            }
          } // end state == STATE_WAIT_REQ_FROM_BROWSER
          else if (state == STATE_WAIT_GET_LEN_FROM_SVR && fd == svrfd) {
            handleRead(fd);
            responseLenFromSvr = atoi(buf);
            state = STATE_SEND_GET_LEN_ACK_TO_SVR;
            fprintf(stderr, "responseLenFromSvr:%d\n", responseLenFromSvr);
          }
          else if (state == STATE_WAIT_GET_RES_FROM_SVR && fd == svrfd) {
            handleRead(fd);
            state = STATE_SEND_GET_RES_TO_BROWSER;
          } else if (state == STATE_WAIT_PUT_ACK_FROM_SVR && fd == svrfd) {
            handleRead(fd);
            if (std::string(buf) != ACK) {
                fprintf(stderr, "Should rcv ACK from server, but rcv %s\n", buf);
            }
            state = STATE_SEND_PUT_LEN_TO_SVR;
          } else if (state == STATE_WAIT_PUT_LEN_ACK_FROM_SVR && fd == svrfd) {
            handleRead(fd);
            if (std::string(buf) != ACK) {
                fprintf(stderr, "Should rcv ACK from server, but rcv %s\n", buf);
            }
            state = STATE_SEND_PUT_CONTENT_TO_SVR;
          }
          // TODO: Other read states.
        } else if (FD_ISSET(fd, &working_wfds)) {
          /** Write **/
            if (fd == svrfd) {
                if (state == STATE_SEND_GET_REQ_TO_SVR) {
                    handleWrite(svrfd, requestToSvr);
                    state = STATE_WAIT_GET_LEN_FROM_SVR;
                } 
                else if (state == STATE_SEND_GET_LEN_ACK_TO_SVR) {
                    handleWrite(svrfd, ACK);
                    state = STATE_WAIT_GET_RES_FROM_SVR;
                }  
                else if (state == STATE_SEND_PUT_REQ_TO_SVR) {                 
                    handleWrite(svrfd, requestToSvr);
                    state = STATE_WAIT_PUT_ACK_FROM_SVR;
                    fprintf(stderr, "STATE_WAIT_PUT_ACK_FROM_SVR\n");
                }
                else if (state == STATE_SEND_PUT_LEN_TO_SVR) {
                    char lenStr[10];
                    sprintf(lenStr, "%lu", fileContentPutToSvr.size());                  
                    handleWrite(svrfd, std::string(lenStr));
                    state = STATE_WAIT_PUT_LEN_ACK_FROM_SVR;
                } 
                else if (state == STATE_SEND_PUT_CONTENT_TO_SVR) {
                    handleWrite(svrfd, fileContentPutToSvr);
                    state = STATE_SEND_PUT_RES_TO_BROWSER;
                } 
                else if (state == STATE_SEND_OTHER_POST_REQ_TO_SVR) {
                    handleWrite(svrfd, requestToSvr);
                    state = STATE_SEND_OTHER_POST_RES_TO_BROWSER;
                }
            } 
            else if (fd == browserfd) {
                if (state == STATE_SEND_GET_RES_TO_BROWSER) {
                  int ret = handleWrite(fd);
                  responseLenFromSvr -= ret;
                  if (responseLenFromSvr <= 0) {
                      handleWrite(fd, CRLF);
                      closeFD(fd);
                      state = STATE_WAIT_REQ_FROM_BROWSER;
                  } else {
                      state = STATE_WAIT_GET_RES_FROM_SVR;
                  }
                }
                else if (state == STATE_SEND_PUT_RES_TO_BROWSER) {
                    // TODO: return fail status.
                    handleWrite(fd, POST_RES_SUCCESS);
                    closeFD(fd);
                    state = STATE_WAIT_REQ_FROM_BROWSER;
                } else if (state == STATE_SEND_OTHER_POST_RES_TO_BROWSER) {
                    handleWrite(fd, POST_RES_SUCCESS);
                    closeFD(fd);
                    state = STATE_WAIT_REQ_FROM_BROWSER;
                }
                // TODO: Other write states to browser
            }
        }     
      }
    }
  }

  void handleGetReqFromBrowser(std::string target) {
    if (target.size() > 2 && target[1] == '?') {
      target = target.substr(2); //rm "/?".
      std::size_t eqPos=target.find('=');
      if (eqPos == std::string::npos || eqPos == target.size() - 1) {
        ERR_EXIT("GET TARGET WRONG FORMAT: no \"=\" or no string after \"=\"");
      } else {
        std::string cmd = target.substr(0, eqPos);
        std::string name = target.substr(eqPos+1);
        if (cmd == "login") {
          username = name;
          requestToSvr = "ls " + username;
        } else if (cmd == "history") {
          requestToSvr = "history " + username + " " + name; // name is friendname.
        } else {
          ERR_EXIT("GET TARGET WRONG FORMAT: <command>=<content>, command is wrong.");
        }
      }
    } else {
      if (target == "/") target = "/index2.html";
      if (target.size() > 1 && target[0] == '/') target = target.substr(1);
      responseTypeToBrowser = contentType(target);
      /** Handle Special Case **/
      if (target == "index2.html" || target == "index2.css" || target == "main2.js") {
        requestToSvr = "get " + std::string(DEFAULTUSERNAME) + " " + target;
      } else {
        requestToSvr = "get " + username + " " + target;
      }
    }
  }

  void closeFD(int fd) {
    fprintf(stderr, "closeFD: %d\n", fd);
    
    close(fd);
    if (fd != svrfd) {
      FD_CLR(fd, &master_rfds);  //?
      FD_CLR(fd, &master_wfds);  //?
      responseLenFromSvr = 0;
      headerSent = false;
    }
  }

  int handleRead(int fd) {
    bzero(buf, BUF_LEN);
    int ret = read(fd, buf, sizeof(buf));
    if (ret <= 0 && fd > 0) {
    //   fprintf(stderr, "handleRead: fd:%d closeFD\n", fd);
      closeFD(fd);
    } else {
      fprintf(stderr, "========handleRead: fd:%d=========\n", fd);
      fprintf(stderr, "%s", buf);
      fprintf(stderr, "=============================\n");
    }
    return ret;
  }

  int handleWrite(int fd, std::string str="") {
    int ret, writeLen;
    std::string tmp;
    if (str=="") {
      ret = _handleWrite(fd);
    } else {
      while (str.size()) {
        writeLen = (str.size() < BUF_LEN)? str.size(): BUF_LEN;
        tmp = str.substr(0, writeLen);
        str = str.substr(writeLen);
        sprintf(buf, "%s", tmp.c_str());
        ret = _handleWrite(fd);
      }
    }
    return ret;
  }

  int _handleWrite(int fd) {
    int writeLen = strlen(buf);
    int ret = write(fd, buf, writeLen);
    if (ret != writeLen && fd > 0) {
    //   fprintf(stderr, "handleWrite: fd:%d closeFD\n", fd);
      closeFD(fd);
      return ret;
    } else {
      fprintf(stderr, "========handleWrite: fd:%d buf_last_char:%d=========\n",
              fd, buf[strlen(buf) - 1]);
      fprintf(stderr, "%s", buf);
      fprintf(stderr, "=============================\n");
    }
    return ret;
  }
} client;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: ./client [ip:port] port\n");
    exit(1);
  }

  char ip[32], port[32];
  char *pch;
  pch = strtok(argv[1], ":");
  strcpy(ip, pch);
  pch = strtok(NULL, ":");
  strcpy(port, pch);

  uint16_t svr_port_num = 0;
  int port_l = strlen(port);
  for (int i = 0; i < port_l; i++) svr_port_num = svr_port_num * 10 + port[i] - '0';

  client.init(ip, svr_port_num, atoi(argv[2]));
  client.work();
}
