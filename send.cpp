#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/epoll.h>

#include <condition_variable>
#include <iostream>

int createSocket(const std::string ip, unsigned short port)
{
    int sock = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (sock < 0)
    {
        std::cout << "create sock error" << std::endl;
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    int ret = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (ret == -1)
    {
        if (errno == ECONNREFUSED)
        {
            std::cout << "connect refused by server" << std::endl;
        } else
        {
            std::cout << "connect failed, errno = " << errno << ", errmsg = " << strerror(errno) << std::endl;
        }
        
        return -1;
    }
    return sock;
}

int main(char argc, char *argv[])
{
    if(argc < 4) {
        std::cout << "Usage:" << argv[0] << " server_ip, server_port, conn_count" << std::endl;
        return -1;
    }

    int conn_count = 0;
    std::string server_ip(argv[1]);
    unsigned short server_port = 0;
    try {
        conn_count = std::stoul(argv[3]); 
        server_port = std::stoi(argv[2]);
    } catch(...) {
        std::cout << "stoul failed" << std::endl;
        return -2;
    }

    for(int i = 0; i < conn_count; i++) {
        if(createSocket(server_ip, server_port) > 0) {
            std::cout << "connect to server succ, i=" << i << std::endl;
        } else {
            std::cout << "connect to server failed." << std::endl;
        }
    }
    getchar();
}