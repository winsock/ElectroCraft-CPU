//
//  ElectroCraftKeyboard.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/8/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include "ElectroCraftKeyboard.h"

void ElectroCraftKeyboard::onKeyPress(int keycode) {
    pressedKeys.push_back(keycode);
}

IOPort::IOPortResult *ElectroCraftKeyboard::onIOPortInterrupt(IOPort::IOPortInterrupt interrupt) {
    IOPort::IOPortResult *result  = new IOPort::IOPortResult;
    if (pressedKeys.size() <= 0) {
        return result;
    }
    
    if (interrupt.ioPort.doubleWord == 0x121) {
        if (!interrupt.read) {
            if (interrupt.data.doubleWord == 0) {
                pressedKeys.clear();
            }
        }
    } else if (interrupt.ioPort.doubleWord == 0x122) {
        if (interrupt.read) {
            result->returnData.word.lowWord.byte.lowByte = pressedKeys.back();
            pressedKeys.pop_back();
        }
    } else if (interrupt.ioPort.doubleWord == 0x123) {
        if (!interrupt.read) {
            
        }
    }
    return result;
}

IOPort::IOPorts ElectroCraftKeyboard::getRequestedIOPorts() {
    IOPort::IOPorts ioPorts;
    ioPorts.ioPorts = new DoubleWord[3];
    ioPorts.ioPorts[0] = 0x121; // Control line
    ioPorts.ioPorts[1] = 0x122; // Read line
    ioPorts.ioPorts[2] = 0x123; // Write line
    ioPorts.number = 3;
    return ioPorts;
}