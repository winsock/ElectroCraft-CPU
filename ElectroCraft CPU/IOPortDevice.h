//
//  IOPortDevice.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/8/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__IOPortDevice__
#define __ElectroCraft_CPU__IOPortDevice__

#include "Utils.h"

namespace IOPort {
    struct IOPorts {
        DoubleWord *ioPorts;
        unsigned int number;
    };
    
    struct IOPortInterrupt {
        DoubleWord ioPort;
        DoubleWord data;
        bool read = true;
    };
    
    struct IOPortResult {
        DoubleWord returnData;
    };
    
    class IOPortDevice {
    public:
        virtual IOPortResult *onIOPortInterrupt(IOPortInterrupt interrupt) = 0;
        virtual IOPorts getRequestedIOPorts() = 0;
    };
}

#endif /* defined(__ElectroCraft_CPU__IOPortDevice__) */
