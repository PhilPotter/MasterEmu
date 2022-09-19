/* MasterEmu CRC32 implementation header file
   copyright Phil Potter, 2022 */

#ifndef CRC32_INCLUDE
#define CRC32_INCLUDE
#include "datatypes.h"

/* function declarations for public use */
emuint calculateCRC32(emubyte *p, emulong length); /* calculates a CRC-32 checksum for byte stream p of specified length */

#endif