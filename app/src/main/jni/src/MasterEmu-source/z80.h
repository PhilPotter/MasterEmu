/* MasterEmu Z80 header file
   copyright Phil Potter, 2019 */

#ifndef Z80_INCLUDE
#define Z80_INCLUDE
#include "datatypes.h"
#include "console.h"

/* define opaque pointer type for dealing with Z80 */
typedef struct Z80 *Z80;

/* function declarations for public use */
Z80 createZ80(Console ms, emubyte *z80State, emubyte *wholePointer); /* creates Z80 object */
void destroyZ80(Z80 z); /* destroys specified Z80 object */
emuint Z80_executeInstruction(Z80 z); /* executes a single instruction of the Z80 */
emubyte *Z80_saveState(Z80 z); /* returns a pointer to the state of the Z80 */
emuint Z80_getMemoryUsage(void); /* returns how many bytes a Z80 object requires */

#endif