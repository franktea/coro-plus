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

    bool Empty()
    {
        return packages_.empty();
    }
private:
    std::list<Package> packages_;
};

//用两个队列通信
Queue client_2_server;
Queue server_2_client;

// 缓存一些回包，故意等到超时了再发送
Queue delayed_response;

void fake_server()
{
    Package request;
    while(client_2_server.Recv(request))
    {
        // 随机丢弃，模拟超时
        std::random_device rd;
        std::mt19937 mt(rd());
        std::uniform_real_distribution<double> dist(0.0, 10.0);
        const double r = dist(mt);
        if(r < 3.0) // 将有些请求丢掉不回包，这些请求就会超时
        {
            continue;
        }
        else if(r < 6.0) // 将这些请求的结果存起来，等到客户端超时了再发，模拟超时以后回包才到达的情况
        {
            Package response;
            switch(request.header.cmd)
            {
            case 1:
                response.header = request.header;
                response.body = std::string("too late: ") + request.body;
                delayed_response.Send(response);
                break;
            default:
                std::cerr<<"undefined cmd="<<request.header.cmd<<"\n";
            }

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
        if(!coro) // 应该是被之前的定时器把对应的coro删掉了
        {
            // 这个包没用了，把包收出来，免得阻塞后面的
            Package pkg;
            server_2_client.Recv(pkg);
            std::cout<<pkg.body<<"\n";
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

    using namespace std::chrono_literals;

    for(int i = 0; i < 10; ++i)
    {
        sch.Spawn([i](CoroID id){
            Package request = {{id, 1}, std::string("world ") + std::to_string(i)};
            client_2_server.Send(request);
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

    // 等待客户端都超时以后，将部分回包发过去，模拟定时器触发以后回包再到达的情况
    sch.Spawn([](CoroID id){
        Yield(1000ms); // 等待1s足够前面所有的定时器都超时了，因为前面的定时器都只等100ms
        while(!delayed_response.Empty())
        {
            Package pkg;
            delayed_response.Recv(pkg);
            server_2_client.Send(pkg);
        }
    });

    while(1)
    {
        sch.RunFor(std::chrono::milliseconds(5));
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        check_response(sch);
        fake_server();
    }
}



