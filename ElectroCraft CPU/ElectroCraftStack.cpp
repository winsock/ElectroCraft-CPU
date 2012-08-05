//
//  ElectroCraftStack.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/4/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include <iostream>
#include "ElectroCraftStack.h"
#include "ElectroCraftMemory.h"

ElectroCraftStack::ElectroCraftStack(ElectroCraftMemory *memory, unsigned int size) {
    this->memory = memory;
    this->memoryBlock = memory->allocate(size);
    this->currentAddress = memoryBlock->startOffset;
    if (memoryBlock == nullptr) {
        std::cerr<<"ElectroCraft CPU: Error allocating the stack with the size: "<<size<<std::endl;
    }
}

void ElectroCraftStack::push(DoubleWord data) {
    if ((currentAddress.doubleWord - memoryBlock->startOffset.doubleWord) + 4 > memoryBlock->memoryLength.doubleWord) {
        std::cerr<<"ElectroCraft CPU: Error! Ran out of stack space!"<<std::endl;
    } else {
        (memoryBlock->front + currentAddress.doubleWord + 1)->data = data.word.highWord.byte.hiByte;
        (memoryBlock->front + currentAddress.doubleWord + 2)->data = data.word.highWord.byte.lowByte;
        (memoryBlock->front + currentAddress.doubleWord + 3)->data = data.word.lowWord.byte.hiByte;
        (memoryBlock->front + currentAddress.doubleWord + 4)->data = data.word.lowWord.byte.lowByte;
        currentAddress.doubleWord += 4;
    }
}

DoubleWord ElectroCraftStack::pop() {
    DoubleWord data;
    if ((currentAddress.doubleWord - memoryBlock->startOffset.doubleWord) >= 4) {
        currentAddress.doubleWord -= 4;
        data.word.highWord.byte.hiByte = (memoryBlock->front + currentAddress.doubleWord + 1)->data;
        data.word.highWord.byte.lowByte = (memoryBlock->front + currentAddress.doubleWord + 2)->data;
        data.word.lowWord.byte.hiByte = (memoryBlock->front + currentAddress.doubleWord + 3)->data;
        data.word.lowWord.byte.lowByte = (memoryBlock->front + currentAddress.doubleWord + 4)->data;
    } else {
        std::cerr<<"ElectroCraft CPU: Error! Tried to pop off an empty stack!"<<std::endl;
        data = memoryBlock->startOffset;
    }
    return data;
}