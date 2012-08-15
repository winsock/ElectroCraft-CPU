//
//  IOPortHandler.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/8/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include "IOPortHandler.h"
#include <iostream>

void IOPort::IOPortHandler::registerDevice(IOPort::IOPortDevice *device) {
    for (int i = 0; i < device->getRequestedIOPorts().number; i++) {
        ioPorts[device->getRequestedIOPorts().ioPorts[i].doubleWord] = device;
        std::cout<<device->getRequestedIOPorts().ioPorts[i].doubleWord<<std::endl;
    }
}

void IOPort::IOPortHandler::unregisterPort(uint32_t port) {
    ioPorts.erase(port);
}

IOPort::IOPortResult* IOPort::IOPortHandler::callIOPort(IOPort::IOPortInterrupt interrupt) {
    if (ioPorts.find(interrupt.ioPort.doubleWord) != ioPorts.end()) {
        return ioPorts[interrupt.ioPort.doubleWord]->onIOPortInterrupt(interrupt);
    }
    
    return nullptr;
}