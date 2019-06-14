/*
 * test.cpp
 *
 *  Created on: Jun 14, 2019
 *      Author: frank
 */

#include <iostream>
#include "coropp.h"

using namespace CoroPP;

class Coro1 : public Coro
{
public:
    virtual void DoRun() override
    {
        std::cout<<"my own coro\n";
        Yield();
        std::cout<<"come back\n";
    }
};

int main()
{
    Coro* coro = new Coro;
    Coro1* c = new Coro1;
    Create(coro);
    Create(c);
    coro->Resume();
    c->Resume();
    std::cout<<"aaaaaa\n";
    std::cout<<"status:"<<coro->status_<<"\n";
    std::cout<<"status2:"<<c->status_<<"\n";
    return 0;
}


