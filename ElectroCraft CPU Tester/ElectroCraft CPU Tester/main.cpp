//
//  main.cpp
//  ElectroCraft CPU Tester
//
//  Created by Andrew Querol on 8/3/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include <iostream>
#include "ElectroCraft_CPU.h"
#include "ElectroCraftTerminal.h"
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
        "mov ecx, [0x8004] ; Get the value at 0x8004 The value there is the size of the display buffer",
        "mov edx, [0x800C] ; Get the display buffer address",
        "add ecx, edx ; Add the lower limit of the diplay buffer",
        "randi edx, ecx ; get a random address between 0x8004 and the value of the size of the display buffer",
        "mov [edx], eax ; set the pixel at the random address",
        "mov cx, 100 ; Loop 100 times",
        "call sleep ; Sleep the program to prevent eating up CPU cycles",
        "jmp rand ; And jump back to the begining and repeat",
        "sleep:",
        "nop ; No instruction",
        "loop sleep ; Loop while cx > 0",
        "ret ; return to the caller"
    };
    
    std::vector<std::string> ecAsm2 = {
        "mov ebx, [0x1010004]",
        "mov ecx, [0x1010000]",
        "mov edx, [0x1010008]",
        "charLoop:",
        "push edx",
        "mov eax, 0",
        "loop:",
        "add edx, eax",
        "mov [edx], 0x4C",
        "cmp eax, ecx",
        "add eax, 1",
        "jne loop",
        "add edx, eax",
        "mov [edx], 0x0",
        "pop edx",
        "jmp charLoop",
        "hlt"
    };
    
    ElectroCraft_CPU *cpu = new ElectroCraft_CPU;
    AssembledData data = cpu->assemble(ecAsm2);
    cpu->start(cpu->loadIntoMemory(data.data, data.length));
    while (cpu->isRunning()) {
        cpu->getTerminal()->getLine(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

