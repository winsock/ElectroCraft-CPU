//
//  ElectroCraftTerminal.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/7/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__ElectroCraftTerminal__
#define __ElectroCraft_CPU__ElectroCraftTerminal__

#include <iostream>
#include <string>
#include <sstream>
#include "MemoryMappedIODevice.h"

class ElectroCraftMemory;
struct MemoryInfo;
struct Memory;

class ElectroCraftTerminal : public MemoryMappedIODevice {
    ElectroCraftMemory* memory;
    MemoryInfo* terminalMemory;
    Memory* terminalMemoryStart;
    int cols, rows;
public:
    ElectroCraftTerminal(ElectroCraftMemory *memory, int cols, int rows);
    ~ElectroCraftTerminal();
    void clear();
    std::string getLine(int row);
    virtual MemoryMappedIOSection getMappedIO();
    virtual void operator()(long tickTime);
};

#endif /* defined(__ElectroCraft_CPU__ElectroCraftTerminal__) */
