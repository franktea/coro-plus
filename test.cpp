/*
 * test.cpp
 *
 *  Created on: Jun 14, 2019
 *      Author: frank
 */

#include <iostream>
#include <chrono>
#include <thread>
#include "coropp.h"

using namespace CoroPP;

int main()
{
    Coro* c1 = Scheduler::Instance().Spawn([](){
        std::cout<<"11111111111111111\n";
        Yield();
        std::cout<<"111111111========\n";
        Yield();
        std::cout<<"111111111........\n";
    }, 1000);
    Coro* c2 = Scheduler::Instance().Spawn([](){
        std::cout<<"22222222222222222\n";
        Yield();
        std::cout<<"2222222222=======\n";
    }, 1000);
/*
    std::cout<<"in mian.\n";
    c1->Resume();
    std::cout<<"in main 2.\n";
    c1->Resume();
    c2->Resume();

    assert(c1->status_ == 2);
    assert(c2->status_ == 2);
    */

    while(1)
    {
        Scheduler::Instance().RunFor(std::chrono::milliseconds(10));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        //std::cout<<"loop....\n";
    }
    return 0;
}


