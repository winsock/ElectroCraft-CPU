//
//  ElectroCraftTerminal.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/7/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include "ElectroCraftTerminal.h"
#include "ElectroCraft_CPU.h"
#include "ElectroCraftMemory.h"

ElectroCraftTerminal::ElectroCraftTerminal(ElectroCraftMemory *memory, int cols, int rows) {
    this->memory = memory;
    this->cols = cols;
    this->rows = rows;
    
    terminalMemory = memory->assignIOMemory(this);

    DoubleWord dColoumns;
    dColoumns.doubleWord = cols;
    DoubleWord dRows;
    dRows.doubleWord = rows;
    Address startAddress;
    startAddress.doubleWord = terminalMemory->startOffset.doubleWord + 12;
    
    terminalMemory->setData(Utils::General::doubleWordToBytes(dColoumns), 0, 4);
    terminalMemory->setData(Utils::General::doubleWordToBytes(dRows), 4, 4);
    terminalMemory->setData(Utils::General::doubleWordToBytes(startAddress), 8, 4);
    
    terminalMemoryStart = terminalMemory->front + 12;
}

ElectroCraftTerminal::~ElectroCraftTerminal() {
    
}

void ElectroCraftTerminal::clear() {
    for (int i = 0; i < rows * cols; i++) {
        (terminalMemoryStart + i)->data = 0;
    }
}

std::string ElectroCraftTerminal::getLine(int row) {
    std::stringstream lineBuffer;
    for (int i = 0; i < cols; i++) {
        if ((terminalMemoryStart + ((row * cols) + i))->data != '\0') {
            lineBuffer<<(terminalMemoryStart + ((row * cols) + i))->data;
        }
    }
    return lineBuffer.str();
}

MemoryMappedIOSection ElectroCraftTerminal::getMappedIO() {
    // 2MB of addressable space
    MemoryMappedIOSection section;
    section.beginAddress.doubleWord = 0x1010000;
    section.endAddress.doubleWord = 0x1210000;
    return section;
}

int ElectroCraftTerminal::getRows() {
    return rows;
}

int ElectroCraftTerminal::getCols() {
    return cols;
}

void ElectroCraftTerminal::operator()(long tickTime) {
}