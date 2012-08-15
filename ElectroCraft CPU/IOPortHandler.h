//
//  IOPortHandler.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/8/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__IOPortHandler__
#define __ElectroCraft_CPU__IOPortHandler__

#include <map>
#include "Utils.h"
#include "IOPortDevice.h"

/*

 0x0 - 0x100 = System/BIOS
 0x101 - 0x103 = HDD
 0x104 - 0x110 = Terminal
 0x111 - 0x120 = VGA
 0x121 - 0x123 = Keyboard
 0x1000 - 0xFFFFFF = Minecraft
 
*/

namespace IOPort {
    class IOPortHandler {
        std::map<uint32_t, IOPortDevice*> ioPorts;
    public:
        void registerDevice(IOPortDevice *device);
        void unregisterPort(uint32_t port);
        IOPortResult *callIOPort(IOPortInterrupt interrupt);
    };
}

#endif /* defined(__ElectroCraft_CPU__IOPortHandler__) */
