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
#include "ElectroCraftKeyboard.h"
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
        "; Puts random text on the terminal buffer",
        "mov eax, 0x1010000",
        "mov edx, [eax + 8]",
        "resetLoop:",
        "mov ecx, [eax]",
        "mul ecx, [eax + 4]",
        "push edx",
        "main:",
        "add edx, 0x1",
        "mov [edx], 0x4c",
        "loop main",
        "pop edx",
        "jmp resetLoop",
    };
    
    std::vector<std::string> ecAsm3 = {
        "mov ecx, 0",
        "mov edx, 0",
        "mov emc, [0x1010008]",
        "mainLoop:",
        "inp eax, 0x122",
        "out 0x121, 0 ; Clear the buffer of any extra keys",
        "cmp eax, 0xA",
        "je enter",
        "push ecx",
        "mul ecx, edx",
        "push emc",
        "add emc, ecx",
        "mov [emc], [ebx]",
        "pop emc",
        "pop ecx",
        "add ecx, 1",
        "jmp end",
        "enter:",
        "add edx, 1",
        "jmp end",
        "end:",
        "jmp mainLoop",
    };
    
    ElectroCraft_CPU *cpu = new ElectroCraft_CPU;
    AssembledData data = cpu->assemble(ecAsm2);
    cpu->start(cpu->loadIntoMemory(data.data, data.length));
    while (cpu->isRunning()) {
        cpu->getKeyboard()->onKeyPress(77);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

