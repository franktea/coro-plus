/*
 * test.cpp
 *
 *  Created on: Jun 14, 2019
 *      Author: frank
 */

#include <iostream>
#include "coropp.h"

using namespace CoroPP;

int main()
{
    Coro* c1 = Create([](){
        std::cout<<"11111111111111111\n";
        Yield();
        std::cout<<"111111111========\n";
        Yield();
        std::cout<<"111111111........\n";
    });
    Coro* c2 = Create([](){
        std::cout<<"22222222222222222\n";
        Yield();
        std::cout<<"2222222222=======\n";
    });

    std::cout<<"in mian.\n";
    c1->Resume();
    std::cout<<"in main 2.\n";
    c1->Resume();
    c2->Resume();

    assert(c1->status_ == 2);
    assert(c2->status_ == 2);
    return 0;
}


