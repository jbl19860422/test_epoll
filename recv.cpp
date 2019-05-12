#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include <condition_variable>
#include <iostream>

bool g_exit = false;
void sigint_handler(int signo)
{
    g_exit = true;
}

int main(char argc, char *argv[])
{
    struct sigaction act, oldact;
    act.sa_handler = sigint_handler;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGINT);
    sigaction(SIGINT, &act, &oldact);

    int listen_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    int reuseaddr = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1234);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        std::cout << "bind sock error" << std::endl;
        if (errno == EADDRINUSE)
        {
            std::cout << "addr already in use" << std::endl;
        }
        return -1;
    }

    int flag = fcntl(listen_sock, F_GETFL);
    flag |= O_NONBLOCK;
    fcntl(listen_sock, F_SETFL, flag);

    int ret = listen(listen_sock, 0);
    if (0 != ret)
    {
        if (errno == EADDRINUSE)
        {
            std::cout << "listen to socket error" << std::endl;
        }
        return -2;
    }

    int epoll_fd = epoll_create(1);
    struct epoll_event ev, evts[1024];
    ev.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    ev.data.fd = listen_sock;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_sock, &ev);

    while (1)
    {
        int nfd = epoll_wait(epoll_fd, evts, 1024, 100);
        if (nfd == -1)
        {
            if (errno == EINTR)
            {
                if (g_exit)
                {
                    break;
                }
                continue;
            }
            else
            {
                std::cout << "epoll error, errno=" << errno << std::endl;
                break;
            }
        }
        else if (nfd > 0)
        {
            for (int i = 0; i < nfd; i++)
            {
                if ((evts[i].events & EPOLLERR) || (evts[i].events & EPOLLRDHUP) || (evts[i].events & EPOLLHUP))
                {
                    if (evts[i].data.fd == listen_sock)
                    {
                        std::cout << "listen sock epoll error" << std::endl;
                        break;
                    }
                    else
                    {
                        if (0 != epoll_ctl(epoll_fd, EPOLL_CTL_DEL, evts[i].data.fd, NULL))
                        {
                            std::cout << "remove sock from epoll failed." << std::endl;
                        } 
                        else 
                        {
                            std::cout << "remove sock from epoll succeed." << std::endl;
                        }
                    }
                }

                if (evts[i].events & EPOLLIN)
                {
                    if (evts[i].data.fd == listen_sock)
                    { //进行连接操作
                        std::cout << "new acceptable sock" << std::endl;
                        // int accept_sock = accept4(listen_sock, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
                        int accept_sock = accept(listen_sock, NULL, NULL);

                        if (-1 == accept_sock)
                        {
                            if (EINTR == errno)
                            { //因为系统中断导致的错误，ET模式最好重新加入队列
                                struct epoll_event ev;
                                ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
                                ev.data.fd = listen_sock;
                                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, listen_sock, &evts[i]);
                            }
                            else
                            {
                                std::cout << "accept failed." << std::endl;
                            }
                        }
                        else if (accept_sock > 0)
                        {
                            int flag = fcntl(accept_sock, F_GETFL);
                            flag |= O_NONBLOCK;
                            fcntl(accept_sock, F_SETFL, flag);

                            struct epoll_event ev;
                            ev.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
                            ev.data.fd = accept_sock;
                            if (0 != epoll_ctl(epoll_fd, EPOLL_CTL_ADD, accept_sock, &ev))
                            {
                                std::cout << "accpet sock add to epoll failed" << std::endl;
                            }
                            else
                            {
                                std::cout << "accept sock succeed, fd=" << accept_sock << std::endl;
                            }
                        }
                    } else {
                        //读取socket，调用fd绑定的处理对象处理
                    }
                }
                else if (evts[i].events & EPOLLOUT)
                {
                    //回调事件对象的写函数，事件对象自己将写的数据写出去
                }
            }
        }
        else
        {
            if (g_exit)
            {
                break;
            }
        }
    }
    close(epoll_fd);
    std::cout << "program exit succeed." << std::endl;
    return 0;
}
