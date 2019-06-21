/*
 * async_server.cpp
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
#include "my_helloworld.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using helloworld::HelloRequest;
using helloworld::HelloReply;
using helloworld::Greeter;
using helloworld::Calculator;
using helloworld::AddRequest;
using helloworld::AddResponse;

class CallData
{
public:
    virtual void Proceed() = 0;
    virtual ~CallData() {}
};

class HelloCallData : public CallData
{
public:
    HelloCallData(Greeter::AsyncService* service, ServerCompletionQueue* cq) :
            service_(service), cq_(cq), responder_(&ctx_), status_(CREATE)
    {
    }

    virtual void Proceed() override
    {
        //std::cout<<"proceed in thread: "<<std::this_thread::get_id()<<"\n";
        if (status_ == CREATE)
        {
            status_ = PROCESS;
            service_->RequestSayHello(&ctx_, &request_, &responder_, cq_,
                    cq_, this);
        }
        else if (status_ == PROCESS)
        {
            (new HelloCallData(service_, cq_))->Proceed();
            std::string prefix("Hello ");
            reply_.set_message(prefix + request_.name());
            status_ = FINISH;
            responder_.Finish(reply_, Status::OK, this);
        }
        else
        {
            GPR_ASSERT(status_ == FINISH);
            delete this;
        }
    }

private:
    Greeter::AsyncService* service_;
    ServerCompletionQueue* cq_;
    ServerContext ctx_;
    HelloRequest request_;
    HelloReply reply_;
    ServerAsyncResponseWriter<HelloReply> responder_;
    enum CallStatus
    {
        CREATE, PROCESS, FINISH
    };
    CallStatus status_;  // The current serving state.
};

class AddCallData : public CallData
{
public:
    AddCallData(Calculator::AsyncService* service, ServerCompletionQueue* cq) :
            service_(service), cq_(cq), responder_(&ctx_), status_(CREATE)
    {
    }

    virtual void Proceed() override
    {
        //std::cout<<"proceed in thread: "<<std::this_thread::get_id()<<"\n";
        if (status_ == CREATE)
        {
            status_ = PROCESS;
            service_->RequestAdd(&ctx_, &request_, &responder_, cq_,
                    cq_, this);
        }
        else if (status_ == PROCESS)
        {
            (new AddCallData(service_, cq_))->Proceed();
            reply_.set_sum(request_.a() + request_.b());
            status_ = FINISH;
            responder_.Finish(reply_, Status::OK, this);
        }
        else
        {
            GPR_ASSERT(status_ == FINISH);
            delete this;
        }
    }

private:
    Calculator::AsyncService* service_;
    ServerCompletionQueue* cq_;
    ServerContext ctx_;
    AddRequest request_;
    AddResponse reply_;
    ServerAsyncResponseWriter<AddResponse> responder_;
    enum CallStatus
    {
        CREATE, PROCESS, FINISH
    };
    CallStatus status_;  // The current serving state.
};

class ServerImpl final
{
public:
    ~ServerImpl()
    {
        server_->Shutdown();
        // Always shutdown the completion queue after the server.
        cq_->Shutdown();
    }

    // There is no shutdown handling in this code.
    void Run()
    {
        std::string server_address("0.0.0.0:50051");

        ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address,
                grpc::InsecureServerCredentials());
        // Register "service_" as the instance through which we'll communicate with
        // clients. In this case it corresponds to an *asynchronous* service.
        builder.RegisterService(&service_);
        builder.RegisterService(&calc_service_);
        // Get hold of the completion queue used for the asynchronous communication
        // with the gRPC runtime.
        cq_ = builder.AddCompletionQueue();
        // Finally assemble the server.
        server_ = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;

        // Proceed to the server's main loop.
        HandleRpcs();
    }

private:
    // This can be run in multiple threads if needed.
    void HandleRpcs()
    {
        // Spawn a new CallData instance to serve new clients.
        //for(int i = 0; i < 500; ++i)
        //{
        (new HelloCallData(&service_, cq_.get()))->Proceed();
        (new AddCallData(&calc_service_, cq_.get()))->Proceed();
        //}
        void* tag;  // uniquely identifies a request.
        bool ok;
        while (true)
        {
            // Block waiting to read the next event from the completion queue. The
            // event is uniquely identified by its tag, which in this case is the
            // memory address of a CallData instance.
            // The return value of Next should always be checked. This return value
            // tells us whether there is any kind of event or cq_ is shutting down.
            GPR_ASSERT(cq_->Next(&tag, &ok));
            if(ok) // 同时发送太多请求时ok为false
            {
                static_cast<CallData*>(tag)->Proceed();
            }
            else
            {
                std::cout<<"not ok\n";
            }
        }
    }

    std::unique_ptr<ServerCompletionQueue> cq_;
    Greeter::AsyncService service_;
    Calculator::AsyncService calc_service_;
    std::unique_ptr<Server> server_;
};

int main(int argc, char** argv)
{
    ServerImpl server;
    server.Run();

    return 0;
}

