/*
 * echo-server.cpp
 *
 *  Created on: Jun 21, 2019
 *      Author: frank
 */
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include "asio.hpp"
#include "coropp.h"

using namespace CoroPP;
using namespace std::chrono_literals;

void Echo(asio::ip::tcp::socket socket_)
{
    auto socket = std::make_shared<asio::ip::tcp::socket>(std::move(socket_));
    auto f = [socket](CoroID id) {
        bool stopped = false;
        while(!stopped) {
            char buff[256];
            bzero(buff, sizeof(buff));
            size_t recv_len = 0;
            socket->async_read_some(asio::buffer(buff), [id, &stopped, &buff, &recv_len](const asio::error_code& err, size_t len) {
                if(err) {
                    std::cout<<"read err: "<<err.message()<<"\n";
                    stopped = true;
                } else {
                    recv_len = len;
                    std::cout<<"recved "<<len<<":"<<std::string(buff, len)<<"\n";
                }
                Coro* coro = CoroPool::Instance().FindCoro(id);
                if(!coro) {
                    std::cout<<"coro not found\n";
                    stopped = true;
                } else {
                    coro->Resume();
                }
            });
            Yield();
            asio::async_write(*socket, asio::buffer(buff, recv_len), [&stopped, id](const asio::error_code& err, size_t len) {
                if(err) {
                    std::cout<<"send err: "<<err.message()<<"\n";
                    stopped = true;
                }
                Coro* coro = CoroPool::Instance().FindCoro(id);
                if(!coro) {
                    std::cout<<"coro not found\n";
                    stopped = true;
                } else {
                    coro->Resume();
                }
            });
            Yield();
        }
    };
    CoroPool::Instance().Spawn(f);
}

void Accept(asio::ip::tcp::acceptor& acceptor)
{
    acceptor.async_accept([&acceptor](const asio::error_code& err, asio::ip::tcp::socket socket) {
        if(err) {
            std::cout<<"accept err:"<<err.message()<<"\n";
            return;
        }
        Echo(std::move(socket));
        Accept(acceptor);
    });
}

int main()
{
    asio::io_context io(1);

    asio::ip::tcp::endpoint ep(asio::ip::address::from_string("127.0.0.1"), 12345);
    asio::ip::tcp::acceptor acceptor(io, ep);
    //Accept(acceptor); // accept不适用协程

    // accept适用协程的方式，看起来像是在协程中又启动了新的协程
    CoroPool::Instance().Spawn([&acceptor](CoroID id){
        bool stopped = false;
        while(!stopped) {
            acceptor.async_accept([&stopped, id](const asio::error_code& err, asio::ip::tcp::socket socket){
               if(err) {
                   stopped = true;
                   std::cout<<"accept err: "<<err.message()<<"\n";
               } else {
                   Echo(std::move(socket)); // 对每个socket启动一个协程来进行数据读写
               }
               Coro* coro = CoroPool::Instance().FindCoro(id);
               if(!coro) {
                   std::cout<<"can not find coro\n";
                   stopped = true;
               } else {
                   coro->Resume();
               }
            });
            Yield();
        }
    });

    while(1)
    {
        CoroPool::Instance().ProcessTimers(10ms);
        io.run_for(10ms);
        std::this_thread::sleep_for(2ms);
    }
}



