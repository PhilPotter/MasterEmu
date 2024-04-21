/* MasterEmu console header file
   copyright Phil Potter, 2022 */

#ifndef CONSOLE_INCLUDE
#define CONSOLE_INCLUDE
#include <stdio.h>
#include "../../SDL-release-2.30.2/include/SDL.h"
#include "datatypes.h"

/* define opaque pointer type for dealing with the Master System console */
typedef struct Console *Console;

/* function declarations for public use */
Console createConsole(emubyte *romData, signed_emulong romSize, emuint romChecksum, emubool isCodemasters, emubool isGameGear, emubool isPal, SDL_Rect *sourceRect, emubyte *saveState, emuint params, emubyte *wholePointer, emuint audioId); /* this sets up a full Master System console */
void destroyConsole(Console ms); /* this destroys the console object */
void console_ioWrite(Console ms, emuint address, emubyte data); /* this deals with Z80 IO port writes */
emubyte console_ioRead(Console ms, emuint address); /* this deals with Z80 IO port reads */
void console_memWrite(Console ms, emuint address, emubyte data); /* this deals with memory space writes */
emubyte console_memRead(Console ms, emuint address); /* this deals with memory space reads */
emubool console_checkNmi(Console ms); /* this lets the Z80 check for NMI triggers */
emubyte console_checkInterrupt(Console ms); /* this lets the Z80 check for maskable interrupts */
void console_interruptHandled(Console ms); /* this tells the signalling device that the Z80 has handled the interrupt */
typedef struct EmuBundle EmuBundle;
emuint console_executeInstruction(EmuBundle *eb); /* this executes the console for one instruction of Z80 time */
emubool console_handleFrame(Console ms, void *external); /* updates the SDL frame buffer from the VDP frame buffer */
emubyte console_handleTempDC(Console ms, emubyte action, emubyte value); /* handles port DC access */
emubool console_handleTempPauseStatus(Console ms, emubyte action, emubool value); /* handles pause access */
void console_tellVDPToStoreHCounterValue(Console ms); /* this function signals the internal VDP to calculate the current
                                                         hCounter and store it */
emubyte console_handleTempStart(Console ms, emubyte action, emubyte value); /* handles start access */
emuint console_getVDPCycles(Console ms); /* retrieves the number of Z80 cycles the VDP is at */
emubyte *console_saveState(Console ms); /* returns a pointer to the memory state of the entire console */
SDL_Rect *console_getSourceRect(Console ms); /* gets the source rect of the VDP */
emubyte console_readPSGReg(Console ms); /* returns contents of GG register at IO port 0x06 */
void console_stopAudio(Console ms); /* stops the SDL sound channel */
emuint console_getWholeMemoryUsage(void); /* reports memory usage for Console object and all sub-components */
emuint console_getMemoryUsage(void); /* reports memory usage for Console object only */
emuint console_getAudioDeviceID(Console ms); /* this function returns the current SDL AudioDeviceID from the sound chip */
emuint console_getCurrentLine(Console ms); /* this function returns the current line from the VDP */

#endif
