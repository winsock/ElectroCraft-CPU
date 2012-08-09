//
//  ElectroCraftStorage.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/8/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__ElectroCraftStorage__
#define __ElectroCraft_CPU__ElectroCraftStorage__

#include <iostream>
#include <fstream>
#include "IOPortDevice.h"

enum HDDMode {
    READ = 0,
    WRITE = 1,
    SEEK = 2
    };

class ElectroCraftStorage : public IOPort::IOPortDevice {
    std::fstream file;
    HDDMode curentMode;
public:
    ElectroCraftStorage();
    ~ElectroCraftStorage();
    void seek(int offset);
    Byte read();
    void write(Byte byte);
    virtual IOPort::IOPortResult *onIOPortInterrupt(IOPort::IOPortInterrupt interrupt);
    virtual IOPort::IOPorts getRequestedIOPorts();
};

#endif /* defined(__ElectroCraft_CPU__ElectroCraftStorage__) */
