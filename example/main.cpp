/*
 * main.cpp
 *
 *  Created on: Jun 11, 2019
 *      Author: frank
 */

#include <iostream>
#include <chrono>
#include <list>
#include <thread>
#include <random>
#include <string>
#include "coropp.h"

using namespace CoroPP;

struct Header
{
    CoroID id;
    int32_t cmd;
};

struct Package
{
    Header header;
    std::string body;
};

class Queue
{
public:
    bool Send(const Package& pkg)
    {
        packages_.push_back(pkg);
        return true;
    }

    bool Recv(Package& pkg)
    {
        if(packages_.empty())
        {
            return false;
        }
        pkg = *packages_.begin();
        packages_.pop_front();
        return true;
    }

    bool Peak(CoroID& id)
    {
        if(packages_.empty())
        {
            return false;
        }
        id = packages_.begin()->header.id;
        return true;
    }
private:
    std::list<Package> packages_;
};

Queue client_2_server;
Queue server_2_client;

void fake_server()
{
    Package request;
    while(client_2_server.Recv(request))
    {
        // 随机丢弃，模拟超时
        std::random_device rd;
        std::mt19937 mt(rd());
        std::uniform_real_distribution<double> dist(0.0, 10.0);
        if(dist(mt) < 5.0) // 将有些请求丢掉不回包，这些请求就会超时
        {
            continue;
        }

        Package response;
        switch(request.header.cmd)
        {
        case 1:
            response.header = request.header;
            response.body = std::string("hello, ") + request.body;
            server_2_client.Send(response);
            break;
        default:
            std::cerr<<"undefined cmd="<<request.header.cmd<<"\n";
            break;
        }
    }
}

void check_response(Scheduler& sch)
{
    CoroID id;
    while(server_2_client.Peak(id))
    {
        Coro* coro = sch.FindCoro(id);
        if(!coro)
        {
            std::cerr<<"can not find coro!\n";
        }
        else
        {
            coro->Resume();
        }
    }
}

int main()
{
    Scheduler& sch = Scheduler::Instance();

    /*
    sch.Spawn([](CoroID id){
        Package request;
        request.header.id = id;
        request.header.cmd = 1;
        request.body = "world";
        client_2_server.Send(request);
        Yield();
        Package response;
        server_2_client.Recv(response);
        std::cout<<"get response: "<<response.body<<"\n";

        for(int i = 0; i < 10; ++i)
        {
            request.body = response.body;
            client_2_server.Send(request);
            Yield();
            server_2_client.Recv(response);
            std::cout<<"get response "<<i+2<<": "<<response.body<<"\n";
        }
    });
*/

    for(int i = 0; i < 10; ++i)
    {
        sch.Spawn([i](CoroID id){
            Package request = {{id, 1}, std::string("world ") + std::to_string(i)};
            client_2_server.Send(request);
            using namespace std::chrono_literals;
            if(Yield(100ms))
            { // 超时了
                std::cout<<"coro "<<i<<" timeout.\n";
                return;
            }

            // 协程正常返回
            Package response;
            server_2_client.Recv(response);
            std::cout<<"coro "<<i<<" get response: "<<response.body<<"\n";
        });
    }

    while(1)
    {
        sch.RunFor(std::chrono::milliseconds(5));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        check_response(sch);
        fake_server();
    }
}



