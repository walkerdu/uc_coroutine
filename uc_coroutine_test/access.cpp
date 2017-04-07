/*
 * =====================================================================================
 *
 *       Filename:  access.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/28/2017 11:24:14 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  anonymalias(), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include <iostream>
#include <string>

#include <memory>

#include "tcp_client_epoll.hpp"
#include "tcp_server_epoll.hpp"

#include "msg.pb.h"

#include "../uc_coroutine_pool.h"

struct Task
{
    UCCoroutine *coro;
    int fd;
    std::string client_req;
    std::string client_res;
};

std::map<uint32_t, Task> task_map;
std::map<int32_t, uint32_t> fd_seq_map;

UCCoroutinePool coro_pool(1024, 512, 1024000, true);

//request to server
TCPClientEpoll tcp_client;

char server_addr[16] = {};
int server_port = 0;

int SendRecv(uint32_t seq)
{
    if(task_map.count(seq) == 0)
    {
        LOG("[ERROR]:task_map has not this seq:%u\n", seq);
        return -1;
    }

    int soc = 0;
    if(!tcp_client.Connect(server_addr, server_port, soc))
    {
        LOG("[ERROR]:Connect failed, errno:%d, info:%s\n", errno, strerror(errno));
        return -1;
    }

    TCPClientEpoll::SetNonBlocking(soc);
    tcp_client.EpollCtl(EPOLL_CTL_ADD, soc);


    int len = tcp_client.Send_NB(soc, task_map[seq].client_req.data(), task_map[seq].client_req.size());
    if(len <= 0){
        LOG("[ERROR]:send error, errno:%d, info:%s\n", errno, strerror(errno));
        return -1;
    }

    fd_seq_map[soc] = seq;

    LOG("[DEBUG]:before yield: soc:%d, seq:%u\n", soc, seq);

    task_map[seq].coro->Yield_S();

    LOG("[DEBUG]:after yield soc:%d, seq:%u\n", soc, seq);

    char recv_data_buffer[1024];

    len = tcp_client.Read_NB(soc, recv_data_buffer, 1024);
    if(len <= 0)
    {
        LOG("[ERROR]:recv error, errno:%d, info:%s\n", errno, strerror(errno));
        return 1;
    }

    task_map[seq].client_res.assign(recv_data_buffer, len);

    return 0;
}

int main(int argc, char **argv)
{
    if(argc != 4)
    {
        std::cout<<"format: cmd listen_port server_addr server_port"<<std::endl;
        return 0;
    }

    strcpy(server_addr, argv[2]);
    server_port = atoi(argv[3]);

    TCPServerEpoll tcp_server;

    int listen_soc;
    if(!tcp_server.Listen(NULL, atoi(argv[1]), listen_soc))
        return 0;

    tcp_server.SetNonBlocking(listen_soc);

    //Since Linux 2.6.8, the size argument is unused, but must be greater  than  zero
    if(tcp_server.EpollInit() < 0)
    {
        LOG("[ERROR]:epoll_create error, errno:%d, info:%s\n", errno, strerror(errno));
        return -1;
    }

    tcp_server.EpollCtl(EPOLL_CTL_ADD, listen_soc);

    epoll_event events[1024];
    std::shared_ptr<char> recv_buf(new char[1024]);

    uint32_t unique_seq = 1;

    if(tcp_client.EpollInit() < 0)
    {
        LOG("[ERROR]:epoll_create error, errno:%d, info:%s\n", errno, strerror(errno));
        return -1;
    }
    

    for(;;)
    {

        int event_num = tcp_server.EpollWait(events, 1024, 100); 
        if(event_num < 0)
        {
            LOG("[ERROR]:epoll_wait error, errno:%d, info:%s\n", errno, strerror(errno));
            continue;
            //return -1;
        }
            

        for(int i = 0; i < event_num; ++i)
        {
            if(events[i].data.fd == listen_soc)
            {
                int ret = tcp_server.Accept();
                if(ret)
                {
                    TCPServerEpoll::SetNonBlocking(ret);
                    
                    tcp_server.EpollCtl(EPOLL_CTL_ADD, ret);
                }

                continue;
            }        

            if(events[i].events & EPOLLIN)
            {
                int ret = tcp_server.Read(events[i].data.fd, recv_buf.get(), 1024);
            
                if(ret == 0)
                {
                    tcp_server.EpollCtl(EPOLL_CTL_DEL, events[i].data.fd);
                    close(events[i].data.fd);
                    LOG("[INFO]:close connect: fd:%d\n",events[i].data.fd);
                            
                }
                else if(ret > 0)
                {
                    LOG("[DEBUG]:recv from client:size:%d", ret);

                    UCCoroutine * coro = coro_pool.Post(std::bind(SendRecv, unique_seq));

                    Task task = {coro, events[i].data.fd, std::string(recv_buf.get(), ret)};
                    task_map[unique_seq++] = task;

                    coro->Resume_S();
                }
            }
        }

        event_num = tcp_client.EpollWait(events, 1024, 100); 
        if(event_num < 0)
        {
            LOG("[ERROR]:epoll_wait error, errno:%d, info:%s\n", errno, strerror(errno));
            continue;
            //return -1;
        }

        for(int i = 0; i < event_num; ++i)
        {
            if(events[i].events & EPOLLIN)
            {
                uint32_t seq = fd_seq_map[events[i].data.fd];
                if(task_map.count(seq) == 0)
                {
                    LOG("[ERROR]:can not find seq:fd:%d, seq_fd:%d\n", events[i].data.fd, task_map[seq].fd);
                    continue;
                }

                Task & task = task_map[seq];

                task.coro->Resume_S();

                tcp_server.Send(task.fd, task.client_res.data(), task.client_res.size());

                tcp_client.EpollCtl(EPOLL_CTL_DEL, events[i].data.fd);
                close(events[i].data.fd);
                //tcp_server.EpollCtl(EPOLL_CTL_DEL, task.fd);
                //close(task.fd);
            }

        }

        //LOG_DEBUG("loop");
    }    
}


