/*
 * echo-client.cpp
 *
 *  Created on: Jun 21, 2019
 *      Author: frank
 */
#include <asio.hpp>
#include <chrono>
#include <thread>
#include <string>
#include "coropp.h"

using namespace CoroPP;
using namespace std::chrono_literals;

int main()
{
    CoroPool& pool = CoroPool::Instance();
    asio::io_context io(1);
    asio::ip::tcp::endpoint ep(asio::ip::address::from_string("127.0.0.1"), 12345);

    for(int i = 0; i < 100; ++i)
    {
        pool.Spawn([i, ep, &io](CoroID id){
            asio::ip::tcp::socket socket(io);
            socket.async_connect(ep, [i, id](const asio::error_code& err){
                 if(err) {
                     std::cout<<"coro "<<i<<" connect err:"<<err.message()<<"\n";
                     return;
                 }
                 Coro* coro = CoroPool::Instance().FindCoro(id);
                 if(!coro) {
                     std::cout<<"can not find coro after connect\n";
                     return;
                 }
                 else {
                     coro->Resume();
                 }
            });
            Yield();
            std::string str = std::string("hello") + std::to_string(i);
            asio::async_write(socket, asio::buffer(str), [id](const asio::error_code& err, size_t len){
                if(err) {
                    std::cout<<"get err:"<<err.message()<<"\n";
                    return;
                }

                Coro* coro = CoroPool::Instance().FindCoro(id);
                if(!coro) {
                    std::cout<<"coro not found\n";
                    return;
                }
                coro->Resume();
            });
            Yield();
            char buff[256];
            bzero(buff, sizeof(buff));
            socket.async_read_some(asio::buffer(buff, sizeof(buff)), [id](const asio::error_code& err, size_t len){
                if(err) {
                    std::cout<<"read err:"<<err.message()<<"\n";
                    return;
                }
                Coro* coro = CoroPool::Instance().FindCoro(id);
                if(!coro) {
                    std::cout<<"coro not found after read\n";
                    return;
                }
                coro->Resume();
            });
            Yield();
            std::cout<<"get result: "<<std::string(buff)<<"\n";
        });
    }

    while(1)
    {
        io.run_for(10ms);
        pool.ProcessTimers(10ms);
        std::this_thread::sleep_for(1ms);
    }
}




