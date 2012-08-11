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
    
    ElectroCraft_CPU *cpu = new ElectroCraft_CPU;
    AssembledData data = cpu->assemble(startup);
    cpu->start(cpu->loadIntoMemory(data.data, data.length, data.codeOffset));
    while (cpu->isRunning()) {
        cpu->getKeyboard()->onKeyPress(77);
        cpu->getKeyboard()->onKeyPress(0xA);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return 0;
}

