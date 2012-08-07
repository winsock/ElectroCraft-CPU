//
//  ElectroCraftVGA.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/5/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__ElectroCraftVGA__
#define __ElectroCraft_CPU__ElectroCraftVGA__

#include "MemoryMappedIODevice.h"
#include <string>

class ElectroCraftMemory;
struct MemoryInfo;
struct Memory;

class ElectroCraftVGA : public MemoryMappedIODevice {
    MemoryInfo* vgaIOMemory;
    Memory* displayBuffer;
    DoubleWord displayBufferSize;
    unsigned int width;
    unsigned int height;
public:
    ElectroCraftVGA(ElectroCraftMemory *memory, unsigned int width, unsigned int height);
    ~ElectroCraftVGA();
    void setScreenSize(unsigned int width, unsigned int height);
    void clear();
    Byte* getScreenData();
    DoubleWord getDisplayBufferSize();
    unsigned int getWidth();
    unsigned int getHeight();
    virtual MemoryMappedIOSection getMappedIO();
    virtual void operator()(long tickTime);
};

#endif /* defined(__ElectroCraft_CPU__ElectroCraftVGA__) */
