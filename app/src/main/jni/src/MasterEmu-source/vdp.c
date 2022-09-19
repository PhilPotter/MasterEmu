/* MasterEmu SN76489 VDP source code file
   copyright Phil Potter, 2022 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <android/log.h>
#include "vdp.h"

/* below is the Action Replay cheat code object reference */
extern ArCheatArray arCheatArray;

/* below is an array that contains all hCounter values for a scanline - 
   it can be accessed using number of Z80 cycles as the index */
static const emubyte hCounterValues[228] = {
    0xF4, 0xF5, 0xF6, 0xF6, 0xF7, 0xF8, 0xF9, 0xF9, 0xFA, 0xFB, 0xFC, 0xFC, 0xFD, 0xFE, 0xFF,
    0xFF, 0x00, 0x01, 0x02, 0x02, 0x03, 0x04, 0x05, 0x05, 0x06, 0x07, 0x08, 0x08, 0x09, 0x0A,
    0x0B, 0x0B, 0x0C, 0x0D, 0x0E, 0x0E, 0x0F, 0x10, 0x11, 0x11, 0x12, 0x13, 0x14, 0x14, 0x15,
    0x16, 0x17, 0x17, 0x18, 0x19, 0x1A, 0x1A, 0x1B, 0x1C, 0x1D, 0x1D, 0x1E, 0x1F, 0x20, 0x20,
    0x21, 0x22, 0x23, 0x23, 0x24, 0x25, 0x26, 0x26, 0x27, 0x28, 0x29, 0x29, 0x2A, 0x2B, 0x2C,
    0x2C, 0x2D, 0x2E, 0x2F, 0x2F, 0x30, 0x31, 0x32, 0x32, 0x33, 0x34, 0x35, 0x35, 0x36, 0x37,
    0x38, 0x38, 0x39, 0x3A, 0x3B, 0x3B, 0x3C, 0x3D, 0x3E, 0x3E, 0x3F, 0x40, 0x41, 0x41, 0x42,
    0x43, 0x44, 0x44, 0x45, 0x46, 0x47, 0x47, 0x48, 0x49, 0x4A, 0x4A, 0x4B, 0x4C, 0x4D, 0x4D,
    0x4E, 0x4F, 0x50, 0x50, 0x51, 0x52, 0x53, 0x53, 0x54, 0x55, 0x56, 0x56, 0x57, 0x58, 0x59,
    0x59, 0x5A, 0x5B, 0x5C, 0x5C, 0x5D, 0x5E, 0x5F, 0x5F, 0x60, 0x61, 0x62, 0x62, 0x63, 0x64,
    0x65, 0x65, 0x66, 0x67, 0x68, 0x68, 0x69, 0x6A, 0x6B, 0x6B, 0x6C, 0x6D, 0x6E, 0x6E, 0x6F,
    0x70, 0x71, 0x71, 0x72, 0x73, 0x74, 0x74, 0x75, 0x76, 0x77, 0x77, 0x78, 0x79, 0x7A, 0x7A,
    0x7B, 0x7C, 0x7D, 0x7D, 0x7E, 0x7F, 0x80, 0x80, 0x81, 0x82, 0x83, 0x83, 0x84, 0x85, 0x86,
    0x86, 0x87, 0x88, 0x89, 0x89, 0x8A, 0x8B, 0x8C, 0x8C, 0x8D, 0x8E, 0x8F, 0x8F, 0x90, 0x91,
    0x92, 0x92, 0x93, 0xE9, 0xEA, 0xEA, 0xEB, 0xEC, 0xED, 0xED, 0xEE, 0xEF, 0xF0, 0xF0, 0xF1,
    0xF2, 0xF3, 0xF3
};

/* below is an array which contains all the VCounter values for PAL */
static const emubyte palVCounterValues[3][313] = {
    { /* 192-line mode */
        /* first contiguous block */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
        0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
        0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
        0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2,
        0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
        0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0,
        0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2,
        
        /* second contiguous block */
        0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8,
        0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
        0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
        0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5,
        0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
    },
    { /* 224-line mode */
        /* first contiguous block */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
        0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
        0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
        0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2,
        0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
        0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0,
        0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 
        0xFF,
        
        /* second contiguous block */
        0x00, 0x01, 0x02,
        
        /* third contiguous block */
        0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8,
        0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
        0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
        0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
    },
    { /* 240-line mode */
        /* first contiguous block */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
        0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
        0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
        0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2,
        0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
        0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0,
        0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 
        0xFF,
        
        /* second contiguous block */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
        
        /* third contiguous block */
        0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0,
        0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 
        0xFF
    }
};

/* below is an array which contains all the VCounter values for PAL */
static const emubyte ntscVCounterValues[3][262] = {
    { /* 192-line mode */
        /* first contiguous block */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
        0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
        0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
        0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2,
        0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
        0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
        
        /* second contiguous block */
        0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE3,
        0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2,
        0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
    },
    { /* 224-line mode */
        /* first contiguous block */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
        0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
        0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
        0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2,
        0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
        0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0,
        0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
        
        /* second contiguous block */
        0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3,
        0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
    },
    { /* 240-line mode */
        /* first contiguous block */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
        0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C,
        0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
        0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A,
        0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
        0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
        0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4,
        0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2,
        0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xD1,
        0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF, 0xE0,
        0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
        0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 
        0xFF,
        
        /* second contiguous block */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05
    }
};

/* this enumerated type lets us specify what TV mode the VDP is in */
enum tvType { PAL, NTSC };
typedef enum tvType tvType;

/* this enumerated type lets us determine the current line height mode */
enum lineMode { mode192 = 0, mode224 = 1, mode240 = 2 };
typedef enum lineMode lineMode;

/* this struct allows us to store sprite attributes together */
typedef struct {
    signed_emuint y;
    signed_emuint x;
    signed_emuint width;
    signed_emuint height;
    emuint patternIndex;
    emubyte present;
} Sprite;

/* this struct models the VDP's internal state */
struct VDP {
    Console ms; /* this stores a reference to the main console object */

    emubyte vdpStatus; /* this is the VDP status byte */
    emubyte *cRam; /* this refers to the colour RAM of the VDP (32 or 64 bytes) */
    emubyte *vRam; /* this refers to the video RAM of the VDP (16 kilobytes) */
    emubyte *vdpRegisters; /* this refers to the VDP registers */
    emuint addressRegister; /* this is the address register */
    emubyte codeRegister; /* this is the code register */
    emubool secondControlByte; /* this helps differentiate between control bytes */
    emuint lineNumber; /* this stores the line number properly */
    emubyte vCounter; /* this is the vertical counter (1 byte) */
    emubyte hCounter; /* this is the horizontal counter value when latched */
    lineMode lines; /* this is the current line mode */
    emubyte dataPortBuffer; /* this buffers data port read */
    emuint z80Cycles; /* this stores cycles before producing a scanline */
    emubool gameGearMode; /* this determines whether or not we start in Game Gear mode */
    emubyte latchedDataByte; /* this is for use with some cRam writes */
    emubyte *frame; /* this stores the pixels for the whole frame in a displayable form */
    emuint *scanline; /* this stores the pixels for the current scanline */
    tvType type; /* this stores whether the VDP is in PAL or NTSC TV mode */
    emubyte lineInterruptFlag; /* this is the line interrupt flag */
    emuint lineInterruptCounter; /* this counts down for the line interrupt */
    Sprite *sprites; /* this is the eight-sprite buffer for each scanline */
    emubyte tempVerticalScrollRegister; /* this stores the scroll value to be used during a frame */
    emubool tempVerticalScrollChange; /* this lets us change the vertical scroll register */
    
    SDL_mutex *frameMutex; /* this prevents the frame buffer being accessed by more than one
                              thread concurrently - it depends on SDL for portability */
    SDL_Rect *sourceRect; /* this is the source rectangle passed through from the initialisation routine -
                             referencing it here allows us to easily change the resolution displayed on screen
                             between 192, 224, and 240 line modes */
};

/* these function definitions deal with functionality internal to the VDP - check
   their individual implementations for further detail and comments */
static emuint returnAddressRegister(VDP v);
static void writeAddressRegister(VDP v, emuint a);
static void incrementAddressRegister(VDP v);
static emubyte returnCodeRegister(VDP v);
static void writeCodeRegister(VDP v, emubyte c);
static emubool handleCountersAndInterrupts(VDP v);
static void setFrameInterruptFlag(VDP v);
static void setLineInterruptFlag(VDP v);
static emubool isActiveDisplayPeriod(VDP v);
static emubool lineIsInActiveDisplayPeriod(VDP v, signed_emuint line);
static void renderSpritesMode4(VDP v);
static void scanForSpritesMode4(VDP v);
static void renderBackgroundMode4(VDP v);

/* this creates and returns a VDP object */
VDP createVDP(Console ms, emubool ggMode, emubool isPal, SDL_Rect *sourceRect, emubyte *vdpState, emubyte *wholePointer)
{
    /* allocate memory for VDP struct */
    VDP v = (VDP)wholePointer;
    wholePointer += sizeof(struct VDP);
    
    /* set all sub-pointers storing locally allocated references to NULL now */
    v->frameMutex = NULL;
    v->cRam = NULL;
    v->vRam = NULL;
    v->vdpRegisters = NULL;
    v->frame = NULL;
    v->scanline = NULL;
    v->sprites = NULL;

    /* store Console reference */
    v->ms = ms;

    /* setup video and colour RAM - colour RAM is 64 bytes to take account of Game Gear mode, 
       although only 32 bytes are used in Master System mode */
    v->cRam = wholePointer;
    memset((void *)v->cRam, 0, 64);
    wholePointer += 64;
    v->vRam = wholePointer;
    memset((void *)v->vRam, 0, 16384);
    wholePointer += 16384;

    /* setup VDP registers */
    v->vdpRegisters = wholePointer;
    memset((void *)v->vdpRegisters, 0, 16);
    wholePointer += 16;
    v->vdpRegisters[0] = 0x36; /* mode control no. 1 register */
    v->vdpRegisters[1] = 0x80; /* mode control no. 2 register */
    v->vdpRegisters[2] = 0xFF; /* name table base address register */
    v->vdpRegisters[3] = 0xFF; /* colour table base address register */
    v->vdpRegisters[4] = 0xFF; /* background pattern generator address register */
    v->vdpRegisters[5] = 0xFF; /* sprite attribute table base address register */
    v->vdpRegisters[6] = 0xFB; /* sprite pattern generator base address register */
    v->vdpRegisters[7] = 0; /* overscan/backdrop colour register */
    v->vdpRegisters[8] = 0; /* background X scroll register */
    v->vdpRegisters[9] = 0; /* background Y scroll register */
    v->vdpRegisters[10] = 0xFF; /* line counter register */
    v->addressRegister = 0;
    v->codeRegister = 0;

    /* setup VDP status byte */
    v->vdpStatus = 0;
    
    /* setup line interrupt flag and counter */
    v->lineInterruptFlag = false;
    v->lineInterruptCounter = v->vdpRegisters[10];

    /* setup counters and line number, plus line mode */
    v->vCounter = 0;
    v->hCounter = 0;
    v->lineNumber = 0;
    v->lines = mode192;
    
    /* setup second control byte */
    v->secondControlByte = false;

    /* setup data port buffer */
    v->dataPortBuffer = 0;

    /* set cycle count */
    v->z80Cycles = 0;
    
    /* set vertical scroll register and associated values */
    v->tempVerticalScrollRegister = v->vdpRegisters[9];
    v->tempVerticalScrollChange = false;

    /* set whether or not we are in Game Gear mode, and setup latched data byte */
    v->gameGearMode = ggMode;
    v->latchedDataByte = 0;
    
    /* setup frame pixel buffer and scanline */
    v->frame = wholePointer;
    memset((void *)v->frame, 0, 184320);
    wholePointer += 184320;
    v->scanline = (emuint *)wholePointer;
    memset((void *)v->scanline, 0, sizeof(emuint) * 256);
    wholePointer += sizeof(emuint) * 256;

    /* setup the sprite buffer */
    v->sprites = (Sprite *)wholePointer;
    memset((void *)v->sprites, 0, sizeof(Sprite) * 8);
    wholePointer += sizeof(Sprite) * 8;
    
    /* setup TV mode */
    if (isPal)
        v->type = PAL;
    else
        v->type = NTSC;
    
    /* setup the frame mutex */
    if ((v->frameMutex = SDL_CreateMutex()) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "vdp.c", "Couldn't create SDL mutex for frame buffer access: %s\n", SDL_GetError());
        destroyVDP(v);
        return NULL;
    }
    
    /* store reference to source rectangle */
    v->sourceRect = sourceRect;

    /* set state if present */
    if (vdpState != NULL) {
        /* set state of simple values */
        emuint marker = 7;
        v->vdpStatus = vdpState[marker++];
        v->addressRegister = (vdpState[marker + 1] << 8) | vdpState[marker];
        marker += 2;
        v->codeRegister = vdpState[marker++];
        if (vdpState[marker++] == 1)
            v->secondControlByte = true;
        else
            v->secondControlByte = false;
        v->lineNumber = (vdpState[marker + 1] << 8) | vdpState[marker];
        marker += 2;
        v->vCounter = vdpState[marker++];
        v->hCounter = vdpState[marker++];
        v->lines = vdpState[marker++];
        v->dataPortBuffer = vdpState[marker++];
        v->z80Cycles = (vdpState[marker + 3] << 24) | (vdpState[marker + 2] << 16) | (vdpState[marker + 1] << 8) | vdpState[marker];
        marker += 4;
        v->latchedDataByte = vdpState[marker++];
        v->lineInterruptFlag = vdpState[marker++];
        v->lineInterruptCounter = (vdpState[marker + 1] << 8) | vdpState[marker];
        marker += 2;
        v->tempVerticalScrollRegister = vdpState[marker++];
        if (vdpState[marker++] == 1)
            v->tempVerticalScrollChange = true;
        else
            v->tempVerticalScrollChange = false;
        v->sourceRect->w = vdpState[marker++];
        v->sourceRect->h = vdpState[marker++];
        v->sourceRect->x = vdpState[marker++];
        v->sourceRect->y = vdpState[marker++];

        /* set state of vRAM, cRAM and VDP registers */
        emubyte *tempPointer = vdpState + marker;
        memcpy((void *)v->vRam, (void *)tempPointer, 16384);
        tempPointer += 16384;
        memcpy((void *)v->cRam, (void *)tempPointer, 64);
        tempPointer += 64;
        memcpy((void *)v->vdpRegisters, (void *)tempPointer, 16);
    }

    /* return VDP object */
    return v;
}

/* this function destroys the specified VDP object */
void destroyVDP(VDP v)
{
    /* destroy frame mutex */
    if (v->frameMutex != NULL)
        SDL_DestroyMutex(v->frameMutex);
}

/* this function executes the VDP for the specified number of VDP cycles - it is
   currently setup to do line based rendering due to its computational efficiency,
   and will return true when it is time to update the frame */
emubool vdp_executeCycles(VDP v, emuint cycles)
{
    /* define variables */
    emubool updateFrame = false;
    
    /* add cycles to cycle count */
    v->z80Cycles += cycles;

    /* if cycle count same as or higher than 228 Z80 cycles, begin scanline render */
    if (v->z80Cycles >= 228) {
        /* reset cycle count */
        v->z80Cycles -= 228;

        /* check if we are in the active display period */
        if (isActiveDisplayPeriod(v)) {
            /* check if mode 4 is enabled and proceed to render sprites and background if so */
            if ((v->vdpRegisters[0] & 0x04) == 0x04) {
                renderSpritesMode4(v);
                renderBackgroundMode4(v);
                vdp_handleFrame(v, 1, v->lineNumber, v->scanline, NULL);
                
                /* clear scanline buffer and priority */
                memset((void *)v->scanline, 0, sizeof(emuint) * 256);
            }
        }

        /* scan for sprites on next line */
        scanForSpritesMode4(v);

        /* check if any value was written to X-scroll temp register and store it to the real one */
        if (v->vdpRegisters[14]) {
            v->vdpRegisters[8] = v->vdpRegisters[15];
            v->vdpRegisters[14] = 0;
        }
        
        /* deal with line numbers, vCounter and interrupts */
        updateFrame = handleCountersAndInterrupts(v);
    }

    return updateFrame;
}

/* this function updates line and VCounter values, triggers interrupts if needed
   and returns true when a frame update is required */
static emubool handleCountersAndInterrupts(VDP v)
{
    /* define variables */
    emubool updateFrame = false;

    /* increase line number and wrap if necessary */
    ++v->lineNumber;
    switch (v->type) {
        case NTSC:
            switch (v->lineNumber) {
                case 262: v->lineNumber = 0; break;
            } break;
        case PAL:
            switch (v->lineNumber) {
                case 313: v->lineNumber = 0; break;
            } break;
        default: break;
    }

    /* deal with vCounter and decimal lineNumber variable adjustments */
    switch (v->type) {
        case PAL: v->vCounter = palVCounterValues[v->lines][v->lineNumber]; break;
        case NTSC: v->vCounter = ntscVCounterValues[v->lines][v->lineNumber]; break;
        default: break;
    }
    
    /* see if frame interrupt flag needs setting, and frame updating */
    switch (v->lines) {
        case mode192:
            switch (v->lineNumber) {
                case 192: setFrameInterruptFlag(v);
                          updateFrame = true;
                          if (v->tempVerticalScrollChange) {
                              v->vdpRegisters[9] = v->tempVerticalScrollRegister;
                              v->tempVerticalScrollChange = false;
                          }
                          break;
            } break;
        case mode224:
            switch (v->lineNumber) {
                case 224: setFrameInterruptFlag(v);
                          updateFrame = true;
                          if (v->tempVerticalScrollChange) {
                              v->vdpRegisters[9] = v->tempVerticalScrollRegister;
                              v->tempVerticalScrollChange = false;
                          }
                          break;
            } break;
        case mode240:
            switch (v->lineNumber) {
                case 240: setFrameInterruptFlag(v);
                          updateFrame = true;
                          if (v->tempVerticalScrollChange) {
                              v->vdpRegisters[9] = v->tempVerticalScrollRegister;
                              v->tempVerticalScrollChange = false;
                          }
                          break;
            } break;
        default: break;
    }
    
    /* see if line interrupt flag needs setting */
    emubyte tempCounterVal = v->lineInterruptCounter;
    switch (v->lines) {
        case mode192: if (v->lineNumber <= 192) {
                          --v->lineInterruptCounter;
                          if (v->lineInterruptCounter > tempCounterVal) {
                              setLineInterruptFlag(v);
                              v->lineInterruptCounter = v->vdpRegisters[10];
                          }
                      } else {
                          v->lineInterruptCounter = v->vdpRegisters[10];
                      } break;
        case mode224: if (v->lineNumber <= 224) {
                          --v->lineInterruptCounter;
                          if (v->lineInterruptCounter > tempCounterVal) {
                              setLineInterruptFlag(v);
                              v->lineInterruptCounter = v->vdpRegisters[10];
                          }
                      } else {
                          v->lineInterruptCounter = v->vdpRegisters[10];
                      } break;
        case mode240: if (v->lineNumber <= 240) {
                          --v->lineInterruptCounter;
                          if (v->lineInterruptCounter > tempCounterVal) {
                              setLineInterruptFlag(v);
                              v->lineInterruptCounter = v->vdpRegisters[10];
                          }
                      } else {
                          v->lineInterruptCounter = v->vdpRegisters[10];
                      } break;
        default: break;
    }
    
    /* check for change in height mode of VDP and act accordingly */
    lineMode tempLines = v->lines;
    switch (v->vdpRegisters[0] & 0x04) { /* check for mode 4 */
        case 0x04:
            switch (v->vdpRegisters[0] & 0x02) { /* check for M2 flag */
                case 0: tempLines = mode192; break; /* 192-line mode */
                case 0x02:
                    switch ((v->vdpRegisters[1] & 0x08) | (v->vdpRegisters[1] & 0x10)) {
                        case 0x18: tempLines = mode192; break; /* M1 and M3 both set */
                        case 0x10: tempLines = mode224; break; /* M1 set */
                        case 0x08: tempLines = mode240; break; /* M3 set */
                        case 0: tempLines = mode192; break; /* neither M1 or M3 set */
                    } break;
            } break;
        case 0: break;
    }
    
    /* check if we are now at a different line height mode */
    if (v->lines != tempLines) {
        /* set line height mode and trigger resolution change */
        v->lines = tempLines;
        /* PLACE RESOLUTION CHANGE CALL HERE */
        if (!v->gameGearMode) {
            switch (v->lines) {
                case mode192: v->sourceRect->h = 192; break;
                case mode224: v->sourceRect->h = 224; break;
                case mode240: v->sourceRect->h = 240; break;
            }
        } else {
            switch (v->lines) {
                case mode192: v->sourceRect->y = 24; break;
                case mode224: v->sourceRect->y = 40; break;
                case mode240: v->sourceRect->y = 48; break;
            }
        }
    }
    
    return updateFrame;
}

/* this function returns the address register value */
static emuint returnAddressRegister(VDP v)
{
    return v->addressRegister & 0x3FFF;
}

/* this function writes the address register value */
static void writeAddressRegister(VDP v, emuint a)
{
    v->addressRegister = a & 0x3FFF;
}

/* this function increments the address register value */
static void incrementAddressRegister(VDP v)
{
    writeAddressRegister(v, 0x3FFF & (returnAddressRegister(v) + 1));
}

/* this function returns the code register value */
static emubyte returnCodeRegister(VDP v)
{
    return v->codeRegister & 0x03;
}

/* this function writes the code register value */
static void writeCodeRegister(VDP v, emubyte c)
{
    v->codeRegister = c & 0x03;
}

/* this function handles writes to the VDP control port - it behaves differently
   depending on if the write is the first or second byte of the control word */
void vdp_controlWrite(VDP v, emubyte b)
{
    if (v->secondControlByte) {
        writeAddressRegister(v, (b << 8) | (returnAddressRegister(v) & 0xFF));
        writeCodeRegister(v, (b >> 6));
        switch (returnCodeRegister(v)) {
            case 0: v->dataPortBuffer = v->vRam[returnAddressRegister(v)];
                    incrementAddressRegister(v);
                    break;
            case 2: switch (returnAddressRegister(v) >> 8) {
                        case 8: v->vdpRegisters[15] =
                                    0xFF & returnAddressRegister(v);
                                v->vdpRegisters[14] = 1;
                                break;
                        case 9: if (isActiveDisplayPeriod(v)) {
                                    v->tempVerticalScrollRegister =
                                        0xFF & returnAddressRegister(v);
                                    v->tempVerticalScrollChange = true;
                                } else { 
                                    v->vdpRegisters[(returnAddressRegister(v) >> 8) & 0xF] =
                                        0xFF & returnAddressRegister(v);
                                } break;
                        default: v->vdpRegisters[(returnAddressRegister(v) >> 8) & 0xF] =
                                     0xFF & returnAddressRegister(v); break;
                    } break;
            default: break;
        }
        v->secondControlByte = false;
    } else {
        writeAddressRegister(v, (returnAddressRegister(v) & 0x3F00) | (b & 0xFF));
        v->secondControlByte = true;
    }
}

/* this function handles reads from the VDP control port */
emubyte vdp_controlRead(VDP v)
{
    /* define temp status variable */
    emubyte tempStatus = v->vdpStatus;

    /* clear flags and reset 1st/2nd control byte flag */
    v->vdpStatus = 0;
    v->secondControlByte = false;
    
    /* reset line interrupt flag */
    v->lineInterruptFlag = false;

    /* return status byte */
    return tempStatus;
}

/* this function handles writes to the VDP data port */
void vdp_dataWrite(VDP v, emubyte b)
{
    /* store write in data port buffer */
    v->dataPortBuffer = b;

    /* write to either cRam or vRam */
    switch (returnCodeRegister(v)) {
        case 3: if (v->gameGearMode) {
                    if ((returnAddressRegister(v) & 0x3F) % 2 == 0) { /* write to latched byte */
                        v->latchedDataByte = b;                       /* on even cRam addresses */
                    } else {
                        /* write supplied byte to current cRam address, then
                           write latched byte to current cRam address - 1 */
                        v->cRam[returnAddressRegister(v) & 0x3F] = b;
                        v->cRam[(returnAddressRegister(v) - 1) & 0x3F] = v->latchedDataByte;
                    }
                } else {
                    v->cRam[returnAddressRegister(v) & 0x1F] = b;
                } break;
        default: v->vRam[returnAddressRegister(v)] = b; break;
    }

    /* increment the address register and reset 1st/2nd control byte flag */
    incrementAddressRegister(v);
    v->secondControlByte = false;
}

/* this function handles reads from the VDP data port */
emubyte vdp_dataRead(VDP v)
{
    /* define temp variable to store data port buffer */
    emubyte tempBuffer = v->dataPortBuffer;

    /* read new byte into data port buffer */
    v->dataPortBuffer = v->vRam[returnAddressRegister(v)];
    incrementAddressRegister(v);

    /* reset 1st/2nd control byte flag and return previous contents of data port buffer */
    v->secondControlByte = false;
    return tempBuffer;
}

/* this function returns the value of the VDP's vCounter */
emubyte vdp_returnVCounter(VDP v)
{
    return v->vCounter;
}

/* this function returns the value of the VDP's hCounter */
emubyte vdp_returnHCounter(VDP v)
{
    return v->hCounter;
}

/* lets the console check if the VDP is currently asserting the IRQ line */
emubyte vdp_isInterruptAsserted(VDP v)
{
    /* define variables */
    emubyte returnVal = 0;

    /* check for frame interrupt first */
    switch (v->vdpStatus & 0x80) {
        case 0x80:
            switch (v->vdpRegisters[1] & 0x20) {
                case 0x20: returnVal |= 0xF0; break;
            } break;
    }
    
    /* check for line interrupt next */
    switch (v->lineInterruptFlag) {
        case 1:
            switch (v->vdpRegisters[0] & 0x10) {
                case 0x10: returnVal |= 0x0F; break;
            } break;
    }
    
    /* assert interrupt if needed */
    return returnVal;
}

/* this function triggers a frame interrupt */
static void setFrameInterruptFlag(VDP v)
{
    /* set bit 7 of status byte */
    v->vdpStatus |= 0x80;

    /* check for Action Replay codes and write to relevant addresses if enabled */
    if (arCheatArray.enabled) {
        for (emuint i = 0; i < arCheatArray.cheatCount; ++i) {
            console_memWrite(v->ms, arCheatArray.cheats[i].address, arCheatArray.cheats[i].value);
        }
    }
}

static void setLineInterruptFlag(VDP v)
{
    /* set line interrupt flag */
    v->lineInterruptFlag = 1;
}

/* this function is intended for use in the rendering routines - it determines if we are
   in the active display period */
static emubool isActiveDisplayPeriod(VDP v)
{
    /* define return value */
    emubool isActive = false;
    
    switch (v->lines) {
        case mode192: isActive = (v->lineNumber < 192); break;
        case mode224: isActive = (v->lineNumber < 224); break;
        case mode240: isActive = (v->lineNumber < 240); break;
        default: break;
    }
    
    return isActive;
}

/* this function is intended for use in the rendering routines - it determines if we are
   in the active display period */
static emubool lineIsInActiveDisplayPeriod(VDP v, signed_emuint line)
{
    /* define return value */
    emubool isActive = false;

    switch (v->lines) {
        case mode192: isActive = (line < 192); break;
        case mode224: isActive = (line < 224); break;
        case mode240: isActive = (line < 240); break;
        default: break;
    }

    return isActive;
}

/* this function will render the sprites for a scanline according to mode 4 behaviour */
static void renderSpritesMode4(VDP v) {
    /* display sprites for this line, unless display is blanked */
    signed_emuint i, p, j;
    if ((v->vdpRegisters[1] & 0x40) == 0x40) {

        /* iterate through sprite buffer and display sprites */
        emuint pixelLine[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        for (i = 0; i <= 7; ++i) {
            /* check if a sprite is present at this index */
            if (v->sprites[i].present != 0) {
                /* work out line we need */
                emubyte neededLine = v->lineNumber - v->sprites[i].y;
                if (neededLine > 15)
                    neededLine /= 2;

                /* create temporary pointer referencing start of pattern line */
                emubyte *patternLine = v->vRam + (v->sprites[i].patternIndex * 32);
                patternLine += (neededLine * 4);

                /* now compose bitfields for each pixel properly, retrieve the colour
                   value, and place it into the buffer */
                emubyte bit0 = patternLine[0];
                emubyte bit1 = patternLine[1];
                emubyte bit2 = patternLine[2];
                emubyte bit3 = patternLine[3];
                for (p = 0; p < 8; ++p) {

                    /* compose colour palette index */
                    emubool transparentPixel = false;
                    pixelLine[p] = (((bit3 >> (7 - p)) & 0x01) << 3) |
                                   (((bit2 >> (7 - p)) & 0x01) << 2) |
                                   (((bit1 >> (7 - p)) & 0x01) << 1) |
                                   ((bit0 >> (7 - p)) & 0x01);
                    if (pixelLine[p] == 0)
                        transparentPixel = true;

                    /* pull colour value from colour palette and convert it for BGR24 display */
                    if (v->gameGearMode) {
                        pixelLine[p] =
                                (v->cRam[32 + (pixelLine[p] * 2) + 1] << 8) |
                                v->cRam[32 + (pixelLine[p] * 2)];
                        pixelLine[p] = 0xFF000000 |
                                       (((pixelLine[p] & 0xF) * 17) << 16) |
                                       ((((pixelLine[p] >> 4) & 0xF) * 17) << 8) |
                                       (((pixelLine[p] >> 8) & 0xF) * 17);
                    } else {
                        pixelLine[p] = v->cRam[16 + pixelLine[p]];
                        pixelLine[p] = 0xFF000000 |
                                       (((pixelLine[p] & 0x3) * 85) << 16) |
                                       ((((pixelLine[p] >> 2) & 0x3) * 85) << 8) |
                                       (((pixelLine[p] >> 4) & 0x3) * 85);
                    }
                    if (transparentPixel)
                        pixelLine[p] = 0x01000000 | (pixelLine[p] & 0xFFFFFF);

                }

                /* now that we have this sprite line, we can add it into the scanline */
                for (j = v->sprites[i].x, p = 0;
                     j < v->sprites[i].x + v->sprites[i].width; ++j, ++p) {
                    if (j < 0 || j > 255) { ;
                    } else {
                        emubyte tempP = p;
                        if (v->sprites[i].width == 16)
                            tempP /= 2;

                        if ((v->scanline[j] & 0xFF000000) == 0) {
                            v->scanline[j] = pixelLine[p];
                        } else {
                            if ((v->scanline[j] & 0xFF000000) == 0x01000000) {
                                v->scanline[j] = pixelLine[p];
                            } else {
                                if ((pixelLine[p] & 0xFF000000) == 0xFF000000) {
                                    v->vdpStatus |= 0x20;
                                }
                            }
                        }
                    }
                }

                /* mark sprite as processed */
                v->sprites[i].present = 0;
            }
        }

    }
}

/* this function will scan for sprites to display on the next line */
static void scanForSpritesMode4(VDP v) {

    /* find sprite attribute table address and assign a temporary pointer to reference
       the table in memory */
    emuint spriteAttributeTableAddress = (v->vdpRegisters[5] & 0x7E) << 7;
    emubyte *spriteAttributeTable = v->vRam + spriteAttributeTableAddress;
    
    /* check registers to enable certain features */
    emubool eightBySixteen = false;
    emubool doubleSize = false;
    emubool leftShift = false;
    eightBySixteen = (emubool)((v->vdpRegisters[1] & 0x02) >> 1);
    doubleSize = (emubool)(v->vdpRegisters[1] & 0x01);
    leftShift = (emubool)((v->vdpRegisters[0] & 0x08) >> 3);
    
    /* calculate sprite width and height */
    emubyte spriteHeight = 8;
    emubyte spriteWidth = 8;
    if (eightBySixteen)
        spriteHeight *= 2;
    if (doubleSize) {
        spriteHeight *= 2;
        spriteWidth *= 2;
    }
    
    /* calculate next line */
    signed_emuint nextLine = v->lineNumber + 1;
    switch (v->type) {
        case PAL: switch (nextLine) {
                      case 313: nextLine = 0; break;
                  } break;
        case NTSC: switch (nextLine) {
                       case 262: nextLine = 0; break;
                   } break;
        default: break;
    }
    
    /* check sprites for next line by iterating through the table */
    signed_emuint i, p = 0;
    for (i = 0; i < 64; ++i) {

        /* check coordinate and adjust as necessary */
        if (spriteAttributeTable[i] == 0xD0 && v->lines == mode192)
            break;
        signed_emuint tempY = spriteAttributeTable[i] + 1;
        if (tempY > 239 && tempY < 256)
            tempY -= 256;
        
        /* check if this sprite will be displayed on the next scanline */
        if (nextLine >= tempY && nextLine < tempY + spriteHeight) {
            if (p > 7) {
                v->vdpStatus |= 0x40;
                break;
            } else if (lineIsInActiveDisplayPeriod(v, nextLine)) {
                v->sprites[p].y = tempY;
                v->sprites[p].x = spriteAttributeTable[128 + (i * 2)] - (((signed_emuint)leftShift) * 8);
                v->sprites[p].width = spriteWidth;
                v->sprites[p].height = spriteHeight;
                v->sprites[p].patternIndex =
                    ((v->vdpRegisters[6] & 0x04) << 6) | spriteAttributeTable[128 + (i * 2) + 1];
                if (eightBySixteen)
                    v->sprites[p].patternIndex &= 0x1FE;
                v->sprites[p].present = 1;
                ++p;
            }
        }
        
    }
}

/* this function will render the background tiles for a scanline according to mode 4 behaviour */
static void renderBackgroundMode4(VDP v)
{
    /* check if display is blanked and return if so */
    if ((v->vdpRegisters[1] & 0x40) == 0)
        return;
    
    /* now determine the name table address */
    emuint nameTableAddress = 0;
    switch (v->lines) {
        case mode192:
            nameTableAddress = (v->vdpRegisters[2] & 0x0E) << 10;
            break;
        default:
            switch ((v->vdpRegisters[2] >> 2) & 0x03) {
                case 0: nameTableAddress = 0x0700; break;
                case 1: nameTableAddress = 0x1700; break;
                case 2: nameTableAddress = 0x2700; break;
                case 3: nameTableAddress = 0x3700; break;
            } break;
    }
    
    /* form temporary pointer with which to address name table */
    emubyte *nameTable = v->vRam + nameTableAddress;
    
    /* retrieve colour 0 from palette 1 and store in ARGB form */
    emuint zeroTemp = 0, zeroColour = 0xFF000000;
    if (v->gameGearMode) {
        zeroTemp = v->cRam[0];
        zeroTemp |= (v->cRam[1] << 8);
        zeroColour |= ((zeroTemp & 0xF) * 17) << 16;
        zeroColour |= (((zeroTemp >> 4) & 0xF) * 17) << 8;
        zeroColour |= ((zeroTemp >> 8) & 0xF) * 17;
    } else {
        zeroTemp = v->cRam[0];
        zeroColour |= ((zeroTemp & 0x3) * 85) << 16;
        zeroColour |= (((zeroTemp >> 2) & 0x3) * 85) << 8;
        zeroColour |= ((zeroTemp >> 4) & 0x3) * 85;
    }
    
    /* retrieve overscan colour index from register 7 and retrieve overscan
       colour from sprite palette, storing in ARGB form */
    emuint overscanTemp = 0, overscanColour = 0xFF000000;
    if (v->gameGearMode) {
        overscanTemp = v->cRam[32 + ((v->vdpRegisters[7] & 0xF) * 2)];
        overscanTemp |= (v->cRam[32 + ((v->vdpRegisters[7] & 0xF) * 2) + 1] << 8);
        overscanColour |= ((overscanTemp & 0xF) * 17) << 16;
        overscanColour |= (((overscanTemp >> 4) & 0xF) * 17) << 8;
        overscanColour |= ((overscanTemp >> 8) & 0xF) * 17;
    } else {
        overscanTemp = v->cRam[16 + (v->vdpRegisters[7] & 0xF)];
        overscanColour |= ((overscanTemp & 0x3) * 85) << 16;
        overscanColour |= (((overscanTemp >> 2) & 0x3) * 85) << 8;
        overscanColour |= ((overscanTemp >> 4) & 0x3) * 85;
    }

    /* retrieve starting column and horizontal fine scroll values */
    emubyte startingColumn = 32 - ((v->vdpRegisters[8] & 0xF8) >> 3);
    emuint horizontalFineScroll = v->vdpRegisters[8] & 0x07;
    
    /* check if horizontal scrolling if disabled for this scanline */
    if ((v->vdpRegisters[0] & 0x40) == 0x40 && v->lineNumber <= 15) {
        startingColumn = 32;
        horizontalFineScroll = 0;
    }

    /* mask starting column value to obtain actual address */
    startingColumn &= 0x1F;
    
    /* retrieve starting row and vertical fine scroll values */
    emubyte startingRow = (v->vdpRegisters[9] & 0xF8) >> 3;
    emubyte verticalFineScroll = v->vdpRegisters[9] & 0x07;
    
    /* fill left column with colour 0 obtained above, up to number of pixels
       specified by horizontal fine scroll value, checking for sprite collisions */
    emuint i;
    for (i = 0; i < horizontalFineScroll; ++i) {
        if ((v->scanline[i] & 0xFF000000) != 0xFF000000) {
            v->scanline[i] = zeroColour;
        }
    }
    
    /* iterate through each column */
    for (i = 0; i < 32; ++i) {
        /* check if vertical scroll is disabled for this column */
        if ((v->vdpRegisters[0] & 0x80) == 0x80 && i >= 24) {
            startingRow = 0;
            verticalFineScroll = 0;
        }
        
        /* calculate row and line we actually need */
        emuint tempRowAndLine = (startingRow << 3) | (verticalFineScroll);
        if (v->lines == mode192 && tempRowAndLine > 223) {
            tempRowAndLine -= 224;
        }
        tempRowAndLine = tempRowAndLine + v->lineNumber;
        switch (v->lines) {
            case mode192: if (tempRowAndLine > 223) {
                              tempRowAndLine -= 224;
                          } break;
            default: tempRowAndLine &= 0xFF;
        }
        
        /* convert the above back into row and line we need */
        emubyte neededRow = (tempRowAndLine & 0xF8) >> 3;
        emubyte neededLine = tempRowAndLine & 0x07;
        
        /* retrieve pattern from name table at required address */
        emuint pattern = nameTable[(neededRow * 64) + (startingColumn * 2)];
        pattern |= (nameTable[(neededRow * 64) + (startingColumn * 2) + 1] << 8);
        
        /* determine flags from pattern */
        emubool priorityFlag = false, verticalFlipFlag = false, horizontalFlipFlag = false;
        if ((pattern & 0x1000) == 0x1000)
            priorityFlag = true;
        if ((pattern & 0x400) == 0x400)
            verticalFlipFlag = true;
        if ((pattern & 0x200) == 0x200)
            horizontalFlipFlag = true;
        
        /* determine palette to use */
        emubyte paletteSelect = (pattern >> 11) & 0x01;
        
        /* reference pattern we need with temporary pointer */
        emubyte *patternStart = v->vRam + ((pattern & 0x1FF) * 32);
        
        /* check for vertical flip */
        if (verticalFlipFlag)
            neededLine = 7 - neededLine;
        
        /* modify pointer */
        patternStart += (neededLine * 4);
        
        /* compose colour indexes and colours */
        emubyte p;
        emubyte bit0 = 0, bit1 = 0, bit2 = 0, bit3 = 0;
        emuint colourArray[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
        bit0 = patternStart[0];
        bit1 = patternStart[1];
        bit2 = patternStart[2];
        bit3 = patternStart[3];
        for (p = 0; p < 8; ++p) {
            emubool transparentPixel = false;
            colourArray[p] = (((bit3 >> (7 - p)) << 3) & 0x08) |
                             (((bit2 >> (7 - p)) << 2) & 0x04) |
                             (((bit1 >> (7 - p)) << 1) & 0x02) |
                             ((bit0 >> (7 - p)) & 0x01);
            
            /* check if pixel is supposed to be transparent */
            if (colourArray[p] == 0)
                transparentPixel = true;
            
            /* now compose the pixel in ARGB form */
            if (v->gameGearMode) {
                colourArray[p] = (v->cRam[(32 * paletteSelect) + (colourArray[p] * 2) + 1] << 8) |
                                 v->cRam[(32 * paletteSelect) + (colourArray[p] * 2)];
                colourArray[p] = 0xFF000000 |
                                 (((colourArray[p] & 0xF) * 17) << 16) |
                                 ((((colourArray[p] >> 4) & 0xF) * 17) << 8) |
                                 (((colourArray[p] >> 8) & 0xF) * 17);
            } else {
                colourArray[p] = v->cRam[(16 * paletteSelect) + colourArray[p]];
                colourArray[p] = 0xFF000000 |
                                 (((colourArray[p] & 0x3) * 85) << 16) |
                                 ((((colourArray[p] >> 2) & 0x3) * 85) << 8) |
                                 (((colourArray[p] >> 4) & 0x3) * 85);
            }
            
            if (transparentPixel)
                colourArray[p] = 0x01000000 | (colourArray[p] & 0xFFFFFF);
        }
        
        /* check for horizontal flip */
        if (horizontalFlipFlag) {
            emuint tempStore = 0;
            for (p = 0; p < 4; ++p) {
                tempStore = colourArray[p];
                colourArray[p] = colourArray[7 - p];
                colourArray[7 - p] = tempStore;
            }
        }
        
        /* incorporate pixels into scanline */
        for (p = 0; p < 8; ++p) {
            /* don't write beyond edge of scanline */
            if (horizontalFineScroll > 255)
                break;
            
            /* add pixel to scanline, taking priority into account */
            if ((v->scanline[horizontalFineScroll] & 0xFF000000) != 0xFF000000) {
                v->scanline[horizontalFineScroll] = colourArray[p];
            } else {
                if (priorityFlag && ((colourArray[p] & 0xFF000000) == 0xFF000000)) {
                    v->scanline[horizontalFineScroll] = colourArray[p];
                }
            }
            
            /* increment horizontal fine scroll */
            ++horizontalFineScroll;
        }
        
        /* now increment starting column value */
        startingColumn = (startingColumn + 1) & 0x1F;
    }
    
    /* check if masking of column 0 is enabled, and modify scanline if so */
    if ((v->vdpRegisters[0] & 0x20) == 0x20) {
        for (i = 0; i < 8; ++i) {
            v->scanline[i] = overscanColour;
        }
    }
}

/* this function allows the calling thread to either copy bytes from scanline to the specified
   row, return the frame as a pointer reference, or clear the frame buffer completely */
emubool vdp_handleFrame(VDP v, emubyte action, emubyte row, emuint *scanline, void *external)
{
    /* define variables */
    emuint i;
    emubool success = true;
    emubyte *temp;
    
    /* lock frame mutex */
    if (SDL_LockMutex(v->frameMutex) == 0) {
        /* act upon specified action */
        switch (action) {
            case 0: memset((void *)v->frame, 0, 256 * 240 * 3); break; /* wipe frame buffer */
            case 1: temp = v->frame; /* copy scanline to right place in buffer */
                    temp += (256 * 3 * row);
                    for (i = 0; i < 256; ++i) {
                        temp[i * 3] = v->scanline[i] & 0xFF;
                        temp[(i * 3) + 1] = (v->scanline[i] >> 8) & 0xFF;
                        temp[(i * 3) + 2] = (v->scanline[i] >> 16) & 0xFF;
                    }
                    break;
            case 2: /* copy frame buffer to external buffer */
                    memcpy(external, (void *)v->frame, 256 * 240 * 3); break;
        }
        
        /* now unlock mutex as we have finished with the frame buffer */
        if (SDL_UnlockMutex(v->frameMutex) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, "vdp.c", "Couldn't unlock frame mutex: %s\n", SDL_GetError());
            success = false;
        }
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "vdp.c", "Couldn't lock frame mutex: %s\n", SDL_GetError());
        success = false;
    }
    
    /* return whether or not the function succeeded */
    return success;
}

/* this function calculates and stores the current hCounter value */
void vdp_storeHCounterValue(VDP v)
{
    /* this stores the correct value in the hCounter variable depending on
       the number of Z80 cycles so far */
    v->hCounter = hCounterValues[v->z80Cycles];
}

/* this function returns the current cycle count of the VDP - it is useful
   for triggering interrupts at the correct HCount value */
emuint vdp_getCycles(VDP v)
{
    return v->z80Cycles;
}

/* this function returns an emubyte pointer to the state of the VDP */
emubyte *vdp_saveState(VDP v)
{
    /* define variables */
    emuint vdpSize = 16489 + 7; /* add extra 7 bytes for header and size */
    emuint marker = 0;

    /* allocate the memory */
    emubyte *vdpState = malloc(sizeof(emubyte) * vdpSize);
    if (vdpState == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "vdp.c", "Couldn't allocate memory for VDP state buffer...\n");
        return NULL;
    }

    /* fill first 7 bytes with header and state size */
    vdpState[marker++] = vdpSize & 0xFF;
    vdpState[marker++] = (vdpSize >> 8) & 0xFF;
    vdpState[marker++] = (vdpSize >> 16) & 0xFF;
    vdpState[marker++] = (vdpSize >> 24) & 0xFF;
    vdpState[marker++] = 'V';
    vdpState[marker++] = 'D';
    vdpState[marker++] = 'P';

    /* copy simple values to state buffer */
    vdpState[marker++] = v->vdpStatus;
    vdpState[marker++] = v->addressRegister & 0xFF;
    vdpState[marker++] = (v->addressRegister >> 8) & 0xFF;
    vdpState[marker++] = v->codeRegister;
    if (v->secondControlByte)
        vdpState[marker++] = 1;
    else
        vdpState[marker++] = 0;
    vdpState[marker++] = v->lineNumber & 0xFF;
    vdpState[marker++] = (v->lineNumber >> 8) & 0xFF;
    vdpState[marker++] = v->vCounter;
    vdpState[marker++] = v->hCounter;
    vdpState[marker++] = v->lines;
    vdpState[marker++] = v->dataPortBuffer;
    vdpState[marker++] = v->z80Cycles & 0xFF;
    vdpState[marker++] = (v->z80Cycles >> 8) & 0xFF;
    vdpState[marker++] = (v->z80Cycles >> 16) & 0xFF;
    vdpState[marker++] = (v->z80Cycles >> 24) & 0xFF;
    vdpState[marker++] = v->latchedDataByte;
    vdpState[marker++] = v->lineInterruptFlag;
    vdpState[marker++] = v->lineInterruptCounter & 0xFF;
    vdpState[marker++] = (v->lineInterruptCounter >> 8) & 0xFF;
    vdpState[marker++] = v->tempVerticalScrollRegister;
    if (v->tempVerticalScrollChange)
        vdpState[marker++] = 1;
    else
        vdpState[marker++] = 0;
    vdpState[marker++] = v->sourceRect->w;
    vdpState[marker++] = v->sourceRect->h;
    vdpState[marker++] = v->sourceRect->x;
    vdpState[marker++] = v->sourceRect->y;

    /* copy vRAM and cRAM to state buffer */
    emubyte *tempPointer = vdpState + marker;
    memcpy((void *)tempPointer, (void *)v->vRam, 16384);
    tempPointer += 16384;
    memcpy((void *)tempPointer, (void *)v->cRam, 64);
    tempPointer += 64;
    memcpy((void *)tempPointer, (void *)v->vdpRegisters, 16);

    /* return VDP state */
    return vdpState;
}

/* this allows us get the SDL source rect */
SDL_Rect *vdp_getSourceRect(VDP v)
{
    return v->sourceRect;
}

/* this function returns the number of bytes needed to create a VDP object */
emuint vdp_getMemoryUsage(void)
{
    return (sizeof(struct VDP) * sizeof(emubyte)) + /* cRam size */ (sizeof(emubyte) * 64) + /* vRam size */ (sizeof(emubyte) * 16384) +
    /* VDP registers size */ (sizeof(emubyte) * 16) + /* frame size */ (sizeof(emubyte) * 184320) + /* scanline size */ (sizeof(emuint) * 256) +
    /* Sprite buffer size */ (sizeof(Sprite) * 8);
}

/* this function returns the current line the VDP is on */
emuint vdp_getCurrentLine(VDP v)
{
    return v->lineNumber;
}
