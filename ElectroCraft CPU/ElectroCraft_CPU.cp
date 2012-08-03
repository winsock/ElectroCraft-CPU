/*
 *  ElectroCraft_CPU.cp
 *  ElectroCraft CPU
 *
 *  Created by Andrew Querol on 8/3/12.
 *  Copyright (c) 2012 Cerios Software. All rights reserved.
 *
 */

#include <iostream>
#include "ElectroCraft_CPU.h"
#include "ElectroCraft_CPUPriv.h"

void ElectroCraft_CPU::HelloWorld(const char * s)
{
	 ElectroCraft_CPUPriv *theObj = new ElectroCraft_CPUPriv;
	 theObj->HelloWorldPriv(s);
	 delete theObj;
};

void ElectroCraft_CPUPriv::HelloWorldPriv(const char * s) 
{
	std::cout << s << std::endl;
};

