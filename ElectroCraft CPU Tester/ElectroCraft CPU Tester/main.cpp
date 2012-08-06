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
    std::vector<std::string> ecAsm = {
        "; A Simple program that changes the display buffer randomly",
        ".code",
        "rand:",
        "mov eax, 0 ; move the lower limit of the random function to eax",
        "randi eax, 255 ; call the random function with the range of 0-255 and store the result in eax",
        "mov ecx, [0x8000] ; Get the value at 0x8000(The start of the VGA IO Memory) The value there is the size of the display buffer",
        "mov ebx, 0x8004 ; move the lower limit of the display buffer",
        "randi ebx, ecx ; get a random address between 0x8004 and the value of the size of the display buffer",
        "mov [ebx], eax ; set the pixel at the random address",
        "call sleep ; Sleep the program to prevent eating up CPU cycles",
        "jmp rand ; And jump back to the begining and repeat",
        "sleep:",
        "mov cx, 100 ; Loop 100 times",
        "nop ; No instruction",
        "loop sleep ; Loop while cx > 0",
        "ret ; return to the caller"
    };
    
    ElectroCraft_CPU *cpu = new ElectroCraft_CPU;
    AssembledData data = cpu->assemble(ecAsm);
    cpu->start(cpu->loadIntoMemory(data.data, data.length));
    while (cpu->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

