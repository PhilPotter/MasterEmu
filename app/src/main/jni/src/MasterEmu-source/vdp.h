/* MasterEmu VDP header file
   copyright Phil Potter, 2019 */

#ifndef VDP_INCLUDE
#define VDP_INCLUDE
#include "../../SDL-release-2.24.0/include/SDL.h"
#include "datatypes.h"
#include "console.h"

/* define opaque pointer type for dealing with the VDP */
typedef struct VDP *VDP;

/* function declarations for public use */
VDP createVDP(Console ms, emubool ggMode, emubool isPal, SDL_Rect *sourceRect, emubyte *vdpState, emubyte *wholePointer); /* creates VDP object and returns a pointer to it */
void destroyVDP(VDP v); /* destroys specified VDP object */
void vdp_controlWrite(VDP v, emubyte b); /* writes to the VDP control port */
emubyte vdp_controlRead(VDP v); /* reads from the VDP control port */
void vdp_dataWrite(VDP v, emubyte b); /* writes to the VDP data port */
emubyte vdp_dataRead(VDP v); /* reads from the VDP data port */
emubool vdp_executeCycles(VDP v, emuint cycles); /* executes the VDP for the specified number of cycles */
emuint vdp_getCycles(VDP v); /* returns the currently stored cycle count of the VDP */
emubyte vdp_returnVCounter(VDP v); /* returns the value of the VDP's vCounter */
emubyte vdp_returnHCounter(VDP v); /* returns the value of the VDP's hCounter */
emubyte vdp_isInterruptAsserted(VDP v); /* returns whether or not the VDP is triggering an interrupt */
void vdp_storeHCounterValue(VDP v); /* calculate and stores hCounter value */
emubyte *vdp_saveState(VDP v); /* this returns a pointer to the state of the VDP */
SDL_Rect *vdp_getSourceRect(VDP v); /* this returns the source rect of the VDP */
emuint vdp_getMemoryUsage(void); /* this returns the number of bytes needed to create a VDP object */
emuint vdp_getCurrentLine(VDP v); /* this returns the current line the VDP is on */

/* this function allows handling of the frame buffer in a thread-safe way */
emubool vdp_handleFrame(VDP v, emubyte action, emubyte row, emuint *scanline, void *external);

#endif
