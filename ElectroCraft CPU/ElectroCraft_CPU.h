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
#include "ElectroCraftTickable.h"

/* The classes below are exported */
#pragma GCC visibility push(default)

// The size of an operation in bytes
#define OPERATION_SZIE 19

enum InstructionSet {
    PUSH = 1,
    POP = 2,
    HLT = 3,
    JE = 4,
    JNE = 5,
    MOV = 6,
    CMP = 7,
    NOP = 8,
    LOOPE = 9,
    LOOPNE = 10,
    NOT = 11,
    AND = 12,
    OR = 13,
    XOR = 14,
    MUL = 15,
    ADD = 16,
    DIV = 17,
    SUB = 18,
    RET = 19,
    POPA = 20,
    PUSHA = 21,
    SHL = 22,
    SHR = 23,
    LOOPWE = 24,
    LOOPWNE = 25,
    JNZ = 26,
    JZ = 27,
    CPUID = 28,
    ROUND = 29,
    LOOPZ = 30,
    LOOPNZ = 31,
    CALL = 32,
    JMP = 33,
    NEG = 34,
    LOOP = 35,
    RANDI = 36,
    INT = 37,
    INP = 38,
    OUT = 39,
    BKP = 40,
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
    
    FLAGS = 28,
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

enum VarType {
    DB = 0,
    DW = 1,
    DD = 2
};

struct TokenData {
    std::string name;
    DoubleWord offset;
    int position = 0;
    
    bool isTokenVar = false;
    VarType varType;
    std::vector<Byte> varData;
};

enum Modifier {
    NOM = 0,
    ADDM = 1,
    SUBM = 2
    };

struct OPCode {
    InstructionSet opCode = InstructionSet::UNKOWN; // 6 Bits max size of 63
    uint8_t numOprands; // Only uses the first two bits MAX number of 3
    std::bitset<8> infoBits;
    std::bitset<8> extendedInfoBits;
    
    OPCode() {};
    
    ~OPCode() {
        delete[] args;
    }
    
    OPCode(const OPCode &copy) {
        opCode = copy.opCode;
        numOprands = copy.numOprands;
        infoBits = copy.infoBits;
        extendedInfoBits = copy.extendedInfoBits;
        args = copy.args;
        modifier = copy.modifier;
    }
    
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
    
    Byte getExtendedInfoByte() {
        return extendedInfoBits.to_ulong();
    }
    
    void readInfoByte(Byte info) {
        infoBits = std::bitset<8>(info);
    }
    
    void readExtendedInfoByte(Byte extendedInfo) {
        extendedInfoBits = std::bitset<8>(extendedInfo);
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
    
    bool isOffsetNegitive(int position) {
        return infoBits.test(position + 6);
    }
    
    void setOffsetNegitiveInPosition(int position) {
        infoBits.set(position + 6);
    }
    
    bool shouldUseRegisterAsAddress(int position) {
        return extendedInfoBits.test(position);
    }
    
    void setShouldUseRegisterAsAddress(int postition) {
        extendedInfoBits.set(postition);
    }
    
    Modifier getModifierForPosition(int position) {
        if (position == 0) {
            if (extendedInfoBits.test(2)) {
                return Modifier::ADDM;
            } else if (extendedInfoBits.test(3)) {
                return Modifier::SUBM;
            } else {
                return Modifier::NOM;
            }
        } else {
            if (extendedInfoBits.test(4)) {
                return Modifier::ADDM;
            } else if (extendedInfoBits.test(5)) {
                return Modifier::SUBM;
            } else {
                return Modifier::NOM;
            }
        }
    }
    
    void setModifierForPosition(int position, Modifier mod, DoubleWord value) {
        if (mod == Modifier::ADDM) {
            if (position == 0) {
                extendedInfoBits.set(2);
            } else {
                extendedInfoBits.set(4);
            }
        } else if (mod == Modifier::SUBM) {
            if (position == 0) {
                extendedInfoBits.set(3);
            } else {
                extendedInfoBits.set(5);
            }
        }
        modifier[position] = value;
    }
    
    void setVarForPosition(int position) {
        extendedInfoBits.set(position + 6);
    }
    
    bool isVarInPosition(int position) {
        return extendedInfoBits.test(position + 6);
    }
    
    DoubleWord* args = new DoubleWord[2];
    DoubleWord* modifier = new DoubleWord[2];
};

struct FirstPassData {
    OPCode* opcode;
    DoubleWord beginOffset;
    TokenData token;
    std::vector<TokenData> unresolvedTokens;
    
    FirstPassData() {}
    ~FirstPassData() {
        delete opcode;
    }
};

struct AssembledData {
    Byte* data;
    unsigned int length;
    unsigned int codeOffset;
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
    ID = 10
};

struct EFLAGSState {
    std::bitset<10> flagStates;
    
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
class ElectroCraftClock;
class ElectroCraftVGA;
class ElectroCraftTerminal;
class ElectroCraftStorage;
class ElectroCraftKeyboard;

namespace IOPort {
    class IOPortHandler;
}

class ElectroCraft_CPU : ElectroCraftTickable {
    RegisterState registers;
    EFLAGSState eState;
    ElectroCraftMemory* memory;
    ElectroCraftStack* stack;
    ElectroCraftClock* clock;
    ElectroCraftVGA* videoCard;
    ElectroCraftTerminal* terminal;
    IOPort::IOPortHandler *ioPortHandler;
    ElectroCraftStorage *storage;
    ElectroCraftKeyboard *keyboard;
    Section currentSection = Section::CODE;
public:
    ElectroCraft_CPU();
    ~ElectroCraft_CPU();
    AssembledData assemble(std::vector<std::string>);
    Address loadIntoMemory(Byte* data, unsigned int length, unsigned int codeOffset);
    void start(Address baseAddress);
    void stop();
    void reset(Address baseAddress);
    ElectroCraftVGA* getVideoCard();
    ElectroCraftTerminal* getTerminal();
    ElectroCraftKeyboard* getKeyboard();
    bool isRunning();
    virtual void operator()(long tickTime);
private:
    FirstPassData* firstPass(std::string line, DoubleWord offset);
    Registers getRegister(std::string token);
    OPCode* readOPCode(std::string token);
    DoubleWord getRegisterData(Registers reg);
    RegisterSizes registerToSize(Registers reg);
    RegisterType getRegisterType(Registers reg);
    void setRegisterData(DoubleWord data, Registers reg);
};

#pragma GCC visibility pop
#endif