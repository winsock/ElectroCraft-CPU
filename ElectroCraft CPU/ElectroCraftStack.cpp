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
#include "ElectroCraft_CPU.h"

ElectroCraftStack::ElectroCraftStack(ElectroCraftMemory *memory, RegisterState *registers, unsigned int size) {
    this->memory = memory;
    this->memoryBlock = memory->allocate(size);
    registers->SP = memoryBlock->startOffset + size;
    this->registers = registers;
    if (memoryBlock == nullptr) {
        std::cerr<<"ElectroCraft CPU: Error allocating the stack with the size: "<<size<<std::endl;
    }
}

void ElectroCraftStack::push(DoubleWord data) {
    if ((registers->SP.doubleWord - memoryBlock->startOffset.doubleWord) >= 4) {
        registers->SP.doubleWord -= 4;
        memory->writeData(registers->SP.doubleWord, 4, Utils::General::doubleWordToBytes(data));
    } else {
        std::cerr<<"ElectroCraft CPU: Error! Ran out of stack space!"<<std::endl;
    }
}

DoubleWord ElectroCraftStack::pop() {
    DoubleWord data;
    if ((registers->SP.doubleWord + 4) <= (memoryBlock->startOffset.doubleWord + memoryBlock->memoryLength.doubleWord)) {
        data = Utils::General::readDoubleWord(memory->readData(registers->SP, 4));
        registers->SP.doubleWord += 4;
    } else {
        std::cerr<<"ElectroCraft CPU: Error! Tried to pop off an empty stack!"<<std::endl;
        data = Utils::General::readDoubleWord(&memoryBlock->front->data);
    }
    return data;
}