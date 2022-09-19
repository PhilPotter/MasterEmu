/* MasterEmu console source code file
   copyright Phil Potter, 2022 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <android/log.h>
#include "crc32_imp.h"
#include "console.h"
#include "z80.h"
#include "cartridge.h"
#include "controllers.h"
#include "vdp.h"
#include "sn76489.h"
#include "util.h"

/* definitions for memory sizes */
#define CONSOLE_MEMORY_SPACE 65536
#define CONSOLE_IO_SPACE 256

/* below is the Game Genie cheat code object reference */
extern GgCheatArray ggCheatArray;

/* this struct models the Master System console's internal state */
struct Console {
    Z80 cpu; /* this represents the Z80 CPU of the Master System */
    Cartridge cart; /* this represents the cartridge of the Master System */
    Controllers controllers; /* this represents the controllers of the Master System */
    VDP vdp; /* this represents the VDP of the Master System */
    SN76489 soundchip; /* this represents the SN76489 sound chip of the Master System */

    /* memory related attributes */
    emubyte *memoryAddressSpace;
    emubyte *ioAddressSpace;
    emuint systemAddressBus;
    emubyte systemDataBus;

    /* interrupt related attributes */
    emubyte interruptSignal;

    /* CRC-32 checksum of currently loaded ROM */
    emuint checksum;
    
    /* stores whether this console object is modelled on a Game Gear or not */
    emubool isGameGear;
};

/* this function initialises a new Master System and all its components,
   returning a pointer to it */
Console createConsole(emubyte *romData, signed_emulong romSize, emuint romChecksum, emubool isCodemasters, emubool isGameGear, emubool isPal, SDL_Rect *sourceRect, emubyte *saveState, emuint params, emubyte *wholePointer, emuint audioId)
{
    /* allocate memory for the Console struct */
    Console ms = (Console)wholePointer;
    wholePointer += sizeof(struct Console);

    /* setup memory spaces */
    ms->memoryAddressSpace = wholePointer;
    memset((void *)ms->memoryAddressSpace, 0, 65536);
    wholePointer += 65536;
    ms->ioAddressSpace = wholePointer;
    memset((void *)ms->ioAddressSpace, 0, 256);
    wholePointer += 256;

    /* split out parameters */
    emubool soundDisabled = false;
    if ((params & 0x01) == 0x01)
        soundDisabled = true;
    emubool sRamOnly = false;
    if ((params & 0x02) == 0x02)
        sRamOnly = true;
    emubool isJapanese = false;
    if ((params & 0x10) == 0x10)
        isJapanese = true;

    /* check save state pointer, and section out to the different component pointers if not NULL */
    emubyte *cartState = NULL;
    emubyte *controllersState = NULL;
    emubyte *z80State = NULL;
    emubyte *soundchipState = NULL;
    emubyte *vdpState = NULL;
    emubyte *consoleState = NULL;
    emubyte *tempPointer = saveState;
    emuint tempSize = 0;
    if (saveState != NULL) {
        tempPointer += 8;
        cartState = tempPointer;
        tempSize = (tempPointer[3] << 24) | (tempPointer[2] << 16) | (tempPointer[1] << 8) | tempPointer[0];
        tempPointer += tempSize;
        controllersState = tempPointer;
        tempSize = (tempPointer[3] << 24) | (tempPointer[2] << 16) | (tempPointer[1] << 8) | tempPointer[0];
        tempPointer += tempSize;
        z80State = tempPointer;
        tempSize = (tempPointer[3] << 24) | (tempPointer[2] << 16) | (tempPointer[1] << 8) | tempPointer[0];
        tempPointer += tempSize;
        soundchipState = tempPointer;
        tempSize = (tempPointer[3] << 24) | (tempPointer[2] << 16) | (tempPointer[1] << 8) | tempPointer[0];
        tempPointer += tempSize;
        vdpState = tempPointer;
        tempSize = (tempPointer[3] << 24) | (tempPointer[2] << 16) | (tempPointer[1] << 8) | tempPointer[0];
        tempPointer += tempSize;
        consoleState = tempPointer;
    }

    /* mask out pointers except cartState if we only want sRAM */
    if (sRamOnly) {
        controllersState = NULL;
        z80State = NULL;
        soundchipState = NULL;
        vdpState = NULL;
        consoleState = NULL;
    }

    /* set all sub-pointers to NULL now */
    ms->cpu = NULL;
    ms->cart = NULL;
    ms->controllers = NULL;
    ms->vdp = NULL;
    ms->soundchip = NULL;

    /* setup Z80 */
    if ((ms->cpu = createZ80(ms, z80State, wholePointer)) == NULL) {
        destroyConsole(ms);
        return NULL;
    }
    wholePointer += Z80_getMemoryUsage();

    /* setup Cartridge */
    if ((ms->cart = createCartridge(romData, romSize, isCodemasters, cartState, sRamOnly, wholePointer)) == NULL) {
        destroyConsole(ms);
        return NULL;
    }
    wholePointer += cart_getMemoryUsage();

    /* setup Controllers */
    if ((ms->controllers = createControllers(ms, isGameGear, controllersState, isJapanese, wholePointer)) == NULL) {
        destroyConsole(ms);
        return NULL;
    }
    wholePointer += controllers_getMemoryUsage();

    /* setup VDP */
    if ((ms->vdp = createVDP(ms, isGameGear, isPal, sourceRect, vdpState, wholePointer)) == NULL) {
        destroyConsole(ms);
        return NULL;
    }
    wholePointer += vdp_getMemoryUsage();

    /* setup SN76489 */
    if ((ms->soundchip = createSN76489(ms, soundchipState, soundDisabled, isGameGear, isPal, wholePointer, audioId)) == NULL) {
        destroyConsole(ms);
        return NULL;
    }

    ms->systemAddressBus = 0;
    ms->systemDataBus = 0;

    /* set interrupt signal to nothing */
    ms->interruptSignal = 0;
    
    /* set console type */
    ms->isGameGear = isGameGear;
    
    /* if console is a Game Gear, setup specific registers */
    if (ms->isGameGear) {
        ms->ioAddressSpace[1] = 0x7F;
        ms->ioAddressSpace[2] = 0xFF;
        ms->ioAddressSpace[3] = 0x00;
        ms->ioAddressSpace[4] = 0xFF;
        ms->ioAddressSpace[5] = 0x00;
        ms->ioAddressSpace[6] = 0xFF;
    }

    /* set port 0x3E to default value (cartridge, RAM, and I/O enabled,
       everything else disabled) */
    ms->ioAddressSpace[0x3E] = 0xAB;

    /* set ROM file checksum */
    ms->checksum = romChecksum;

    /* set state if present */
    if (consoleState != NULL) {
        tempPointer = consoleState + 11;
        ms->systemAddressBus = (tempPointer[1] << 8) | tempPointer[0];
        ms->systemDataBus = tempPointer[2];
        ms->interruptSignal = tempPointer[3];
        tempPointer += 8;
        memcpy((void *)ms->memoryAddressSpace, (void *)tempPointer, 65536);
        tempPointer += 65536;
        memcpy((void *)ms->ioAddressSpace, (void *)tempPointer, 256);
    }

    /* free save state memory */
    if (saveState != NULL)
        free((void *)saveState);

    /* return Console object */
    return ms;
}

/* this function destroys a Master System object and its component objects */
void destroyConsole(Console ms)
{
    /* destroy Z80 */
    if (ms->cpu != NULL)
        destroyZ80(ms->cpu);

    /* destroy Cartridge */
    if (ms->cart != NULL)
        destroyCartridge(ms->cart);

    /* destroy Controllers */
    if (ms->controllers != NULL)
        destroyControllers(ms->controllers);

    /* destroy VDP */
    if (ms->vdp != NULL)
        destroyVDP(ms->vdp);

    /* destroy SN76489 */
    if (ms->soundchip != NULL)
        destroySN76489(ms->soundchip);
}

/* this runs the console for one instruction of CPU time */
emuint console_executeInstruction(EmuBundle *eb)
{
    /* define frame update variable */
    emubool updateFrame = false;
    
    /* run components for one instruction */
    emuint c = Z80_executeInstruction(eb->ec->console->cpu);
    updateFrame = vdp_executeCycles(eb->ec->console->vdp, c);
    soundchip_executeCycles(eb->ec->console->soundchip, c);

    /* update controller state and draw frame */
    if (updateFrame) {
        controllers_updateValues(eb->ec->console->controllers);
        util_triggerPainting(eb);
    }

    /* return cycle count */
    return c;
}

/* this function deals with writes to any of the IO ports on the Z80 */
void console_ioWrite(Console ms, emuint address, emubyte data)
{
    ms->systemAddressBus = 0xFFFF & address;
    ms->systemDataBus = data;

    /* this switch statement decides where to write to (depending on console type) */
    if (ms->isGameGear) { /* console is Game Gear */
        switch (address & 0xFF) {        
            /* this section handles writing to Game Gear specific registers */
            case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
            case 0x06: ms->ioAddressSpace[address & 0xFF] = ms->systemDataBus; break;
        
            /* this section handles writing to the I/O control port and mirrors */
            case 0x07: case 0x09: case 0x0B: case 0x0D: case 0x0F: case 0x11: case 0x13:
            case 0x15: case 0x17: case 0x19: case 0x1B: case 0x1D: case 0x1F: case 0x21:
            case 0x23: case 0x25: case 0x27: case 0x29: case 0x2B: case 0x2D: case 0x2F:
            case 0x31: case 0x33: case 0x35: case 0x37: case 0x39: case 0x3B: case 0x3D:
            case 0x3F: controllers_handle3F(ms->controllers, 1, ms->systemDataBus); break; /* 0x3F is the real port */
                
            /* this section handles writing to port 3E to enable/disable RAM etc. */
            case 0x08: case 0x0A: case 0x0C: case 0x0E: case 0x10: case 0x12: case 0x14:
            case 0x16: case 0x18: case 0x1A: case 0x1C: case 0x1E: case 0x20: case 0x22:
            case 0x24: case 0x26: case 0x28: case 0x2A: case 0x2C: case 0x2E: case 0x30:
            case 0x32: case 0x34: case 0x36: case 0x38: case 0x3A: case 0x3C:
            case 0x3E: ms->ioAddressSpace[0x3E] = ms->systemDataBus; break; /* 0x3E is the real port */
                
            /* this section handles writes to the VDP data port and mirrors */
            case 0x80: case 0x82: case 0x84: case 0x86: case 0x88: case 0x8A: case 0x8C:
            case 0x8E: case 0x90: case 0x92: case 0x94: case 0x96: case 0x98: case 0x9A:
            case 0x9C: case 0x9E: case 0xA0: case 0xA2: case 0xA4: case 0xA6: case 0xA8:
            case 0xAA: case 0xAC: case 0xAE: case 0xB0: case 0xB2: case 0xB4: case 0xB6:
            case 0xB8: case 0xBA: case 0xBC:
            case 0xBE: vdp_dataWrite(ms->vdp, ms->systemDataBus); break; /* 0xBE is the real port */
                
            /* this section handles writes to the VDP control port and mirrors */
            case 0x81: case 0x83: case 0x85: case 0x87: case 0x89: case 0x8B: case 0x8D:
            case 0x8F: case 0x91: case 0x93: case 0x95: case 0x97: case 0x99: case 0x9B:
            case 0x9D: case 0x9F: case 0xA1: case 0xA3: case 0xA5: case 0xA7: case 0xA9:
            case 0xAB: case 0xAD: case 0xAF: case 0xB1: case 0xB3: case 0xB5: case 0xB7:
            case 0xB9: case 0xBB: case 0xBD:
            case 0xBF: vdp_controlWrite(ms->vdp, ms->systemDataBus); break; /* 0xBF is the real port */
                
            /* this section handles writes to the SN76489 sound chip */
            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46:
            case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D:
            case 0x4E: case 0x4F: case 0x50: case 0x51: case 0x52: case 0x53: case 0x54:
            case 0x55: case 0x56: case 0x57: case 0x58: case 0x59: case 0x5A: case 0x5B:
            case 0x5C: case 0x5D: case 0x5E: case 0x5F: case 0x60: case 0x61: case 0x62:
            case 0x63: case 0x64: case 0x65: case 0x66: case 0x67: case 0x68: case 0x69:
            case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F: case 0x70:
            case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E:
            case 0x7F: soundchip_soundWrite(ms->soundchip, ms->systemDataBus); break; /* 0x7F is the real port */
        }
    } else { /* console is Master System */
        switch (address & 0xFF) {
            /* this section handles writing to the I/O control port and mirrors */
            case 0x01: case 0x03: case 0x05: case 0x07: case 0x09: case 0x0B: case 0x0D:
            case 0x0F: case 0x11: case 0x13: case 0x15: case 0x17: case 0x19: case 0x1B:
            case 0x1D: case 0x1F: case 0x21: case 0x23: case 0x25: case 0x27: case 0x29:
            case 0x2B: case 0x2D: case 0x2F: case 0x31: case 0x33: case 0x35: case 0x37:
            case 0x39: case 0x3B: case 0x3D:
            case 0x3F: controllers_handle3F(ms->controllers, 1, ms->systemDataBus); break; /* 0x3F is the real port */
                
            /* this section handles writing to port 3E to enable/disable RAM etc. */
            case 0x00: case 0x02: case 0x04: case 0x06: case 0x08: case 0x0A: case 0x0C:
            case 0x0E: case 0x10: case 0x12: case 0x14: case 0x16: case 0x18: case 0x1A:
            case 0x1C: case 0x1E: case 0x20: case 0x22: case 0x24: case 0x26: case 0x28:
            case 0x2A: case 0x2C: case 0x2E: case 0x30: case 0x32: case 0x34: case 0x36:
            case 0x38: case 0x3A: case 0x3C:
            case 0x3E: ms->ioAddressSpace[0x3E] = ms->systemDataBus; break; /* 0x3E is the real port */
                
            /* this section handles writes to the VDP data port and mirrors */
            case 0x80: case 0x82: case 0x84: case 0x86: case 0x88: case 0x8A: case 0x8C:
            case 0x8E: case 0x90: case 0x92: case 0x94: case 0x96: case 0x98: case 0x9A:
            case 0x9C: case 0x9E: case 0xA0: case 0xA2: case 0xA4: case 0xA6: case 0xA8:
            case 0xAA: case 0xAC: case 0xAE: case 0xB0: case 0xB2: case 0xB4: case 0xB6:
            case 0xB8: case 0xBA: case 0xBC:
            case 0xBE: vdp_dataWrite(ms->vdp, ms->systemDataBus); break; /* 0xBE is the real port */
                
            /* this section handles writes to the VDP control port and mirrors */
            case 0x81: case 0x83: case 0x85: case 0x87: case 0x89: case 0x8B: case 0x8D:
            case 0x8F: case 0x91: case 0x93: case 0x95: case 0x97: case 0x99: case 0x9B:
            case 0x9D: case 0x9F: case 0xA1: case 0xA3: case 0xA5: case 0xA7: case 0xA9:
            case 0xAB: case 0xAD: case 0xAF: case 0xB1: case 0xB3: case 0xB5: case 0xB7:
            case 0xB9: case 0xBB: case 0xBD:
            case 0xBF: vdp_controlWrite(ms->vdp, ms->systemDataBus); break; /* 0xBF is the real port */
                
            /* this section handles writes to the SN76489 sound chip */
            case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46:
            case 0x47: case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D:
            case 0x4E: case 0x4F: case 0x50: case 0x51: case 0x52: case 0x53: case 0x54:
            case 0x55: case 0x56: case 0x57: case 0x58: case 0x59: case 0x5A: case 0x5B:
            case 0x5C: case 0x5D: case 0x5E: case 0x5F: case 0x60: case 0x61: case 0x62:
            case 0x63: case 0x64: case 0x65: case 0x66: case 0x67: case 0x68: case 0x69:
            case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F: case 0x70:
            case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E:
            case 0x7F: soundchip_soundWrite(ms->soundchip, ms->systemDataBus); break; /* 0x7F is the real port */
        }
    }
}

/* this function deals with reads from the Z80 IO ports */
emubyte console_ioRead(Console ms, emuint address)
{
    ms->systemAddressBus = 0xFFFF & address;
    emubyte returnVal = 0;

    /* this switch statement decides where to read from (depending on console type) */
    if (ms->isGameGear) { /* console is Game Gear */
        switch (address & 0xFF) {
            /* this section handles reading from the start button register */
            case 0x00: returnVal = controllers_handleStart(ms->controllers, 0, 0); break;
        
            /* this section handles reading from Game Gear specific registers */
            case 0x01: case 0x02: case 0x03: case 0x04: case 0x05:
            case 0x06: returnVal = ms->ioAddressSpace[address & 0xFF]; break;
        
            /* this section handles reading from the I/O control port and mirrors */
            case 0x07: case 0x09: case 0x0B: case 0x0D: case 0x0F: case 0x11: case 0x13:
            case 0x15: case 0x17: case 0x19: case 0x1B: case 0x1D: case 0x1F: case 0x21:
            case 0x23: case 0x25: case 0x27: case 0x29: case 0x2B: case 0x2D: case 0x2F:
            case 0x31: case 0x33: case 0x35: case 0x37: case 0x39: case 0x3B: case 0x3D:
            case 0x3F: returnVal = 0xFF; break; /* 0x3F is the real port */
                
            /* this section handles reading from joypad port 0xDC and mirrors */
            case 0xC0:
            case 0xDC: returnVal = controllers_handleDC(ms->controllers, 0, 0); break; /* 0xDC is the real port */
            
            /* this section handles reading from joypad port 0xDD and mirrors */
            case 0xC1:
            case 0xDD: returnVal = controllers_handleDD(ms->controllers, 0, 0); break; /* 0xDD is the real port */
            
            /* this section returns FF */
            case 0xC2: case 0xC4: case 0xC6: case 0xC8: case 0xCA: case 0xCC: case 0xCE:
            case 0xD0: case 0xD2: case 0xD4: case 0xD6: case 0xD8: case 0xDA: case 0xDE:
            case 0xE0: case 0xE2: case 0xE4: case 0xE6: case 0xE8: case 0xEA: case 0xEC:
            case 0xEE: case 0xF0: case 0xF2: case 0xF4: case 0xF6: case 0xF8: case 0xFA:
            case 0xFC: case 0xFE: case 0xC3: case 0xC5: case 0xC7: case 0xC9: case 0xCB:
            case 0xCF: case 0xD1: case 0xD3: case 0xD5: case 0xD7: case 0xD9: case 0xDB:
            case 0xDF: case 0xE1: case 0xE3: case 0xE5: case 0xE7: case 0xE9: case 0xEB:
            case 0xED: case 0xEF: case 0xF1: case 0xF3: case 0xF5: case 0xF7: case 0xF9:
            case 0xFB: case 0xFD: case 0xCD:
            case 0xFF: returnVal = 0xFF; break;
                
            /* this section handles reading from port 3E and mirrors */
            case 0x08: case 0x0A: case 0x0C: case 0x0E: case 0x10: case 0x12: case 0x14:
            case 0x16: case 0x18: case 0x1A: case 0x1C: case 0x1E: case 0x20: case 0x22:
            case 0x24: case 0x26: case 0x28: case 0x2A: case 0x2C: case 0x2E: case 0x30:
            case 0x32: case 0x34: case 0x36: case 0x38: case 0x3A: case 0x3C:
            case 0x3E: returnVal = 0xFF; break; /* 0x3E is the real port */
                
            /* this section handles reading from the VDP vcounter and mirrors */
            case 0x40: case 0x42: case 0x44: case 0x46: case 0x48: case 0x4A: case 0x4C:
            case 0x4E: case 0x50: case 0x52: case 0x54: case 0x56: case 0x58: case 0x5A:
            case 0x5C: case 0x5E: case 0x60: case 0x62: case 0x64: case 0x66: case 0x68:
            case 0x6A: case 0x6C: case 0x6E: case 0x70: case 0x72: case 0x74: case 0x76:
            case 0x78: case 0x7A: case 0x7C:
            case 0x7E: returnVal = 0xFF & vdp_returnVCounter(ms->vdp); break; /* 0x7E is the real port */
                
            /* this section handles reading from the VDP hcounter and mirrors */
            case 0x41: case 0x43: case 0x45: case 0x47: case 0x49: case 0x4B: case 0x4D:
            case 0x4F: case 0x51: case 0x53: case 0x55: case 0x57: case 0x59: case 0x5B:
            case 0x5D: case 0x5F: case 0x61: case 0x63: case 0x65: case 0x67: case 0x69:
            case 0x6B: case 0x6D: case 0x6F: case 0x71: case 0x73: case 0x75: case 0x77:
            case 0x79: case 0x7B: case 0x7D:
            case 0x7F: returnVal = 0xFF & vdp_returnHCounter(ms->vdp); break; /* 0x7F is the real port */
                
            /* this section handles reads of the VDP data port contents and mirrors */
            case 0x80: case 0x82: case 0x84: case 0x86: case 0x88: case 0x8A: case 0x8C:
            case 0x8E: case 0x90: case 0x92: case 0x94: case 0x96: case 0x98: case 0x9A:
            case 0x9C: case 0x9E: case 0xA0: case 0xA2: case 0xA4: case 0xA6: case 0xA8:
            case 0xAA: case 0xAC: case 0xAE: case 0xB0: case 0xB2: case 0xB4: case 0xB6:
            case 0xB8: case 0xBA: case 0xBC:
            case 0xBE: returnVal = 0xFF & vdp_dataRead(ms->vdp); break; /* 0xBE is the real port */
                
            /* this section handles reads from the VDP control port and mirrors */
            case 0x81: case 0x83: case 0x85: case 0x87: case 0x89: case 0x8B: case 0x8D:
            case 0x8F: case 0x91: case 0x93: case 0x95: case 0x97: case 0x99: case 0x9B:
            case 0x9D: case 0x9F: case 0xA1: case 0xA3: case 0xA5: case 0xA7: case 0xA9:
            case 0xAB: case 0xAD: case 0xAF: case 0xB1: case 0xB3: case 0xB5: case 0xB7:
            case 0xB9: case 0xBB: case 0xBD:
            case 0xBF: returnVal = 0xFF & vdp_controlRead(ms->vdp); break; /* 0xBF is the real port */
        }
    } else { /* console is Master System */
        switch (address & 0xFF) {
            /* this section handles reading from the I/O control port and mirrors */
            case 0x01: case 0x03: case 0x05: case 0x07: case 0x09: case 0x0B: case 0x0D:
            case 0x0F: case 0x11: case 0x13: case 0x15: case 0x17: case 0x19: case 0x1B:
            case 0x1D: case 0x1F: case 0x21: case 0x23: case 0x25: case 0x27: case 0x29:
            case 0x2B: case 0x2D: case 0x2F: case 0x31: case 0x33: case 0x35: case 0x37:
            case 0x39: case 0x3B: case 0x3D:
            case 0x3F: /* 0x3F is the real port */
                       returnVal = 0xFF; break;
                
            /* this section handles reading from joypad port 0xDC and mirrors */
            case 0xC0: case 0xC2: case 0xC4: case 0xC6: case 0xC8: case 0xCA: case 0xCC:
            case 0xCE: case 0xD0: case 0xD2: case 0xD4: case 0xD6: case 0xD8: case 0xDA:
            case 0xDE: case 0xE0: case 0xE2: case 0xE4: case 0xE6: case 0xE8: case 0xEA:
            case 0xEC: case 0xEE: case 0xF0: case 0xF2: case 0xF4: case 0xF6: case 0xF8:
            case 0xFA: case 0xFC: case 0xFE:
            case 0xDC: returnVal = controllers_handleDC(ms->controllers, 0, 0); break; /* 0xDC is the real port */
                
            /* this section handles reading from joypad port 0xDD and mirrors */
            case 0xC1: case 0xC3: case 0xC5: case 0xC7: case 0xC9: case 0xCB: case 0xCD:
            case 0xCF: case 0xD1: case 0xD3: case 0xD5: case 0xD7: case 0xD9: case 0xDB:
            case 0xDF: case 0xE1: case 0xE3: case 0xE5: case 0xE7: case 0xE9: case 0xEB:
            case 0xED: case 0xEF: case 0xF1: case 0xF3: case 0xF5: case 0xF7: case 0xF9:
            case 0xFB: case 0xFD: case 0xFF:
            case 0xDD: returnVal = controllers_handleDD(ms->controllers, 0, 0); break; /* 0xDD is the real port */
                
            /* this section handles reading from port 3E and mirrors */
            case 0x00: case 0x02: case 0x04: case 0x06: case 0x08: case 0x0A: case 0x0C:
            case 0x0E: case 0x10: case 0x12: case 0x14: case 0x16: case 0x18: case 0x1A:
            case 0x1C: case 0x1E: case 0x20: case 0x22: case 0x24: case 0x26: case 0x28:
            case 0x2A: case 0x2C: case 0x2E: case 0x30: case 0x32: case 0x34: case 0x36:
            case 0x38: case 0x3A: case 0x3C:
            case 0x3E: returnVal = 0xFF; break; /* 0x3E is the real port */
                
            /* this section handles reading from the VDP vcounter and mirrors */
            case 0x40: case 0x42: case 0x44: case 0x46: case 0x48: case 0x4A: case 0x4C:
            case 0x4E: case 0x50: case 0x52: case 0x54: case 0x56: case 0x58: case 0x5A:
            case 0x5C: case 0x5E: case 0x60: case 0x62: case 0x64: case 0x66: case 0x68:
            case 0x6A: case 0x6C: case 0x6E: case 0x70: case 0x72: case 0x74: case 0x76:
            case 0x78: case 0x7A: case 0x7C:
            case 0x7E: returnVal = 0xFF & vdp_returnVCounter(ms->vdp); break; /* 0x7E is the real port */
                
            /* this section handles reading from the VDP hcounter and mirrors */
            case 0x41: case 0x43: case 0x45: case 0x47: case 0x49: case 0x4B: case 0x4D:
            case 0x4F: case 0x51: case 0x53: case 0x55: case 0x57: case 0x59: case 0x5B:
            case 0x5D: case 0x5F: case 0x61: case 0x63: case 0x65: case 0x67: case 0x69:
            case 0x6B: case 0x6D: case 0x6F: case 0x71: case 0x73: case 0x75: case 0x77:
            case 0x79: case 0x7B: case 0x7D:
            case 0x7F: returnVal = 0xFF & vdp_returnHCounter(ms->vdp); break; /* 0x7F is the real port */
                
            /* this section handles reads of the VDP data port contents and mirrors */
            case 0x80: case 0x82: case 0x84: case 0x86: case 0x88: case 0x8A: case 0x8C:
            case 0x8E: case 0x90: case 0x92: case 0x94: case 0x96: case 0x98: case 0x9A:
            case 0x9C: case 0x9E: case 0xA0: case 0xA2: case 0xA4: case 0xA6: case 0xA8:
            case 0xAA: case 0xAC: case 0xAE: case 0xB0: case 0xB2: case 0xB4: case 0xB6:
            case 0xB8: case 0xBA: case 0xBC:
            case 0xBE: returnVal = 0xFF & vdp_dataRead(ms->vdp); break; /* 0xBE is the real port */
                
            /* this section handles reads from the VDP control port and mirrors */
            case 0x81: case 0x83: case 0x85: case 0x87: case 0x89: case 0x8B: case 0x8D:
            case 0x8F: case 0x91: case 0x93: case 0x95: case 0x97: case 0x99: case 0x9B:
            case 0x9D: case 0x9F: case 0xA1: case 0xA3: case 0xA5: case 0xA7: case 0xA9:
            case 0xAB: case 0xAD: case 0xAF: case 0xB1: case 0xB3: case 0xB5: case 0xB7:
            case 0xB9: case 0xBB: case 0xBD:
            case 0xBF: returnVal = 0xFF & vdp_controlRead(ms->vdp); break; /* 0xBF is the real port */
        }
    }

    ms->systemDataBus = returnVal;
    return ms->systemDataBus;
}

/* this function deals with writes to the memory space */
void console_memWrite(Console ms, emuint address, emubyte data)
{
    ms->systemAddressBus = 0xFFFF & address;
    ms->systemDataBus = data;

    /* all writes to this address range go to the cartridge */
    if (ms->systemAddressBus <= 0xBFFF) {
        /* check for Codemasters mapper */
        if (cart_isCodemasters(ms->cart)) {
            switch (ms->systemAddressBus) {
            case 0: cart_writeFFFD(ms->cart, ms->systemDataBus); break;
            case 0x4000: cart_writeFFFE(ms->cart, ms->systemDataBus); break;
            case 0x8000: cart_writeFFFF(ms->cart, ms->systemDataBus); break;
            }
        }
        cart_writeCartridge(ms->cart, ms->systemAddressBus, ms->systemDataBus);
    } /* writes to this address range can go to either RAM or cartridge */
    else if (ms->systemAddressBus <= 0xFFFF) {
        /* check if RAM is enabled */
        if ((ms->ioAddressSpace[0x3E] & 0x10) == 0 || !cart_isCartOverridingSystemRam(ms->cart)) {
            /* this makes sure the mirrored RAM works as it should */
            if (ms->systemAddressBus <= 0xDFFF) /* actual RAM */
                ms->memoryAddressSpace[ms->systemAddressBus] = ms->systemDataBus;
            else if (ms->systemAddressBus <= 0xFFFF) /* mirrored RAM */
                ms->memoryAddressSpace[ms->systemAddressBus - 8192] = ms->systemDataBus;

            /* check if we are writing to control registers as well */
            if (!cart_isCodemasters(ms->cart)) {
                switch (ms->systemAddressBus) {
                case 0xFFFC: cart_writeFFFC(ms->cart, ms->systemDataBus); break;
                case 0xFFFD: cart_writeFFFD(ms->cart, ms->systemDataBus); break;
                case 0xFFFE: cart_writeFFFE(ms->cart, ms->systemDataBus); break;
                case 0xFFFF: cart_writeFFFF(ms->cart, ms->systemDataBus); break;
                }
            }
        } /* RAM is disabled in this case so send all writes to cartridge */
        else {
            /* write to cartridge */
            cart_writeCartridge(ms->cart, ms->systemAddressBus, ms->systemDataBus);

            /* check if we are writing to control registers as well */
            if (!cart_isCodemasters(ms->cart)) {
                switch (ms->systemAddressBus) {
                case 0xFFFC: cart_writeFFFC(ms->cart, ms->systemDataBus); break;
                case 0xFFFD: cart_writeFFFD(ms->cart, ms->systemDataBus); break;
                case 0xFFFE: cart_writeFFFE(ms->cart, ms->systemDataBus); break;
                case 0xFFFF: cart_writeFFFF(ms->cart, ms->systemDataBus); break;
                }
            }
        }
    }
}

/* this function deals with reads from the memory space */
emubyte console_memRead(Console ms, emuint address)
{
    ms->systemAddressBus = 0xFFFF & address;
    emubyte returnVal = 0;
    
    /* all reads from the address range go to the cartridge */
    if (ms->systemAddressBus <= 0xBFFF) {
        returnVal = cart_readCartridge(ms->cart, ms->systemAddressBus);
    } /* reads from this address range can go to either RAM or cartridge */
    else if (ms->systemAddressBus <= 0xFFFF) {
        /* check if RAM is enabled */
        if ((ms->ioAddressSpace[0x3E] & 0x10) == 0 || !cart_isCartOverridingSystemRam(ms->cart)) {
            /* this makes sure the mirrored RAM works as it should */
            if (ms->systemAddressBus <= 0xDFFF) /* actual RAM */
                returnVal = ms->memoryAddressSpace[ms->systemAddressBus];
            else if (ms->systemAddressBus <= 0xFFFF) /* mirrored RAM */
                returnVal = ms->memoryAddressSpace[ms->systemAddressBus - 8192];
        } /* RAM is disabled in this case so read from cartridge */
        else {
            returnVal = cart_readCartridge(ms->cart, ms->systemAddressBus);
        }
    }

    /* check for Game Genie codes */
    if (ggCheatArray.enabled) {
        for (emuint i = 0; i < ggCheatArray.cheatCount; ++i) {
            if (ms->systemAddressBus == ggCheatArray.cheats[i].address) {
                if (ggCheatArray.cheats[i].cloakAndReferencePresent) {
                    if (returnVal == ggCheatArray.cheats[i].reference) {
                        returnVal = ggCheatArray.cheats[i].value;
                    }
                } else {
                    returnVal = ggCheatArray.cheats[i].value;
                }
                break;
            }
        }
    }

    ms->systemDataBus = returnVal;
    return ms->systemDataBus;
}

/* this function allows the Z80 to check for NMI triggers */
emubool console_checkNmi(Console ms)
{
    /* check if pause button on console is pressed */
    emubool nmi = controllers_handlePauseStatus(ms->controllers, 0, 0);

    /* reset pause button status */
    controllers_handlePauseStatus(ms->controllers, 1, false);

    return nmi;
}

/* this function allows the Z80 to check for maskable interrupts from a signalling device */
emubyte console_checkInterrupt(Console ms)
{
    ms->interruptSignal = vdp_isInterruptAsserted(ms->vdp);
    return ms->interruptSignal;
}

/* this function allows the Z80 to tell the signalling device that the interrupt has been handled */
void console_interruptHandled(Console ms)
{
    ms->interruptSignal = 0;
}

/* this function fills the SDL buffer pointer to by external with the contents of the VDP's
   internal buffer - it is thread-safe */
emubool console_handleFrame(Console ms, void *external)
{
    return vdp_handleFrame(ms->vdp, 2, 0, NULL, external);
}

/* this function handles access to I/O port DC */
emubyte console_handleTempDC(Console ms, emubyte action, emubyte value)
{
    return controllers_handleTempDC(ms->controllers, action, value);
}

/* this function handles access to the pause button */
emubool console_handleTempPauseStatus(Console ms, emubyte action, emubool value)
{
    return controllers_handleTempPauseStatus(ms->controllers, action, value);
}

/* this function signals the VDP, telling it to calculate the current hCounter value and store it */
void console_tellVDPToStoreHCounterValue(Console ms)
{
    vdp_storeHCounterValue(ms->vdp);
}

/* this function passes through start access calls to the internal controller object */
emubyte console_handleTempStart(Console ms, emubyte action, emubyte value)
{
    return controllers_handleTempStart(ms->controllers, action, value);
}

/* this function returns the number of Z80 cycles the VDP is currently at */
emuint console_getVDPCycles(Console ms)
{
    return vdp_getCycles(ms->vdp);
}

/* this function saves the state of the console into a memory buffer and returns it as an emubyte pointer */
emubyte *console_saveState(Console ms)
{
    /* define variables */
    emubyte *tempPointer;
    emuint marker = 0;

    /* save state of cartridge */
    emubyte *cartridge = cart_saveState(ms->cart);
    if (cartridge == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "console.c", "Couldn't get cartridge state...\n");
        return NULL;
    }
    emuint cartridgeSize = (cartridge[3] << 24) | (cartridge[2] << 16) | (cartridge[1] << 8) | cartridge[0];
    
    /* save state of controllers */
    emubyte *controllers = controllers_saveState(ms->controllers);
    if (controllers == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "console.c", "Couldn't get controllers state...\n");
        free((void *)cartridge);
        return NULL;
    }
    emuint controllersSize = (controllers[3] << 24) | (controllers[2] << 16) | (controllers[1] << 8) | controllers[0];

    /* save state of CPU */
    emubyte *z80 = Z80_saveState(ms->cpu);
    if (z80 == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "console.c", "Coudn't get Z80 state...\n");
        free((void *)cartridge);
        free((void *)controllers);
        return NULL;
    }
    emuint z80Size = (z80[3] << 24) | (z80[2] << 16) | (z80[1] << 8) | z80[0];

    /* save state of sound chip */
    emubyte *soundchip = soundchip_saveState(ms->soundchip);
    if (soundchip == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "console.c", "Couldn't get soundchip state...\n");
        free((void *)cartridge);
        free((void *)controllers);
        free((void *)z80);
        return NULL;
    }
    emuint soundchipSize = (soundchip[3] << 24) | (soundchip[2] << 16) | (soundchip[1] << 8) | soundchip[0];

    /* save state of VDP */
    emubyte *vdp = vdp_saveState(ms->vdp);
    if (vdp == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "console.c", "Couldn't get VDP state...\n");
        free((void *)cartridge);
        free((void *)controllers);
        free((void *)z80);
        free((void *)soundchip);
        return NULL;
    }
    emuint vdpSize = (vdp[3] << 24) | (vdp[2] << 16) | (vdp[1] << 8) | vdp[0];

    /* save state of console itself */
    emuint consoleSize = 8 + 65536 + 256 + 11; /* 11 extra bytes for header and size info */
    emubyte *console = malloc(sizeof(emubyte) * consoleSize);
    if (console == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "console.c", "Couldn't allocate memory for console state...\n");
        free((void *)cartridge);
        free((void *)controllers);
        free((void *)z80);
        free((void *)soundchip);
        free((void *)vdp);
        return NULL;
    }
    
    /* console header */
    console[marker++] = consoleSize & 0xFF;
    console[marker++] = (consoleSize >> 8) & 0xFF;
    console[marker++] = (consoleSize >> 16) & 0xFF;
    console[marker++] = (consoleSize >> 24) & 0xFF;
    console[marker++] = 'C';
    console[marker++] = 'O';
    console[marker++] = 'N';
    console[marker++] = 'S';
    console[marker++] = 'O';
    console[marker++] = 'L';
    console[marker++] = 'E';

    /* other console data */
    console[marker++] = ms->systemAddressBus & 0xFF;
    console[marker++] = (ms->systemAddressBus >> 8) & 0xFF;
    console[marker++] = ms->systemDataBus;
    console[marker++] = ms->interruptSignal;
    console[marker++] = ms->checksum & 0xFF;
    console[marker++] = (ms->checksum >> 8) & 0xFF;
    console[marker++] = (ms->checksum >> 16) & 0xFF;
    console[marker++] = (ms->checksum >> 24) & 0xFF;

    /* console memory maps */
    tempPointer = console + marker;
    memcpy((void *)tempPointer, (void *)ms->memoryAddressSpace, 65536);
    tempPointer += 65536;
    memcpy((void *)tempPointer, (void *)ms->ioAddressSpace, 256);

    /* now compose final buffer - extra twelve bytes for file header and CRC-32 checksum at end */
    emuint finalSize = cartridgeSize +
                       controllersSize +
                       z80Size +
                       soundchipSize +
                       vdpSize +
                       consoleSize +
                       12;
    emubyte *finalBuffer = malloc(sizeof(emubyte) * finalSize);
    if (finalBuffer == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "console.c", "Couldn't allocate memory for final state...\n");
        free((void *)cartridge);
        free((void *)controllers);
        free((void *)z80);
        free((void *)soundchip);
        free((void *)vdp);
        free((void *)console);
        return NULL;
    }

    /* set file header */
    tempPointer = finalBuffer;
    marker = 0;
    tempPointer[marker++] = 'C';
    tempPointer[marker++] = 'M';
    tempPointer[marker++] = '0';
    tempPointer[marker++] = '2';
    tempPointer[marker++] = finalSize & 0xFF;
    tempPointer[marker++] = (finalSize >> 8) & 0xFF;
    tempPointer[marker++] = (finalSize >> 16) & 0xFF;
    tempPointer[marker++] = (finalSize >> 24) & 0xFF;
    tempPointer += marker;

    /* now copy each section into final buffer */
    memcpy((void *)tempPointer, (void *)cartridge, cartridgeSize);
    tempPointer += cartridgeSize;
    memcpy((void *)tempPointer, (void *)controllers, controllersSize);
    tempPointer += controllersSize;
    memcpy((void *)tempPointer, (void *)z80, z80Size);
    tempPointer += z80Size;
    memcpy((void *)tempPointer, (void *)soundchip, soundchipSize);
    tempPointer += soundchipSize;
    memcpy((void *)tempPointer, (void *)vdp, vdpSize);
    tempPointer += vdpSize;
    memcpy((void *)tempPointer, (void *)console, consoleSize);
    tempPointer += consoleSize;

    /* set checksum for whole file in last four bytes */
    emuint finalChecksum = calculateCRC32(finalBuffer, finalSize - 4);
    tempPointer[0] = finalChecksum & 0xFF;
    tempPointer[1] = (finalChecksum >> 8) & 0xFF;
    tempPointer[2] = (finalChecksum >> 16) & 0xFF;
    tempPointer[3] = (finalChecksum >> 24) & 0xFF;

    /* free temporary buffers */
    free((void *)cartridge);
    free((void *)controllers);
    free((void *)z80);
    free((void *)soundchip);
    free((void *)vdp);
    free((void *)console);

    /* return state */
    return finalBuffer;
}

SDL_Rect *console_getSourceRect(Console ms) {
    return vdp_getSourceRect(ms->vdp);
}

/* this function returns the contents of the GG register at IO port 0x06 - only for use in GG mode */
emubyte console_readPSGReg(Console ms)
{
    return ms->ioAddressSpace[0x06];
}

/* this function stops the SDL sound channel */
void console_stopAudio(Console ms) {
    soundchip_stopAudio(ms->soundchip);
}

/* this function reports how many bytes are required by a Console object */
emuint console_getMemoryUsage(void)
{
    return (sizeof(struct Console) * sizeof(emubyte)) + CONSOLE_MEMORY_SPACE + CONSOLE_IO_SPACE;
}

/* this functions reports how many bytes are required in total by a Console object and all its
   sub-components */
emuint console_getWholeMemoryUsage(void)
{
    return console_getMemoryUsage() + Z80_getMemoryUsage() + vdp_getMemoryUsage() + soundchip_getMemoryUsage() +
    controllers_getMemoryUsage() + cart_getMemoryUsage();
}

/* this function returns the SDL AudioDeviceID from the sound chip */
emuint console_getAudioDeviceID(Console ms)
{
    return soundchip_getAudioDeviceID(ms->soundchip);
}

/* this function returns the current line from the VDP */
emuint console_getCurrentLine(Console ms)
{
    return vdp_getCurrentLine(ms->vdp);
}
