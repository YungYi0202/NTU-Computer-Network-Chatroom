#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <err.h>
#include <strings.h>
#include <string>
#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)
#define BUF_LEN 1024

std::string response = "HTTP/1.1 200 OK\r\n"
"Content-Type: text/html; charset=UTF-8\r\n\r\n"
"<!DOCTYPE html><html><head><title>Bye-bye baby bye-bye</title>"
"<style>body { background-color: #111 }"
"h1 { font-size:4cm; text-align: center; color: black;"
" text-shadow: 0 0 2mm red}</style></head>"
"<body><h1>Goodbye, world!</h1></body></html>\r\n";

typedef struct {
    int fd;  // fd to talk with client
    char username[10];
} client;

// Global variables.
client* clients = NULL;  // List of clients 
fd_set master_RDset, working_RDset; 
int max_fd;
char buf [BUF_LEN];

static void init_client(client* cli) {
    cli->fd = -1;
    bzero(cli->username, sizeof(cli->username));
}

static void init_clients_table(int table_size) {
    // Get file descripter table size and initize request table
    clients = (client*) malloc(sizeof(client) * table_size);
    if (clients == NULL) {
        ERR_EXIT("out of memory allocating all clients");
    }
    for (int i = 0; i < table_size; i++) {
        init_client(&clients[i]);
    }
} 

static void free_client(client* cli) {
    init_client(cli);
}

void closeFD(client* cli){
    fprintf(stderr, "closeFD: %d\n", cli->fd);
    FD_CLR(cli->fd, &master_RDset);   //?
    FD_CLR(cli->fd, &working_RDset);   //?
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
    int writeLen = (isHTTP)? strlen(buf) - 1: strlen(buf);
    int ret = write(fd, buf, writeLen);
    if (ret != writeLen && fd > 0 && clients[fd].fd != -1) {
        fprintf(stderr, "handle_write: fd:%d closeFD\n", fd);
        closeFD(&clients[fd]);
    } else {
        fprintf(stderr, "handle_write: fd:%d str:%s\n", fd, str.c_str());
    }
    return ret;
}

int main()
{
    max_fd = getdtablesize();

    // init server
    int one = 1;
    struct sockaddr_in svr_addr;
   
    
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
        err(1, "can't open socket");
    
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int));
    
    int port = 8080;
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = INADDR_ANY;
    svr_addr.sin_port = htons(port);
    
    if (bind(sock_fd, (struct sockaddr *) &svr_addr, sizeof(svr_addr)) == -1) {
        close(sock_fd);
        err(1, "Can't bind");
    }
    
    listen(sock_fd, max_fd);

    init_clients_table(max_fd);
    clients[sock_fd].fd = sock_fd;
    
    FD_ZERO(&master_RDset); 
    FD_SET(sock_fd, &master_RDset);   

    struct sockaddr_in client_addr;
    int cli_fd;
    int clilen = sizeof(client_addr);
    
    client* clientP;        // Pointer to a specific client.


    while (1) {
        memcpy(&working_RDset, &master_RDset, sizeof(master_RDset));
        select(max_fd, &working_RDset, NULL, NULL, NULL);
        if (FD_ISSET(sock_fd, &working_RDset)) {
            if ((cli_fd = accept(sock_fd, (struct sockaddr*)& client_addr, (socklen_t*)&clilen)) < 0) {
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