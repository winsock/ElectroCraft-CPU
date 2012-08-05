//
//  ElectroCraft_Memory.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/3/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__ElectroCraftMemory__
#define __ElectroCraft_CPU__ElectroCraftMemory__

#include <cstdint>
#include <map>
#include <string>
#include "Utils.h"
#include "MemoryMappedIODevice.h"

enum MemoryState {
    FREE = 0,
    SYSTEM = 1,
    ALLOCATED = 2,
    UNKNOWN_STATE = 3
    };

struct Memory {
    Byte data;
    MemoryState state = MemoryState::FREE;
};

struct MemoryInfo {
    MemoryInfo *parent;
    MemoryState stateOfMemory;
    Address startOffset;
    DoubleWord memoryLength;
    Memory* front;
    Memory* back;
    MemoryInfo* next;
    MemoryInfo* previous;
    
    Byte* readData(Address offset, unsigned int length) {
        if (length > memoryLength.doubleWord) {
            length = memoryLength.doubleWord;
        }
        
        Byte* data = new Byte[length];
        for (int i = offset.doubleWord, j = 0; i < offset.doubleWord + length; i++) {
            data[j++] = (front + i)->data;
        }
        return data;
    }
    
    void setData(Byte* data, unsigned int length) {
        if (length > memoryLength.doubleWord) {
            length = memoryLength.doubleWord;
        }
        
        for (int i = 0; i < length; i ++) {
            (front + i)->data = data[i];
        }
    }
};

class ElectroCraftMemory {
    Memory* memory;
    std::vector<MemoryInfo*> memoryStates;
    std::map<MemoryMappedIODevice*, MemoryInfo*> memoryMappedIO;
    Address virtualBaseAddress;
    unsigned int sizeOfMemory = 128 * 1024 * 1024; // 128MB
public:
    ElectroCraftMemory(unsigned int size, Address baseAddress);
    ~ElectroCraftMemory();
    Byte* readData(Address offset, unsigned int length);
    void writeData(Address offset, unsigned int length, Byte* data);
    MemoryInfo* allocate(unsigned int size);
    MemoryInfo* assignIOMemory(MemoryMappedIODevice* device);
    void free(MemoryInfo *memory);
};

#endif /* defined(__ElectroCraft_CPU__ElectroCraft_Memory__) */
