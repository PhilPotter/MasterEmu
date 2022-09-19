/* MasterEmu controllers header file
   copyright Phil Potter, 2022 */

#ifndef CONTROLLERS_INCLUDE
#define CONTROLLERS_INCLUDE
#include <stdio.h>
#include "datatypes.h"
#include "console.h"

/* define opaque pointer type for dealing with controllers */
typedef struct Controllers *Controllers;

/* function declarations for public use */
Controllers createControllers(Console ms, emubool isGameGear, emubyte *controllersState, emubool isJapanese, emubyte *wholePointer); /* creates Controllers object */
void destroyControllers(Controllers ct); /* destroys specified Controllers object */
emubyte controllers_handle3F(Controllers ct, emubyte action, emubyte value); /* handles port 3F access */
emubyte controllers_handleDC(Controllers ct, emubyte action, emubyte value); /* handles port DC access */
emubyte controllers_handleDD(Controllers ct, emubyte action, emubyte value); /* handles port DD access */
emubool controllers_handlePauseStatus(Controllers ct, emubyte action, emubool value); /* handles pause access */
emubyte controllers_handleStart(Controllers ct, emubyte action, emubyte value); /* handles start access */
emubyte controllers_handleTempDC(Controllers ct, emubyte action, emubyte value); /* handles port DC access */
emubool controllers_handleTempPauseStatus(Controllers ct, emubyte action, emubool value); /* handles pause access */
emubyte controllers_handleTempStart(Controllers ct, emubyte action, emubyte value); /* handles start access */
void controllers_updateValues(Controllers ct); /* copies temp values to real values in thread-safe fashion */
emubyte *controllers_saveState(Controllers ct); /* returns state of Controllers object */
emuint controllers_getMemoryUsage(void); /* returns the number of bytes needed for a Controllers object */

#endif