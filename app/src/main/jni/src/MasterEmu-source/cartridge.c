/* MasterEmu cartridge source code file
   copyright Phil Potter, 2022 */

#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include "cartridge.h"

/* this struct models the cartridge's internal state */
struct Cartridge {
    signed_emulong romSize; /* this tells us the size of the ROM in bytes */
    emubyte *romData; /* this points to the raw ROM data */
    emubyte **ramBanks; /* this points to both of the RAM banks */
    emubyte *ramBank1; /* this points to the first RAM bank */
    emubyte *ramBank2; /* this points to the second RAM bank */
    emubyte fffc; /* this is the RAM mapping/miscellaneous register */
    emubyte fffd; /* this represents the ROM bank select for slot 0 */
    emubyte fffe; /* this represents the ROM bank select for slot 1 */
    emubyte ffff; /* this represents the ROM bank select for slot 2 */
    emubyte divisor; /* this is for calculating the bank that is selected */
    emubyte **romBanks; /* this points to each bank */
    emubool isCodemasters; /* this allows switching of the mapper to Codemasters mode */
};

/* this function creates a new Cartridge object and returns a pointer to it -
   initialise it with a FILE pointer to the ROM file */
Cartridge createCartridge(emubyte *romData, signed_emulong romSize, emubool isCodemasters, emubyte *cartState, emubool sRamOnly, emubyte *wholePointer)
{
    /* define needed local variables */
    emuint i;

    /* first, we allocate memory for a Cartridge struct */
    Cartridge c = (Cartridge)wholePointer;
    wholePointer += sizeof(struct Cartridge);
    
    /* set all sub-pointers to NULL in advance */
    c->ramBanks = NULL;
    c->ramBank1 = NULL;
    c->ramBank2 = NULL;
    c->romBanks = NULL;

    /* assign ROM size and data */
    c->romSize = romSize;
    c->romData = romData;

    /* initialise miscellaneous register and divisor, and setup ROM banks */
    c->fffc = 0;
    c->divisor = (emubyte)(c->romSize / 16384);
    c->romBanks = (emubyte **)wholePointer;
    wholePointer += sizeof(emubyte *) * 128;
    for (i = 0; i < c->divisor; ++i)
        c->romBanks[i] = c->romData + (16384 * i);

    /* allocate memory for RAM banks and initialise them */
    c->ramBanks = (emubyte **)wholePointer;
    wholePointer += sizeof(emubyte *) * 2;
    c->ramBank1 = wholePointer;
    memset((void *)c->ramBank1, 0, 16384);
    wholePointer += 16384;
    c->ramBank2 = wholePointer;
    memset((void *)c->ramBank2, 0, 16384);
    wholePointer += 16384;
    c->ramBanks[0] = c->ramBank1;
    c->ramBanks[1] = c->ramBank2;

    /* set default bank numbers for each slot */
    c->fffd = 0;
    c->fffe = 1;
    if (isCodemasters)
        c->ffff = 0;
    else
        c->ffff = 2;

    /* set mapper to correct mode*/
    c->isCodemasters = isCodemasters;

    /* set state if provided */
    if (cartState != NULL) {
        if (sRamOnly) {
            emuint marker = 13;
            emubyte *tempPointer = cartState + marker + 4;
            memcpy((void *)c->ramBank1, (void *)tempPointer, 16384);
            tempPointer += 16384;
            memcpy((void *)c->ramBank2, (void *)tempPointer, 16384);
        } else {
            emuint marker = 13;
            c->fffc = cartState[marker++];
            c->fffd = cartState[marker++];
            c->fffe = cartState[marker++];
            c->ffff = cartState[marker++];
            emubyte *tempPointer = cartState + marker;
            memcpy((void *)c->ramBank1, (void *)tempPointer, 16384);
            tempPointer += 16384;
            memcpy((void *)c->ramBank2, (void *)tempPointer, 16384);
        }
    }

    /* return Cartridge object */
    return c;
}

/* this function destroys the Cartridge object */
void destroyCartridge(Cartridge c)
{
    ;
}

/* this function returns whether or not the Codemasters mapper is enabled */
emubool cart_isCodemasters(Cartridge c)
{
    return c->isCodemasters;
}

/* this function writes the specified byte to the miscellanerous register */
void cart_writeFFFC(Cartridge c, emubyte data)
{
    c->fffc = data;
}

/* this function writes the ROM banking value to the slot 0 register */
void cart_writeFFFD(Cartridge c, emubyte data)
{
    /* define local variables */
    emubyte bank = data % c->divisor;
    c->fffd = bank;
}

/* this function writes the ROM banking value to the slot 1 register */
void cart_writeFFFE(Cartridge c, emubyte data)
{
    /* define local variables */
    emubyte bank = data % c->divisor;
    c->fffe = bank;
}

/* this function writes the ROM banking value to the slot 2 register */
void cart_writeFFFF(Cartridge c, emubyte data)
{
    /* define local variables */
    emubyte bank = data % c->divisor;
    c->ffff = bank;
}

/* this function returns the correct byte from the Cartridge object, taking account
   of mapping settings in the process */
emubyte cart_readCartridge(Cartridge c, emuint address)
{
    /* define local variables */
    emuint addressVal = address & 0xFFFF;
    emubyte returnVal = 0;

    /* this if statement construct selects the correct region of ROM or RAM  to return a byte from */
    if (addressVal <= 0x3FFF) { /* this checks for a slot 0 read */
        /* check if read is from first 1KiB of slot 0, in which
           case always return the value from bank 0 */
        if ((addressVal <= 0x3FF) && !c->isCodemasters)
            returnVal = c->romBanks[0][addressVal];
        else
            returnVal = c->romBanks[c->fffd][addressVal];
    } else if (addressVal <= 0x7FFF) { /* this checks for a slot 1 read */
        returnVal = c->romBanks[c->fffe][addressVal - 16384];
    } else if (addressVal <= 0xBFFF) { /* this checks for a slot 2 read from RAM/ROM */
        /* this check enables RAM to take precedence
           over any ROM bank mapped to slot 2 */
        if ((c->fffc & 0x08) == 8)
            /* select appropriate value depending on
               which RAM chip is banked*/
            returnVal = c->ramBanks[(c->fffc >> 2) & 0x01][addressVal - 32768];
        else
            /* read from ROM bank as normal */
            returnVal = c->romBanks[c->ffff][addressVal - 32768];
    } else if (addressVal <= 0xFFFF) { /* this reads from RAM if system RAM is disabled */
        /* bank selection has no effect on this segment
           so just map the first bank to it */
        if ((c->fffc & 0x10) == 16)
            returnVal = c->ramBanks[0][addressVal - 49152];
    }

    /* return byte */
    return returnVal;
}

/* this function writes the specified byte to the correct place based on the specified
   address, taking care to check bank mapping values - bear in mind we don't need
   to check slots 0 and 1 as these slots only allow ROM banking */
void cart_writeCartridge(Cartridge c, emuint address, emubyte data)
{
    /* define local variables */
    emuint addressVal = 0xFFFF & address;

    /* check slot 2 first */
    if ((addressVal >= 0x8000) && (addressVal <= 0xBFFF)) {
        /* check RAM is actually enabled on this slot */
        if ((c->fffc & 0x08) == 8)
            c->ramBanks[(c->fffc >> 2) & 0x01][addressVal - 32768] = data;
    } else if (addressVal <= 0xFFFF) { /* this will only work if the main system RAM 
                                          is disabled via Z80 IO port 3E */
        /* bank selection has no effect on this segment so just map
           the first bank to it */
        if ((c->fffc & 0x10) == 16)
            c->ramBanks[0][addressVal - 49152] = data;
    }
}

/* this function returns an emubyte pointer to the cartridge's state */
emubyte *cart_saveState(Cartridge c)
{
    /* define variables */
    emuint cartSize = 4 + 16384 + 16384 + 13; /* add extra 13 bytes for header and size */
    emuint marker = 0;

    /* allocate the memory */
    emubyte *cartState = malloc(sizeof(emubyte) * cartSize);
    if (cartState == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "cartridge.c", "Couldn't allocate memory for cartridge state buffer...\n");
        return NULL;
    }

    /* fill first 13 bytes with header and state size*/
    cartState[marker++] = cartSize & 0xFF;
    cartState[marker++] = (cartSize >> 8) & 0xFF;
    cartState[marker++] = (cartSize >> 16) & 0xFF;
    cartState[marker++] = (cartSize >> 24) & 0xFF;
    cartState[marker++] = 'C';
    cartState[marker++] = 'A';
    cartState[marker++] = 'R';
    cartState[marker++] = 'T';
    cartState[marker++] = 'R';
    cartState[marker++] = 'I';
    cartState[marker++] = 'D';
    cartState[marker++] = 'G';
    cartState[marker++] = 'E';

    /* copy register values to state buffer */
    cartState[marker++] = c->fffc;
    cartState[marker++] = c->fffd;
    cartState[marker++] = c->fffe;
    cartState[marker++] = c->ffff;

    /* copy both RAM banks */
    emubyte *tempPointer = cartState + marker;
    memcpy((void *)tempPointer, (void *)c->ramBank1, 16384);
    tempPointer += 16384;
    memcpy((void *)tempPointer, (void *)c->ramBank2, 16384);

    /* return cartridge state */
    return cartState;
}

/* this function returns the number of bytes required to create a Cartridge object */
emuint cart_getMemoryUsage(void)
{
    return (sizeof(struct Cartridge) * sizeof(emubyte)) + /* ram banks */ (sizeof(emubyte) * 16384 * 2) +
    /* ram banks array */ (sizeof(emubyte *) * 2) + /* rom banks array */ (sizeof(emubyte *) * 128);
}

/* this function returns whether or not the cartridge is mapping RAM to the address space normally
   occupied by system RAM */
emubool cart_isCartOverridingSystemRam(Cartridge c)
{
   return (c->fffc & 0x10) == 0x10;
}
