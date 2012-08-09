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
        "main:",
        "mov ecx, 0",
        "mov ebx [0x1010000]",
        "mul ebx [0x1010004]",
        "input:",
        "inp eax, 0x122",
        "cmp eax, 0",
        "je input",
        "mov edx, [0x1010008]",
        "add edx, ecx",
        "mov [edx], eax",
        "cmp ecx, ebx",
        "je main",
        "add ecx, 1",
        "jmp input"
    };
    
    std::vector<std::string> simpleCallTest = {
        "main:",
        "push 1",
        "call test",
        "pop eax",
        "jmp main",
        "test:",
        "push ebx",
        "mov ebx, esp",
        "add [ebx + 8], 1",
        "mov esp, ebx",
        "pop ebx",
        "ret"
    };
    
    std::vector<std::string> ecAsmComplex = {
        ".code",
        "mov ecx, [0x1010008]",
        "main:",
        "push ecx",
        "call processKey",
        "pop emc",
        "cmp emc, 1",
        "je remove",
        "cmp emc, 2",
        "je doEnter",
        "add ecx, 1",
        "jmp main",
        "remove:",
        "sub ecx, 1",
        "mov [ecx], 0",
        "jmp main",
        "doEnter:",
        "add ecx, [0x1010000]",
        "jmp main",
        
        "processKey:",
        "push ebp",
        "mov ebp, esp",
        "sub esp, 4 ; Allocate 4 bytes to store result",
        "call getKey",
        "pop ebx",
        "cmp ebx, 8",
        "je backspace",
        "cmp ebx, 0xA",
        "je enter",
        "cmp ebx, 0xD",
        "je enter",
        "cmp ebx, 0",
        "je end",
        "push ebx",
        "push [ebp + 8]",
        "call displayKey",
        "add esp, 8",
        "mov [ebp + 8], 0",
        "jmp end",
        "enter:",
        "mov [ebp + 8] , 2",
        "jmp end",
        "backspace:",
        "mov [ebp + 8], 1",
        "end:",
        "mov esp, ebp",
        "pop ebp",
        "ret",
        
        "getKey:",
        "push ebp",
        "mov ebp, esp",
        "inp [ebp + 8], 0x122",
        "mov esp, ebp",
        "pop ebp",
        "ret",
        
        "displayKey:",
        "push ebp",
        "mov ebp, esp",
        "mov [ebp + 8], [ebp + 12]",
        "mov esp, ebp",
        "pop ebp",
        "ret"
    };
    
    std::vector<std::string> dbTest {
        ".data",
        "string db \"hello world how is it going today?\", 0, 1, 0",
        ".code",
        "mov eax, [0x1010008]",
        "mov [eax], string"
    };
    
    ElectroCraft_CPU *cpu = new ElectroCraft_CPU;
    AssembledData data = cpu->assemble(dbTest);
    cpu->start(cpu->loadIntoMemory(data.data, data.length, data.codeOffset));
    while (cpu->isRunning()) {
        cpu->getKeyboard()->onKeyPress(77);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

