/*
 * =====================================================================================
 *
 *       Filename:  server.cpp
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

#include "tcp_server_epoll.hpp"
#include "msg.pb.h"

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        std::cout<<"format: cmd port"<<std::endl;
        return 0;
    }

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

    for(;;)
    {
        int event_num = tcp_server.EpollWait(events, 1024, -1); 
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
                    LOG("[INFO]:close connect: fd:%d\n", events[i].data.fd);
                            
                }
                else if(ret > 0)
                {
                    LOG("[DEBUG]:receive msg from access:size:%d", ret);

                    Msg msg;
                    if(!msg.ParseFromArray(recv_buf.get(), ret))
                    {
                        LOG("[ERROR]:ParseFromArray failed!\n");
                        continue;
                    }

                    std::cout<<msg.DebugString().c_str()<<std::endl;      

                    msg.set_info("server rsp");
                    std::string rsp_msg;
                    msg.SerializeToString(&rsp_msg);

                    tcp_server.Send(events[i].data.fd, rsp_msg.c_str(), rsp_msg.size());

                    tcp_server.EpollCtl(EPOLL_CTL_DEL, events[i].data.fd);
                    close(events[i].data.fd);
                }
            }
        }
    }    
}


