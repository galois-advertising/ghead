#include <iostream>
#include <string>
#include <fstream>
#include "ghead.h"

void read(uint8_t * buf)
{
    std::fstream  afile;
    afile.open("./client_ghead.txt", std::ios::in);
    if (afile.is_open() == false) {
        std::cout<<"Cannot find ./client_ghead.txt"<<std::endl;
        return;
    }
    galois::ghead::ghead * phead = reinterpret_cast<galois::ghead::ghead*>(buf);
    afile>>phead->id>>phead->version>>phead->log_id;
    afile>>phead->provider>>phead->magic_num;
    afile>>phead->reserved1>>phead->reserved2>>phead->reserved3;
    afile>>phead->body_len;
    afile>>(buf+ sizeof(galois::ghead::ghead));
    *(buf + sizeof(galois::ghead::ghead) + phead->body_len) = '\0';

}


int main(int argc, char * argv[])
{
    uint8_t buf[10240];
    read(buf);
    std::cout<<reinterpret_cast<char*>(buf + sizeof(galois::ghead::ghead))<<std::endl;
    
    return 0;
}