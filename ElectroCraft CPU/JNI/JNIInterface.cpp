//
//  JNIInterface.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/5/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include "info_cerios_electrocraft_core_computer_XECInterface.h"
#include "../ElectroCraft_CPU.h"
#include <vector>
#include <string>
#include <sstream>

ElectroCraft_CPU *cpu;

JNIEXPORT jobject JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_assemble (JNIEnv *env, jobject caller, jstring data) {
    if (cpu != nullptr) {
        jclass assembledDataClass = env->FindClass("info/cerios/electrocraft/core/computer/XECInterface$AssembledData");
        jmethodID constructor = env->GetMethodID(assembledDataClass, "<init>", "()V");
        jobject assembedData = env->NewObject(assembledDataClass, constructor, caller);
        
        const jchar* chars = env->GetStringChars(data, false);
        
        std::vector<std::wstring> instructions;
        std::wstringstream buffer;
        for (int i = 0; i < env->GetStringLength(data); i++) {
            if (chars[i] == L'\n') {
                instructions.push_back(buffer.str());
                buffer.str(std::wstring());
            } else {
                buffer<<static_cast<char16_t>(chars[i]);
            }
        }
        
        AssembledData cppAssembledData = cpu->assemble(instructions);
        jfieldID dataField = env->GetFieldID(assembledDataClass, "data", "[B");
        jfieldID lenthField = env->GetFieldID(assembledDataClass, "length", "I");
        jbyteArray byteData = env->NewByteArray(cppAssembledData.length);
        env->SetByteArrayRegion(byteData, 0, cppAssembledData.length, reinterpret_cast<jbyte*>(cppAssembledData.data));
        env->SetObjectField(assembedData, dataField, byteData);
        env->SetIntField(assembedData, lenthField, cppAssembledData.length);
        
        // Release the data
        env->ReleaseStringChars(data, chars);
        
        return assembedData;
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return nullptr;
        env->ThrowNew(cls, "CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return nullptr;
    }
    return nullptr;
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_createCPU (JNIEnv *, jobject) {
    cpu = new ElectroCraft_CPU;
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
        env->ThrowNew(cls, "CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return;
    }
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_manualTick (JNIEnv *env, jobject) {
    if (cpu != nullptr) {
        cpu->operator()(0l);
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return;
    }
}

JNIEXPORT jlong JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_loadIntoMemory (JNIEnv *env, jobject, jobjectArray data, jint length) {
    jbyteArray byteData = reinterpret_cast<jbyteArray>(data);
    return cpu->loadIntoMemory(reinterpret_cast<Byte*>(env->GetByteArrayElements(byteData, false)), length).doubleWord;
}

JNIEXPORT void JNICALL Java_info_cerios_electrocraft_core_computer_XECInterface_stop (JNIEnv *env, jobject) {
    if (cpu != nullptr) {
        cpu->stop();
    } else {
        jclass cls = env->FindClass("java/lang/IllegalStateException");
        if (cls == nullptr)
            return;
        env->ThrowNew(cls, "CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
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
        env->ThrowNew(cls, "CPU is uninitalized!\n Call info.cerios.electrocraft.core.computer.XECInterface.createCPU() First!");
        return;
    }
}