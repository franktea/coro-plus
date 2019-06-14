/*
 * main.cpp
 *
 *  Created on: Jun 11, 2019
 *      Author: frank
 */

#include <chrono>
#include "coropp.h"

using namespace CoroPP;

int main()
{
    Scheduler sch;

    while(1)
    {
        sch.RunFor(std::chrono::milliseconds(5));
    }
}



