//
//  ElectroCraftVGA.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/5/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include <iostream>
#include <sstream>
#include "ElectroCraftVGA.h"
#include "ElectroCraftMemory.h"

ElectroCraftVGA::ElectroCraftVGA(ElectroCraftMemory *memory, unsigned int width, unsigned int height) {
    vgaIOMemory = memory->assignIOMemory(this);
    // Three bytes per pixel(RGB)
    if ((width * height * 3) > vgaIOMemory->memoryLength.doubleWord) {
        std::cerr<<"ElectroCraft VGA: Error! Not enough memory for the requested screen size!"<<std::endl;
    }
    
    // 3 Bytes per pixel (RGB)
    displayBufferSize.doubleWord = width * height;
    // Right after the VGA Info section
    displayBuffer = vgaIOMemory->front + 8;
    
    Address displayBufferStartAddress;
    displayBufferStartAddress.doubleWord = vgaIOMemory->startOffset.doubleWord + 8;
    vgaIOMemory->setData(Utils::General::doubleWordToBytes(displayBufferSize), 0, 4);
    vgaIOMemory->setData(Utils::General::doubleWordToBytes(displayBufferStartAddress), 4, 4);
    
    this->width = width;
    this->height = height;
}

MemoryMappedIOSection ElectroCraftVGA::getMappedIO() {
    // 16MB of addressable space
    MemoryMappedIOSection section;
    section.beginAddress.doubleWord = 0x8000;
    section.endAddress.doubleWord = 0x1008000;
    return section;
}

// WARNING: A very expensive operation, especialy for large virtual display sizes
void ElectroCraftVGA::clear() {
    for (Memory* displayPixel = displayBuffer; displayPixel < displayBuffer + displayBufferSize.doubleWord; displayPixel++) {
        displayPixel->data = 0x0;
    }
}

Byte* ElectroCraftVGA::getScreenData() {
    return &displayBuffer->data;
}

DoubleWord ElectroCraftVGA::getDisplayBufferSize() {
    return displayBufferSize;
}

void ElectroCraftVGA::setScreenSize(unsigned int width, unsigned int height) {
    if ((width * height * 3) > vgaIOMemory->memoryLength.doubleWord) {
        std::cerr<<"ElectroCraft VGA: Error! Not enough memory for the requested screen size!"<<std::endl;
    }
    displayBufferSize.doubleWord = width * height * 3;
    vgaIOMemory->setData(Utils::General::doubleWordToBytes(displayBufferSize), 0, 4);
    this->width = width;
    this->height = height;
}

unsigned int ElectroCraftVGA::getWidth() {
    return width;
}

unsigned int ElectroCraftVGA::getHeight() {
    return height;
}

void ElectroCraftVGA::operator()(long tickTime) {
    
}