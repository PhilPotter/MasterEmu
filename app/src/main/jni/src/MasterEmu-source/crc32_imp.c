/* MasterEmu CRC32 implementation source code file
   copyright Phil Potter, 2022 */

#include <stdio.h>
#include <stdlib.h>
#include <android/log.h>
#include "crc32_imp.h"

/* This function calculates a CRC-32 checksum for a given byte array -
   it works the following way (conceptually):
   
   1. Every byte is reflected.
   2. Four zero bytes are added to the end.
   3. FFFFFFFF is XORed with the first four bytes.
   4. CRC arithmetic is used, with the polynomial of 0x04C11DB7.
   5. The remainder has each byte reflected again.
   6. The remainder is then XORed with FFFFFFFF.
   7. Endianness swap is done (last byte becomes first byte etc.).
   8. Checksum is formed from above, voila!
   
   Not very efficient, but was how I formed my understanding of polynomial
   arithmetic, and CRC-32 in particular. */
emuint calculateCRC32(emubyte *p, emulong length)
{
    /* check length is at least 1 byte */
    if (length < 1) {
        __android_log_print(ANDROID_LOG_ERROR, "crc32_imp.c", "Byte stream must be at least one byte in size...\n");
        exit(1);
    }

    /* create byte array to hold temporary values */
    emubyte *tempArray = calloc(1, (size_t)(length + 4));
    if (tempArray == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "crc32_imp.c", "Couldn't allocate memory for temporary CRC array...\n");
        exit(1);
    }

    /* read bytes in one at a time, reflecting them as we go */
    emulong i;
    for (i = 0; i < length; ++i) {
        tempArray[i] = (p[i] & 0x01) << 7;
        tempArray[i] |= (p[i] & 0x02) << 5;
        tempArray[i] |= (p[i] & 0x04) << 3;
        tempArray[i] |= (p[i] & 0x08) << 1;
        tempArray[i] |= (p[i] & 0x10) >> 1;
        tempArray[i] |= (p[i] & 0x20) >> 3;
        tempArray[i] |= (p[i] & 0x40) >> 5;
        tempArray[i] |= (p[i] & 0x80) >> 7;
    }

    /* now XOR first four bytes with 0xFF */
    tempArray[0] ^= 0xFF;
    tempArray[1] ^= 0xFF;
    tempArray[2] ^= 0xFF;
    tempArray[3] ^= 0xFF;

    /* now prepare temporary variable to store result in */
    emulong tempCalc = (tempArray[0] << 24) | (tempArray[1] << 16) |
                       (tempArray[2] << 8) | tempArray[3];
    emulong selectedSection;

    /* iterate through byte array, taking appropriate action each time */
    emubyte mask = 0x80;
    emubyte tempBit;
    emulong polynomial = 0x100000000L | 0x04C11DB7;
    for (selectedSection = 4; selectedSection < length + 4;) {
        /* shift tempCalc left 1 bit and add next bit from dividend */
        tempCalc = (tempCalc << 1) & 0x1FFFFFFFFL;
        tempBit = tempArray[selectedSection] & mask;
        switch (mask) {
            case 0x80: tempBit = tempBit >> 7; break;
            case 0x40: tempBit = tempBit >> 6; break;
            case 0x20: tempBit = tempBit >> 5; break;
            case 0x10: tempBit = tempBit >> 4; break;
            case 0x08: tempBit = tempBit >> 3; break;
            case 0x04: tempBit = tempBit >> 2; break;
            case 0x02: tempBit = tempBit >> 1; break;
        }
        tempCalc |= tempBit;

        /* check most-significant bit of tempCalc */
        if ((tempCalc & 0x100000000L) != 0) {
            /* XOR in polynomial */
            tempCalc ^= polynomial;
        }

        /* shift mask right and check */
        mask = mask >> 1;
        if (mask == 0) {
            mask = 0x80;
            ++selectedSection;
        }
    }

    /* calculation is complete, now we need only the least significant 4 bytes of tempCalc -
       we must first reflect each of these four bytes */
    emuint remainder = tempCalc & 0xFFFFFFFF;
    emubyte tempReflect[4];
    signed_emubyte j;
    for (j = 3; j >= 0; --j) {
        tempReflect[j] = (remainder & 0x01) << 7;
        tempReflect[j] |= (remainder & 0x02) << 5;
        tempReflect[j] |= (remainder & 0x04) << 3;
        tempReflect[j] |= (remainder & 0x08) << 1;
        tempReflect[j] |= (remainder & 0x10) >> 1;
        tempReflect[j] |= (remainder & 0x20) >> 3;
        tempReflect[j] |= (remainder & 0x40) >> 5;
        tempReflect[j] |= (remainder & 0x80) >> 7;
        remainder = remainder >> 8;
    }

    /* now XOR with 0xFF again */
    tempReflect[0] ^= 0xFF;
    tempReflect[1] ^= 0xFF;
    tempReflect[2] ^= 0xFF;
    tempReflect[3] ^= 0xFF;

    /* reverse byte endianness and we have the final checksum */
    remainder = (tempReflect[3] << 24) | (tempReflect[2] << 16) | (tempReflect[1] << 8) | tempReflect[0];

    /* free memory we allocated earlier */
    free((void *)tempArray);

    return remainder;
}
