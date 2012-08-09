//
//  ElectroCraftStorage.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/8/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include "ElectroCraftStorage.h"

ElectroCraftStorage::ElectroCraftStorage() {
    file.open("electrocraft.xec", std::ios_base::in | std::ios_base::out | std::ios_base::binary);
    if (!file.is_open()) {
        file.open("electrocraft.xec", std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
    }
}

ElectroCraftStorage::~ElectroCraftStorage() {
    file.close();
}

IOPort::IOPortResult* ElectroCraftStorage::onIOPortInterrupt(IOPort::IOPortInterrupt interrupt) {
    IOPort::IOPortResult *result  = new IOPort::IOPortResult;
    if (interrupt.ioPort.doubleWord == 0x101) {
        if (interrupt.data.doubleWord == 0) {
            curentMode = HDDMode::READ;
        } else if (interrupt.data.doubleWord == 1) {
            curentMode = HDDMode::WRITE;
        } else if (interrupt.data.doubleWord == 2) {
            curentMode = HDDMode::SEEK;
        }
    } else if (interrupt.ioPort.doubleWord == 0x102) {
        if (interrupt.read) {
            if (curentMode == HDDMode::READ) {
                result->returnData.word.lowWord.byte.lowByte = read();
            }
        }
    } else if (interrupt.ioPort.doubleWord == 0x103) {
        if (!interrupt.read) {
            if (curentMode == HDDMode::WRITE) {
                write(interrupt.data.word.lowWord.byte.lowByte);
            } else if (curentMode == HDDMode::SEEK) {
                seek(interrupt.data.doubleWord);
            }
        }
    }
    return result;
}

IOPort::IOPorts ElectroCraftStorage::getRequestedIOPorts() {
    IOPort::IOPorts ioPorts;
    ioPorts.ioPorts = new DoubleWord[3];
    ioPorts.ioPorts[0] = 0x101; // Control line
    ioPorts.ioPorts[1] = 0x102; // Read line
    ioPorts.ioPorts[2] = 0x103; // Write line
    ioPorts.number = 3;
    return ioPorts;
}

void ElectroCraftStorage::seek(int offset) {
    file.seekg(offset);
    file.seekp(offset);
}

void ElectroCraftStorage::write(Byte byte) {
    file<<byte;
    file.flush();
}

Byte ElectroCraftStorage::read() {
    Byte data;
    file>>data;
    return data;
}