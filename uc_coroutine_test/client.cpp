/*
 * =====================================================================================
 *
 *       Filename:  client.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  03/28/2017 11:13:10 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include <iostream>
#include <string>

#include "tcp_client_epoll.hpp"
#include "msg.pb.h"

int main(int argc, char **argv)
{
    if(argc < 3)
    {
        std::cout<<"input parameter format:cmd ip port[conn_num = 1]"<<std::endl;
        return 0;       
    }
    
    const char * ip_addr = argv[1];
    int32_t port = atoi(argv[2]);
    
    int conn_num = 1;
    if(argc == 4)
        conn_num = atoi(argv[3]);
    
    int soc;
    int * sock_array = new int[conn_num];

    TCPClientEpoll tcp_client;
    
    for(int loop_i = 0; loop_i < conn_num; ++loop_i)
    {
        if(!tcp_client.Connect(ip_addr, port, soc))
            return 0;
        sock_array[loop_i] = soc;
        
        TCPClientEpoll::SetNonBlocking(sock_array[loop_i]);
    }
    
    
    struct timeval time_out;
    
    
    if(tcp_client.EpollInit() < 0)
    {
        LOG("[ERROR]:epoll_create error, errno:%d, info:%s\n", errno, strerror(errno));
        return -1;
    }

    for(int loop_i = 0; loop_i < conn_num; ++loop_i)
    {
        tcp_client.EpollCtl(EPOLL_CTL_ADD, sock_array[loop_i]);
    }

    while(1)
    {        
        static int32_t seq = 1;


        for(int loop_i = 0; loop_i < conn_num; ++loop_i)
        {
            Msg msg;
            msg.set_info("client req");
            msg.set_idx(seq++);

            std::string send_data; 
            msg.SerializeToString(&send_data);

            ssize_t len = tcp_client.Send_NB(sock_array[loop_i], send_data.data(), send_data.size());
            if(len <= 0){
                LOG("[ERROR]:send error, errno:%d, info:%s\n", errno, strerror(errno));
                usleep(1);
                continue;
            }

            msg.Clear();
            msg.ParseFromArray(send_data.data(), send_data.size());

            LOG("send msg:info:%s, idx:%d\n", msg.info().c_str(), msg.idx());
            LOG("send msg:size:%lu\n", send_data.size());
        }
        
        epoll_event events[1024];

        int event_num = tcp_client.EpollWait(events, 1024, -1); 
        if(event_num < 0)
        {
            LOG("[ERROR]:epoll_wait error, errno:%d, info:%s\n", errno, strerror(errno));
            return -1;
        }

        //sleep(100);

        for(int i = 0; i < event_num; ++i)
        {
            // 接受数据
            char recv_data_buffer[10240];
            unsigned short recv_data_len = sizeof(recv_data_buffer);
            
            int len = tcp_client.Read_NB(events[i].data.fd, recv_data_buffer, recv_data_len);
            if(len <= 0)
            {
                LOG("[ERROR]:recv error, ret:%d, errno:%d, info:%s\n", len, errno, strerror(errno));
                return 1;
            }

            Msg msg;
            if(msg.ParseFromArray(recv_data_buffer, len) < 0)
            {
                LOG("[ERROR]:ParseFromArray failed, errno:%d, info:%s\n", errno, strerror(errno));
                return 1;
            }

            std::cout<<msg.DebugString().c_str()<<std::endl;
        }
        
    }
}
