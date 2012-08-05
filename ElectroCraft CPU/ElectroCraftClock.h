//
//  ElectroCraftClock.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/5/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__ElectroCraftClock__
#define __ElectroCraft_CPU__ElectroCraftClock__

#include "ElectroCraftTickable.h"
#include <vector>
#include <thread>

class ElectroCraftClock {
    std::vector<ElectroCraftTickable*> callbacks;
    bool running = false;
    std::thread clockThread;
    unsigned long IPS = 541000000l;
public:
    ElectroCraftClock(unsigned long ips);
    ~ElectroCraftClock();
    void operator()();
    void start();
    void stop();
    bool isRunning();
    void registerCallback(ElectroCraftTickable *device);
};

#endif /* defined(__ElectroCraft_CPU__ElectroCraftClock__) */
