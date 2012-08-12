/*
 *  ElectroCraft_CPU.cpp
 *  ElectroCraft CPU
 *
 *  Created by Andrew Querol on 8/3/12.
 *  Copyright (c) 2012 Cerios Software. All rights reserved.
 *
 */

// CPP Headers
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include <cstring>

// ElectroCraft Headers
#include "ElectroCraft_CPU.h"
#include "ElectroCraftMemory.h"
#include "ElectroCraftStack.h"
#include "ElectroCraftVGA.h"
#include "ElectroCraftClock.h"
#include "ElectroCraftTerminal.h"
#include "ElectroCraftStorage.h"
#include "ElectroCraftKeyboard.h"
#include "IOPortHandler.h"
#include "Utils.h"

ElectroCraft_CPU::ElectroCraft_CPU() {
    Address baseAddress;
    baseAddress.doubleWord = 0x0;
    
    memory = new ElectroCraftMemory(16 * 1024 * 1024, baseAddress);
    ioPortHandler = new IOPort::IOPortHandler();
    videoCard = new ElectroCraftVGA(memory, 320, 240);
    terminal = new ElectroCraftTerminal(memory, 80, 20);
    stack = new ElectroCraftStack(memory, &registers, 4096);
    
    storage = new ElectroCraftStorage();
    ioPortHandler->registerDevice(storage);
    
    keyboard = new ElectroCraftKeyboard();
    ioPortHandler->registerDevice(keyboard);
    
    clock = new ElectroCraftClock(5000000);
    clock->registerCallback(this);
    clock->registerCallback(videoCard);
    clock->registerCallback(terminal);
}

ElectroCraft_CPU::~ElectroCraft_CPU() {
    delete memory;
}

AssembledData ElectroCraft_CPU::assemble(std::vector<std::string> data) {
    // The first pass
    std::vector<FirstPassData*> firstPassData;
    std::map<std::string, TokenData> tokens;
    
    DoubleWord currentOffset = 0;
    DoubleWord totalDataSize = 0;

    for (unsigned int line = 0; line < data.size(); line++) {
        FirstPassData *readData = firstPass(data[line], currentOffset);
        if (readData != nullptr) {
            if (!readData->token.name.empty()) {
                // Only way this can happen if there was a token returned
                tokens[readData->token.name] = readData->token;
                if (readData->token.isTokenVar) {
                    totalDataSize.doubleWord += readData->token.varData.size();
                }
            } else if (readData->opcode->opCode != InstructionSet::UNKOWN) {
                firstPassData.push_back(readData);
                currentOffset.doubleWord += OPERATION_SZIE;
            }
        }
    }
    
    AssembledData assembledData;
    Byte* rawData = new Byte[currentOffset.doubleWord + totalDataSize.doubleWord];
    currentOffset.doubleWord = 0;

    // Finally pack the program into a byte array
    unsigned int currentDataOffset = 0;
    
    // Pack the data
    for (auto &tokenPair : tokens) {
        if (tokenPair.second.isTokenVar) {
            tokenPair.second.offset = currentDataOffset;
            for (auto &varData : tokenPair.second.varData) {
                rawData[currentDataOffset] = varData;
                currentDataOffset++;
            }
        }
    }
    
    // The second pass try to resolve unknown tokens
    for (FirstPassData *firstPass : firstPassData) {
        for (int i = 0; i < firstPass->unresolvedTokens.size(); i++) {
            if (!firstPass->unresolvedTokens[i].name.empty()) {
                if (tokens.find(firstPass->unresolvedTokens[i].name) != tokens.end()) {
                    long realOffset;
                    if (tokens[firstPass->unresolvedTokens[i].name].isTokenVar) {
                        // Set that it is a varaible
                        firstPass->opcode->setVarForPosition(firstPass->unresolvedTokens[i].position);
                        // Calcuate the offset
                        realOffset = (long)tokens[firstPass->unresolvedTokens[i].name].offset.doubleWord - (long)currentDataOffset - (long)currentOffset.doubleWord;
                    } else {
                        realOffset = (long)tokens[firstPass->unresolvedTokens[i].name].offset.doubleWord - (long)currentOffset.doubleWord;
                    }
                    firstPass->opcode->setOffsetInPosition(firstPass->unresolvedTokens[i].position);
                    if (realOffset < 0) {
                        firstPass->opcode->setOffsetNegitiveInPosition(firstPass->unresolvedTokens[i].position);
                        realOffset = -realOffset;
                    }
                    DoubleWord offset;
                    offset.doubleWord = static_cast<uint32_t>(realOffset);
                    firstPass->opcode->args[firstPass->unresolvedTokens[i].position] = offset;
                } else {
                    std::cerr<<"Unkown token: "<<firstPass->unresolvedTokens[i].name<<std::endl;
                    std::cerr<<"Program may or may not run correctly!"<<std::endl;
                }
            }
        }
        currentOffset.doubleWord += OPERATION_SZIE;
    }
    
    assembledData.codeOffset = currentDataOffset;
    
    for (int i = 0; i < firstPassData.size(); i++) {
        rawData[currentDataOffset] = firstPassData[i]->opcode->getOpCodeByte();
        rawData[currentDataOffset + 1] = firstPassData[i]->opcode->getInfoByte();
        rawData[currentDataOffset + 2] = firstPassData[i]->opcode->getExtendedInfoByte();
        rawData[currentDataOffset + 3] = firstPassData[i]->opcode->getExtraInfoByte();
        std::memcpy(&rawData[currentDataOffset + 4], &firstPassData[i]->opcode->args[0].doubleWord, sizeof(uint32_t));
        std::memcpy(&rawData[currentDataOffset + 4 + sizeof(uint32_t)], &firstPassData[i]->opcode->args[1].doubleWord, sizeof(uint32_t));
        std::memcpy(&rawData[currentDataOffset + 4 + (sizeof(uint32_t) * 2)], &firstPassData[i]->opcode->modifier[0].doubleWord, sizeof(uint32_t));
        std::memcpy(&rawData[currentDataOffset + 4 + (sizeof(uint32_t) * 3)], &firstPassData[i]->opcode->modifier[1].doubleWord, sizeof(uint32_t));
        currentDataOffset += OPERATION_SZIE;
    }
    assembledData.data = rawData;
    assembledData.length = currentDataOffset;
    
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
    
    FirstPassData *data = new FirstPassData;
    data->beginOffset = beginOffset;
    
    std::stringstream tokenBuffer;
    bool insideBrackets = false;
    bool insideOfQuotes = false;
    bool wasInsideQuotes = false;
    bool isVariableVarible = false;
    TokenData var;
    int tokenNumber = 0;
    for (int i = 0; i < line.size(); i++) {
        if (line[i] == '[')
            insideBrackets = true;
        else if(line[i] == ']')
            insideBrackets = false;
        if (line[i] == '"') {
            wasInsideQuotes = insideOfQuotes;
            insideOfQuotes = !insideOfQuotes;
        }
        
        if (line[i] == '?')
            isVariableVarible = true;
        
        // Ignore comments
        if (line[i] == ';') {
            if (tokenNumber > 0)
                return data;
            else
                return nullptr;
        } if ((line[i] == ' ' || line[i] == ',' || &line[i] == &line.back()) && !(insideBrackets || insideOfQuotes)) {
            // If this is the last char make sure it gets put into the buffer
            if (&line[i] == &line.back()) {
                tokenBuffer<<line[i];
            }
            
            std::string token = tokenBuffer.str();
            tokenBuffer.str(std::string());
            if (data->token.isTokenVar && currentSection == Section::DATA) {
                if (isVariableVarible) {
                    bool isHexNumber = (token.find_first_not_of("xX0123456789ABCDEFabcdef") == std::string::npos);
                    bool isDecimalNumber = (token.find_first_not_of("0123456789") == std::string::npos);
                    if (isDecimalNumber) {
                        // Lets see if it is a decimal number
                        std::stringstream ss;
                        ss << std::dec << token;
                        if (ss.good()) {
                            DoubleWord number;
                            ss >> number.doubleWord;
                            if (data->token.varType == VarType::DB) {
                                for (int d = 0; d < number.doubleWord; d++) {
                                    data->token.varData.push_back(0);
                                }
                            } else if (data->token.varType == VarType::DW) {
                                for (int d = 0; d < 2 * number.doubleWord; d++) {
                                    data->token.varData.push_back(0);
                                }
                            } else {
                                for (int d = 0; d < 4 * number.doubleWord; d++) {
                                    data->token.varData.push_back(0);
                                }
                            }
                        }
                    } else if (isHexNumber) {
                        // Lets see if it is a hex number
                        std::stringstream ss;
                        ss<< std::hex << token;
                        if (ss) {
                            DoubleWord number;
                            ss >> number.doubleWord;
                            if (data->token.varType == VarType::DB) {
                                for (int d = 0; d < number.doubleWord; d++) {
                                    data->token.varData.push_back(0);
                                }
                            } else if (data->token.varType == VarType::DW) {
                                for (int d = 0; d < 2 * number.doubleWord; d++) {
                                    data->token.varData.push_back(0);
                                }
                            } else {
                                for (int d = 0; d < 4 * number.doubleWord; d++) {
                                    data->token.varData.push_back(0);
                                }
                            }
                        }
                    }
                } else if (wasInsideQuotes) {
                    if (data->token.varType == VarType::DB) {
                        for (int c = 0; c < token.size(); c++) {
                            if (token[c] != '"')
                                data->token.varData.push_back(token[c]);
                        }
                    } else {
                        std::cerr<<"ElectroCraft Assembler: Error! String Literials can only be put into a byte type!"<<std::endl;
                    }
                    wasInsideQuotes =!wasInsideQuotes;
                } else {
                    bool isHexNumber = ((token.find_first_not_of("xX0123456789ABCDEFabcdef") == std::string::npos) &&!token.empty());
                    bool isDecimalNumber = ((token.find_first_not_of("0123456789") == std::string::npos) &&!token.empty());
                    if (isDecimalNumber) {
                        // Lets see if it is a decimal number
                        std::stringstream ss;
                        ss << std::dec << token;
                        if (ss.good()) {
                            DoubleWord number;
                            ss >> number.doubleWord;
                            if (data->token.varType == VarType::DB) {
                                data->token.varData.push_back(number.word.lowWord.byte.lowByte);
                            } else if (data->token.varType == VarType::DW) {
                                Byte* nData = Utils::General::wordToBytes(number.word);
                                for (int d = 0; d < 2; d++) {
                                    data->token.varData.push_back(nData[d]);
                                }
                            } else {
                                Byte* nData = Utils::General::doubleWordToBytes(number);
                                for (int d = 0; d < 4; d++) {
                                    data->token.varData.push_back(nData[d]);
                                }
                            }
                        }
                    } else if (isHexNumber) {
                        // Lets see if it is a hex number
                        std::stringstream ss;
                        ss<< std::hex << token;
                        if (ss) {
                            DoubleWord number;
                            ss >> number.doubleWord;
                            if (data->token.varType == VarType::DB) {
                                data->token.varData.push_back(number.word.lowWord.byte.lowByte);
                            } else if (data->token.varType == VarType::DW) {
                                Byte* nData = Utils::General::wordToBytes(number.word);
                                for (int d = 0; d < 2; d++) {
                                    data->token.varData.push_back(nData[d]);
                                }
                            } else {
                                Byte* nData = Utils::General::doubleWordToBytes(number);
                                for (int d = 0; d < 4; d++) {
                                    data->token.varData.push_back(nData[d]);
                                }
                            }
                        }
                    }
                }
                if (!token.empty())
                    tokenNumber++;
            } else if (token.size() > 0 && tokenNumber == 0) {
                std::transform(token.begin(), token.end(), token.begin(), ::toupper);
                if(token.back() == ':') {
                    TokenData tokenData;
                    tokenData.name = token.substr(0, token.size() - 1);
                    tokenData.offset = beginOffset;
                    data->token = tokenData;
                    return data;
                } else {
                    OPCode* opcode = readOPCode(token);
                    if (opcode->opCode != InstructionSet::UNKOWN) {
                        data->opcode = opcode;
                    } else if (currentSection == Section::DATA) {
                        var.name = token;
                        var.offset = beginOffset;
                    }
                }
                if (!token.empty())
                    tokenNumber++;
            } else if (token.size() > 0 && tokenNumber > 0) {
                std::transform(token.begin(), token.end(), token.begin(), ::toupper);
                // Lets see if it is a register!
                Registers reg = getRegister(token);
                if (reg != Registers::UNKNOWN) {
                    data->opcode->infoBits.set(tokenNumber - 1);
                    data->opcode->args[tokenNumber - 1].doubleWord = reg;
                    tokenNumber++;
                } else if (token == "BYTE" || token == "WORD" || token == "DWORD") {
                    if (token == "BYTE") {
                        data->opcode->setByteInPosition(tokenNumber - 1);
                    } else if (token == "WORD") {
                        data->opcode->setWordInPosition(tokenNumber - 1);
                    } else {
                        data->opcode->setDoubleWordInPosition(tokenNumber - 1);
                    }
                } else if (((token == "DB" || token == "DW" || token == "DD") && var.name.size() > 0) && currentSection == Section::DATA) {
                    var.isTokenVar = true;
                    data->token = var;
                    if (token == "DB") {
                        data->token.varType = VarType::DB;
                    } else if (token == "DW") {
                        data->token.varType = VarType::DW;
                    } else {
                        data->token.varType = VarType::DD;
                    }
                    tokenNumber++;
                } else {
                    // Lets see if is a address
                    if (token.front() == '[' && token.back() == ']') {
                        bool isHexNumber = ((token.substr(1, token.size() - 2).find_first_not_of("zX0123456789ABCDEFabcdef") == std::string::npos) && !token.empty());
                        bool hasAddModifier = (token.substr(1, token.size() - 2).find_first_of("+") != std::string::npos);
                        bool hasSubtractModifier = (token.substr(1, token.size() - 2).find_first_of("-") != std::string::npos);
                        
                        Modifier modifier = Modifier::NOM;
                        DoubleWord modifierValue;
                        
                        if (hasAddModifier) {
                            modifier = Modifier::ADDM;
                        } else if (hasSubtractModifier) {
                            modifier = Modifier::SUBM;
                        } else {
                            modifier = Modifier::NOM;
                        }
                        
                        if (modifier != Modifier::NOM){
                            std::stringstream modifierBuff;
                            for (int j = 0; j < token.size(); j++) {
                                if (token[j] == '[' || token[j] == ' ' || token[j] == ',' || token[j] == ']' || &token[j] == &token.back()) {
                                    if (&token[j] == &token.back() && token[j] != ']') {
                                        modifierBuff<<token[j];
                                    }
                                    
                                    std::string subToken = modifierBuff.str();
                                    modifierBuff.str(std::string());
                                    
                                    bool isHexNumber = ((subToken.find_first_not_of("xX0123456789ABCDEFabcdef") == std::string::npos) &&!token.empty());
                                    bool isDecimalNumber = ((subToken.find_first_not_of("0123456789") == std::string::npos) &&!token.empty());
                                    bool subTokenHasModifer = (subToken.find_first_of("-+") != std::string::npos);
                                    
                                    if (!subTokenHasModifer) {
                                        Registers reg = getRegister(subToken);
                                        if (reg != Registers::UNKNOWN) {
                                            data->opcode->setShouldUseRegisterAsAddress(tokenNumber - 1);
                                            data->opcode->infoBits.set(tokenNumber - 1);
                                            data->opcode->args[tokenNumber - 1].doubleWord = reg;
                                        } else if (isDecimalNumber) {
                                            std::stringstream ss;
                                            ss << std::dec << subToken;
                                            if (ss.good()) {
                                                ss>>modifierValue.doubleWord;
                                            }
                                        } else if (isHexNumber) {
                                            std::stringstream ss;
                                            ss << std::hex << subToken;
                                            if (ss.good()) {
                                                ss>>modifierValue.doubleWord;
                                            }
                                        } else {
                                            // Must be some token we haven't solved yet
                                            TokenData unresolved;
                                            unresolved.name = subToken;
                                            unresolved.position = tokenNumber - 1;
                                            unresolved.offset = beginOffset;
                                            data->opcode->setAddressInPosition(tokenNumber - 1);
                                            data->unresolvedTokens.push_back(unresolved);
                                        }
                                    }
                                } else {
                                    modifierBuff<<token[j];
                                }
                            }
                            data->opcode->setModifierForPosition(tokenNumber - 1, modifier, modifierValue);
                            tokenNumber++;
                        } else if (isHexNumber && *(&token.front() + 1) == '0' && *(&token.front() + 2) == 'X') {
                            std::stringstream ss;
                            ss << std::hex << token.substr(1, token.size() - 2);
                            if (ss.good()) {
                                DoubleWord address;
                                ss >> address.doubleWord;
                                data->opcode->args[tokenNumber - 1] = address;
                                data->opcode->setAddressInPosition(tokenNumber - 1);
                                tokenNumber++;
                            }
                        } else {
                            Registers reg = getRegister(token.substr(1, token.size() - 2));
                            if (reg != Registers::UNKNOWN) {
                                data->opcode->setShouldUseRegisterAsAddress(tokenNumber - 1);
                                data->opcode->infoBits.set(tokenNumber - 1);
                                data->opcode->args[tokenNumber - 1].doubleWord = reg;
                                tokenNumber++;
                            } else {
                                // Must be some token we haven't solved yet
                                TokenData unresolved;
                                unresolved.name = token.substr(1, token.size() - 2);
                                unresolved.position = tokenNumber - 1;
                                unresolved.offset = beginOffset;
                                data->opcode->setAddressInPosition(tokenNumber - 1);
                                data->unresolvedTokens.push_back(unresolved);
                                tokenNumber++;
                            }
                        }
                    } else {
                        bool isHexNumber = ((token.find_first_not_of("xX0123456789ABCDEFabcdef") == std::string::npos) &&!token.empty());
                        bool isDecimalNumber = ((token.find_first_not_of("0123456789") == std::string::npos) &&!token.empty());
                        if (isDecimalNumber) {
                            // Lets see if it is a decimal number
                            std::stringstream ss;
                            ss << std::dec << token;
                            if (ss.good()) {
                                DoubleWord number;
                                ss >> number.doubleWord;
                                data->opcode->args[tokenNumber - 1].doubleWord = number.doubleWord;
                            }
                        } else if (isHexNumber) {
                            // Lets see if it is a hex number
                            std::stringstream ss;
                            ss<< std::hex << token;
                            if (ss) {
                                DoubleWord number;
                                ss >> number.doubleWord;
                                data->opcode->args[tokenNumber - 1] = number;
                            }
                        } else {
                            // Must be some token we haven't solved yet
                            TokenData unresolved;
                            unresolved.name = token;
                            unresolved.position = tokenNumber - 1;
                            unresolved.offset = beginOffset;
                            data->unresolvedTokens.push_back(unresolved);
                        }
                        if (!token.empty())
                            tokenNumber++;
                    }
                }
            }
        } else {
            tokenBuffer<<line[i];
        }
    }
    
    if (tokenNumber == 0) {
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
    } else if (token == "FLAGS") {
        return Registers::FLAGS;
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
    } else if (token == "AL") {
        return Registers::AL;
    } else if (token == "BH") {
        return Registers::BH;
    } else if (token == "BL") {
        return Registers::BL;
    } else if (token == "CH") {
        return Registers::CH;
    } else if (token == "CL") {
        return Registers::CL;
    } else if (token == "DH") {
        return Registers::DH;
    } else if (token == "DL") {
        return Registers::DL;
    } else {
        return Registers::UNKNOWN;
    }
}

OPCode* ElectroCraft_CPU::readOPCode(std::string token) {
    OPCode *opcode = new OPCode;
#pragma mark Instruction Set
    if (token == "PUSH") {
        opcode->opCode = InstructionSet::PUSH;
        opcode->numOprands = 1;
    } else if (token == "POP") {
        opcode->opCode = InstructionSet::POP;
        opcode->numOprands = 1;
    } else if (token == "HLT") {
        opcode->opCode = InstructionSet::HLT;
        opcode->numOprands = 0;
    } else if (token == "JE") {
        opcode->opCode = InstructionSet::JE;
        opcode->numOprands = 1;
    } else if (token == "JNE") {
        opcode->opCode = InstructionSet::JNE;
        opcode->numOprands = 1;
    } else if (token == "MOV") {
        opcode->opCode = InstructionSet::MOV;
        opcode->numOprands = 2;
    } else if (token == "CMP") {
        opcode->opCode = InstructionSet::CMP;
        opcode->numOprands = 2;
    } else if (token == "NOP") {
        opcode->opCode = InstructionSet::NOP;
        opcode->numOprands = 0;
    } else if (token == "LOOPE") {
        opcode->opCode = InstructionSet::LOOPE;
        opcode->numOprands = 1;
    } else if (token == "LOOPNE") {
        opcode->opCode = InstructionSet::LOOPNE;
        opcode->numOprands = 1;
    } else if (token == "NOT") {
        opcode->opCode = InstructionSet::NOT;
        opcode->numOprands = 2;
    } else if (token == "AND") {
        opcode->opCode = InstructionSet::AND;
        opcode->numOprands = 2;
    } else if (token == "OR") {
        opcode->opCode = InstructionSet::OR;
        opcode->numOprands = 2;
    } else if (token == "XOR") {
        opcode->opCode = InstructionSet::XOR;
        opcode->numOprands = 2;
    } else if (token == "MUL") {
        opcode->opCode = InstructionSet::MUL;
        opcode->numOprands = 2;
    } else if (token == "ADD") {
        opcode->opCode = InstructionSet::ADD;
        opcode->numOprands = 2;
    } else if (token == "DIV") {
        opcode->opCode = InstructionSet::DIV;
        opcode->numOprands = 2;
    } else if (token == "SUB") {
        opcode->opCode = InstructionSet::SUB;
        opcode->numOprands = 2;
    } else if (token == "RET") {
        opcode->opCode = InstructionSet::RET;
        opcode->numOprands = 0;
    } else if (token == "POPA") {
        opcode->opCode = InstructionSet::POPA;
        opcode->numOprands = 0;
    } else if (token == "PUSHA") {
        opcode->opCode = InstructionSet::PUSHA;
        opcode->numOprands = 0;
    } else if (token == "SA") {
        opcode->opCode = InstructionSet::SHL;
        opcode->numOprands = 2;
    } else if (token == "SAR") {
        opcode->opCode = InstructionSet::SHR;
        opcode->numOprands = 2;
    } else if (token == "LOOPWE") {
        opcode->opCode = InstructionSet::LOOPWE;
        opcode->numOprands = 1;
    } else if (token == "LOOPWNE") {
        opcode->opCode = InstructionSet::LOOPWNE;
        opcode->numOprands = 1;
    } else if (token == "JNZ") {
        opcode->opCode = InstructionSet::JNZ;
        opcode->numOprands = 1;
    } else if (token == "JZ") {
        opcode->opCode = InstructionSet::JZ;
        opcode->numOprands = 1;
    } else if (token == "CPUID") {
        opcode->opCode = InstructionSet::CPUID;
        opcode->numOprands = 0;
    } else if (token == "ROUND") {
        opcode->opCode = InstructionSet::ROUND;
        opcode->numOprands = 2;
    } else if (token == "LOOPZ") {
        opcode->opCode = InstructionSet::LOOPZ;
        opcode->numOprands = 1;
    } else if (token == "LOOPNZ") {
        opcode->opCode = InstructionSet::LOOPNZ;
        opcode->numOprands = 1;
    } else if (token == "CALL") {
        opcode->opCode = InstructionSet::CALL;
        opcode->numOprands = 1;
    } else if (token == "JMP") {
        opcode->opCode = InstructionSet::JMP;
        opcode->numOprands = 1;
    } else if (token == "NEG") {
        opcode->opCode = InstructionSet::NEG;
        opcode->numOprands = 1;
    } else if (token == "LOOP") {
        opcode->opCode = InstructionSet::LOOP;
        opcode->numOprands = 1;
    } else if (token == "RANDI") {
        opcode->opCode = InstructionSet::RANDI;
        opcode->numOprands = 2;
    } else if (token == "INT") {
        opcode->opCode = InstructionSet::INT;
        opcode->numOprands = 1;
    } else if (token == "INP") {
        opcode->opCode = InstructionSet::INP;
        opcode->numOprands = 1;
    } else if (token == "OUT") {
        opcode->opCode = InstructionSet::OUT;
        opcode->numOprands = 1;
    } else if (token == "BKP") {
        opcode->opCode = InstructionSet::BKP;
        opcode->numOprands = 0;
    } else if (token == "JL") {
        opcode->opCode = InstructionSet::JL;
        opcode->numOprands = 1;
    } else if (token == "JG") {
        opcode->opCode = InstructionSet::JG;
        opcode->numOprands = 1;
    } else {
        opcode->opCode = InstructionSet::UNKOWN;
        opcode->numOprands = 0;
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
        case Registers::FLAGS:
        {
            data.doubleWord = static_cast<uint32_t>(eState.flagStates.to_ulong());
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
        case Registers::FLAGS:
            eState.flagStates = std::bitset<10>(data.doubleWord);
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
        case Registers::FLAGS:
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
        case Registers::FLAGS:
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
    
    // Instruction data
    instructionData = memory->readData(registers.IP, OPERATION_SZIE);
    instruction.opCodeFromByte(instructionData[0]);
    instruction.readInfoByte(instructionData[1]);
    instruction.readExtendedInfoByte(instructionData[2]);
    instruction.readExtraInfoByte(instructionData[3]);
    instruction.args[0] = Utils::General::readDoubleWord(&instructionData[4]);
    instruction.args[1] = Utils::General::readDoubleWord(&instructionData[8]);
    instruction.modifier[0] = Utils::General::readDoubleWord(&instructionData[12]);
    instruction.modifier[1] = Utils::General::readDoubleWord(&instructionData[16]);
    
    // Parameters
    Registers reg = Registers::UNKNOWN;
    Registers reg1 = Registers::UNKNOWN;
    RegisterSizes regSize = RegisterSizes::ZERO;
    RegisterSizes regSize1 = RegisterSizes::ZERO;
    
    // Final data types
    DoubleWord data;
    DoubleWord data1;
    unsigned int dataSize = 4;
    unsigned int data1Size = 4;
    
    // Addresses
    Address address;
    Address address1;
    
    // Modifier data
    Modifier modifierType;
    DoubleWord modifier;
    Modifier modifierType1;
    DoubleWord modifier1;
    
    // Default values for data
    address = data = instruction.args[0];
    address1 = data1 = instruction.args[1];
    
    // Size Info
    if (instruction.isByteInPosition(0)) {
        dataSize = 1;
    }
    if (instruction.isByteInPosition(1)) {
        data1Size = 1;
    }
    if (instruction.isWordInPosition(0)) {
        dataSize = 2;
    }
    if (instruction.isWordInPosition(1)) {
        data1Size = 2;
    }
    if (instruction.isDoubleWordInPosition(0)) {
        dataSize = 4;
    }
    if (instruction.isDoubleWordInPosition(1)) {
        data1Size = 4;
    }
    
    // Defined Variables
    if (instruction.isVarInPosition(0)) {
        if (dataSize == 1) {
            if (instruction.isOffsetNegitive(0)) {
                address.word.lowWord.byte.lowByte = registers.IP.word.lowWord.byte.lowByte - instruction.args[0].word.lowWord.byte.lowByte;
            } else {
                address.word.lowWord.byte.lowByte = registers.IP.word.lowWord.byte.lowByte + instruction.args[0].word.lowWord.byte.lowByte;
            }
        } else if (dataSize == 2) {
            if (instruction.isOffsetNegitive(0)) {
                address.word.lowWord.word = registers.IP.word.lowWord.word - instruction.args[0].word.lowWord.word;
            } else {
                address.word.lowWord.word = registers.IP.word.lowWord.word + instruction.args[0].word.lowWord.word;
            }
        } else {
            if (instruction.isOffsetNegitive(0)) {
                address = registers.IP.doubleWord - instruction.args[0].doubleWord;
            } else {
                address = registers.IP.doubleWord + instruction.args[0].doubleWord;
            }
        }
    }
    if (instruction.isVarInPosition(1)) {
        if (dataSize == 1) {
            if (instruction.isOffsetNegitive(1)) {
                address1.word.lowWord.byte.lowByte = registers.IP.word.lowWord.byte.lowByte - instruction.args[1].word.lowWord.byte.lowByte;
            } else {
                address1.word.lowWord.byte.lowByte = registers.IP.word.lowWord.byte.lowByte + instruction.args[1].word.lowWord.byte.lowByte;
            }
        } else if (dataSize == 2) {
            if (instruction.isOffsetNegitive(1)) {
                address1.word.lowWord.word = registers.IP.word.lowWord.word - instruction.args[1].word.lowWord.word;
            } else {
                address1.word.lowWord.word = registers.IP.word.lowWord.word + instruction.args[1].word.lowWord.word;
            }
        } else {
            if (instruction.isOffsetNegitive(1)) {
                address1 = registers.IP.doubleWord - instruction.args[1].doubleWord;
            } else {
                address1 = registers.IP.doubleWord + instruction.args[1].doubleWord;
            }
        }
    }
    
    // Registers
    if (instruction.isRegisterInPosition(0)) {
        if (instruction.shouldUseRegisterAsAddress(0)) {
            address = getRegisterData(Registers(instruction.args[0].doubleWord));
        } else {
            data = getRegisterData(Registers(instruction.args[0].doubleWord));
        }
        reg = Registers(instruction.args[0].doubleWord);
        regSize = registerToSize(reg);
    }
    if (instruction.isRegisterInPosition(1)) {
        if (instruction.shouldUseRegisterAsAddress(1)) {
            address1 = getRegisterData(Registers(instruction.args[1].doubleWord));
        } else {
            data1 = getRegisterData(Registers(instruction.args[1].doubleWord));
        }
        reg1 = Registers(instruction.args[1].doubleWord);
        regSize1 = registerToSize(reg1);
    }
    
    // Modifiers
    if (instruction.getModifierForPosition(0) != Modifier::NOM) {
        modifierType = instruction.getModifierForPosition(0);
        modifier = instruction.modifier[0];
        
        if (modifierType == Modifier::ADDM) {
            address.doubleWord += modifier.doubleWord;
        } else if (modifierType == Modifier::SUBM) {
            address.doubleWord -= modifier.doubleWord;
        }
    }
    if (instruction.getModifierForPosition(1) != Modifier::NOM) {
        modifierType1 = instruction.getModifierForPosition(1);
        modifier1 = instruction.modifier[1];
        
        if (modifierType1 == Modifier::ADDM) {
            address1.doubleWord += modifier1.doubleWord;
        } else if (modifierType1 == Modifier::SUBM) {
            address1.doubleWord -= modifier1.doubleWord;
        }
    }
    
    // Read basic data if the type requires it
    if (instruction.isAddressInPosition(0) || instruction.isVarInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
        if (dataSize == 1) {
            data = *memory->readData(address, dataSize);
            if (data.doubleWord == 0xA) {
                asm("nop");
            }
        } else if (dataSize == 2) {
            data.word = Utils::General::readWord(memory->readData(address, dataSize));
        } else {
            data = Utils::General::readDoubleWord(memory->readData(address, dataSize));
        }
    }
    if (instruction.isAddressInPosition(1) || instruction.isVarInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) {
        if (data1Size == 1) {
            data1 = *memory->readData(address1, data1Size);
        } else if (data1Size == 2) {
            data1.word = Utils::General::readWord(memory->readData(address1, data1Size));
        } else {
            data1 = Utils::General::readDoubleWord(memory->readData(address1, data1Size));
        }
    }
    
    // Lets match the opcode to the operation
    switch (instruction.opCode) {
        case InstructionSet::ADD:
        {
            // [Address], <Register, Const>
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data + data1, dataSize));
                // <Register>, [Address]
            } else if ((instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) && reg != Registers::UNKNOWN) {
                setRegisterData(data1 + data, reg);
                // <Register>, <Register>
            } else if (reg != Registers::UNKNOWN && reg1 != Registers::UNKNOWN) {
                setRegisterData(data + data1, reg);
                // <Register>, <Const>
            } else if (regSize != RegisterSizes::ZERO) {
                setRegisterData(data + data1, reg);
                // <Varaible>, <Reg, Const, Address>
            } else if (instruction.isVarInPosition(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data + data1, dataSize));
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for ADD!"<<std::endl;
            }
        }
            break;
        case InstructionSet::AND:
        {
            // [Address], <Register, Const>
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data.doubleWord & data1.doubleWord, dataSize));
                // <Register>, [Address]
            } else if ((instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) && reg != Registers::UNKNOWN) {
                setRegisterData(data1.doubleWord & data.doubleWord, reg);
                // <Register>, <Register>
            } else if (reg != Registers::UNKNOWN && reg1 != Registers::UNKNOWN) {
                setRegisterData(data.doubleWord & data1.doubleWord, reg);
                // <Register>, <Const>
            } else if (regSize != RegisterSizes::ZERO) {
                setRegisterData(data.doubleWord & data1.doubleWord, reg);
                // <Varaible>, <Reg, Const, Address>
            } else if (instruction.isVarInPosition(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data.doubleWord & data1.doubleWord, dataSize));
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for ADD!"<<std::endl;
            }
        }
            break;
        case InstructionSet::BKP:
        {
            char enterBuffer;
            printf("EAX = %d, EBX = %d, ECX = %d, EDX = %d, ESI = %d, EDI = %d\n", getRegisterData(Registers::EAX).doubleWord, getRegisterData(Registers::EBX).doubleWord, getRegisterData(Registers::ECX).doubleWord, getRegisterData(Registers::EDX).doubleWord ,getRegisterData(Registers::ESI).doubleWord, getRegisterData(Registers::EDI).doubleWord);
            printf("EBP = %d, EIP = %d, ESP = %d, EMC = %d, AX = %d, BX = %d\n", getRegisterData(Registers::EBP).doubleWord, getRegisterData(Registers::EIP).doubleWord, getRegisterData(Registers::ESP).doubleWord, getRegisterData(Registers::EMC).doubleWord ,getRegisterData(Registers::AX).doubleWord, getRegisterData(Registers::BX).doubleWord);
            printf("CX = %d, DX = %d, SI = %d, DI = %d, BP = %d, IP = %d\n", getRegisterData(Registers::CX).doubleWord, getRegisterData(Registers::DX).doubleWord, getRegisterData(Registers::SI).doubleWord, getRegisterData(Registers::DI).doubleWord ,getRegisterData(Registers::BP).doubleWord, getRegisterData(Registers::IP).doubleWord);
            printf("SP = %d, MC = %d, AH = %d, AL = %d, BH = %d, BL = %d\n", getRegisterData(Registers::SP).doubleWord, getRegisterData(Registers::MC).doubleWord, getRegisterData(Registers::AH).word.lowWord.byte.hiByte, getRegisterData(Registers::AL).word.lowWord.byte.lowByte, getRegisterData(Registers::BH).word.lowWord.byte.hiByte, getRegisterData(Registers::BL).word.lowWord.byte.lowByte);
            printf("CH = %d, CL = %d, DH = %d, DL = %d\n", getRegisterData(Registers::CH).word.lowWord.byte.hiByte, getRegisterData(Registers::CL).word.lowWord.byte.lowByte, getRegisterData(Registers::DH).word.lowWord.byte.hiByte, getRegisterData(Registers::DL).word.lowWord.byte.lowByte);
            std::cout<<"Press Any Key To Continue!"<<std::endl;
            std::cin>>enterBuffer;
        }
            break;
        case InstructionSet::CALL:
        {
            if (instruction.isOffsetInPosition(0)) {
                Address addressToCall;
                if (instruction.isOffsetNegitive(0)) {
                    addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                } else {
                    addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                }
                stack->push(getRegisterData(Registers::EIP).doubleWord);
                setRegisterData(addressToCall, Registers::EIP);
                hasJumped = true;
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for CALL <Label>!"<<std::endl;
            }
        }
            break;
        case InstructionSet::CMP:
        {
            long result = 0;
            if (dataSize == 1) {
                if (data1Size == 1) {
                    result = (long)data.word.lowWord.byte.lowByte  - (long)data1.word.lowWord.byte.lowByte;
                } else if (data1Size == 2) {
                    result = (long)data.word.lowWord.byte.lowByte  - (long)data1.word.lowWord.word;
                } else if (data1Size == 4) {
                    result = (long)data.word.lowWord.byte.lowByte  - (long)data1.doubleWord;
                }
            } else if (dataSize == 2) {
                if (data1Size == 1) {
                    result = (long)data.word.lowWord.word  - (long)data1.word.lowWord.byte.lowByte;
                } else if (data1Size == 2) {
                    result = (long)data.word.lowWord.word  - (long)data1.word.lowWord.word;
                } else if (data1Size == 4) {
                    result = (long)data.word.lowWord.word  - (long)data1.doubleWord;
                }
            } else if (dataSize == 4) {
                if (data1Size == 1) {
                    result = (long)data.doubleWord  - (long)data1.word.lowWord.byte.lowByte;
                } else if (data1Size == 2) {
                    result = (long)data.doubleWord  - (long)data1.word.lowWord.word;
                } else if (data1Size == 4) {
                    result = (long)data.doubleWord  - (long)data1.doubleWord;
                }
            }
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                if (result == 0) {
                    eState.setFlagState(EFLAGS::ZF);
                    eState.resetFlag(EFLAGS::CF);
                } else {
                    eState.resetFlag(EFLAGS::ZF);
                }
                if (result > 0) {
                    eState.resetFlag(EFLAGS::ZF);
                    eState.resetFlag(EFLAGS::CF);
                }
                if (result < 0) {
                    eState.resetFlag(EFLAGS::ZF);
                    eState.setFlagState(EFLAGS::CF);
                }
            } else if (instruction.isRegisterInPosition(0)) {
                if (result == 0) {
                    eState.setFlagState(EFLAGS::ZF);
                    eState.resetFlag(EFLAGS::CF);
                } else {
                    eState.resetFlag(EFLAGS::ZF);
                }
                if (result > 0) {
                    eState.resetFlag(EFLAGS::ZF);
                    eState.resetFlag(EFLAGS::CF);
                }
                if (result < 0) {
                    eState.resetFlag(EFLAGS::ZF);
                    eState.setFlagState(EFLAGS::CF);
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for CMP!"<<std::endl;
            }
        }
            break;
        case InstructionSet::CPUID:
            break;
        case InstructionSet::DIV:
        {
            if (instruction.isRegisterInPosition(0)) {
                if (instruction.isRegisterInPosition(1)) {
                    DoubleWord result;
                    result.doubleWord = data.doubleWord / data1.doubleWord;
                    setRegisterData(result, reg);
                } else if (instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) {
                    DoubleWord result;
                    result.doubleWord = data.doubleWord  / data1.doubleWord;
                    setRegisterData(result, reg);
                } else {
                    std::cerr<<"ElectroCraft CPU: Invalid arguments for DIV!"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invalid arguments for DIV!"<<std::endl;
            }
        }
            break;
        case InstructionSet::HLT:
        {
            this->stop();
            return;
        }
        case InstructionSet::INP:
        {
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                if (instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) {
                    IOPort::IOPortInterrupt interrupt;
                    interrupt.read = true;
                    interrupt.ioPort = data1;
                    IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                    memory->writeData(address, dataSize, Utils::General::numberToBytes(result->returnData, dataSize));
                } else if (instruction.isRegisterInPosition(1)) {
                    if (instruction.shouldUseRegisterAsAddress(1)) {
                        IOPort::IOPortInterrupt interrupt;
                        interrupt.read = true;
                        interrupt.ioPort = data1;
                        IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                        memory->writeData(address, dataSize, Utils::General::numberToBytes(result->returnData, dataSize));
                    } else {
                        IOPort::IOPortInterrupt interrupt;
                        interrupt.read = true;
                        interrupt.ioPort = data1;
                        IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                        memory->writeData(address, dataSize, Utils::General::numberToBytes(result->returnData, dataSize));
                    }
                } else {
                    IOPort::IOPortInterrupt interrupt;
                    interrupt.read = true;
                    interrupt.ioPort = data1;
                    IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                    memory->writeData(address, dataSize, Utils::General::numberToBytes(result->returnData, dataSize));
                }
            } else if (instruction.isRegisterInPosition(0)) {
                if (instruction.shouldUseRegisterAsAddress(0)) {
                    if (instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) {
                        IOPort::IOPortInterrupt interrupt;
                        interrupt.read = true;
                        interrupt.ioPort = data1;
                        IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                        memory->writeData(address, dataSize, Utils::General::numberToBytes(result->returnData, dataSize));
                    } else if (instruction.isRegisterInPosition(1)) {
                        if (instruction.shouldUseRegisterAsAddress(1)) {
                            IOPort::IOPortInterrupt interrupt;
                            interrupt.read = true;
                            interrupt.ioPort = data1;
                            IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                            memory->writeData(address, dataSize, Utils::General::numberToBytes(result->returnData, dataSize));
                        } else {
                            IOPort::IOPortInterrupt interrupt;
                            interrupt.read = true;
                            interrupt.ioPort = data1;
                            IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                            memory->writeData(address, dataSize, Utils::General::numberToBytes(result->returnData, dataSize));
                        }
                    } else {
                        IOPort::IOPortInterrupt interrupt;
                        interrupt.read = true;
                        interrupt.ioPort = data1;
                        IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                        memory->writeData(address, dataSize, Utils::General::numberToBytes(result->returnData, dataSize));
                    }
                } else {
                    if (instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) {
                        IOPort::IOPortInterrupt interrupt;
                        interrupt.read = true;
                        interrupt.ioPort = data1;
                        IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                        setRegisterData(result->returnData, reg);
                    } else if (instruction.isRegisterInPosition(1)) {
                        if (instruction.shouldUseRegisterAsAddress(1)) {
                            IOPort::IOPortInterrupt interrupt;
                            interrupt.read = true;
                            interrupt.ioPort = data1;
                            IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                            setRegisterData(result->returnData, reg);
                        } else {
                            IOPort::IOPortInterrupt interrupt;
                            interrupt.read = true;
                            interrupt.ioPort = data1;
                            IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                            setRegisterData(result->returnData, reg);
                        }
                    } else {
                        IOPort::IOPortInterrupt interrupt;
                        interrupt.read = true;
                        interrupt.ioPort = data1;
                        IOPort::IOPortResult *result = ioPortHandler->callIOPort(interrupt);
                        setRegisterData(result->returnData, reg);
                    }
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invalid arguments for INP!"<<std::endl;
            }
        }
            break;
        case InstructionSet::INT:
            break;
        case InstructionSet::JG:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (!eState.getFlagState(EFLAGS::ZF) && !eState.getFlagState(EFLAGS::CF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            }
        }
            break;
        case InstructionSet::JL:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (!eState.getFlagState(EFLAGS::ZF) && eState.getFlagState(EFLAGS::CF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            }
        }
            break;
        case InstructionSet::JE:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (eState.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for JE!"<<std::endl;
            }
        }
            break;
        case InstructionSet::JMP:
        {
            if (instruction.isOffsetInPosition(0)) {
                Address addressToCall;
                if (instruction.isOffsetNegitive(0)) {
                    addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                } else {
                    addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                }
                setRegisterData(addressToCall, Registers::EIP);
                hasJumped = true;
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for JMP!"<<std::endl;
            }
        }
            break;
        case InstructionSet::JNE:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (!eState.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for JNE!"<<std::endl;
            }
        }
            break;
        case InstructionSet::JNZ:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (!eState.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for JNZ!"<<std::endl;
            }
        }
            break;
        case InstructionSet::JZ:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (eState.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for JZ!"<<std::endl;
            }
        }
            break;
        case InstructionSet::LOOP:
        {
            if (instruction.isOffsetInPosition(0)) {
                DoubleWord counter = getRegisterData(Registers::CX);
                if (counter.word.lowWord.word > 0) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    counter.word.lowWord.word--;
                    setRegisterData(counter, Registers::CX);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOP!"<<std::endl;
            }
        }
            break;
        case InstructionSet::LOOPE:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (eState.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPE!"<<std::endl;
            }
        }
            break;
        case InstructionSet::LOOPNE:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (!eState.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPNE!"<<std::endl;
            }
        }
            break;
        case InstructionSet::LOOPNZ:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (!eState.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPNZ!"<<std::endl;
            }
        }
            break;
        case InstructionSet::LOOPWE:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (eState.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPE!"<<std::endl;
            }
        }
            break;
        case InstructionSet::LOOPWNE:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (!eState.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPNZ!"<<std::endl;
            }
        }
            break;
        case InstructionSet::LOOPZ:
        {
            if (instruction.isOffsetInPosition(0)) {
                if (eState.getFlagState(EFLAGS::ZF)) {
                    Address addressToCall;
                    if (instruction.isOffsetNegitive(0)) {
                        addressToCall.doubleWord = registers.IP.doubleWord - data.doubleWord;
                    } else {
                        addressToCall.doubleWord = registers.IP.doubleWord + data.doubleWord;
                    }
                    setRegisterData(addressToCall, Registers::EIP);
                    hasJumped = true;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for LOOPE!"<<std::endl;
            }
        }
            break;
        case InstructionSet::MOV:
        {
            // [Address], <Register, Const>
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data1, data1Size));
                // <Register>, [Address]
            } else if ((instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) && reg != Registers::UNKNOWN) {
                setRegisterData(data1, reg);
                // <Register>, <Register>
            } else if (reg != Registers::UNKNOWN && reg1 != Registers::UNKNOWN) {
                setRegisterData(data1, reg);
                // <Register>, <Const>
            } else if (regSize != RegisterSizes::ZERO) {
                setRegisterData(data1, reg);
                // <Varaible>, <Reg, Const, Address>
            } else if (instruction.isVarInPosition(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data1, dataSize));
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for MOV!"<<std::endl;
            }
        }
            break;
        case InstructionSet::MUL:
        {
            if (instruction.isRegisterInPosition(0)) {
                DoubleWord result;
                result.doubleWord = data.doubleWord * data1.doubleWord;
                setRegisterData(result, reg);
            } else if (instruction.isAddressInPosition(0) || instruction.isVarInPosition(0)) {
                DoubleWord result;
                result.doubleWord = data.doubleWord * data1.doubleWord;
                memory->writeData(address, dataSize, Utils::General::numberToBytes(result, dataSize));
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for MUL!"<<std::endl;
            }
        }
            break;
        case InstructionSet::NEG:
        {
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                DoubleWord result;
                result.doubleWord = -data.doubleWord;
                memory->writeData(address, dataSize, Utils::General::numberToBytes(result, dataSize));
            } else if (instruction.isRegisterInPosition(0)) {
                DoubleWord result;
                result.doubleWord = -data.doubleWord;
                if (data.doubleWord > 0) {
                    eState.setFlagState(EFLAGS::SF);
                    if (result.doubleWord == 0) {
                        eState.setFlagState(EFLAGS::ZF);
                    }
                } else {
                    eState.resetFlag(EFLAGS::SF);
                    if (result.doubleWord == 0) {
                        eState.resetFlag(EFLAGS::ZF);
                    }
                }
                setRegisterData(result, reg);
            } else {
                std::cerr<<"ElectroCraft CPU: Invalid arguments for NEG!"<<std::endl;
            }
        }
            break;
        case InstructionSet::NOP:
            break;
        case InstructionSet::NOT:
        {
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                DoubleWord result;
                result.doubleWord = ~data.doubleWord;
                memory->writeData(address, dataSize, Utils::General::numberToBytes(result, dataSize));
            } else if (instruction.isRegisterInPosition(0)) {
                DoubleWord result;
                result.doubleWord = ~data.doubleWord;
                setRegisterData(result, reg);
            } else {
                std::cerr<<"ElectroCraft CPU: Invalid arguments for NOT!"<<std::endl;
            }
        }
            break;
        case InstructionSet::OR:
        {
            // [Address], <Register, Const>
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data.doubleWord | data1.doubleWord, dataSize));
                // <Register>, [Address]
            } else if ((instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) && reg != Registers::UNKNOWN) {
                setRegisterData(data1.doubleWord | data.doubleWord, reg);
                // <Register>, <Register>
            } if (reg != Registers::UNKNOWN && reg1 != Registers::UNKNOWN) {
                setRegisterData(data.doubleWord | data1.doubleWord, reg);
                // <Register>, <Const>
            } else if (regSize != RegisterSizes::ZERO) {
                setRegisterData(data.doubleWord | data1.doubleWord, reg);
                // <Varaible>, <Reg, Const, Address>
            } else if (instruction.isVarInPosition(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data.doubleWord | data1.doubleWord, dataSize));
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for OR!"<<std::endl;
            }
        }
            break;
        case InstructionSet::OUT:
        {
            IOPort::IOPortInterrupt interrupt;
            interrupt.read = false;
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                interrupt.ioPort = data;
            } else if (instruction.isRegisterInPosition(0)) {
                interrupt.ioPort = data;
            } else {
                interrupt.ioPort = data;
            }
            
            if (instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) {
                interrupt.data = data1;
            } else if (instruction.isRegisterInPosition(1)) {
                if (instruction.shouldUseRegisterAsAddress(1)) {
                    interrupt.data = data1;
                } else {
                    interrupt.data = data1;
                }
            } else {
                interrupt.data = data1;
            }
            ioPortHandler->callIOPort(interrupt);
        }
            break;
        case InstructionSet::POP:
        {
            if (instruction.isRegisterInPosition(0)) {
                setRegisterData(stack->pop(), reg);
            } else if (instruction.isAddressInPosition(0)) {
                memory->writeData(address, 4, Utils::General::doubleWordToBytes(stack->pop()));
            } else {
                std::cerr<<"ElectroCraft CPU: No register or address in position 0 for POP"<<std::endl;
            }
        }
            break;
        case InstructionSet::POPA:
        {
            for (int i = Registers::TOAL_SIZE; i > 0; i--) {
                setRegisterData(stack->pop(), Registers(i));
            }
        }
            break;
        case InstructionSet::PUSH:
        {
            if (instruction.isVarInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                stack->push(address);
            } else {
                stack->push(data);
            }
        }
            break;
        case InstructionSet::PUSHA:
        {
            for (int i = 0; i < Registers::TOAL_SIZE; i++) {
                DoubleWord data = getRegisterData(Registers(i));
                stack->push(data);
            }
        }
            break;
        case InstructionSet::RANDI:
        {
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                if (instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) {
                    DoubleWord upperBound = data1;
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(data.doubleWord, upperBound.doubleWord);
                    result.doubleWord = dis(gen);
                    memory->writeData(address, dataSize, Utils::General::numberToBytes(result, dataSize));
                } else if (instruction.isRegisterInPosition(1)) {
                    DoubleWord upperBound = data1;
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(data.doubleWord, upperBound.doubleWord);
                    result.doubleWord = dis(gen);
                    memory->writeData(address, dataSize, Utils::General::numberToBytes(result, dataSize));
                } else {
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(data.doubleWord, data1.doubleWord);
                    result.doubleWord = dis(gen);
                    memory->writeData(address, dataSize, Utils::General::numberToBytes(result, dataSize));
                }
            } else if (instruction.isRegisterInPosition(0)) {
                if (instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) {
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(data.doubleWord, data1.doubleWord);
                    result.doubleWord = dis(gen);
                    setRegisterData(result, reg);
                } else if (instruction.isRegisterInPosition(1)) {
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(data.doubleWord, data1.doubleWord);
                    result.doubleWord = dis(gen);
                    setRegisterData(result, reg);
                } else {
                    DoubleWord result;
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(data.doubleWord, data1.doubleWord);
                    result.doubleWord = dis(gen);
                    setRegisterData(result, reg);
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Incorrect arguments for RANDI!"<<std::endl;
            }
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
        {
            if (instruction.isRegisterInPosition(0)) {
                if (instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) {
                    if (reg == Registers::CL) {
                        DoubleWord result;
                        result.doubleWord = data.doubleWord << data1.word.lowWord.byte.lowByte;
                        if (result.doubleWord == 0) {
                            eState.setFlagState(EFLAGS::ZF);
                        }
                        setRegisterData(result, reg);
                    } else if ((!instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) && !instruction.isOffsetInPosition(1)) {
                        DoubleWord result;
                        result.doubleWord = data.doubleWord << data1.word.lowWord.byte.lowByte;
                        if (result.doubleWord == 0) {
                            eState.setFlagState(EFLAGS::ZF);
                        }
                        setRegisterData(result, reg);
                    } else {
                        std::cerr<<"ElectroCraft CPU: Incorrect arguments for SHL!"<<std::endl;
                    }
                }
            } else if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                if (reg1 == Registers::CL) {
                    DoubleWord result;
                    result.doubleWord = data.doubleWord << data1.word.lowWord.byte.lowByte;
                    if (result.doubleWord == 0) {
                        eState.setFlagState(EFLAGS::ZF);
                    }
                    memory->writeData(address, dataSize, Utils::General::numberToBytes(result, dataSize));
                } else if ((!instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) && !instruction.isOffsetInPosition(1)) {
                    DoubleWord result;
                    result.doubleWord = data.doubleWord << data1.word.lowWord.byte.lowByte;
                    if (result.doubleWord == 0) {
                        eState.setFlagState(EFLAGS::ZF);
                    }
                    memory->writeData(address, dataSize, Utils::General::numberToBytes(result, dataSize));
                } else {
                    std::cerr<<"ElectroCraft CPU: Incorrect arguments for SHL!"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Incorrect arguments for SHL!"<<std::endl;
            }
        }
            break;
        case InstructionSet::SHR:
        {
            if (instruction.isRegisterInPosition(0)) {
                if (reg == Registers::CL) {
                    DoubleWord result;
                    result.doubleWord = data.doubleWord >> data1.word.lowWord.byte.lowByte;
                    if (result.doubleWord == 0) {
                        eState.setFlagState(EFLAGS::ZF);
                    }
                    setRegisterData(result, reg);
                } else if ((!instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) && !instruction.isOffsetInPosition(1)) {
                    DoubleWord result;
                    result.doubleWord = data.doubleWord >> data1.word.lowWord.byte.lowByte;
                    if (result.doubleWord == 0) {
                        eState.setFlagState(EFLAGS::ZF);
                    }
                    setRegisterData(result, reg);
                } else {
                    std::cerr<<"ElectroCraft CPU: Incorrect arguments for SHR!"<<std::endl;
                }
            } else if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                if (reg1 == Registers::CL) {
                    DoubleWord result;
                    result.doubleWord = data.doubleWord >> data1.word.lowWord.byte.lowByte;
                    if (result.doubleWord == 0) {
                        eState.setFlagState(EFLAGS::ZF);
                    }
                    memory->writeData(address, dataSize, Utils::General::numberToBytes(result, dataSize));
                } else if ((!instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) && !instruction.isOffsetInPosition(1)) {
                    DoubleWord result;
                    result.doubleWord = data.doubleWord >> data1.word.lowWord.byte.lowByte;
                    if (result.doubleWord == 0) {
                        eState.setFlagState(EFLAGS::ZF);
                    }
                    memory->writeData(address, dataSize, Utils::General::numberToBytes(result, dataSize));
                } else {
                    std::cerr<<"ElectroCraft CPU: Incorrect arguments for SHR!"<<std::endl;
                }
            } else {
                std::cerr<<"ElectroCraft CPU: Incorrect arguments for SHR!"<<std::endl;
            }
        }
            break;
        case InstructionSet::SUB:
        {
            // [Address], <Register, Const>
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data.doubleWord - data1.doubleWord, dataSize));
                // <Register>, [Address]
            } else if ((instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) && reg != Registers::UNKNOWN) {
                setRegisterData(data1.doubleWord - data.doubleWord, reg);
                // <Register>, <Register>
            } else if (reg != Registers::UNKNOWN && reg1 != Registers::UNKNOWN) {
                setRegisterData(data.doubleWord - data1.doubleWord, reg);
                // <Register>, <Const>
            } else if (regSize != RegisterSizes::ZERO) {
                setRegisterData(data.doubleWord - data1.doubleWord, reg);
                // <Varaible>, <Reg, Const, Address>
            } else if (instruction.isVarInPosition(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data.doubleWord - data1.doubleWord, dataSize));
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for SUB!"<<std::endl;
            }
        }
            break;
        case InstructionSet::XOR:
        {
            // [Address], <Register, Const>
            if (instruction.isAddressInPosition(0) || instruction.shouldUseRegisterAsAddress(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data.doubleWord ^ data1.doubleWord, dataSize));
                // <Register>, [Address]
            } else if ((instruction.isAddressInPosition(1) || instruction.shouldUseRegisterAsAddress(1)) && reg != Registers::UNKNOWN) {
                setRegisterData(data1.doubleWord ^ data.doubleWord, reg);
                // <Register>, <Register>
            } else if (reg != Registers::UNKNOWN && reg1 != Registers::UNKNOWN) {
                setRegisterData(data.doubleWord ^ data1.doubleWord, reg);
                // <Register>, <Const>
            } else if (regSize != RegisterSizes::ZERO) {
                setRegisterData(data.doubleWord ^ data1.doubleWord, reg);
                // <Varaible>, <Reg, Const, Address>
            } else if (instruction.isVarInPosition(0)) {
                memory->writeData(address, dataSize, Utils::General::numberToBytes(data.doubleWord ^ data1.doubleWord, dataSize));
            } else {
                std::cerr<<"ElectroCraft CPU: Invaid arguments for XOR!"<<std::endl;
            }
        }
            break;
        default:
            std::cerr<<"Unknown Instruction! Halting CPU to prevent damage!"<<std::endl;
            stop();
    }
    if (!hasJumped)
        registers.IP.doubleWord += OPERATION_SZIE;
    delete instructionData;
}

ElectroCraftVGA* ElectroCraft_CPU::getVideoCard() {
    return videoCard;
}

ElectroCraftTerminal* ElectroCraft_CPU::getTerminal() {
    return terminal;
}

ElectroCraftKeyboard* ElectroCraft_CPU::getKeyboard() {
    return keyboard;
}

bool ElectroCraft_CPU::isRunning() {
    return clock->isRunning();
}

Address ElectroCraft_CPU::loadIntoMemory(Byte *data, unsigned int length, unsigned int codeOffset) {
    MemoryInfo* memoryBlock = memory->allocate(length);
    memoryBlock->setData(data, length);
    return memoryBlock->startOffset.doubleWord + codeOffset;
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