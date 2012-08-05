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

class ElectroCraftMemory;
struct MemoryInfo;

class ElectroCraftVGA : MemoryMappedIODevice {
    MemoryInfo* vgaIOMemory;
    
public:
    ElectroCraftVGA(ElectroCraftMemory *memory, unsigned int width, unsigned int height);
    ~ElectroCraftVGA();
    void setScreenSize(unsigned int width, unsigned int height);
    void clear();
    Byte* getScreenData();
    virtual MemoryMappedIOSection getMappedIO();
};

#endif /* defined(__ElectroCraft_CPU__ElectroCraftVGA__) */