/* MasterEmu SN76489 sound chip header file
   copyright Phil Potter, 2022 */

#ifndef SN76489_INCLUDE
#define SN76489_INCLUDE
#include "datatypes.h"
#include "console.h"

/* define opaque pointer type for dealing with sound chip */
typedef struct SN76489 *SN76489;

/* function declarations for public use */
SN76489 createSN76489(Console ms, emubyte *soundchipState, emubool soundDisabled, emubool isGameGear, emubool isPal, emubyte *wholePointer, emuint audioId); /* creates SN76489 object and returns a pointer to it */
void destroySN76489(SN76489 s); /* destroys specified SN76489 object */
void soundchip_soundWrite(SN76489 s, emubyte b); /* writes data to SN76489 registers */
void soundchip_executeCycles(SN76489 s, emuint c); /* runs the sound chip for c cycles */
emubyte *soundchip_saveState(SN76489 s); /* this returns a pointer to the SN76489 object's state */
void soundchip_stopAudio(SN76489 s); /* this allows us to stop the audio from outside */
SN76489 soundchip_startAudio(SN76489 s); /* this allows us to start the audio from outside */
emuint soundchip_getMemoryUsage(void); /* this returns the number of bytes required by an SN76489 object */
emuint soundchip_getAudioDeviceID(SN76489 s); /* this allows us to destroy the chip and present the same sound channel to the new one */

#endif