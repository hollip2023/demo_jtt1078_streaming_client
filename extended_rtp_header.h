#include <stdint.h>

typedef struct {
    uint32_t dwValue;     // Starting at byte offset 0, DWORD (32 bits)
    uint32_t V : 2;       // 2 bits, assumed to start at byte offset 4
    uint32_t P : 1;       // 1 bit, immediately following the V field

    uint32_t X : 1;       // 1 bit
    uint32_t CC : 4;      // 4 bits
    uint32_t M : 1;       // 1 bit
    uint32_t PT : 7;      // 7 bits

    uint16_t sequenceNum; // WORD (16 bits), starting at byte offset 6

    uint8_t SIM[6];       // BCD format, starting at byte offset 8, total 6 bytes
    uint8_t byteValue;    // BYTE, starting at byte offset 14

    uint8_t dataType : 4;    // 4 bits
    uint8_t segmentType : 4;    // another 4 bits, these names are placeholders since the actual purpose is not clear

    uint8_t timestamp[8];     // BYTE[8], starting at byte offset 16

    uint16_t lastIFrameInterval; // WORD (16 bits), starting at byte offset 24
    uint16_t lastFrameInterval;  // WORD (16 bits), starting at byte offset 26

    uint16_t wordValue;         // WORD, starting at byte offset 28
    uint8_t byteSeq[950];       // BYTE[n], n = 950 bytes, starting at byte offset 30
} JTT1078Header;

