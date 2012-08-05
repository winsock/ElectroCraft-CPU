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
#include <thread>

int main(int argc, const char * argv[])
{
    std::vector<std::wstring> ecAsm = {
        L"PUSH EBX",
        L"PUSH EAX",
        L"MOV EAX, 10",
        L"MOV ECX, 5",
        L"loopl:",
        L"MOV EBX, 1",
        L"SUB EAX, EBX",
        L"loop loopl",
        L"halt:",
        L"HLT"
    };
    
    ElectroCraft_CPU *cpu = new ElectroCraft_CPU;
    AssembledData data = cpu->assemble(ecAsm);
    cpu->start(cpu->loadIntoMemory(data.data, data.length));
    while (cpu->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

