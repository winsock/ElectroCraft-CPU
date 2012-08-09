//
//  ElectroCraftKeyboard.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/8/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__ElectroCraftKeyboard__
#define __ElectroCraft_CPU__ElectroCraftKeyboard__

#include <vector>
#include "IOPortDevice.h"

class ElectroCraftKeyboard : public IOPort::IOPortDevice {
    std::vector<int> pressedKeys;
public:
    void onKeyPress(int keycode);
    virtual IOPort::IOPortResult *onIOPortInterrupt(IOPort::IOPortInterrupt interrupt);
    virtual IOPort::IOPorts getRequestedIOPorts();
};

#endif /* defined(__ElectroCraft_CPU__ElectroCraftKeyboard__) */
