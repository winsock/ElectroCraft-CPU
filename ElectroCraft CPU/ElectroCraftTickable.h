//
//  ElectroCraftTickable.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/5/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__ElectroCraftTickable__
#define __ElectroCraft_CPU__ElectroCraftTickable__

class ElectroCraftTickable {
public:
    virtual void operator()(long tickTime) = 0;
};

#endif /* defined(__ElectroCraft_CPU__ElectroCraftTickable__) */
