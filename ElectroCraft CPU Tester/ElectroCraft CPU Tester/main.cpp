//
//  main.cpp
//  ElectroCraft CPU Tester
//
//  Created by Andrew Querol on 8/3/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include <iostream>
#include "ElectroCraft_CPU.h"
#include <vector>
#include <string>

int main(int argc, const char * argv[])
{
    std::vector<std::string> ecAsm = {
        "PUSH EBX",
        "MOV EBX, 564123",
        "ADD EBX, 1",
        "PUSH EBX",
        "MOV EBX, 5",
        "SUB EBX, 1",
        "POP EAX",
        "ADD EAX, EBX",
        "POP EBX",
        "HLT"
    };
    
    ElectroCraft_CPU *cpu = new ElectroCraft_CPU;
    AssembledData data = cpu->assemble(ecAsm);
    cpu->execMemory(cpu->loadIntoMemory(data.data, data.length));
    return 0;
}

