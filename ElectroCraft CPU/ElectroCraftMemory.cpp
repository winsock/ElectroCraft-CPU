//
//  ElectroCraft_Memory.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/3/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include "ElectroCraftMemory.h"
#include <iostream>

ElectroCraftMemory::~ElectroCraftMemory() {
    // Free up our allocated memory
    delete [] memory;
}

ElectroCraftMemory::ElectroCraftMemory(unsigned int size, Address baseAddress) {
    sizeOfMemory = size;
    virtualBaseAddress = baseAddress;
    // Allocate our memory
    memory = (Memory*)malloc(sizeof(Memory) * size);
    
    MemoryInfo* freeMemoryInfo = new MemoryInfo;
    freeMemoryInfo->startOffset = baseAddress;
    freeMemoryInfo->stateOfMemory = MemoryState::FREE;
    freeMemoryInfo->front = &memory[0];
    freeMemoryInfo->back = memory + size;
    freeMemoryInfo->memoryLength.doubleWord = size;
    freeMemoryInfo->next = nullptr;
    freeMemoryInfo->previous = nullptr;
    memoryStates.push_back(freeMemoryInfo);
}

MemoryInfo* ElectroCraftMemory::allocate(unsigned int size) {
    if(size <= 0) {
        return nullptr;
    }
    
    for (int i = 0; i < memoryStates.size(); i++) {
        if (memoryStates[i]->stateOfMemory == MemoryState::FREE) {
            if ((memoryStates[i]->startOffset.doubleWord + memoryStates[i]->memoryLength.doubleWord) > size) {
                // Setup the new state
                MemoryInfo* newState = new MemoryInfo;
                newState->front = memoryStates[i]->front;
                newState->back = memoryStates[i]->front + size;
                newState->memoryLength.doubleWord = size;
                newState->next = memoryStates[i];
                newState->previous = memoryStates[i]->previous;
                newState->startOffset = memoryStates[i]->startOffset;
                newState->stateOfMemory = MemoryState::ALLOCATED;
                memoryStates.push_back(newState);
                
                // Reconfigure the old free space
                memoryStates[i]->front = memoryStates[i]->front + size + 1;
                memoryStates[i]->memoryLength.doubleWord = memoryStates[i]->memoryLength.doubleWord - size;
                memoryStates[i]->previous = newState;
                memoryStates[i]->startOffset.doubleWord = memoryStates[i]->startOffset.doubleWord + size + 1;
                memoryStates[i]->stateOfMemory = MemoryState::FREE;
                
                return newState;
            } else if ((memoryStates[i]->startOffset.doubleWord + memoryStates[i]->memoryLength.doubleWord) == size) {
                memoryStates[i]->stateOfMemory = MemoryState::ALLOCATED;
                return memoryStates[i];
            }
        }
    }
    
    std::cerr<<"ElectroCraft Memory: Error ran out of memory!"<<std::endl;
    // Otherwise return that there isn't enough free memory
    return nullptr;
}

MemoryInfo* ElectroCraftMemory::assignIOMemory(Address beginOffset, Address endOffset) {
    if(endOffset.doubleWord - beginOffset.doubleWord <= 0) {
        return nullptr;
    }
    
    if (endOffset.doubleWord > sizeOfMemory) {
        std::cerr<<"ElectroCraft Memory: Error range out of bounds!"<<std::endl;
        return nullptr;
    }
    
    MemoryInfo *info = new MemoryInfo;
    MemoryInfo *next;
    MemoryInfo *previous;
    for (int i = 0; i < this->memoryStates.size(); i++) {
        if (beginOffset.doubleWord >= memoryStates[i]->startOffset.doubleWord && beginOffset.doubleWord <= memoryStates[i]->startOffset.doubleWord + memoryStates[i]->memoryLength.doubleWord) {
            std::cerr<<"ElectroCraft Memory: Error requested range is ocupied!"<<std::endl;
            return nullptr;
        } else if (endOffset.doubleWord >= memoryStates[i]->startOffset.doubleWord && endOffset.doubleWord <= memoryStates[i]->startOffset.doubleWord + memoryStates[i]->memoryLength.doubleWord) {
            std::cerr<<"ElectroCraft Memory: Error requested range is ocupied!"<<std::endl;
            return nullptr;
        }
        
        if (memoryStates[i]->startOffset.doubleWord + memoryStates[i]->memoryLength.doubleWord < beginOffset.doubleWord) {
            previous = memoryStates[i];
        }
        
        if (memoryStates[i]->startOffset.doubleWord > endOffset.doubleWord) {
            next = memoryStates[i];
        }
    }
    
    info->front = this->memory + beginOffset.doubleWord;
    info->back = this->memory + endOffset.doubleWord;
    info->next = next;
    info->previous = previous;
    info->startOffset = beginOffset;
    info->stateOfMemory = MemoryState::SYSTEM;
    info->memoryLength.doubleWord = endOffset.doubleWord - beginOffset.doubleWord;
    memoryStates.push_back(info);
    
    return info;
}

void ElectroCraftMemory::free(MemoryInfo *memoryInfo) {
    MemoryInfo* freeBlock = new MemoryInfo;
    DoubleWord memoryLength = memoryInfo->memoryLength;
    bool wasPreviousMerged = false;
    bool wasNextMerged = false;
    
    if (memoryInfo->previous != nullptr && memoryInfo->previous->stateOfMemory == MemoryState::FREE) {
        freeBlock->front = memoryInfo->previous->front;
        freeBlock->back = memoryInfo->back;
        freeBlock->next = memoryInfo->next;
        freeBlock->previous = memoryInfo->previous->previous;
        freeBlock->startOffset = memoryInfo->previous->startOffset;
        memoryLength.doubleWord += memoryInfo->previous->memoryLength.doubleWord;
        memoryStates.erase(std::remove(memoryStates.begin(), memoryStates.end(), memoryInfo->previous), memoryStates.end());
        delete memoryInfo->previous;
        wasPreviousMerged = true;
    }
    
    if (memoryInfo->next != nullptr && memoryInfo->next->stateOfMemory == MemoryState::FREE) {
        if (wasPreviousMerged) {
            freeBlock->back = memoryInfo->next->back;
            freeBlock->next = memoryInfo->next->next;
            memoryLength.doubleWord += memoryInfo->previous->memoryLength.doubleWord;
        } else {
            freeBlock->front = memoryInfo->front;
            freeBlock->back = memoryInfo->next->back;
            freeBlock->next = memoryInfo->next->next;
            freeBlock->previous = memoryInfo->previous;
            freeBlock->startOffset = memoryInfo->startOffset;
        }
        memoryLength.doubleWord += memoryInfo->next->memoryLength.doubleWord;
        memoryStates.erase(std::remove(memoryStates.begin(), memoryStates.end(), memoryInfo->next), memoryStates.end());
        delete memoryInfo->next;
        wasNextMerged = true;
    }

    if (!wasNextMerged && !wasPreviousMerged) {
        freeBlock = memoryInfo;
        memoryStates.erase(std::remove(memoryStates.begin(), memoryStates.end(), memoryInfo), memoryStates.end());
    }
    
    freeBlock->stateOfMemory = MemoryState::FREE;
    freeBlock->memoryLength = memoryLength;
    memoryStates.push_back(freeBlock);
}

Byte* ElectroCraftMemory::readData(Address offset, unsigned int length) {
    if (length > sizeOfMemory) {
        length = sizeOfMemory;
    }
    
    Byte* data = new Byte[length];
    for (int i = offset.doubleWord, j = 0; i < offset.doubleWord + length; i++) {
        data[j++] = memory[i].data;
    }
    return data;
}

void ElectroCraftMemory::writeData(Address offset, unsigned int length, Byte *data) {
    if (offset.doubleWord > sizeOfMemory) {
        return;
    }
    
    if (offset.doubleWord + length > sizeOfMemory) {
        length = sizeOfMemory;
    }
    
    for (int i = 0; i < length; i++) {
        memory[offset.doubleWord + i].data = data[i];
    }
}