//
//  ElectroCraftStack.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/4/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__ElectroCraftStack__
#define __ElectroCraft_CPU__ElectroCraftStack__

#include "Utils.h"

struct MemoryInfo;
class ElectroCraftMemory;
struct RegisterState;

class ElectroCraftStack {
    MemoryInfo* memoryBlock;
    RegisterState *registers;
    ElectroCraftMemory * memory;
public:
    ElectroCraftStack(ElectroCraftMemory *memory, RegisterState * registers, unsigned int size);
    ~ElectroCraftStack();
    void push(DoubleWord data);
    DoubleWord pop();
};

#endif /* defined(__ElectroCraft_CPU__ElectroCraftStack__) */
