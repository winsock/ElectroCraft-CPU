//
//  Utils.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/3/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include "Utils.h"

std::vector<std::string> &Utils::split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> Utils::split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    return split(s, delim, elems);
}

DoubleWord Utils::readDoubleWord(Byte *data) {
    DoubleWord doubleWord ;
    memcpy(&doubleWord.doubleWord, data, 4);
    return doubleWord;
}

DoubleWord::Word Utils::readWord(Byte *data) {
    DoubleWord::Word word;
    memcpy(&word.lowWord.word, data, 2);
    return word;
}

Byte* Utils::doubleWordToBytes(DoubleWord word) {
    Byte* data = new Byte[4];
    memcpy(data, &word.doubleWord, 4);
    return data;
}

Byte* Utils::wordToBytes(DoubleWord::Word word) {
    Byte* data = new Byte[2];
    memcpy(data, &word.lowWord.word, 2);
    return data;
}