/*
 * async_client.cpp
 *
 *  Created on: Jun 20, 2019
 *      Author: frank
 */
#include <memory>
#include <iostream>
#include <string>
#include <thread>
#include <grpcpp/grpcpp.h>
#include <grpc/support/log.h>
#include <chrono>
#include <utility>
#include <type_traits> // static_assert
#include "my_helloworld.grpc.pb.h"
#include "coropp.h"

using namespace CoroPP;
using namespace std;

using grpc::Status;
using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using helloworld::HelloRequest;
using helloworld::HelloReply;
using helloworld::Greeter;
using helloworld::Calculator;
using helloworld::AddRequest;
using helloworld::AddResponse;
using namespace std::chrono_literals;

static_assert(sizeof(CoroID) == 16, "coroid changed?????");

// 从队列里面收取回包，最多收取tp时间
void AsyncCompleteRpc(CompletionQueue& cq, gpr_timespec tp) {
    void* got_tag;
    bool ok = false;

    while (CompletionQueue::GOT_EVENT == cq.AsyncNext(&got_tag, &ok, tp)) {
        CoroID* id = static_cast<CoroID*>(got_tag);
        GPR_ASSERT(ok);
        //std::cout<<"get call\n";
        Coro* coro = CoroPool::Instance().FindCoro(*id);
        if(!coro)
        { // 找不到，应该是提前超时了

        }
        else
        {
            coro->Resume();
            delete id;
        }
    }
}

int main()
{
    CoroPool& pool = CoroPool::Instance();
    CompletionQueue cq;
    std::shared_ptr<::grpc::Channel> channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
    std::shared_ptr<Greeter::Stub> hello_stub(Greeter::NewStub(channel));
    std::shared_ptr<Calculator::Stub> calc_stub(Calculator::NewStub(channel));

    for(int i = 0; i < 200; ++i)
    {

        pool.Spawn([&cq, hello_stub, channel, i](CoroID id){
            // 构造hello请求
            HelloRequest request;
            std::string name = std::string("name") + std::to_string(i);
            request.set_name(name);
            //通过grpc发送
            // Context for the client. It could be used to convey extra information to
            // the server and/or tweak certain RPC behaviors.
            ClientContext context;
            // Storage for the status of the RPC upon completion.
            Status status;
            std::unique_ptr<ClientAsyncResponseReader<HelloReply>> response_reader =
                    hello_stub->PrepareAsyncSayHello(&context, request, &cq);
            // StartCall initiates the RPC call
            response_reader->StartCall();
            HelloReply response;
            //tag参数不好传，因为考虑到协程resume可能是由定时器触发的，参数中的CoroID id可能已经释放，不能传其地址。
            //此处是new一个CoroID，可以保证该对象始终有效，但是需要手动delete，目前没有想到更好的方法来作为tag传递
            CoroID* tag = new CoroID(id);
            response_reader->Finish(&response, &status, (void*)tag);
            if(Yield(100ms))
            {
                delete tag;
                std::cout<<"time out\n";
                return;
            }

            if(!status.ok())
            {
                std::cout<<"error "<<status.error_code()<<":"<<status.error_message()<<"\n";
            }
            else
            {
                std::cout<<"get reply:"<<response.ShortDebugString()<<"\n";
            }
        });

        pool.Spawn([&cq, calc_stub, channel, i](CoroID id){
            AddRequest request;
            request.set_a(i);
            request.set_b(i);
            //通过grpc发送
            // Context for the client. It could be used to convey extra information to
            // the server and/or tweak certain RPC behaviors.
            ClientContext context;
            // Storage for the status of the RPC upon completion.
            Status status;
            std::unique_ptr<ClientAsyncResponseReader<AddResponse>> response_reader =
                    calc_stub->PrepareAsyncAdd(&context, request, &cq);
            // StartCall initiates the RPC call
            response_reader->StartCall();
            AddResponse response;
            //tag参数不好传，因为考虑到协程resume可能是由定时器触发的，参数中的CoroID id可能已经释放，不能传其地址。
            //此处是new一个CoroID，可以保证该对象始终有效，但是需要手动delete，目前没有想到更好的方法来作为tag传递
            CoroID* tag = new CoroID(id);
            response_reader->Finish(&response, &status, (void*)tag);
            if(Yield(100ms))
            {
                //delete tag;
                std::cout<<"time out\n";
                return;
            }

            //AddResponse的定义为：
            //message AddResponse
            //{
            //   int32 sum = 1;
            //}
            //在protobuf3中，如果一个int的值为0，则打包的时候会忽略掉（因为解包对于不存在的int值依然会解出0）。
            //所以，如果response的sum为0，则response.ShortDebugString()结果为空字符串。
            //按照本例的逻辑，当i=0时sum也会为0，会出现空字符串的情况。
            std::cout<<"get add reply:"<<response.ShortDebugString()<<"\n";
        });
    }

    while(1)
    {
        gpr_timespec tp = gpr_time_add(gpr_now(GPR_CLOCK_MONOTONIC), gpr_time_from_millis(5, GPR_TIMESPAN));
        AsyncCompleteRpc(cq, tp);
        pool.ProcessTimers(10ms);
        std::this_thread::sleep_for(5ms);
    }
}






