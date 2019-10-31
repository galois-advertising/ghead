#include <iostream>
#include <string>
#include <fstream>
#include <sys/types.h>    /* basic system data types */
#include <sys/socket.h>    /* basic socket definitions */
#include <sys/time.h>    /* timeval{} for select() */
#include <time.h>        /* timespec{} for pselect() */
#include <netinet/in.h>    /* sockaddr_in{} and other Internet defns */
#include <arpa/inet.h>    /* inet(3) functions */
#include <errno.h>
#include <fcntl.h>        /* for nonblocking */
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>    /* for S_xxx file mode constants */
#include <sys/uio.h>        /* for iovec{} and readv/writev */
#include <unistd.h>
#include <sys/wait.h>
#include <sys/un.h>    
#include "ghead.h"
#include <thread>
using namespace galois::ghead;

void read(uint8_t * buf)
{
    std::fstream  afile;
    afile.open("./client_ghead.txt", std::ios::in);
    if (afile.is_open() == false) {
        std::cout<<"Cannot find ./client_ghead.txt"<<std::endl;
        return;
    }
    ghead * phead = reinterpret_cast<ghead*>(buf);
    afile>>phead->id>>phead->version>>phead->log_id;
    afile>>phead->provider>>phead->magic_num;
    afile>>phead->reserved1>>phead->reserved2>>phead->reserved3;
    afile>>phead->body_len;
    afile>>(buf+ sizeof(ghead));
    *(buf + sizeof(ghead) + phead->body_len) = '\0';

}


int main(int argc, char * argv[])
{
    uint8_t buf[10240];
    read(buf);
    std::cout<<reinterpret_cast<char*>(buf + sizeof(ghead))<<std::endl;

    int sockfd;
    struct sockaddr_in servaddr;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8707);
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
    connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr));
    write(sockfd, buf, sizeof(ghead));
    for (int i = 0; i < 10; i++) {
        //char ch = getchar();
        //char ch = 'c';
        //write(sockfd, &ch, 1);
        write(sockfd, "cccccccccc", 10);
    };
    //std::cout<<"out"<<std::endl;
    //char receive[1024];
    //ghead * p = reinterpret_cast<ghead*>(receive);
    //auto res = ghead::gread(sockfd, reinterpret_cast<ghead*>(receive), sizeof(1024), 10000);
    //std::cout<<res<<std::endl;
    //if (res > 0) {
    //    receive[sizeof(ghead) + p->body_len] = 0;
    //    std::cout<<p->body<<std::endl;
    //}
    close(sockfd);
    return 0;
}