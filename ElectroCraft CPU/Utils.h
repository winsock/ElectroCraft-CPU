//
//  Utils.h
//  ElectroCraft CPU
//
//  Created by Andrew Querol on 8/3/12.
//  Copyright (c) 2012 Cerios Software. All rights reserved.
//

#ifndef __ElectroCraft_CPU__Utils__
#define __ElectroCraft_CPU__Utils__

#include <vector>
#include <sstream>
#include <string>

union DoubleWord {
    struct Word{
        union HighWord {
            struct  HighLowByte {
                uint8_t hiByte;
                uint8_t lowByte;
            };
            uint16_t word;
            HighLowByte byte;
        };
        union LowWord {
            struct  HighLowByte {
                uint8_t hiByte;
                uint8_t lowByte;
            };
            uint16_t word;
            HighLowByte byte;
        };
        LowWord lowWord;
        HighWord highWord;
    };
    uint32_t doubleWord = 0;
    Word word;
};

typedef DoubleWord Address;
typedef unsigned char Byte;

class Utils {
public:
    static std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
    static std::vector<std::string> split(const std::string &s, char delim);
};

#endif /* defined(__ElectroCraft_CPU__Utils__) */
