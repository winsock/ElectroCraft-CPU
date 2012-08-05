//
//  ElectroCraftClock.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/5/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include "ElectroCraftClock.h"
#include <chrono>

ElectroCraftClock::ElectroCraftClock(unsigned long IPS) {
    this->IPS = IPS;
}

void ElectroCraftClock::registerCallback(ElectroCraftTickable *device) {
    callbacks.push_back(device);
}

void ElectroCraftClock::start() {
    running = true;
    clockThread = std::thread(std::ref(*this));
}

void ElectroCraftClock::stop() {
    running = false;
    clockThread.detach();
}

bool ElectroCraftClock::isRunning() {
    return running;
}

void ElectroCraftClock::operator()() {
    unsigned long currentOperations = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTickTime = std::chrono::high_resolution_clock::now();
    while (running) {
        std::chrono::milliseconds timeSinceLastTick = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastTickTime);
        if (currentOperations >= IPS && timeSinceLastTick < std::chrono::milliseconds(1000)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 - timeSinceLastTick.count()));
        }
        if(timeSinceLastTick >= std::chrono::milliseconds(1000)) {
            lastTickTime = std::chrono::high_resolution_clock::now();
            currentOperations = 0;
        } else if (currentOperations < IPS) {
            for (ElectroCraftTickable *tickable : callbacks) {
                (*tickable)(timeSinceLastTick.count()); // Call tickable
                currentOperations++; // Each "Device" can execute one operation per tick
            }
        }
    }
}