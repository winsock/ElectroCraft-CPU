//
//  ElectroCraftVGA.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/5/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include "ElectroCraftVGA.h"
#include "ElectroCraftMemory.h"

ElectroCraftVGA::ElectroCraftVGA(ElectroCraftMemory *memory, unsigned int width, unsigned int height) {
    vgaIOMemory = memory->assignIOMemory(this);
}

MemoryMappedIOSection ElectroCraftVGA::getMappedIO() {
    // 16MB of addressable space
    MemoryMappedIOSection section;
    section.beginAddress.doubleWord = 0x8000;
    section.endAddress.doubleWord = 0x1000020;
    return section;
}