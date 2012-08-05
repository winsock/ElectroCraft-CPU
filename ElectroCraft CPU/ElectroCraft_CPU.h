/*
 *  ElectroCraft_CPU.h
 *  ElectroCraft CPU
 *
 *  Created by Andrew Querol on 8/3/12.
 *  Copyright (c) 2012 Cerios Software. All rights reserved.
 *
 */

#ifndef ElectroCraft_CPU_
#define ElectroCraft_CPU_

#include <string>
#include <cstdint>
#include <bitset>
#include <vector>
#include "Utils.h"

/* The classes below are exported */
#pragma GCC visibility push(default)

// The size of an operation in bytes
#define OPERATION_SZIE 10

enum InstructionSet {
    PUSH = 0,
    POP = 1,
    HLT = 2,
    JE = 3,
    JNE = 4,
    MOV = 5,
    CMP = 6,
    NOP = 7,
    LOOPE = 8,
    LOOPNE = 9,
    NOT = 10,
    AND = 11,
    OR = 12,
    XOR = 13,
    MUL = 14,
    ADD = 15,
    DIV = 16,
    SUB = 17,
    RET = 18,
    POPA = 19,
    PUSHA = 20,
    SHL = 21,
    SHR = 22,
    LOOPWE = 23,
    LOOPWNE = 24,
    JNZ = 25,
    JZ = 26,
    CPUID = 27,
    ROUND = 28,
    LOOPZ = 29,
    LOOPNZ = 30,
    CALL = 31,
    JMP = 32,
    NEG = 33,
    LOOP = 34,
    UNKOWN = 63
    };

enum Section {
    CODE,
    DATA
    };

enum Registers {
    EAX = 0,
    EBX = 1,
    ECX = 2,
    EDX = 3,
    ESI = 4,
    EDI = 5,
    EBP = 6,
    EIP = 7,
    ESP = 8,
    EMC = 9,
    
    AX = 10,
    BX = 11,
    CX = 12,
    DX = 13,
    SI = 14,
    DI = 15,
    BP = 16,
    IP = 17,
    SP = 18,
    MC = 19,
    
    AH = 20,
    AL = 21,
    BH = 22,
    BL = 23,
    CH = 24,
    CL = 25,
    DH = 26,
    DL = 27,
    
    EFLAGS = 28,
    TOAL_SIZE,
    UNKNOWN = 63
    };

enum RegisterSizes {
    FOUR = 4,
    TWO = 2,
    ONE = 1,
    ZERO = 0
    };

enum RegisterType {
    LOW = 0,
    HIGH = 1,
    NEITHER = 2
    };

struct RegisterState {
    DoubleWord A;
    DoubleWord B;
    DoubleWord C;
    DoubleWord D;
    DoubleWord SI;
    DoubleWord DI;
    DoubleWord BP;
    DoubleWord IP;
    DoubleWord SP;
    DoubleWord MC;
    DoubleWord EFLAGS;
};

typedef unsigned char Byte;

struct StackData {
    Byte* data;
    unsigned int length;
};

struct TokenData {
    std::string name;
    DoubleWord offset;
};

struct OPCode {
    InstructionSet opCode = InstructionSet::UNKOWN; // 6 Bits max size of 63
    uint8_t numOprands; // Only uses the first two bits MAX number of 3
    std::bitset<8> infoBits;
    
    Byte getOpCodeByte() {
        return ((numOprands << 6) | (opCode & 0x3F));
    }
    
    void opCodeFromByte(Byte byte) {
        opCode = InstructionSet(byte & 0x3F);
        numOprands = ((byte & 0xC0) >> 6);
    }
    
    Byte getInfoByte() {
        return infoBits.to_ulong();
    }
    
    void readInfoByte(Byte info) {
        infoBits = std::bitset<8>(info);
    }
    
    bool isRegisterInPosition(int position) {
        return infoBits.test(position);
    }
    
    bool isAddressInPosition(int position) {
        return infoBits.test(position + 2);
    }
    
    void setAddressInPosition(int position) {
        infoBits.set(position + 2);
    }
    
    bool isOffsetInPosition(int position) {
        return infoBits.test(position + 4);
    }
    
    void setOffsetInPosition(int position) {
        infoBits.set(position + 4);
    }
    
    DoubleWord* args = new DoubleWord[2];
};

struct FirstPassData {
    OPCode opcode;
    DoubleWord beginOffset;
    TokenData token;
    TokenData* unresolvedTokens = new TokenData[2];
};

struct AssembledData {
    Byte* data;
    int length;
};

enum EFLAGS {
    CF = 0,
    PF = 1,
    AF = 2,
    ZF = 3,
    SF = 4,
    TF = 5,
    IF = 6,
    DF = 7,
    OF = 8,
    RF = 9,
    ID = 10,
    TOTAL_SIZE
};

struct EFLAGSState {
    std::bitset<EFLAGS::TOTAL_SIZE> flagStates;
    
    bool getFlagState(enum EFLAGS flag) {
        return flagStates.test(flag);
    }
    
    void setFlagState(enum EFLAGS flag) {
        flagStates.set(flag);
    }
    
    void resetFlag(enum EFLAGS flag) {
        flagStates.reset(flag);
    }
};

class ElectroCraftMemory;
class ElectroCraftStack;

class ElectroCraft_CPU
{
    RegisterState registers;
    EFLAGSState eflags;
    ElectroCraftMemory *memory;
    ElectroCraftStack *stack;
    Section currentSection = Section::CODE;
public:
    ElectroCraft_CPU();
    ~ElectroCraft_CPU();
    AssembledData assemble(std::vector<std::string>);
    void execMemory(Address baseAddress);
    Address loadIntoMemory(Byte* data, int length);
    void stop();
    void reset();
private:
    FirstPassData* firstPass(std::string line, DoubleWord offset);
    Registers getRegister(std::string token);
    DoubleWord readDoubleWord(Byte* data);
    DoubleWord::Word readWord(Byte* data);
    Byte* doubleWordToBytes(DoubleWord word);
    Byte* wordToBytes(DoubleWord::Word word);
    OPCode readOPCode(std::string token);
    DoubleWord getRegisterData(Registers reg);
    RegisterSizes registerToSize(Registers reg);
    RegisterType getRegisterType(Registers reg);
    void setRegisterData(DoubleWord data, Registers reg);
};

#pragma GCC visibility pop
#endif
