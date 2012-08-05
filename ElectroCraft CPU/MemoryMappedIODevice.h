//
//  MemoryMappedIODevice.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/5/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef ElectroCraft_CPU_MemoryMappedIODevice_h
#define ElectroCraft_CPU_MemoryMappedIODevice_h

#include "Utils.h"

class MemoryMappedIODevice {
public:
    virtual MemoryMappedIOSection getMappedIO() = 0;
};

#endif
