//
//  Utils.cpp
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/3/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#include <cstring>
#include "Utils.h"

std::vector<std::string> &Utils::General::split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while(std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

std::vector<std::string> Utils::General::split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    return split(s, delim, elems);
}

DoubleWord Utils::General::readDoubleWord(Byte *data) {
    DoubleWord doubleWord ;
    std::memcpy(&doubleWord.doubleWord, data, 4);
    return doubleWord;
}

DoubleWord::Word Utils::General::readWord(Byte *data) {
    DoubleWord::Word word;
    std::memcpy(&word.lowWord.word, data, 2);
    return word;
}

Byte* Utils::General::doubleWordToBytes(DoubleWord word) {
    Byte* data = new Byte[4];
    std::memcpy(data, &word.doubleWord, 4);
    return data;
}

Byte* Utils::General::wordToBytes(DoubleWord::Word word) {
    Byte* data = new Byte[2];
    std::memcpy(data, &word.lowWord.word, 2);
    return data;
}

Byte* Utils::General::numberToBytes(DoubleWord data, unsigned int size) {
    Byte *returnData = new Byte[size];
    if (size == 1) {
        returnData[0] = data.word.lowWord.byte.lowByte;
    } else if (size == 2) {
        returnData = wordToBytes(data.word);
    } else {
        returnData = doubleWordToBytes(data);
    }
    return returnData;
}