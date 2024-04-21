/* MasterEmu cartridge header file
   copyright Phil Potter, 2024 */

#ifndef CARTRIDGE_INCLUDE
#define CARTRIDGE_INCLUDE
#include <stdio.h>
#include "datatypes.h"

/* define opaque pointer type for dealing with cartridge */
typedef struct Cartridge *Cartridge;

/* function declarations for public use */
Cartridge createCartridge(emubyte *romData, signed_emulong romSize, emubool isCodemasters, emubyte *cartState, emubool sRamOnly, emubyte *wholePointer); /* creates Cartridge object from FILE handle */
void destroyCartridge(Cartridge c); /* destroys specified Cartridge object */
emubool cart_isCodemasters(Cartridge c); /* returns if Codemasters mapper is enabled on the Cartridge object */
void cart_writeFFFC(Cartridge c, emubyte data); /* writes the specified byte to the fffc register of the Cartridge object */
void cart_writeFFFD(Cartridge c, emubyte data); /* writes slot 0 ROM banking value */
void cart_writeFFFE(Cartridge c, emubyte data); /* writes slot 1 ROM banking value */
void cart_writeFFFF(Cartridge c, emubyte data); /* writes slot 2 ROM banking value */
emubyte cart_readCartridge(Cartridge c, emuint address); /* retrieves the byte located at the specified address of the Cartridge object */
void cart_writeCartridge(Cartridge c, emuint address, emubyte data); /* writes the byte to the specified address of the Cartridge object */
emubyte *cart_saveState(Cartridge c); /* returns a pointer to the Cartridge object's state */
emuint cart_getMemoryUsage(void); /* returns the number of bytes needed by a Cartridge object */
emubool cart_isCartOverridingSystemRam(Cartridge c); /* returns whether or not this cartridge is currently mapping RAM into the address space
                                                        normally used by the main system */

#endif
