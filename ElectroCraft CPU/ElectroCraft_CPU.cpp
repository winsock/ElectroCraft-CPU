/*
 *  ElectroCraft_CPU.cpp
 *  ElectroCraft CPU
 *
 *  Created by Andrew Querol on 8/3/12.
 *  Copyright (c) 2012 Cerios Software. All rights reserved.
 *
 */

#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include "ElectroCraft_CPU.h"
#include "ElectroCraftMemory.h"
#include "ElectroCraftStack.h"
#include "ElectroCraftVGA.h"
#include "ElectroCraftClock.h"
#include "Utils.h"

ElectroCraft_CPU::ElectroCraft_CPU() {
    Address baseAddress;
    baseAddress.doubleWord = 0x0;
    memory = new ElectroCraftMemory(128 * 1024 * 1024, baseAddress);
    videoCard = new ElectroCraftVGA(memory, 800, 600);
    stack = new ElectroCraftStack(memory, 4096);
    clock = new ElectroCraftClock(540000000);
    clock->registerCallback(this);
    clock->registerCallback(videoCard);
}

ElectroCraft_CPU::~ElectroCraft_CPU() {
    delete memory;
}

AssembledData ElectroCraft_CPU::assemble(std::vector<std::string> data) {
    // The first pass
    std::vector<FirstPassData> firstPassData;
    std::map<std::string, TokenData> tokens;
    DoubleWord currentOffset;
    currentOffset.doubleWord = 0;
    for (unsigned int line = 0; line < data.size(); line++) {
        FirstPassData *readData = firstPass(data[line], currentOffset);
        if (readData != nullptr) {
            if (!readData->token.name.empty()) {
                // Only way this can happen if there was a token returned
                tokens[readData->token.name] = readData->token;
            } else if (readData->opcode.opCode != InstructionSet::UNKOWN) {
                firstPassData.push_back(*readData);
                currentOffset.doubleWord += OPERATION_SZIE;
            }
        }
    }
    
    AssembledData assembledData;
    currentOffset.doubleWord = 0;
    
    // The second pass try to resolve unknown tokens
    for (FirstPassData &firstPass : firstPassData) {
        for (int i = 0; i < 2; i++) {
            if (!firstPass.unresolvedTokens[i].name.empty()) {
                if (tokens.find(firstPass.unresolvedTokens[i].name) != tokens.end()) {
                    firstPass.opcode.setOffsetInPosition(i);
                    long realOffset = tokens[firstPass.unresolvedTokens[i].name].offset.doubleWord - currentOffset.doubleWord;
                    if (realOffset < 0) {
                        firstPass.opcode.setOffsetNegitiveInPosition(i);
                        realOffset = -realOffset;
                    }
                    DoubleWord offset;
                    offset.doubleWord = static_cast<uint32_t>(realOffset);
                    firstPass.opcode.args[i] = offset;
                } else {
                    std::cerr<<"Unkown token: "<<&firstPass.unresolvedTokens[i].name<<std::endl;
                    assembledData.data = nullptr;
                    return assembledData;
                }
            }
        }
        currentOffset.doubleWord += OPERATION_SZIE;
    }
    
    // Finally pack the program into a byte array
    Byte* rawData = new Byte[currentOffset.doubleWord];
    for (int i = 0; i < firstPassData.size(); i++) {
        rawData[firstPassData[i].beginOffset.doubleWord] = firstPassData[i].opcode.getOpCodeByte();
        rawData[firstPassData[i].beginOffset.doubleWord + 1] = firstPassData[i].opcode.getInfoByte();
        rawData[firstPassData[i].beginOffset.doubleWord + 2] = firstPassData[i].opcode.getExtendedInfoByte();
        memcpy(&rawData[firstPassData[i].beginOffset.doubleWord + 3], &firstPassData[i].opcode.args[0].doubleWord, sizeof(uint32_t));
        memcpy(&rawData[firstPassData[i].beginOffset.doubleWord + 3 + sizeof(uint32_t)], &firstPassData[i].opcode.args[1].doubleWord, sizeof(uint32_t));
    }
    assembledData.data = rawData;
    assembledData.length = currentOffset.doubleWord;
    
    return assembledData;
}

FirstPassData* ElectroCraft_CPU::firstPass(std::string line, DoubleWord beginOffset) {
    if (line == ".code") {
        currentSection = Section::CODE;
        return nullptr;
    } else if (line == ".data") {
        currentSection = Section::DATA;
        return nullptr;
    }
    
    std::transform(line.begin(), line.end(), line.begin(), ::toupper);
    FirstPassData *data = new FirstPassData;
    data->beginOffset = beginOffset;
    
    std::stringstream tokenBuffer;
    int tokenNumber = 0;
    for (int i = 0; i < line.size(); i++) {
        // We only allow max of 1 Op Code and 2 arguments
        if (tokenNumber > 3) {
            break;
        }
        // Ignore comments
        if (line[i] == ';') {
            if (tokenNumber > 0)
                return data;
            else
                return nullptr;
        } else if (line[i] == ' ' || line[i] == ',' || &line[i] == &line.back()) {
            // If this is the last char make sure it gets put into the buffer
            if (line[i] == line.back()) {
                tokenBuffer<<line[i];
            }
            
            std::string token = tokenBuffer.str();
            tokenBuffer.str(std::string());
            if (token.size() > 0 && tokenNumber == 0) {
                if(token.back() == ':') {
                    TokenData tokenData;
                    tokenData.name = token.substr(0, token.size() - 1);
                    tokenData.offset = beginOffset;
                    data->token = tokenData;
                    return data;
                } else {
                    OPCode opcode = readOPCode(token);
                    if (opcode.opCode != InstructionSet::UNKOWN) {
                        data->opcode = opcode;
                    } else {
                        std::cerr<<"ElectroCraft CPU: Unknown operation: "<<token<<std::endl;
                        continue;
                    }
                }
                tokenNumber++;
            } else if (token.size() > 0 && tokenNumber > 0) {
                // Lets see if it is a register!
                Registers reg = getRegister(token);
                if (reg != Registers::UNKNOWN) {
                    data->opcode.infoBits.set(tokenNumber - 1);
                    data->opcode.args[tokenNumber - 1].doubleWord = reg;
                    tokenNumber++;
                } else {
                    // Lets see if is a address
                    if (token.front() == '[' && token.back() == ']') {
                        bool isHexNumber = (token.substr(1, token.size() - 2).find_first_not_of("X0123456789ABCDEF") == std::string::npos);
                        if (isHexNumber && *(&token.front() + 1) == '0' && *(&token.front() + 2) == 'X') {
                            std::stringstream ss;
                            ss << std::hex << token.substr(1, token.size() - 2);
                            if (ss.good()) {
                                DoubleWord address;
                                ss >> address.doubleWord;
                                data->opcode.args[tokenNumber - 1] = address;
                                data->opcode.setAddressInPosition(tokenNumber - 1);
                                tokenNumber++;
                            }
                        } else {
                            Registers reg = getRegister(token.substr(1, token.size() - 2));
                            if (reg != Registers::UNKNOWN) {
                                data->opcode.setShouldUseRegisterAsAddress(tokenNumber - 1);
                                data->opcode.args[tokenNumber - 1].doubleWord = reg;
                            }
                        }
                    } else {
                        bool isHexNumber = (token.find_first_not_of("X0123456789ABCDEF") == std::string::npos);
                        bool isDecimalNumber = (token.find_first_not_of("0123456789") == std::string::npos);
                        if (isDecimalNumber) {
                            // Lets see if it is a decimal number
                            std::stringstream ss;
                            ss << std::dec << token;
                            if (ss.good()) {
                                DoubleWord number;
                                ss >> number.doubleWord;
                                data->opcode.args[tokenNumber - 1].doubleWord = number.doubleWord;
                            }
                        } else if (isHexNumber) {
                            // Lets see if it is a hex number
                            std::stringstream ss;
                            ss<< std::hex << token;
                            if (ss) {
                                DoubleWord number;
                                ss >> number.doubleWord;
                                data->opcode.args[tokenNumber - 1] = number;
                            }
                        } else {
                            // Must be some token we haven't solved yet
                            TokenData unresolved;
                            unresolved.name = token;
                            data->unresolvedTokens[tokenNumber - 1] = unresolved;
                        }
                        tokenNumber++;
                    }
                }
            }
        } else {
            tokenBuffer<<line[i];
        }
    }
    
    if (tokenNumber == 0 || data->opcode.opCode == InstructionSet::UNKOWN) {
        return nullptr;
    }
    
    return data;
}

Registers ElectroCraft_CPU::getRegister(std::string token) {
    if (token == "EAX") {
        return Registers::EAX;
    } else if (token == "EBX") {
        return Registers::EBX;
    } else if (token == "ECX") {
        return Registers::ECX;
    } else if (token == "EDX") {
        return Registers::EDX;
    } else if (token == "ESI") {
        return Registers::ESI;
    } else if (token == "EDI") {
        return Registers::EDI;
    } else if (token == "EBP") {
        return Registers::EBP;
    } else if (token == "EIP") {
        return Registers::EIP;
    } else if (token == "ESP") {
        return Registers::ESP;
    } else if (token == "EMC") {
        return Registers::EMC;
    } else if (token == "EFLAGS") {
        return Registers::EFLAGS;
    } else if (token == "AX") {
        return Registers::AX;
    } else if (token == "BX") {
        return Registers::BX;
    } else if (token == "CX") {
        return Registers::CX;
    } else if (token == "DX") {
        return Registers::DX;
    } else if (token == "SI") {
        return Registers::SI;
    } else if (token == "DI") {
        return Registers::DI;
    } else if (token == "BP") {
        return Registers::BP;
    } else if (token == "IP") {
        return Registers::IP;
    } else if (token == "SP") {
        return Registers::SP;
    } else if (token == "MC") {
        return Registers::MC;
    } else if (token == "AH") {
        return Registers::AH;
    } else if (token == "A") {
        return Registers::AL;
    } else if (token == "BH") {
        return Registers::BH;
    } else if (token == "B") {
        return Registers::BL;
    } else if (token == "CH") {
        return Registers::CH;
    } else if (token == "C") {
        return Registers::CL;
    } else if (token == "DH") {
        return Registers::DH;
    } else if (token == "D") {
        return Registers::DL;
    } else {
        return Registers::UNKNOWN;
    }
}

OPCode ElectroCraft_CPU::readOPCode(std::string token) {
    OPCode opcode;
#pragma mark Instruction Set
    if (token == "PUSH") {
        opcode.opCode = InstructionSet::PUSH;
        opcode.numOprands = 1;
    } else if (token == "POP") {
        opcode.opCode = InstructionSet::POP;
        opcode.numOprands = 1;
    } else if (token == "HLT") {
        opcode.opCode = InstructionSet::HLT;
        opcode.numOprands = 0;
    } else if (token == "JE") {
        opcode.opCode = InstructionSet::JE;
        opcode.numOprands = 1;
    } else if (token == "JNE") {
        opcode.opCode = InstructionSet::JNE;
        opcode.numOprands = 1;
    } else if (token == "MOV") {
        opcode.opCode = InstructionSet::MOV;
        opcode.numOprands = 2;
    } else if (token == "CMP") {
        opcode.opCode = InstructionSet::CMP;
        opcode.numOprands = 2;
    } else if (token == "NOP") {
        opcode.opCode = InstructionSet::NOP;
        opcode.numOprands = 0;
    } else if (token == "LOOPE") {
        opcode.opCode = InstructionSet::LOOPE;
        opcode.numOprands = 1;
    } else if (token == "LOOPNE") {
        opcode.opCode = InstructionSet::LOOPNE;
        opcode.numOprands = 1;
    } else if (token == "NOT") {
        opcode.opCode = InstructionSet::NOT;
        opcode.numOprands = 2;
    } else if (token == "AND") {
        opcode.opCode = InstructionSet::AND;
        opcode.numOprands = 2;
    } else if (token == "OR") {
        opcode.opCode = InstructionSet::OR;
        opcode.numOprands = 2;
    } else if (token == "XOR") {
        opcode.opCode = InstructionSet::XOR;
        opcode.numOprands = 2;
    } else if (token == "MU") {
        opcode.opCode = InstructionSet::MUL;
        opcode.numOprands = 2;
    } else if (token == "ADD") {
        opcode.opCode = InstructionSet::ADD;
        opcode.numOprands = 2;
    } else if (token == "DIV") {
        opcode.opCode = InstructionSet::DIV;
        opcode.numOprands = 2;
    } else if (token == "SUB") {
        opcode.opCode = InstructionSet::SUB;
        opcode.numOprands = 2;
    } else if (token == "RET") {
        opcode.opCode = InstructionSet::RET;
        opcode.numOprands = 0;
    } else if (token == "POPA") {
        opcode.opCode = InstructionSet::POPA;
        opcode.numOprands = 0;
    } else if (token == "PUSHA") {
        opcode.opCode = InstructionSet::PUSHA;
        opcode.numOprands = 0;
    } else if (token == "SA") {
        opcode.opCode = InstructionSet::SHL;
        opcode.numOprands = 2;
    } else if (token == "SAR") {
        opcode.opCode = InstructionSet::SHR;
        opcode.numOprands = 2;
    } else if (token == "LOOPWE") {
        opcode.opCode = InstructionSet::LOOPWE;
        opcode.numOprands = 1;
    } else if (token == "LOOPWNE") {
        opcode.opCode = InstructionSet::LOOPWNE;
        opcode.numOprands = 1;
    } else if (token == "JNZ") {
        opcode.opCode = InstructionSet::JNZ;
        opcode.numOprands = 1;
    } else if (token == "JZ") {
        opcode.opCode = InstructionSet::JZ;
        opcode.numOprands = 1;
    } else if (token == "CPUID") {
        opcode.opCode = InstructionSet::CPUID;
        opcode.numOprands = 0;
    } else if (token == "ROUND") {
        opcode.opCode = InstructionSet::ROUND;
        opcode.numOprands = 2;
    } else if (token == "LOOPZ") {
        opcode.opCode = InstructionSet::LOOPZ;
        opcode.numOprands = 1;
    } else if (token == "LOOPNZ") {
        opcode.opCode = InstructionSet::LOOPNZ;
        opcode.numOprands = 1;
    } else if (token == "CALL") {
        opcode.opCode = InstructionSet::CALL;
        opcode.numOprands = 1;
    } else if (token == "JMP") {
        opcode.opCode = InstructionSet::JMP;
        opcode.numOprands = 1;
    } else if (token == "NEG") {
        opcode.opCode = InstructionSet::NEG;
        opcode.numOprands = 1;
    } else if (token == "LOOP") {
        opcode.opCode = InstructionSet::LOOP;
        opcode.numOprands = 1;
    } else if (token == "RANDI") {
        opcode.opCode = InstructionSet::RANDI;
        opcode.numOprands = 2;
    } else {
        opcode.opCode = InstructionSet::UNKOWN;
        opcode.numOprands = 0;
    }
    return opcode;
}

DoubleWord ElectroCraft_CPU::getRegisterData(Registers reg) {
    DoubleWord data;
    switch (reg) {
        case Registers::AL:
        case Registers::AH:
        case Registers::AX:
        case Registers::EAX:
            data = registers.A;
            break;
        case Registers::BL:
        case Registers::BH:
        case Registers::BX:
        case Registers::EBX:
            data = registers.B;
            break;
        case Registers::CL:
        case Registers::CH:
        case Registers::CX:
        case Registers::ECX:
            data = registers.C;
            break;
        case Registers::DL:
        case Registers::DH:
        case Registers::DX:
        case Registers::EDX:
            data = registers.D;
            break;
        case Registers::SI:
        case Registers::ESI:
            data = registers.SI;
            break;
        case Registers::DI:
        case Registers::EDI:
            data = registers.DI;
            break;
        case Registers::BP:
        case Registers::EBP:
            data = registers.BP;
            break;
        case Registers::IP:
        case Registers::EIP:
            data = registers.IP;
            break;
        case Registers::SP:
        case Registers::ESP:
            data = registers.SP;
            break;
        case Registers::MC:
        case Registers::EMC:
            data = registers.MC;
            break;
        case Registers::EFLAGS:
        {
            data.doubleWord = static_cast<uint32_t>(eflags.flagStates.to_ulong());
        }
        case Registers::UNKNOWN:
        default:
            std::cerr<<"ElectroCraft CPU: Unknown register number: "<<reg<<std::endl;
            break;
    }
    return data;
}

void ElectroCraft_CPU::setRegisterData(DoubleWord data, Registers reg) {
    switch (reg) {
        case Registers::AX:
        case Registers::EAX:
            registers.A = data;
            break;
        case Registers::BX:
        case Registers::EBX:
            registers.B = data;
            break;
        case Registers::CX:
        case Registers::ECX:
            registers.C = data;
            break;
        case Registers::DX:
        case Registers::EDX:
            registers.D = data;
            break;
        case Registers::SI:
        case Registers::ESI:
            registers.SI = data;
            break;
        case Registers::DI:
        case Registers::EDI:
            registers.DI = data;
            break;
        case Registers::BP:
        case Registers::EBP:
            registers.BP = data;
            break;
        case Registers::IP:
        case Registers::EIP:
            registers.IP = data;
            break;
        case Registers::SP:
        case Registers::ESP:
            registers.SP = data;
            break;
        case Registers::MC:
        case Registers::EMC:
            registers.MC = data;
            break;
        case Registers::EFLAGS:
            eflags.flagStates = std::bitset<EFLAGS::TOTAL_SIZE>(data.doubleWord);
            break;
        case Registers::AL:
            registers.A.word.lowWord.byte.lowByte = data.word.lowWord.byte.lowByte;
            break;
        case Registers::AH:
            registers.A.word.lowWord.byte.hiByte = data.word.lowWord.byte.hiByte;
            break;
        case Registers::BL:
            registers.B.word.lowWord.byte.lowByte = data.word.lowWord.byte.lowByte;
            break;
        case Registers::BH:
            registers.B.word.lowWord.byte.hiByte = data.word.lowWord.byte.hiByte;
            break;
        case Registers::CL:
            registers.C.word.lowWord.byte.lowByte = data.word.lowWord.byte.lowByte;
            break;
        case Registers::CH:
            registers.C.word.lowWord.byte.hiByte = data.word.lowWord.byte.hiByte;
            break;
        case Registers::DL:
            registers.D.word.lowWord.byte.lowByte = data.word.lowWord.byte.lowByte;
            break;
        case Registers::DH:
            registers.D.word.lowWord.byte.hiByte = data.word.lowWord.byte.hiByte;
            break;
        case Registers::UNKNOWN:
        default:
            std::cerr<<"ElectroCraft CPU: Unknown register number: "<<reg<<std::endl;
            break;
    }
}

RegisterSizes ElectroCraft_CPU::registerToSize(Registers reg) {
    switch (reg) {
        case Registers::EAX:
        case Registers::EBX:
        case Registers::ECX:
        case Registers::EDX:
        case Registers::ESI:
        case Registers::EDI:
        case Registers::EBP:
        case Registers::EIP:
        case Registers::ESP:
        case Registers::EMC:
        case Registers::EFLAGS:
            return RegisterSizes::FOUR;
            
        case Registers::AX:
        case Registers::BX:
        case Registers::CX:
        case Registers::DX:
        case Registers::SI:
        case Registers::DI:
        case Registers::BP:
        case Registers::IP:
        case Registers::SP:
        case Registers::MC:            
            return RegisterSizes::TWO;
            
        case Registers::AL:
        case Registers::AH:
        case Registers::BL:
        case Registers::BH:
        case Registers::CL:
        case Registers::CH:
        case Registers::DL:
        case Registers::DH:
            return RegisterSizes::ONE;
            
        case Registers::UNKNOWN:
        default:
            std::cerr<<"ElectroCraft CPU: Unknown register number: "<<reg<<std::endl;
            return RegisterSizes::ZERO;
    }
}

RegisterType ElectroCraft_CPU::getRegisterType(Registers reg) {
    switch (reg) {
        case Registers::EAX:
        case Registers::EBX:
        case Registers::ECX:
        case Registers::EDX:
        case Registers::ESI:
        case Registers::EDI:
        case Registers::EBP:
        case Registers::EIP:
        case Registers::ESP:
        case Registers::EMC:
        case Registers::EFLAGS:
        case Registers::AX:
        case Registers::BX:
        case Registers::CX:
        case Registers::DX:
        case Registers::SI:
        case Registers::DI:
        case Registers::BP:
        case Registers::IP:
        case Registers::SP:
        case Registers::MC:
            return RegisterType::NEITHER;
            
        case Registers::AL:
        case Registers::BL:
        case Registers::CL:
        case Registers::DL:
            return RegisterType::LOW;
            
        case Registers::AH:
        case Registers::BH:
        case Registers::CH:
        case Registers::DH:
            return RegisterType::HIGH;
            
        case Registers::UNKNOWN:
        default:
            std::cerr<<"ElectroCraft CPU: Unknown register number: "<<reg<<std::endl;
            return RegisterType::NEITHER;
    }
}

void ElectroCraft_CPU::operator()(long tickTime) {
    Byte* instructionData;
    OPCode instruction;
    bool hasJumped = false;
    instructionData = memory->readData(registers.IP, OPERATION_SZIE);
    instruction.opCodeFromByte(instructionData[0]);
    instruction.readInfoByte(instructionData[1]);
    instruction.readExtendedInfoByte(instructionData[2]);
    instruction.args[0] = Utils::readDoubleWord(&instructionData[3]);
    instruction.args[1] = Utils::readDoubleWord(&instructionData[7]);
    switch (instruction.opCode) {
        case InstructionSet::ADD:
            if ((instruction.isRegisterInPosition(0) || instruction.isRegisterInPosition(1) || instruction.isAddressInPosition(0) || instruction.isAddressInPosition(1)) && (instruction.numOprands > 1)) {
                Registers reg = Registers::UNKNOWN;
                Registers reg1 = Registers::UNKNOWN;
                RegisterSizes regSize = RegisterSizes::ZERO;
                RegisterSizes regSize1 = RegisterSizes::ZERO;
                Byte* data = nullptr;
                Byte* data1 = nullptr;
                
                if (instruction.isRegisterInPosition(0)) {
                    reg = Registers(instruction.args[0].doubleWord);
                    regSize = registerToSize(reg);
                } else {
                    data = Utils::doubleWordToBytes(instruction.args[0]);
                }
                
                if (instruction.isRegisterInPosition(1)) {
                    reg1 = Registers(instruction.args[1].doubleWord);
                    regSize1 = registerToSize(reg1);
                } else {
                    data1 = Utils::doubleWordToBytes(instruction.args[1]);
                }
                
                if (reg != Registers::UNKNOWN) {
                    switch (regSize) {
                        case RegisterSizes::FOUR:
                            data = memory->readData(getRegisterData(reg), 4);
                            break;
                        case RegisterSizes::TWO:
                            data = memory->readData(getRegisterData(reg), 2);
                        case RegisterSizes::ONE:
                            data = memory->readData(getRegisterData(reg), 1);
                        default:
                            break;
                    }
                }
                
                if (reg1 != Registers::UNKNOWN) {
                    switch (regSize1) {
                        case RegisterSizes::FOUR:
                            data1 = memory->readData(getRegisterData(reg1), 4);
                            break;
                        case RegisterSizes::TWO:
                            data1 = memory->readData(getRegisterData(reg1), 2);
                        case RegisterSizes::ONE:
                            data1 = memory->readData(getRegisterData(reg1), 1);
                        default:
                            break;
                    }
                }
                
                if(data != nullptr && data1 != nullptr) {
                    // <REG>, <REG>
                    if (regSize != RegisterSizes::ZERO && regSize1 != RegisterSizes::ZERO) {
                        if (regSize == RegisterSizes::FOUR || regSize1 == RegisterSizes::FOUR) {
                            DoubleWord result;
                            if (regSize < regSize1) {
                                std::cerr<<"ElectroCraft CPU: Invaid arguments for ADD: First register too small for storage!"<<std::endl;
                            } else if(regSize > regSize1) {
                                if (regSize1 == RegisterSizes::TWO) {
                                    result.doubleWord = getRegisterData(reg).doubleWord + getRegisterData(reg1).word.lowWord.word;
                                } else {
                                    if (getRegisterType(reg1) == RegisterType::HIGH) {
                                        result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord + getRegisterData(reg1).word.lowWord.byte.hiByte;
                                    } else {
                                        result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord + getRegisterData(reg1).word.lowWord.byte.lowByte;
                                    }
                                }
                                setRegisterData(result, reg);
                            } else {
                                result.doubleWord = getRegisterData(reg).doubleWord + getRegisterData(reg1).doubleWord;
                                setRegisterData(result, reg);
                            }
                        } else if (regSize == RegisterSizes::TWO || regSize1 == RegisterSizes::TWO) {
                            DoubleWord result;
                            if (regSize < regSize1) {
                                std::cerr<<"ElectroCraft CPU: Invaid arguments for ADD: First register too small for storage!"<<std::endl;
                            } else if(regSize > regSize1) {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.word = getRegisterData(reg).word.lowWord.word + getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.word = getRegisterData(reg).word.lowWord.word + getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                                setRegisterData(result, reg);
                            } else {
                                result.doubleWord = getRegisterData(reg).word.lowWord.word + getRegisterData(reg1).word.lowWord.word;
                                setRegisterData(result, reg);
                            }
                        } else {
                            DoubleWord result;
                            if (getRegisterType(reg) == RegisterType::HIGH) {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte + getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte + getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                            } else {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.lowByte + getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.lowByte + getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                            }
                            setRegisterData(result, reg);
                        }
                        // [Address], <Register, Const>
                    } if (instruction.isAddressInPosition(0)) {
                        if (regSize1 != RegisterSizes::ZERO) {
                            if (regSize1 == RegisterSizes::FOUR) {
                                DoubleWord result;
                                result.doubleWord = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data), 4)).doubleWord + getRegisterData(reg1).doubleWord;
                                memory->writeData(Utils::readDoubleWord(data), 4, Utils::doubleWordToBytes(result));
                            } else if (regSize1 == RegisterSizes::TWO) {
                                DoubleWord result;
                                result.word.lowWord.word = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data), 4)).word.lowWord.word + getRegisterData(reg1).word.lowWord.word;
                                memory->writeData(Utils::readDoubleWord(data), 4, Utils::doubleWordToBytes(result));
                            } else {
                                DoubleWord *result = new DoubleWord;
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result->word.lowWord.byte.hiByte = *memory->readData(Utils::readDoubleWord(data), 1) + getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result->word.lowWord.byte.lowByte = *memory->readData(Utils::readDoubleWord(data), 1) + getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                                memory->writeData(Utils::readDoubleWord(data1), 4, Utils::doubleWordToBytes(*result));
                            }
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for ADD [Address], <Register, Const>"<<std::endl;
                        }
                        // <Register>, [Address]
                    } else if (instruction.isAddressInPosition(1)) {
                        if (regSize != RegisterSizes::ZERO) {
                            if (regSize == RegisterSizes::FOUR) {
                                DoubleWord result;
                                result.doubleWord = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data1), 4)).doubleWord + getRegisterData(reg).doubleWord;
                                setRegisterData(result, reg);
                            } else if (regSize == RegisterSizes::TWO) {
                                DoubleWord result;
                                result.word.lowWord.word = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data1), 4)).word.lowWord.word + getRegisterData(reg).word.lowWord.word;
                                setRegisterData(result, reg);
                            } else {
                                DoubleWord result;
                                if (getRegisterType(reg) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte + *memory->readData(Utils::readDoubleWord(data1), 1);
                                } else {
                                    result.word.lowWord.byte.lowByte = getRegisterData(reg).word.lowWord.byte.lowByte + *memory->readData(Utils::readDoubleWord(data1), 1);
                                }
                                setRegisterData(result, reg);
                            }
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for ADD <Register>, [Address]!"<<std::endl;
                        }
                    } else if (regSize != RegisterSizes::ZERO && data1 != nullptr) {
                        // <REG>, <CONST>
                        DoubleWord result;
                        if (regSize == RegisterSizes::FOUR) {
                            result.doubleWord = getRegisterData(reg).doubleWord + Utils::readDoubleWord(data1).doubleWord;
                            setRegisterData(result, reg);
                        } else if (regSize == RegisterSizes::TWO) {
                            result.word.lowWord.word = getRegisterData(reg).word.lowWord.word + Utils::readDoubleWord(data1).doubleWord;
                            setRegisterData(result, reg);
                        } else if (regSize == RegisterSizes::ONE) {
                            if (getRegisterType(reg) == RegisterType::HIGH) {
                                result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord + Utils::readDoubleWord(data1).doubleWord;
                            } else {
                                result.word.lowWord.byte.lowByte = getRegisterData(reg).doubleWord + Utils::readDoubleWord(data1).doubleWord;
                            }
                            setRegisterData(result, reg);
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for ADD!"<<std::endl;
                        }
                    } else {
                        std::cerr<<"ElectroCraft CPU: Invaid arguments for ADD!"<<std::endl;
                    }
                    
                } else {
                    std::cerr<<"ElectroCraft CPU: Invaid arguments for ADD!"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for ADD!"<<std::endl;
            }
            break;
        case InstructionSet::AND:
            if ((instruction.isRegisterInPosition(0) || instruction.isRegisterInPosition(1) || instruction.isAddressInPosition(0) || instruction.isAddressInPosition(1)) && (instruction.numOprands > 1)) {
                Registers reg = Registers::UNKNOWN;
                Registers reg1 = Registers::UNKNOWN;
                RegisterSizes regSize = RegisterSizes::ZERO;
                RegisterSizes regSize1 = RegisterSizes::ZERO;
                Byte* data = nullptr;
                Byte* data1 = nullptr;
                
                if (instruction.isRegisterInPosition(0)) {
                    reg = Registers(instruction.args[0].doubleWord);
                    regSize = registerToSize(reg);
                } else {
                    data = Utils::doubleWordToBytes(instruction.args[0]);
                }
                
                if (instruction.isRegisterInPosition(1)) {
                    reg1 = Registers(instruction.args[1].doubleWord);
                    regSize1 = registerToSize(reg1);
                } else {
                    data1 = Utils::doubleWordToBytes(instruction.args[1]);
                }
                
                if (reg != Registers::UNKNOWN) {
                    switch (regSize) {
                        case RegisterSizes::FOUR:
                            data = memory->readData(getRegisterData(reg), 4);
                            break;
                        case RegisterSizes::TWO:
                            data = memory->readData(getRegisterData(reg), 2);
                        case RegisterSizes::ONE:
                            data = memory->readData(getRegisterData(reg), 1);
                        default:
                            break;
                    }
                }
                
                if (reg1 != Registers::UNKNOWN) {
                    switch (regSize1) {
                        case RegisterSizes::FOUR:
                            data1 = memory->readData(getRegisterData(reg1), 4);
                            break;
                        case RegisterSizes::TWO:
                            data1 = memory->readData(getRegisterData(reg1), 2);
                        case RegisterSizes::ONE:
                            data1 = memory->readData(getRegisterData(reg1), 1);
                        default:
                            break;
                    }
                }
                
                if(data != nullptr && data1 != nullptr) {
                    // <REG>, <REG>
                    if (regSize != RegisterSizes::ZERO && regSize1 != RegisterSizes::ZERO) {
                        if (regSize == RegisterSizes::FOUR || regSize1 == RegisterSizes::FOUR) {
                            DoubleWord result;
                            if (regSize < regSize1) {
                                std::cerr<<"ElectroCraft CPU: Invaid arguments for AND: First register too small for storage!"<<std::endl;
                            } else if(regSize > regSize1) {
                                if (regSize1 == RegisterSizes::TWO) {
                                    result.doubleWord = getRegisterData(reg).doubleWord - getRegisterData(reg1).word.lowWord.word;
                                } else {
                                    if (getRegisterType(reg1) == RegisterType::HIGH) {
                                        result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord & getRegisterData(reg1).word.lowWord.byte.hiByte;
                                    } else {
                                        result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord & getRegisterData(reg1).word.lowWord.byte.lowByte;
                                    }
                                }
                                setRegisterData(result, reg);
                            } else {
                                result.doubleWord = getRegisterData(reg).doubleWord & getRegisterData(reg1).doubleWord;
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else if (regSize == RegisterSizes::TWO || regSize1 == RegisterSizes::TWO) {
                            DoubleWord result;
                            if (regSize < regSize1) {
                                std::cerr<<"ElectroCraft CPU: Invaid arguments for AND: First register too small for storage!"<<std::endl;
                            } else if(regSize > regSize1) {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.word = getRegisterData(reg).word.lowWord.word & getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.word = getRegisterData(reg).word.lowWord.word & getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                                setRegisterData(result, reg);
                            } else {
                                result.doubleWord = getRegisterData(reg).word.lowWord.word & getRegisterData(reg1).word.lowWord.word;
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else {
                            DoubleWord result;
                            if (getRegisterType(reg) == RegisterType::HIGH) {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte & getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte & getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                            } else {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.lowByte & getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.lowByte & getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                            setRegisterData(result, reg);
                        }
                        // [Address], <Register, Const>
                    } if (instruction.isAddressInPosition(0)) {
                        if (regSize1 != RegisterSizes::ZERO) {
                            if (regSize1 == RegisterSizes::FOUR) {
                                DoubleWord result;
                                result.doubleWord = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data), 4)).doubleWord & getRegisterData(reg1).doubleWord;
                                if (result.doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data), 4, Utils::doubleWordToBytes(result));
                            } else if (regSize1 == RegisterSizes::TWO) {
                                DoubleWord result;
                                result.word.lowWord.word = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data), 4)).word.lowWord.word & getRegisterData(reg1).word.lowWord.word;
                                if (result.doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data), 4, Utils::doubleWordToBytes(result));
                            } else {
                                DoubleWord *result = new DoubleWord;
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result->word.lowWord.byte.hiByte = *memory->readData(Utils::readDoubleWord(data), 1) & getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result->word.lowWord.byte.lowByte = *memory->readData(Utils::readDoubleWord(data), 1) & getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                                if (result->doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data1), 4, Utils::doubleWordToBytes(*result));
                            }
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for AND [Address], <Register, Const>"<<std::endl;
                        }
                        // <Register>, [Address]
                    } else if (instruction.isAddressInPosition(1)) {
                        if (regSize != RegisterSizes::ZERO) {
                            DoubleWord result;
                            if (regSize == RegisterSizes::FOUR) {
                                result.doubleWord = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data1), 4)).doubleWord & getRegisterData(reg).doubleWord;
                                setRegisterData(result, reg);
                            } else if (regSize == RegisterSizes::TWO) {
                                result.word.lowWord.word = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data1), 4)).word.lowWord.word & getRegisterData(reg).word.lowWord.word;
                                setRegisterData(result, reg);
                            } else {
                                if (getRegisterType(reg) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte & *memory->readData(Utils::readDoubleWord(data1), 1);
                                } else {
                                    result.word.lowWord.byte.lowByte = getRegisterData(reg).word.lowWord.byte.lowByte & *memory->readData(Utils::readDoubleWord(data1), 1);
                                }
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for AND <Register>, [Address]!"<<std::endl;
                        }
                    } else if (regSize != RegisterSizes::ZERO && data1 != nullptr) {
                        // <REG>, <CONST>
                        DoubleWord result;
                        if (regSize == RegisterSizes::FOUR) {
                            result.doubleWord = getRegisterData(reg).doubleWord & Utils::readDoubleWord(data1).doubleWord;
                            setRegisterData(result, reg);
                        } else if (regSize == RegisterSizes::TWO) {
                            result.word.lowWord.word = getRegisterData(reg).word.lowWord.word & Utils::readDoubleWord(data1).doubleWord;
                            setRegisterData(result, reg);
                        } else if (regSize == RegisterSizes::ONE) {
                            if (getRegisterType(reg) == RegisterType::HIGH) {
                                result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord & Utils::readDoubleWord(data1).doubleWord;
                            } else {
                                result.word.lowWord.byte.lowByte = getRegisterData(reg).doubleWord & Utils::readDoubleWord(data1).doubleWord;
                            }
                            setRegisterData(result, reg);
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for AND!"<<std::endl;
                        }
                        if (result.doubleWord == 0) {
                            eflags.setFlagState(EFLAGS::ZF);
                        }
                    } else {
                        std::cerr<<"ElectroCraft CPU: Invaid arguments for AND!"<<std::endl;
                    }
                    
                } else {
                    std::cerr<<"ElectroCraft CPU: Invaid arguments for AND!"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for AND!"<<std::endl;
            }
            break;
        case InstructionSet::CALL:
            if (instruction.isOffsetInPosition(0)) {
                Address addressToCall;
                if (instruction.isOffsetNegitive(0)) {
                    addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                } else {
                    addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                }
                stack->push(getRegisterData(Registers::EIP));
                setRegisterData(addressToCall, Registers::EIP);
                hasJumped = true;
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for CALL <Label>!"<<std::endl;
            }
            break;
        case InstructionSet::CMP:
            if (instruction.isAddressInPosition(0)) {
                if (instruction.isRegisterInPosition(1)) {
                    if ((Utils::readDoubleWord(memory->readData(instruction.args[0], 4)).doubleWord  - getRegisterData(Registers(instruction.args[1].doubleWord)).doubleWord) == 0) {
                        eflags.setFlagState(EFLAGS::ZF);
                    } else {
                        eflags.resetFlag(EFLAGS::ZF);
                    }
                } else {
                    std::cerr<<"ElectroCraft CPU: Invaid arguments for CMP [address], <register>!"<<std::endl;
                }
            } else if (instruction.isRegisterInPosition(0)) {
                if (instruction.isAddressInPosition(1)) {
                    if ((Utils::readDoubleWord(memory->readData(instruction.args[1], 4)).doubleWord  - getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord) == 0) {
                        eflags.setFlagState(EFLAGS::ZF);
                    } else {
                        eflags.resetFlag(EFLAGS::ZF);
                    }
                } else if (instruction.isRegisterInPosition(1)) {
                    if ((getRegisterData(Registers(instruction.args[1].doubleWord)).doubleWord - getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord) == 0) {
                        eflags.setFlagState(EFLAGS::ZF);
                    } else {
                        eflags.resetFlag(EFLAGS::ZF);
                    }
                } else {
                    if ((instruction.args[1].doubleWord - getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord) == 0) {
                        eflags.setFlagState(EFLAGS::ZF);
                    } else {
                        eflags.resetFlag(EFLAGS::ZF);
                    }
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for CMP!"<<std::endl;
            }
            break;
        case InstructionSet::CPUID:
            break;
        case InstructionSet::DIV:
            if (instruction.isRegisterInPosition(0)) {
                if (instruction.isRegisterInPosition(1)) {
                    DoubleWord result;
                    result.doubleWord = getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord / getRegisterData(Registers(instruction.args[1].doubleWord)).doubleWord;
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                } else if (instruction.isAddressInPosition(1)) {
                    DoubleWord result;
                    result.doubleWord = getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord / Utils::readDoubleWord(memory->readData(instruction.args[1], 4)).doubleWord;
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                } else {
                    std::cerr<<"ElectroCraft CPU: Invalid arguments for DIV!"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invalid arguments for DIV!"<<std::endl;
            }
            break;
        case InstructionSet::HLT:
            this->stop();
            return;
        case InstructionSet::JE:
            if (instruction.isOffsetInPosition(0)) {
                if (eflags.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for JE!"<<std::endl;
            }
            break;
        case InstructionSet::JMP:
            if (instruction.isOffsetInPosition(0)) {
                Address addressToCall;
                if (instruction.isOffsetNegitive(0)) {
                    addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                } else {
                    addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                }
                setRegisterData(addressToCall, Registers::EIP);
                hasJumped = true;
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for JMP!"<<std::endl;
            }
            break;
        case InstructionSet::JNE:
            if (instruction.isOffsetInPosition(0)) {
                if (!eflags.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for JNE!"<<std::endl;
            }
            break;
        case InstructionSet::JNZ:
            if (instruction.isOffsetInPosition(0)) {
                if (!eflags.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for JNZ!"<<std::endl;
            }
            break;
        case InstructionSet::JZ:
            if (instruction.isOffsetInPosition(0)) {
                if (eflags.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for JZ!"<<std::endl;
            }
            break;
        case InstructionSet::LOOP:
            if (instruction.isOffsetInPosition(0)) {
                DoubleWord counter = getRegisterData(Registers::CX);
                if (counter.word.lowWord.word > 0) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    counter.word.lowWord.word--;
                    setRegisterData(counter, Registers::CX);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOP!"<<std::endl;
            }
            break;
        case InstructionSet::LOOPE:
            if (instruction.isOffsetInPosition(0)) {
                if (eflags.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPE!"<<std::endl;
            }
            break;
        case InstructionSet::LOOPNE:
            if (instruction.isOffsetInPosition(0)) {
                if (!eflags.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPNE!"<<std::endl;
            }
            break;
        case InstructionSet::LOOPNZ:
            if (instruction.isOffsetInPosition(0)) {
                if (!eflags.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPNZ!"<<std::endl;
            }
            break;
        case InstructionSet::LOOPWE:
            if (instruction.isOffsetInPosition(0)) {
                if (eflags.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPE!"<<std::endl;
            }
            break;
        case InstructionSet::LOOPWNE:
            if (instruction.isOffsetInPosition(0)) {
                if (!eflags.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPNZ!"<<std::endl;
            }
            break;
        case InstructionSet::LOOPZ:
            if (instruction.isOffsetInPosition(0)) {
                if (eflags.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - instruction.args[0].doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + instruction.args[0].doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPE!"<<std::endl;
            }
            break;
        case InstructionSet::MOV:
            if ((instruction.isRegisterInPosition(0) || instruction.isRegisterInPosition(1) || instruction.isAddressInPosition(0) || instruction.isAddressInPosition(1)) && (instruction.numOprands > 1)) {
                Registers reg = Registers::UNKNOWN;
                Registers reg1 = Registers::UNKNOWN;
                RegisterSizes regSize = RegisterSizes::ZERO;
                RegisterSizes regSize1 = RegisterSizes::ZERO;
                Address address;
                Address address1;
                DoubleWord* optionalConstant = nullptr;
                
                if (instruction.isRegisterInPosition(0)) {
                    if (instruction.shouldUseRegisterAsAddress(0)) {
                        address = getRegisterData(Registers(instruction.args[0].doubleWord));
                    } else {
                        reg = Registers(instruction.args[0].doubleWord);
                        regSize = registerToSize(reg);
                    }
                } else if (instruction.isAddressInPosition(0)) {
                    address = instruction.args[0];
                } else {
                    std::cerr<<"ElectroCraft CPU: Invaid arguments for MOV: Has to be a register or address for the first argument"<<std::endl;
                    break;
                }
                
                if (instruction.isRegisterInPosition(1)) {
                    reg1 = Registers(instruction.args[1].doubleWord);
                    regSize1 = registerToSize(reg1);
                } else if (instruction.isAddressInPosition(1)) {
                    address1 = instruction.args[1];
                } else if (instruction.args != nullptr){
                    optionalConstant = &instruction.args[1];
                }
                
                if(optionalConstant != nullptr) {
                    if (reg != Registers::UNKNOWN) {
                        setRegisterData(*optionalConstant, reg);
                    } else {
                        memory->writeData(address, 4, Utils::doubleWordToBytes(*optionalConstant));
                    }
                } else {
                    if (reg != Registers::UNKNOWN) {
                        if (reg1 != Registers::UNKNOWN) {
                            setRegisterData(getRegisterData(reg1), reg);
                        } else {
                            setRegisterData(Utils::readDoubleWord(memory->readData(address1, 4)), reg);
                        }
                    } else {
                        if (reg != Registers::UNKNOWN) {
                            setRegisterData(getRegisterData(reg), reg1);
                        } else {
                            setRegisterData(Utils::readDoubleWord(memory->readData(address, 4)), reg1);
                        }
                    }
                }
            }
            break;
        case InstructionSet::MUL:
            if (instruction.isRegisterInPosition(0)) {
                if (instruction.isRegisterInPosition(1)) {
                    DoubleWord result;
                    result.doubleWord = getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord * getRegisterData(Registers(instruction.args[1].doubleWord)).doubleWord;
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                } else if (instruction.isAddressInPosition(1)) {
                    DoubleWord result;
                    result.doubleWord = getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord * Utils::readDoubleWord(memory->readData(instruction.args[1], 4)).doubleWord;
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                } else {
                    std::cerr<<"ElectroCraft CPU: Invalid arguments for MUL!"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invalid arguments for MUL!"<<std::endl;
            }
            break;
        case InstructionSet::NEG:
            if (instruction.isAddressInPosition(0)) {
                DoubleWord result;
                result.doubleWord = -Utils::readDoubleWord(memory->readData(instruction.args[0], 4)).doubleWord;
                memory->writeData(instruction.args[0], 4, Utils::doubleWordToBytes(result));
            } else if (instruction.isRegisterInPosition(0)) {
                DoubleWord result;
                result.doubleWord = getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord;
                if (getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord > 0) {
                    eflags.setFlagState(EFLAGS::SF);
                    if (result.doubleWord == 0) {
                        eflags.setFlagState(EFLAGS::ZF);
                    }
                } else {
                    eflags.resetFlag(EFLAGS::SF);
                    if (result.doubleWord == 0) {
                        eflags.resetFlag(EFLAGS::ZF);
                    }
                }
                setRegisterData(result, Registers(instruction.args[0].doubleWord));
            } else {
                std::cerr<<"ElectroCraft CPU: Invalid arguments for NEG!"<<std::endl;
            }
            break;
        case InstructionSet::NOP:
            break;
        case InstructionSet::NOT:
            if (instruction.isAddressInPosition(0)) {
                DoubleWord result;
                result.doubleWord = ~Utils::readDoubleWord(memory->readData(instruction.args[0], 4)).doubleWord;
                memory->writeData(instruction.args[0], 4, Utils::doubleWordToBytes(result));
            } else if (instruction.isRegisterInPosition(0)) {
                DoubleWord result;
                result.doubleWord = ~getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord;
                setRegisterData(result, Registers(instruction.args[0].doubleWord));
            } else {
                std::cerr<<"ElectroCraft CPU: Invalid arguments for NOT!"<<std::endl;
            }
            break;
        case InstructionSet::OR:
            if ((instruction.isRegisterInPosition(0) || instruction.isRegisterInPosition(1) || instruction.isAddressInPosition(0) || instruction.isAddressInPosition(1)) && (instruction.numOprands > 1)) {
                Registers reg = Registers::UNKNOWN;
                Registers reg1 = Registers::UNKNOWN;
                RegisterSizes regSize = RegisterSizes::ZERO;
                RegisterSizes regSize1 = RegisterSizes::ZERO;
                Byte* data = nullptr;
                Byte* data1 = nullptr;
                
                if (instruction.isRegisterInPosition(0)) {
                    reg = Registers(instruction.args[0].doubleWord);
                    regSize = registerToSize(reg);
                } else {
                    data = Utils::doubleWordToBytes(instruction.args[0]);
                }
                
                if (instruction.isRegisterInPosition(1)) {
                    reg1 = Registers(instruction.args[1].doubleWord);
                    regSize1 = registerToSize(reg1);
                } else {
                    data1 = Utils::doubleWordToBytes(instruction.args[1]);
                }
                
                if (reg != Registers::UNKNOWN) {
                    switch (regSize) {
                        case RegisterSizes::FOUR:
                            data = memory->readData(getRegisterData(reg), 4);
                            break;
                        case RegisterSizes::TWO:
                            data = memory->readData(getRegisterData(reg), 2);
                        case RegisterSizes::ONE:
                            data = memory->readData(getRegisterData(reg), 1);
                        default:
                            break;
                    }
                }
                
                if (reg1 != Registers::UNKNOWN) {
                    switch (regSize1) {
                        case RegisterSizes::FOUR:
                            data1 = memory->readData(getRegisterData(reg1), 4);
                            break;
                        case RegisterSizes::TWO:
                            data1 = memory->readData(getRegisterData(reg1), 2);
                        case RegisterSizes::ONE:
                            data1 = memory->readData(getRegisterData(reg1), 1);
                        default:
                            break;
                    }
                }
                
                if(data != nullptr && data1 != nullptr) {
                    // <REG>, <REG>
                    if (regSize != RegisterSizes::ZERO && regSize1 != RegisterSizes::ZERO) {
                        if (regSize == RegisterSizes::FOUR || regSize1 == RegisterSizes::FOUR) {
                            DoubleWord result;
                            if (regSize < regSize1) {
                                std::cerr<<"ElectroCraft CPU: Invaid arguments for OR: First register too small for storage!"<<std::endl;
                            } else if(regSize > regSize1) {
                                if (regSize1 == RegisterSizes::TWO) {
                                    result.doubleWord = getRegisterData(reg).doubleWord - getRegisterData(reg1).word.lowWord.word;
                                } else {
                                    if (getRegisterType(reg1) == RegisterType::HIGH) {
                                        result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord | getRegisterData(reg1).word.lowWord.byte.hiByte;
                                    } else {
                                        result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord | getRegisterData(reg1).word.lowWord.byte.lowByte;
                                    }
                                }
                                setRegisterData(result, reg);
                            } else {
                                result.doubleWord = getRegisterData(reg).doubleWord | getRegisterData(reg1).doubleWord;
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else if (regSize == RegisterSizes::TWO || regSize1 == RegisterSizes::TWO) {
                            DoubleWord result;
                            if (regSize < regSize1) {
                                std::cerr<<"ElectroCraft CPU: Invaid arguments for OR: First register too small for storage!"<<std::endl;
                            } else if(regSize > regSize1) {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.word = getRegisterData(reg).word.lowWord.word | getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.word = getRegisterData(reg).word.lowWord.word | getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                                setRegisterData(result, reg);
                            } else {
                                result.doubleWord = getRegisterData(reg).word.lowWord.word | getRegisterData(reg1).word.lowWord.word;
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else {
                            DoubleWord result;
                            if (getRegisterType(reg) == RegisterType::HIGH) {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte | getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte | getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                            } else {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.lowByte | getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.lowByte | getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                            setRegisterData(result, reg);
                        }
                        // [Address], <Register, Const>
                    } if (instruction.isAddressInPosition(0)) {
                        if (regSize1 != RegisterSizes::ZERO) {
                            if (regSize1 == RegisterSizes::FOUR) {
                                DoubleWord result;
                                result.doubleWord = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data), 4)).doubleWord | getRegisterData(reg1).doubleWord;
                                if (result.doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data), 4, Utils::doubleWordToBytes(result));
                            } else if (regSize1 == RegisterSizes::TWO) {
                                DoubleWord result;
                                result.word.lowWord.word = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data), 4)).word.lowWord.word | getRegisterData(reg1).word.lowWord.word;
                                if (result.doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data), 4, Utils::doubleWordToBytes(result));
                            } else {
                                DoubleWord *result = new DoubleWord;
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result->word.lowWord.byte.hiByte = *memory->readData(Utils::readDoubleWord(data), 1) | getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result->word.lowWord.byte.lowByte = *memory->readData(Utils::readDoubleWord(data), 1) | getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                                if (result->doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data1), 4, Utils::doubleWordToBytes(*result));
                            }
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for OR [Address], <Register, Const>"<<std::endl;
                        }
                        // <Register>, [Address]
                    } else if (instruction.isAddressInPosition(1)) {
                        if (regSize != RegisterSizes::ZERO) {
                            DoubleWord result;
                            if (regSize == RegisterSizes::FOUR) {
                                result.doubleWord = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data1), 4)).doubleWord | getRegisterData(reg).doubleWord;
                                setRegisterData(result, reg);
                            } else if (regSize == RegisterSizes::TWO) {
                                result.word.lowWord.word = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data1), 4)).word.lowWord.word | getRegisterData(reg).word.lowWord.word;
                                setRegisterData(result, reg);
                            } else {
                                if (getRegisterType(reg) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte | *memory->readData(Utils::readDoubleWord(data1), 1);
                                } else {
                                    result.word.lowWord.byte.lowByte = getRegisterData(reg).word.lowWord.byte.lowByte | *memory->readData(Utils::readDoubleWord(data1), 1);
                                }
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for OR <Register>, [Address]!"<<std::endl;
                        }
                    } else if (regSize != RegisterSizes::ZERO && data1 != nullptr) {
                        // <REG>, <CONST>
                        DoubleWord result;
                        if (regSize == RegisterSizes::FOUR) {
                            result.doubleWord = getRegisterData(reg).doubleWord | Utils::readDoubleWord(data1).doubleWord;
                            setRegisterData(result, reg);
                        } else if (regSize == RegisterSizes::TWO) {
                            result.word.lowWord.word = getRegisterData(reg).word.lowWord.word | Utils::readDoubleWord(data1).doubleWord;
                            setRegisterData(result, reg);
                        } else if (regSize == RegisterSizes::ONE) {
                            if (getRegisterType(reg) == RegisterType::HIGH) {
                                result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord | Utils::readDoubleWord(data1).doubleWord;
                            } else {
                                result.word.lowWord.byte.lowByte = getRegisterData(reg).doubleWord | Utils::readDoubleWord(data1).doubleWord;
                            }
                            setRegisterData(result, reg);
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for OR!"<<std::endl;
                        }
                        if (result.doubleWord == 0) {
                            eflags.setFlagState(EFLAGS::ZF);
                        }
                    } else {
                        std::cerr<<"ElectroCraft CPU: Invaid arguments for OR!"<<std::endl;
                    }
                    
                } else {
                    std::cerr<<"ElectroCraft CPU: Invaid arguments for OR!"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for OR!"<<std::endl;
            }
            break;
        case InstructionSet::POP:
            if (instruction.isRegisterInPosition(0)) {
                Registers reg = Registers(instruction.args[0].doubleWord);
                if (reg != Registers::UNKNOWN) {
                    Registers reg = Registers(instruction.args[0].doubleWord);
                    if (reg != Registers::UNKNOWN) {
                        setRegisterData(stack->pop(), reg);
                    } else {
                        std::cerr<<"ElectroCraft CPU: Unknown register in position 0 for POP"<<std::endl;
                    }
                }
            } else {
                std::cerr<<"ElectroCraft CPU: No register in position 0 for POP"<<std::endl;
            }
            break;
        case InstructionSet::POPA:
            for (int i = Registers::TOAL_SIZE; i > 0; i--) {
                setRegisterData(stack->pop(), Registers(i));
            }
            break;
        case InstructionSet::PUSH:
            if (instruction.isRegisterInPosition(0)) {
                Registers reg = Registers(instruction.args[0].doubleWord);
                if (reg != Registers::UNKNOWN) {
                    DoubleWord data = getRegisterData(reg);
                    stack->push(data);
                } else {
                    std::cerr<<"ElectroCraft CPU: Unknown register in position 0 for PUSH"<<std::endl;
                }
            } else if (instruction.isAddressInPosition(0)) {
                stack->push(Utils::readDoubleWord(memory->readData(instruction.args[0], 4)));
            } else {
                std::cerr<<"ElectroCraft CPU: Incorrect arguments for PUSH"<<std::endl;
            }
            break;
        case InstructionSet::PUSHA:
            for (int i = 0; i < Registers::TOAL_SIZE; i++) {
                DoubleWord data = getRegisterData(Registers(i));
                stack->push(data);
            }
            break;
        case InstructionSet::RANDI:
            if (instruction.isAddressInPosition(0)) {
                if (instruction.isAddressInPosition(1)) {
                    DoubleWord upperBound = Utils::readDoubleWord(memory->readData(instruction.args[1], 4));
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(Utils::readDoubleWord(memory->readData(instruction.args[0], 4)).doubleWord, upperBound.doubleWord);
                    result.doubleWord = dis(gen);
                    memory->writeData(instruction.args[0], 4, Utils::doubleWordToBytes(result));
                } else if (instruction.isRegisterInPosition(1)) {
                    DoubleWord upperBound = getRegisterData(Registers(instruction.args[1].doubleWord));
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(Utils::readDoubleWord(memory->readData(instruction.args[0], 4)).doubleWord, upperBound.doubleWord);
                    result.doubleWord = dis(gen);
                    memory->writeData(instruction.args[0], 4, Utils::doubleWordToBytes(result));
                } else {
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(Utils::readDoubleWord(memory->readData(instruction.args[0], 4)).doubleWord, instruction.args[1].doubleWord);
                    result.doubleWord = dis(gen);
                    memory->writeData(instruction.args[0], 4, Utils::doubleWordToBytes(result));
                }
            } else if (instruction.isRegisterInPosition(0)) {
                if (instruction.isAddressInPosition(1)) {
                    DoubleWord upperBound = Utils::readDoubleWord(memory->readData(instruction.args[1], 4));
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord, upperBound.doubleWord);
                    result.doubleWord = dis(gen);
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                } else if (instruction.isRegisterInPosition(1)) {
                    DoubleWord upperBound = getRegisterData(Registers(instruction.args[1].doubleWord));
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord, upperBound.doubleWord);
                    result.doubleWord = dis(gen);
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                } else {
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord, instruction.args[1].doubleWord);
                    result.doubleWord = dis(gen);
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Incorrect arguments for RANDI!"<<std::endl;
            }
            break;
        case InstructionSet::RET:
        {
            Address addressToReturnTo = stack->pop();
            setRegisterData(addressToReturnTo, Registers::EIP);
        }
            break;
        case InstructionSet::ROUND:
            break;
        case InstructionSet::SHL:
            if (instruction.isRegisterInPosition(0)) {
                if (instruction.isAddressInPosition(1)) {
                    if (Registers(instruction.args[0].doubleWord) == Registers::CL) {
                        DoubleWord result;
                        result.doubleWord = getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord << getRegisterData(Registers::CL).word.lowWord.byte.lowByte;
                        if (result.doubleWord == 0) {
                            eflags.setFlagState(EFLAGS::ZF);
                        }
                        setRegisterData(result, Registers(instruction.args[0].doubleWord));
                    } else if (!instruction.isAddressInPosition(1) && !instruction.isOffsetInPosition(1)) {
                        DoubleWord result;
                        result.doubleWord = getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord << instruction.args[1].word.lowWord.byte.lowByte;
                        if (result.doubleWord == 0) {
                            eflags.setFlagState(EFLAGS::ZF);
                        }
                        setRegisterData(result, Registers(instruction.args[0].doubleWord));
                    } else {
                        std::cerr<<"ElectroCraft CPU: Incorrect arguments for SH"<<std::endl;
                    }
                }
            } else if (instruction.isAddressInPosition(0)) {
                if (Registers(instruction.args[0].doubleWord) == Registers::CL) {
                    DoubleWord result;
                    result.doubleWord = Utils::readDoubleWord(memory->readData(instruction.args[0], 4)).doubleWord << getRegisterData(Registers::CL).word.lowWord.byte.lowByte;
                    if (result.doubleWord == 0) {
                        eflags.setFlagState(EFLAGS::ZF);
                    }
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                } else if (!instruction.isAddressInPosition(1) && !instruction.isOffsetInPosition(1)) {
                    DoubleWord result;
                    result.doubleWord = Utils::readDoubleWord(memory->readData(instruction.args[0], 4)).doubleWord << instruction.args[1].word.lowWord.byte.lowByte;
                    if (result.doubleWord == 0) {
                        eflags.setFlagState(EFLAGS::ZF);
                    }
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                } else {
                    std::cerr<<"ElectroCraft CPU: Incorrect arguments for SH"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Incorrect arguments for SH"<<std::endl;
            }
            break;
        case InstructionSet::SHR:
            if (instruction.isRegisterInPosition(0)) {
                if (instruction.isAddressInPosition(1)) {
                    if (Registers(instruction.args[0].doubleWord) == Registers::CL) {
                        DoubleWord result;
                        result.doubleWord = getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord >> getRegisterData(Registers::CL).word.lowWord.byte.lowByte;
                        if (result.doubleWord == 0) {
                            eflags.setFlagState(EFLAGS::ZF);
                        }
                        setRegisterData(result, Registers(instruction.args[0].doubleWord));
                    } else if (!instruction.isAddressInPosition(1) && !instruction.isOffsetInPosition(1)) {
                        DoubleWord result;
                        result.doubleWord = getRegisterData(Registers(instruction.args[0].doubleWord)).doubleWord >> instruction.args[1].word.lowWord.byte.lowByte;
                        if (result.doubleWord == 0) {
                            eflags.setFlagState(EFLAGS::ZF);
                        }
                        setRegisterData(result, Registers(instruction.args[0].doubleWord));
                    } else {
                        std::cerr<<"ElectroCraft CPU: Incorrect arguments for SH"<<std::endl;
                    }
                }
            } else if (instruction.isAddressInPosition(0)) {
                if (Registers(instruction.args[0].doubleWord) == Registers::CL) {
                    DoubleWord result;
                    result.doubleWord = Utils::readDoubleWord(memory->readData(instruction.args[0], 4)).doubleWord >> getRegisterData(Registers::CL).word.lowWord.byte.lowByte;
                    if (result.doubleWord == 0) {
                        eflags.setFlagState(EFLAGS::ZF);
                    }
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                } else if (!instruction.isAddressInPosition(1) && !instruction.isOffsetInPosition(1)) {
                    DoubleWord result;
                    result.doubleWord = Utils::readDoubleWord(memory->readData(instruction.args[0], 4)).doubleWord >> instruction.args[1].word.lowWord.byte.lowByte;
                    if (result.doubleWord == 0) {
                        eflags.setFlagState(EFLAGS::ZF);
                    }
                    setRegisterData(result, Registers(instruction.args[0].doubleWord));
                } else {
                    std::cerr<<"ElectroCraft CPU: Incorrect arguments for SH"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Incorrect arguments for SH"<<std::endl;
            }
            break;
        case InstructionSet::SUB:
            if ((instruction.isRegisterInPosition(0) || instruction.isRegisterInPosition(1) || instruction.isAddressInPosition(0) || instruction.isAddressInPosition(1)) && (instruction.numOprands > 1)) {
                Registers reg = Registers::UNKNOWN;
                Registers reg1 = Registers::UNKNOWN;
                RegisterSizes regSize = RegisterSizes::ZERO;
                RegisterSizes regSize1 = RegisterSizes::ZERO;
                Byte* data = nullptr;
                Byte* data1 = nullptr;
                
                if (instruction.isRegisterInPosition(0)) {
                    reg = Registers(instruction.args[0].doubleWord);
                    regSize = registerToSize(reg);
                } else {
                    data = Utils::doubleWordToBytes(instruction.args[0]);
                }
                
                if (instruction.isRegisterInPosition(1)) {
                    reg1 = Registers(instruction.args[1].doubleWord);
                    regSize1 = registerToSize(reg1);
                } else {
                    data1 = Utils::doubleWordToBytes(instruction.args[1]);
                }
                
                if (reg != Registers::UNKNOWN) {
                    switch (regSize) {
                        case RegisterSizes::FOUR:
                            data = memory->readData(getRegisterData(reg), 4);
                            break;
                        case RegisterSizes::TWO:
                            data = memory->readData(getRegisterData(reg), 2);
                        case RegisterSizes::ONE:
                            data = memory->readData(getRegisterData(reg), 1);
                        default:
                            break;
                    }
                }
                
                if (reg1 != Registers::UNKNOWN) {
                    switch (regSize1) {
                        case RegisterSizes::FOUR:
                            data1 = memory->readData(getRegisterData(reg1), 4);
                            break;
                        case RegisterSizes::TWO:
                            data1 = memory->readData(getRegisterData(reg1), 2);
                        case RegisterSizes::ONE:
                            data1 = memory->readData(getRegisterData(reg1), 1);
                        default:
                            break;
                    }
                }
                
                if(data != nullptr && data1 != nullptr) {
                    // <REG>, <REG>
                    if (regSize != RegisterSizes::ZERO && regSize1 != RegisterSizes::ZERO) {
                        if (regSize == RegisterSizes::FOUR || regSize1 == RegisterSizes::FOUR) {
                            DoubleWord result;
                            if (regSize < regSize1) {
                                std::cerr<<"ElectroCraft CPU: Invaid arguments for SUB: First register too small for storage!"<<std::endl;
                            } else if(regSize > regSize1) {
                                if (regSize1 == RegisterSizes::TWO) {
                                    result.doubleWord = getRegisterData(reg).doubleWord - getRegisterData(reg1).word.lowWord.word;
                                } else {
                                    if (getRegisterType(reg1) == RegisterType::HIGH) {
                                        result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord - getRegisterData(reg1).word.lowWord.byte.hiByte;
                                    } else {
                                        result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord - getRegisterData(reg1).word.lowWord.byte.lowByte;
                                    }
                                }
                                setRegisterData(result, reg);
                            } else {
                                result.doubleWord = getRegisterData(reg).doubleWord - getRegisterData(reg1).doubleWord;
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else if (regSize == RegisterSizes::TWO || regSize1 == RegisterSizes::TWO) {
                            DoubleWord result;
                            if (regSize < regSize1) {
                                std::cerr<<"ElectroCraft CPU: Invaid arguments for SUB: First register too small for storage!"<<std::endl;
                            } else if(regSize > regSize1) {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.word = getRegisterData(reg).word.lowWord.word - getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.word = getRegisterData(reg).word.lowWord.word - getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                                setRegisterData(result, reg);
                            } else {
                                result.doubleWord = getRegisterData(reg).word.lowWord.word - getRegisterData(reg1).word.lowWord.word;
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else {
                            DoubleWord result;
                            if (getRegisterType(reg) == RegisterType::HIGH) {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte - getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte - getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                            } else {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.lowByte - getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.lowByte - getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                            setRegisterData(result, reg);
                        }
                        // [Address], <Register, Const>
                    } if (instruction.isAddressInPosition(0)) {
                        if (regSize1 != RegisterSizes::ZERO) {
                            if (regSize1 == RegisterSizes::FOUR) {
                                DoubleWord result;
                                result.doubleWord = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data), 4)).doubleWord - getRegisterData(reg1).doubleWord;
                                if (result.doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data), 4, Utils::doubleWordToBytes(result));
                            } else if (regSize1 == RegisterSizes::TWO) {
                                DoubleWord result;
                                result.word.lowWord.word = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data), 4)).word.lowWord.word - getRegisterData(reg1).word.lowWord.word;
                                if (result.doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data), 4, Utils::doubleWordToBytes(result));
                            } else {
                                DoubleWord *result = new DoubleWord;
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result->word.lowWord.byte.hiByte = *memory->readData(Utils::readDoubleWord(data), 1) - getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result->word.lowWord.byte.lowByte = *memory->readData(Utils::readDoubleWord(data), 1) - getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                                if (result->doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data1), 4, Utils::doubleWordToBytes(*result));
                            }
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for SUB [Address], <Register, Const>"<<std::endl;
                        }
                        // <Register>, [Address]
                    } else if (instruction.isAddressInPosition(1)) {
                        DoubleWord result;
                        if (regSize != RegisterSizes::ZERO) {
                            if (regSize == RegisterSizes::FOUR) {
                                result.doubleWord = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data1), 4)).doubleWord - getRegisterData(reg).doubleWord;
                                setRegisterData(result, reg);
                            } else if (regSize == RegisterSizes::TWO) {
                                result.word.lowWord.word = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data1), 4)).word.lowWord.word - getRegisterData(reg).word.lowWord.word;
                                setRegisterData(result, reg);
                            } else {
                                if (getRegisterType(reg) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte - *memory->readData(Utils::readDoubleWord(data1), 1);
                                } else {
                                    result.word.lowWord.byte.lowByte = getRegisterData(reg).word.lowWord.byte.lowByte - *memory->readData(Utils::readDoubleWord(data1), 1);
                                }
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for SUB <Register>, [Address]!"<<std::endl;
                        }
                    } else if (regSize != RegisterSizes::ZERO && data1 != nullptr) {
                        // <REG>, <CONST>
                        DoubleWord result;
                        if (regSize == RegisterSizes::FOUR) {
                            result.doubleWord = getRegisterData(reg).doubleWord - Utils::readDoubleWord(data1).doubleWord;
                            setRegisterData(result, reg);
                        } else if (regSize == RegisterSizes::TWO) {
                            result.word.lowWord.word = getRegisterData(reg).word.lowWord.word - Utils::readDoubleWord(data1).doubleWord;
                            setRegisterData(result, reg);
                        } else if (regSize == RegisterSizes::ONE) {
                            if (getRegisterType(reg) == RegisterType::HIGH) {
                                result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord - Utils::readDoubleWord(data1).doubleWord;
                            } else {
                                result.word.lowWord.byte.lowByte = getRegisterData(reg).doubleWord - Utils::readDoubleWord(data1).doubleWord;
                            }
                            setRegisterData(result, reg);
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for SUB!"<<std::endl;
                        }
                        if (result.doubleWord == 0) {
                            eflags.setFlagState(EFLAGS::ZF);
                        }
                    } else {
                        std::cerr<<"ElectroCraft CPU: Invaid arguments for SUB!"<<std::endl;
                    }
                    
                } else {
                    std::cerr<<"ElectroCraft CPU: Invaid arguments for SUB!"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for SUB!"<<std::endl;
            }
            break;
        case InstructionSet::XOR:
            if ((instruction.isRegisterInPosition(0) || instruction.isRegisterInPosition(1) || instruction.isAddressInPosition(0) || instruction.isAddressInPosition(1)) && (instruction.numOprands > 1)) {
                Registers reg = Registers::UNKNOWN;
                Registers reg1 = Registers::UNKNOWN;
                RegisterSizes regSize = RegisterSizes::ZERO;
                RegisterSizes regSize1 = RegisterSizes::ZERO;
                Byte* data = nullptr;
                Byte* data1 = nullptr;
                
                if (instruction.isRegisterInPosition(0)) {
                    reg = Registers(instruction.args[0].doubleWord);
                    regSize = registerToSize(reg);
                } else {
                    data = Utils::doubleWordToBytes(instruction.args[0]);
                }
                
                if (instruction.isRegisterInPosition(1)) {
                    reg1 = Registers(instruction.args[1].doubleWord);
                    regSize1 = registerToSize(reg1);
                } else {
                    data1 = Utils::doubleWordToBytes(instruction.args[1]);
                }
                
                if (reg != Registers::UNKNOWN) {
                    switch (regSize) {
                        case RegisterSizes::FOUR:
                            data = memory->readData(getRegisterData(reg), 4);
                            break;
                        case RegisterSizes::TWO:
                            data = memory->readData(getRegisterData(reg), 2);
                        case RegisterSizes::ONE:
                            data = memory->readData(getRegisterData(reg), 1);
                        default:
                            break;
                    }
                }
                
                if (reg1 != Registers::UNKNOWN) {
                    switch (regSize1) {
                        case RegisterSizes::FOUR:
                            data1 = memory->readData(getRegisterData(reg1), 4);
                            break;
                        case RegisterSizes::TWO:
                            data1 = memory->readData(getRegisterData(reg1), 2);
                        case RegisterSizes::ONE:
                            data1 = memory->readData(getRegisterData(reg1), 1);
                        default:
                            break;
                    }
                }
                
                if(data != nullptr && data1 != nullptr) {
                    // <REG>, <REG>
                    if (regSize != RegisterSizes::ZERO && regSize1 != RegisterSizes::ZERO) {
                        if (regSize == RegisterSizes::FOUR || regSize1 == RegisterSizes::FOUR) {
                            DoubleWord result;
                            if (regSize < regSize1) {
                                std::cerr<<"ElectroCraft CPU: Invaid arguments for XOR: First register too small for storage!"<<std::endl;
                            } else if(regSize > regSize1) {
                                if (regSize1 == RegisterSizes::TWO) {
                                    result.doubleWord = getRegisterData(reg).doubleWord - getRegisterData(reg1).word.lowWord.word;
                                } else {
                                    if (getRegisterType(reg1) == RegisterType::HIGH) {
                                        result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord ^ getRegisterData(reg1).word.lowWord.byte.hiByte;
                                    } else {
                                        result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord ^ getRegisterData(reg1).word.lowWord.byte.lowByte;
                                    }
                                }
                                setRegisterData(result, reg);
                            } else {
                                result.doubleWord = getRegisterData(reg).doubleWord ^ getRegisterData(reg1).doubleWord;
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else if (regSize == RegisterSizes::TWO || regSize1 == RegisterSizes::TWO) {
                            DoubleWord result;
                            if (regSize < regSize1) {
                                std::cerr<<"ElectroCraft CPU: Invaid arguments for XOR: First register too small for storage!"<<std::endl;
                            } else if(regSize > regSize1) {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.word = getRegisterData(reg).word.lowWord.word ^ getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.word = getRegisterData(reg).word.lowWord.word ^ getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                                setRegisterData(result, reg);
                            } else {
                                result.doubleWord = getRegisterData(reg).word.lowWord.word ^ getRegisterData(reg1).word.lowWord.word;
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else {
                            DoubleWord result;
                            if (getRegisterType(reg) == RegisterType::HIGH) {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte ^ getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte ^ getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                            } else {
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.lowByte ^ getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.lowByte ^ getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                            setRegisterData(result, reg);
                        }
                        // [Address], <Register, Const>
                    } if (instruction.isAddressInPosition(0)) {
                        if (regSize1 != RegisterSizes::ZERO) {
                            if (regSize1 == RegisterSizes::FOUR) {
                                DoubleWord result;
                                result.doubleWord = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data), 4)).doubleWord ^ getRegisterData(reg1).doubleWord;
                                if (result.doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data), 4, Utils::doubleWordToBytes(result));
                            } else if (regSize1 == RegisterSizes::TWO) {
                                DoubleWord result;
                                result.word.lowWord.word = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data), 4)).word.lowWord.word ^ getRegisterData(reg1).word.lowWord.word;
                                if (result.doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data), 4, Utils::doubleWordToBytes(result));
                            } else {
                                DoubleWord *result = new DoubleWord;
                                if (getRegisterType(reg1) == RegisterType::HIGH) {
                                    result->word.lowWord.byte.hiByte = *memory->readData(Utils::readDoubleWord(data), 1) ^ getRegisterData(reg1).word.lowWord.byte.hiByte;
                                } else {
                                    result->word.lowWord.byte.lowByte = *memory->readData(Utils::readDoubleWord(data), 1) ^ getRegisterData(reg1).word.lowWord.byte.lowByte;
                                }
                                if (result->doubleWord == 0) {
                                    eflags.setFlagState(EFLAGS::ZF);
                                }
                                memory->writeData(Utils::readDoubleWord(data1), 4, Utils::doubleWordToBytes(*result));
                            }
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for XOR [Address], <Register, Const>"<<std::endl;
                        }
                        // <Register>, [Address]
                    } else if (instruction.isAddressInPosition(1)) {
                        if (regSize != RegisterSizes::ZERO) {
                            DoubleWord result;
                            if (regSize == RegisterSizes::FOUR) {
                                result.doubleWord = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data1), 4)).doubleWord ^ getRegisterData(reg).doubleWord;
                                setRegisterData(result, reg);
                            } else if (regSize == RegisterSizes::TWO) {
                                result.word.lowWord.word = Utils::readDoubleWord(memory->readData(Utils::readDoubleWord(data1), 4)).word.lowWord.word ^ getRegisterData(reg).word.lowWord.word;
                                setRegisterData(result, reg);
                            } else {
                                if (getRegisterType(reg) == RegisterType::HIGH) {
                                    result.word.lowWord.byte.hiByte = getRegisterData(reg).word.lowWord.byte.hiByte ^ *memory->readData(Utils::readDoubleWord(data1), 1);
                                } else {
                                    result.word.lowWord.byte.lowByte = getRegisterData(reg).word.lowWord.byte.lowByte ^ *memory->readData(Utils::readDoubleWord(data1), 1);
                                }
                                setRegisterData(result, reg);
                            }
                            if (result.doubleWord == 0) {
                                eflags.setFlagState(EFLAGS::ZF);
                            }
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for XOR <Register>, [Address]!"<<std::endl;
                        }
                    } else if (regSize != RegisterSizes::ZERO && data1 != nullptr) {
                        // <REG>, <CONST>
                        DoubleWord result;
                        if (regSize == RegisterSizes::FOUR) {
                            result.doubleWord = getRegisterData(reg).doubleWord ^ Utils::readDoubleWord(data1).doubleWord;
                            setRegisterData(result, reg);
                        } else if (regSize == RegisterSizes::TWO) {
                            result.word.lowWord.word = getRegisterData(reg).word.lowWord.word ^ Utils::readDoubleWord(data1).doubleWord;
                            setRegisterData(result, reg);
                        } else if (regSize == RegisterSizes::ONE) {
                            if (getRegisterType(reg) == RegisterType::HIGH) {
                                result.word.lowWord.byte.hiByte = getRegisterData(reg).doubleWord ^ Utils::readDoubleWord(data1).doubleWord;
                            } else {
                                result.word.lowWord.byte.lowByte = getRegisterData(reg).doubleWord ^ Utils::readDoubleWord(data1).doubleWord;
                            }
                            setRegisterData(result, reg);
                        } else {
                            std::cerr<<"ElectroCraft CPU: Invaid arguments for XOR!"<<std::endl;
                        }
                        if (result.doubleWord == 0) {
                            eflags.setFlagState(EFLAGS::ZF);
                        }
                    } else {
                        std::cerr<<"ElectroCraft CPU: Invaid arguments for XOR!"<<std::endl;
                    }
                    
                } else {
                    std::cerr<<"ElectroCraft CPU: Invaid arguments for XOR!"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for XOR!"<<std::endl;
            }
            break;
        default:
            break;
    }
    if (!hasJumped)
        registers.IP.doubleWord += OPERATION_SZIE;
}

ElectroCraftVGA* ElectroCraft_CPU::getVideoCard() {
    return videoCard;
}

bool ElectroCraft_CPU::isRunning() {
    return clock->isRunning();
}

Address ElectroCraft_CPU::loadIntoMemory(Byte *data, int length) {
    MemoryInfo* memoryBlock = memory->allocate(length);
    memoryBlock->setData(data, length);
    return memoryBlock->startOffset;
}

void ElectroCraft_CPU::start(Address baseAddress) {
    registers.IP = baseAddress;
    clock->start();
}

void ElectroCraft_CPU::stop() {
    clock->stop();
}

void ElectroCraft_CPU::reset(Address baseAddress) {
    clock->stop();
    registers.IP = baseAddress;
    clock->start();
}