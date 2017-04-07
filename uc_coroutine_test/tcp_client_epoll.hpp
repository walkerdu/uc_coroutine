/*
 * =====================================================================================
 *
 *       Filename:  tcp_client_epoll.hpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/28/2017 12:25:20 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  anonymalias(), 
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef _TCP_CLIENT_EPOLL_H_
#define _TCP_CLIENT_EPOLL_H_

#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "../../common/util/log.hpp"

class TCPClientEpoll
{
public:
    TCPClientEpoll() : epoll_fd(-1)
    {}

public:

    bool Connect(const char* ip, int port, int& ret_socket)
    {
        ret_socket = socket(AF_INET, SOCK_STREAM, 0);
        if(ret_socket < 0)
        {
            LOG("[ERROR]:errno:%d, info:%s\n", errno, strerror(errno));
            return false;
        }

        sockaddr_in ser_addr;
        bzero(&ser_addr, sizeof(ser_addr));
        ser_addr.sin_family = AF_INET;
        ser_addr.sin_addr.s_addr = inet_addr(ip);
        ser_addr.sin_port=htons(port);

        int ret = connect(ret_socket, (sockaddr*)&ser_addr, sizeof(sockaddr));
        if(ret < 0)
        {
            LOG("[ERROR]:errno:%d, info:%s\n", errno, strerror(errno));
            close(ret_socket);
            return false;
        }

        //int flags = fcntl(ret_socket, F_GETFL, 0);
        //if(fcntl(ret_socket, F_SETFL, flags | O_NONBLOCK) < 0)
        //{
        //  perror("fcntl");
        //  return false;
        //}

        return true;
    }

    int Read(int soc, char *recv_buf, int buf_size)
    {
        if(soc <= 0 || recv_buf == NULL || buf_size <= 0)
        {
            LOG("[ERROR]:input parameter error\n");
            return -1;
        }

        int recv_len = recv(soc, recv_buf, buf_size, 0);
        if(recv_len < 0)
        {
            LOG("[ERROR]:errno:%d, info:%s\n", errno, strerror(errno));
            return -1;
        }

        return recv_len;
    }

    int Send(int soc, const char *data_buf, int data_len)
    {
        if(soc <= 0 || data_buf == NULL || data_len < 0)
        {
            LOG("[ERROR]:input param error...\n");
            return -1;
        }

        int send_len = send(soc, data_buf, data_len, 0); 
        if(send_len < 0)
        {
            LOG("[ERROR]:errno:%d, info:%s\n", errno, strerror(errno));
            return -1;
        }

        return send_len;
    }

    int Read_NB(int soc, char *recv_buf, int buf_size)
    {
        if(soc <= 0 || recv_buf == NULL || buf_size <= 0)
        {
            LOG("[ERROR]:input parameter error\n");
            return -1;
        }

        int remain_buf_size = buf_size;

        while(remain_buf_size)
        {
            int recv_len = recv(soc, recv_buf + buf_size - remain_buf_size, remain_buf_size, 0);
            if(recv_len < 0)
            {
                if(errno == EINTR)
                {//system call interrupt
                    continue;
                }

                if(errno == EAGAIN || errno == EWOULDBLOCK)
                {//fd not ready, read finis
                    return buf_size - remain_buf_size;
                }

                LOG("[ERROR]:errno:%d, info:%s\n", errno, strerror(errno));
                return -1;
            }
            else if(recv_len == 0)
            {
                //return 0;
                return buf_size - remain_buf_size;
            }

            remain_buf_size -= recv_len;
        }

        return 0;
    }

    int Send_NB(int soc, const char *data_buf, int data_len)
    {
        if(soc <= 0 || data_buf == NULL || data_len < 0)
        {
            LOG("[ERROR]:input param error...\n");
            return -1;
        }

        char * send_ptr = const_cast<char*>(data_buf);
        int remain_size = data_len;

        while(1)
        {

            int send_size = send(soc, send_ptr, remain_size, 0); 
            if(send_size != remain_size)
            {
                if(send_size < 0)
                {
                    if(EINTR == errno)
                    {
                        continue;
                    }
                    else if(errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        continue;
                    }

                    LOG("[ERROR]:send error, errno:%d, info:%s\n", errno, strerror(errno));
                    return -1;
                }

                send_ptr += send_size;
                remain_size -= send_size;
                continue;

            }

            break;
        }

        LOG("[TRACE]:send to client\n");

        return data_len;
    }

    static int SetNonBlocking(int soc)
    {
        int flags = fcntl(soc, F_GETFL, 0);
        if(fcntl(soc, F_SETFL, flags | O_NONBLOCK) < 0) 
        {
            LOG("[ERROR]:errno:%d, info:%s\n", errno, strerror(errno));
            return -1;
        }
        
        return 0;
    }

    int EpollInit()
    {
        //Since Linux 2.6.8, the size argument is unused, but must be greater  than  zero
        epoll_fd = epoll_create(1024);
        if(epoll_fd < 0)
        {
            LOG("[ERROR]:epoll_create error, errno:%d, info:%s\n", errno, strerror(errno));
            return -1;
        }

        return 0;
    }

    int EpollCtl(int epoll_mod, int fd)
    {
        epoll_event ev;
        ev.data.fd = fd; 
        ev.events = EPOLLIN;
        epoll_ctl(epoll_fd, epoll_mod, fd, &ev);
    }


    int EpollWait(struct epoll_event * events, int32_t max_events, int32_t timeout)
    {
        int event_num = epoll_wait(epoll_fd, events, max_events, timeout); 
        if(event_num < 0)
        {
            LOG("[ERROR]:epoll_wait error, errno:%d, info:%s\n", errno, strerror(errno));
        }

        return event_num;
    }

private:
    int epoll_fd;
};

#endif
