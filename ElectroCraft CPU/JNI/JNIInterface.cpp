//
//  JNIInterface.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/5/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include "info_cerios_electrocraft_core_computer_XECInterface.h"
#include "info_cerios_electrocraft_core_computer_XECVGACard.h"
#include "info_cerios_electrocraft_core_computer_XECTerminal.h"
#include "info_cerios_electrocraft_core_computer_XECKeyboard.h"
#include "../ElectroCraft_CPU.h"
#include "../ElectroCraftVGA.h"
#include "../ElectroCraftTerminal.h"
#include "../ElectroCraftKeyboard.h"
#include <vector>
#include <chrono>
#include <map>
#include <string>
#include <sstream>
#include <iostream>

ElectroCraft_CPU *cpu;
// VGA
std::map<unsigned long, ElectroCraftVGA*> idVideoCardMap;
// Terminal
std::map<unsigned long, ElectroCraftTerminal*> idTerminalMap;
// Keyboard
std::map<unsigned long, ElectroCraftKeyboard*> idKeyboardMap;

// ################################################################# //
//                                                                   //
//                          ElectroCraft CPU                         //
//                                                                   //
// ################################################################# //

JNIEXPORT jobject JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_assemble (JNIEnv *env, jobject, jstring data) {
    if (cpu != nullptr) {
        jclass assembledDataClass = env->FindClass("info/cerios/electrocraft/core/computer/XECInterface$AssembledData");
        jmethodID constructor = env->GetMethodID(assembledDataClass, "<init>", "()V");
        jobject assembedData = env->NewObject(assembledDataClass, constructor);
        
        const jchar* chars = env->GetStringChars(data, false);
        
        std::vector<std::string> instructions;
        std::stringstream buffer;
        for (int i = 0; i < env->GetStringLength(data); i++) {
            if (chars[i] == '\n') {
                instructions.push_back(buffer.str());
                buffer.str(std::string());
            } else {
                buffer<<static_cast<char>(chars[i]);
            }
        }
        if (buffer.str().size() > 0) {
            instructions.push_back(buffer.str());
            buffer.str(std::string());
        }
        
        AssembledData cppAssembledData = cpu->assemble(instructions);
        jfieldID dataField = env->GetFieldID(assembledDataClass, "data", "[B");
        jfieldID lenthField = env->GetFieldID(assembledDataClass, "length", "I");
        jfieldID codeOffsetField = env->GetFieldID(assembledDataClass, "codeStart", "I");
        jbyteArray byteData = env->NewByteArray(cppAssembledData.length);
        env->SetByteArrayRegion(byteData, 0, cppAssembledData.length, reinterpret_cast<jbyte*>(cppAssembledData.data));
        env->SetObjectField(assembedData, dataField, byteData);
        env->SetIntField(assembedData, lenthField, env->GetArrayLength(byteData));
        env->SetIntField(assembedData, codeOffsetField, cppAssembledData.codeOffset);
        
        // Release the data
        env->ReleaseStringChars(data, chars);
        
        return assembedData;
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return nullptr;
        env->ThrowNew(cls, "ElectroCraft JNI: CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return nullptr;
    }
    return nullptr;
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_createCPU (JNIEnv *, jobject) {
    cpu = new ElectroCraft_CPU;
}

JNIEXPORT jobject JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_getVideoCard (JNIEnv *env, jobject) {
    if (cpu != nullptr) {
        unsigned long id = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now()).time_since_epoch()).count();
        jclass videoCardClass = env->FindClass("info/cerios/electrocraft/core/computer/XECVGACard");
        jmethodID constructor = env->GetMethodID(videoCardClass, "<init>", "(J)V");
        jobject videoCard = env->NewObject(videoCardClass, constructor, id);
        idVideoCardMap[id] = cpu->getVideoCard();
        return videoCard;
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return nullptr;
        env->ThrowNew(cls, "ElectroCraft JNI: CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return nullptr;
    }
}

JNIEXPORT jobject JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_getTerminal (JNIEnv *env, jobject) {
    if (cpu != nullptr) {
        unsigned long id = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now()).time_since_epoch()).count();
        jclass terminalClass = env->FindClass("info/cerios/electrocraft/core/computer/XECTerminal");
        jmethodID constructor = env->GetMethodID(terminalClass, "<init>", "(J)V");
        jobject terminal = env->NewObject(terminalClass, constructor, id);
        idTerminalMap[id] = cpu->getTerminal();
        return terminal;
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return nullptr;
        env->ThrowNew(cls, "ElectroCraft JNI: CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return nullptr;
    }
}

JNIEXPORT jobject JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_getKeyboard (JNIEnv *env, jobject) {
    if (cpu != nullptr) {
        unsigned long id = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now()).time_since_epoch()).count();
        jclass keyboardClass = env->FindClass("info/cerios/electrocraft/core/computer/XECKeyboard");
        jmethodID constructor = env->GetMethodID(keyboardClass, "<init>", "(J)V");
        jobject keyboard = env->NewObject(keyboardClass, constructor, id);
        idKeyboardMap[id] = cpu->getKeyboard();
        return keyboard;
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return nullptr;
        env->ThrowNew(cls, "ElectroCraft JNI: CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return nullptr;
    }
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_start (JNIEnv *env, jobject, jlong address) {
    if (cpu != nullptr) {
        Address cppAddress;
        cppAddress.doubleWord = static_cast<unsigned int>(address);
        cpu->start(cppAddress);
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "ElectroCraft JNI: CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return;
    }
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_manualTick (JNIEnv *env, jobject) {
    if (cpu != nullptr) {
        (*cpu)(0l);
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "ElectroCraft JNI: CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return;
    }
}

JNIEXPORT jlong JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_loadIntoMemory (JNIEnv *env, jobject, jbyteArray data, jint length, jint codeOffset){
    jbyteArray byteData = reinterpret_cast<jbyteArray>(data);
    return cpu->loadIntoMemory(reinterpret_cast<Byte*>(env->GetByteArrayElements(byteData, false)), length, codeOffset).doubleWord;
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_stop (JNIEnv *env, jobject) {
    if (cpu != nullptr) {
        cpu->stop();
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "ElectroCraft JNI: CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return;
    }
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_reset (JNIEnv *env, jobject, jlong address) {
    if (cpu != nullptr) {
        Address cppAddress;
        cppAddress.doubleWord = static_cast<unsigned int>(address);
        cpu->reset(cppAddress);
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "ElectroCraft JNI: CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return;
    }
}

// ################################################################# //
//                                                                   //
//                          ElectroCraft GPU                         //
//                                                                   //
// ################################################################# //

ElectroCraftVGA* getVideoCardFromJVideoCard(JNIEnv *env, jobject jVideoCard) {
    jclass videoCardClass = env->FindClass("info/cerios/electrocraft/core/computer/XECVGACard");
    jfieldID idField = env->GetFieldID(videoCardClass, "internalID", "J");
    unsigned long id = env->GetLongField(jVideoCard, idField);
    
    if (idVideoCardMap.find(id) != idVideoCardMap.end()) {
        ElectroCraftVGA* videoCard = idVideoCardMap[id];
        return videoCard;
    } else {
        return nullptr;
    }
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECVGACard_clearScreen (JNIEnv *env, jobject caller) {
    ElectroCraftVGA *videoCard = getVideoCardFromJVideoCard(env, caller);
    if (videoCard != nullptr) {
        videoCard->clear();
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown GPU ID!");
        return;
    }
}

JNIEXPORT jbyteArray JNICALL Java_info_cerios_electrocraft_core_computer_XECVGACard_getScreenData (JNIEnv *env, jobject caller) {
    ElectroCraftVGA *videoCard = getVideoCardFromJVideoCard(env, caller);
    if (videoCard != nullptr) {
        jbyteArray byteData = env->NewByteArray(videoCard->getDisplayBufferSize().doubleWord);
        env->SetByteArrayRegion(byteData, 0, videoCard->getDisplayBufferSize().doubleWord, reinterpret_cast<jbyte*>(videoCard->getScreenData()));
        return byteData;
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return nullptr;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown GPU ID!");
        return nullptr;
    }
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECVGACard_setScreenSize (JNIEnv *env, jobject caller, jint width, jint height) {
    ElectroCraftVGA *videoCard = getVideoCardFromJVideoCard(env, caller);
    if (videoCard != nullptr) {
        if (width < 0 || height < 0) {
            jclass cls = env->FindClass("java/lang/IndexOutOfBoundsException");
            if (cls == nullptr)
                return;
            env->ThrowNew(cls, "ElectroCraft JNI: Error! Width and Height must be greater than zero!");
            return;
        } else {
            videoCard->setScreenSize(width, height);
        }
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown GPU ID!");
        return;
    }
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECVGACard_manualTick (JNIEnv *env, jobject caller) {
    ElectroCraftVGA *videoCard = getVideoCardFromJVideoCard(env, caller);
    if (videoCard != nullptr) {
        (*videoCard)(0l);
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown GPU ID!");
        return;
    }
}

JNIEXPORT jint JNICALL Java_info_cerios_electrocraft_core_computer_XECVGACard_getScreenWidth (JNIEnv *env, jobject caller) {
    ElectroCraftVGA *videoCard = getVideoCardFromJVideoCard(env, caller);
    if (videoCard != nullptr) {
        return videoCard->getWidth();
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return 0;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown GPU ID!");
        return 0;
    }
}

JNIEXPORT jint JNICALL Java_info_cerios_electrocraft_core_computer_XECVGACard_getScreenHeight (JNIEnv *env, jobject caller) {
    ElectroCraftVGA *videoCard = getVideoCardFromJVideoCard(env, caller);
    if (videoCard != nullptr) {
        return videoCard->getHeight();
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return 0;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown GPU ID!");
        return 0;
    }
}

// ################################################################# //
//                                                                   //
//                       ElectroCraft Terminal                       //
//                                                                   //
// ################################################################# //

ElectroCraftTerminal* getTerminalFromJTerminal(JNIEnv *env, jobject jTerminal) {
    jclass terminalClass = env->FindClass("info/cerios/electrocraft/core/computer/XECTerminal");
    jfieldID idField = env->GetFieldID(terminalClass, "internalID", "J");
    unsigned long id = env->GetLongField(jTerminal, idField);
    
    if (idTerminalMap.find(id) != idTerminalMap.end()) {
        ElectroCraftTerminal* terminal = idTerminalMap[id];
        return terminal;
    } else {
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL Java_info_cerios_electrocraft_core_computer_XECTerminal_getLine (JNIEnv *env, jobject caller, jint line) {
    ElectroCraftTerminal *terminal = getTerminalFromJTerminal(env, caller);
    if (terminal != nullptr) {
        return env->NewStringUTF(terminal->getLine(line).c_str());
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return 0;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown Terminal ID!");
        return 0;
    }
}

JNIEXPORT jint JNICALL Java_info_cerios_electrocraft_core_computer_XECTerminal_getColoumns (JNIEnv *env, jobject caller) {
    ElectroCraftTerminal *terminal = getTerminalFromJTerminal(env, caller);
    if (terminal != nullptr) {
        return terminal->getCols();
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return 0;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown Terminal ID!");
        return 0;
    }
}

JNIEXPORT jint JNICALL Java_info_cerios_electrocraft_core_computer_XECTerminal_getRows (JNIEnv *env, jobject caller) {
    ElectroCraftTerminal *terminal = getTerminalFromJTerminal(env, caller);
    if (terminal != nullptr) {
        return terminal->getRows();
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return 0;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown Terminal ID!");
        return 0;
    }
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECTerminal_clear (JNIEnv *env, jobject caller) {
    ElectroCraftTerminal *terminal = getTerminalFromJTerminal(env, caller);
    if (terminal != nullptr) {
        terminal->clear();
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown Terminal ID!");
        return;
    }
}

// ################################################################# //
//                                                                   //
//                       ElectroCraft Keyboard                       //
//                                                                   //
// ################################################################# //

ElectroCraftKeyboard* getKeyboardFromJKeyboard(JNIEnv *env, jobject jTerminal) {
    jclass terminalClass = env->FindClass("info/cerios/electrocraft/core/computer/XECKeyboard");
    jfieldID idField = env->GetFieldID(terminalClass, "internalID", "J");
    unsigned long id = env->GetLongField(jTerminal, idField);
    
    if (idKeyboardMap.find(id) != idKeyboardMap.end()) {
        ElectroCraftKeyboard* keyboard = idKeyboardMap[id];
        return keyboard;
    } else {
        return nullptr;
    }
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECKeyboard_onKeyPress(JNIEnv *env, jobject caller, jbyte key) {
    ElectroCraftKeyboard *keyboard = getKeyboardFromJKeyboard(env, caller);
    if (keyboard != nullptr) {
        keyboard->onKeyPress(key);
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "ElectroCraft JNI: Unknown Keyboard ID!");
        return;
    }
}