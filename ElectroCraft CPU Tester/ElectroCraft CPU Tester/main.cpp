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
#include <fstream>
#include <sstream>

int main(int argc, const char * argv[])
{
    std::ifstream testInput("startup.xsm");
    std::vector<std::string> startup;
    
    while (testInput.good()) {
        std::string line;
        std::getline(testInput, line);
        if (line[0] != '\n' || line[0] != '\0') {
            startup.push_back(line);
        }
    }
    
    ElectroCraft_CPU *cpu = new ElectroCraft_CPU(320, 240, 24, 80, 16 * 1024 * 1024, 4096, 5000000);
    AssembledData data = cpu->assemble(startup);
    cpu->start(cpu->loadIntoMemory(data.data, data.length, data.codeOffset));
    while (cpu->isRunning()) {
        cpu->getKeyboard()->onKeyPress(0x8);
        cpu->getKeyboard()->onKeyPress(0x4C);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

