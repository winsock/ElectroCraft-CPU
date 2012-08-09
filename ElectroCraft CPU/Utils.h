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
                uint8_t lowByte;
                uint8_t hiByte;
            };
            uint16_t word;
            HighLowByte byte;
        };
        union LowWord {
            struct  HighLowByte {
                uint8_t lowByte;
                uint8_t hiByte;
            };
            uint16_t word;
            HighLowByte byte;
        };
        LowWord lowWord;
        HighWord highWord;
    };
    uint32_t doubleWord = 0;
    Word word;
    
    // Functions
    DoubleWord(uint32_t initalValue) {
        doubleWord = initalValue;
    }
    
    DoubleWord() {}
    
    const inline DoubleWord operator+(const DoubleWord augend) const {
        DoubleWord result;
        result.doubleWord = doubleWord + augend.doubleWord;
        return result;
    }
};

typedef DoubleWord Address;
typedef unsigned char Byte;

struct MemoryMappedIOSection {
    Address beginAddress;
    MemoryMappedIOSection* subSections;
    Address endAddress;
    
    inline bool containsAddress(Address address) {
        return address.doubleWord >= beginAddress.doubleWord && address.doubleWord <= endAddress.doubleWord;
    }
};

namespace Utils {
    template <typename IntegerType>
    IntegerType bitsToInt(IntegerType& result, const unsigned char* bits, bool little_endian = true) {
        result = 0;
        if (little_endian)
            for (int n = sizeof(result); n >= 0; n--)
                result = (result << 8) +bits[ n ];
        else
            for (unsigned n = 0; n < sizeof(result); n++)
                result = (result << 8) +bits[ n ];
        return result;
    }
    
    class General {
    public:
        static std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems);
        static std::vector<std::string> split(const std::string &s, char delim);
        static DoubleWord readDoubleWord(Byte* data);
        static DoubleWord::Word readWord(Byte* data);
        static Byte* doubleWordToBytes(DoubleWord word);
        static Byte* wordToBytes(DoubleWord::Word word);
    };
}

#endif /* defined(__ElectroCraft_CPU__Utils__) */
