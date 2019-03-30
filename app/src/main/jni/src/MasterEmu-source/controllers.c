/* MasterEmu controllers source code file
   copyright Phil Potter, 2019 */

#include <stdlib.h>
#include <android/log.h>
#include "../../SDL/include/SDL.h"
#include "console.h"
#include "controllers.h"

/* this struct models the internal state of both controllers */
struct Controllers {
    /* the following are the real values queried from the Console */
    emubyte port3F; /* this is for I/O port control */
    emubyte portDC; /* this is for ports A and B */
    emubyte portDD; /* this is for port B and miscellaneous functions */
    emubool pauseButton; /* this triggers an NMI on the Z80, as with
                            a real Sega Master System */
    emubool unpauseFlag; /* this helps not to trigger another NMI during the value swap */
    emubyte startButton; /* this is for the start button on a Game Gear */
    
    /* these are the temporary values that are set by SDL events between frames */
    emubyte tempPortDC;
    emubool tempPauseButton;
    emubyte tempStartButton;

    /* this references the main console */
    Console ms;
    
    /* this stores whether or not the console is a Game Gear */
    emubool isGameGear;

    /* this stores whether the console is in Japanese mode */
    emubool isJapanese;

    /* these prevent any thread from accessing the controller state
       at the same time as another thread */
    SDL_mutex *controllerDCMutex;
    SDL_mutex *pauseMutex;
    SDL_mutex *startMutex;
};

/* this function creates a new Controllers object and returns a pointer to it */
Controllers createControllers(Console ms, emubool isGameGear, emubyte *controllersState, emubool isJapanese, emubyte *wholePointer)
{
    /* first, we allocate memory for a Controllers struct */
    Controllers ct = (Controllers)wholePointer;

    /* set Japanese mode */
    ct->isJapanese = isJapanese;
    
    /* set SDL mutex pointers to NULL now */
    ct->controllerDCMutex = NULL;
    ct->pauseMutex = NULL;
    ct->startMutex = NULL;
    
    /* set console type */
    ct->isGameGear = isGameGear;

    /* inputs in DC and DD are initialised to 1 as they are set up
       for active-low functioning on the real hardware */
    ct->port3F = 0x0F;
    ct->portDC = 0xFF;
    ct->portDD = 0xFF;
    ct->pauseButton = false;
    ct->unpauseFlag = false;
    ct->startButton = 0xC0;
    
    /* set the temp variables up the same as above */
    ct->tempPortDC = 0xFF;
    ct->tempPauseButton = false;
    ct->tempStartButton = 0xC0;

    /* set console reference */
    ct->ms = ms;
    
    /* setup the controller mutexes */
    if ((ct->controllerDCMutex = SDL_CreateMutex()) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "controllers.c", "Couldn't create SDL mutex for controller port DC access: %s\n", SDL_GetError());
        destroyControllers(ct);
        return NULL;
    }
    if ((ct->pauseMutex = SDL_CreateMutex()) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "controllers.c", "Couldn't create SDL mutex for console pause access: %s\n", SDL_GetError());
        destroyControllers(ct);
        return NULL;
    }
    if ((ct->startMutex = SDL_CreateMutex()) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "controllers.c", "Couldn't create SDL mutex for console start access: %s\n", SDL_GetError());
        destroyControllers(ct);
        return NULL;
    }

    /* set state if provided */
    if (controllersState != NULL) {
        emuint marker = 15;
        ct->port3F = controllersState[marker++];
        ct->portDC = controllersState[marker++];
        ct->portDD = controllersState[marker++];
    }

    /* return Controllers object */
    return ct;
}

/* this function destroys the Controllers object */
void destroyControllers(Controllers ct)
{
    /* destroy mutexes */
    if (ct->controllerDCMutex != NULL)
        SDL_DestroyMutex(ct->controllerDCMutex);
    if (ct->pauseMutex != NULL)
        SDL_DestroyMutex(ct->pauseMutex);
    if (ct->startMutex != NULL)
        SDL_DestroyMutex(ct->startMutex);
}

/* this function reads input from port3F */
emubyte controllers_handle3F(Controllers ct, emubyte action, emubyte value)
{
    emubyte returnValue = 0;
    emubyte tempValue = 0;
    
    /* act upon specified action */
    switch (action) {
        case 0: returnValue = ct->port3F; break; /* read port */
        case 1: tempValue = ct->port3F & 0xAA;
            ct->port3F = value; /* write port */
            if ((value & 0xAA) > tempValue) { /* check for VDP hCounter latch (TH state change) */
                console_tellVDPToStoreHCounterValue(ct->ms);
            } break;
    }
    
    return returnValue;
}

/* this function reads input from portDC */
emubyte controllers_handleTempDC(Controllers ct, emubyte action, emubyte value)
{
    emubyte returnValue = 0;
    
    /* lock mutex */
    if (SDL_LockMutex(ct->controllerDCMutex) == 0) {
        /* act upon specified action */
        switch (action) {
            case 0: returnValue = ct->tempPortDC; break; /* read port */
            case 1: ct->tempPortDC = value; break; /* write port */
            case 2: ct->portDC = ct->tempPortDC; break; /* copy temp value to real value */
        }
        
        /* now unlock mutex */
        if (SDL_UnlockMutex(ct->controllerDCMutex) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, "controllers.c", "Couldn't unlock controller port DC mutex: %s\n", SDL_GetError());
        }
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "controllers.c", "Couldn't lock controller port DC mutex: %s\n", SDL_GetError());
    }

    return returnValue;
}

/* this function reads input from portDC */
emubyte controllers_handleDC(Controllers ct, emubyte action, emubyte value)
{
    emubyte returnValue = 0;
    emubyte portA_TR_outputting = (ct->port3F & 0x01);
    
    /* act upon specified action */
    switch (action) {
        case 0: if (ct->isJapanese) {
                    returnValue = ct->portDC; /* read port */
                } else {
                    returnValue = ct->portDC;
                    /* merge in port 3F values if output is set */
                    if (portA_TR_outputting == 0) {
                        emubyte portA_TR_bit = (ct->port3F & 0x10) << 1;
                        returnValue &= 0xDF;
                        returnValue |= portA_TR_bit;
                    }
                }
                break;
        case 1: ct->portDC = value; break; /* write port */
    }
    
    return returnValue;
}

/* this function reads input from portDD */
emubyte controllers_handleDD(Controllers ct, emubyte action, emubyte value)
{
    emubyte returnValue = 0;
    emubyte portA_TH_outputting = ((ct->port3F & 0x02) >> 1);
    emubyte portB_TH_outputting = ((ct->port3F & 0x08) >> 3);
    emubyte portB_TR_outputting = ((ct->port3F & 0x04) >> 2);
    
    /* act upon specified action */
    switch (action) {
        case 0: if (ct->isJapanese) {
                    returnValue = ct->portDD; /* read port */
                } else {
                    returnValue = ct->portDD;
                    /* merge in port 3F values if output is set */
                    if (portA_TH_outputting == 0) {
                        emubyte portA_TH_bit = (ct->port3F & 0x20) << 1;
                        returnValue &= 0xBF;
                        returnValue |= portA_TH_bit;
                    }
                    if (portB_TH_outputting == 0) {
                        emubyte portB_TH_bit = (ct->port3F & 0x80);
                        returnValue &= 0x7F;
                        returnValue |= portB_TH_bit;
                    }
                    if (portB_TR_outputting == 0) {
                        emubyte portB_TR_bit = (ct->port3F & 0x40) >> 3;
                        returnValue &= 0xF7;
                        returnValue |= portB_TR_bit;
                    }
                }
                break;
        case 1: ct->portDD = value; break; /* write port */
    }

    return returnValue;
}

/* this function returns whether or not the pause button is pressed */
emubool controllers_handleTempPauseStatus(Controllers ct, emubyte action, emubool value)
{
    emubool returnValue = false;
    
    /* lock mutex */
    if (SDL_LockMutex(ct->pauseMutex) == 0) {
        /* act upon specified action */
        switch (action) {
            case 0: returnValue = ct->tempPauseButton; break; /* read button */
            case 1: ct->tempPauseButton = value; break; /* write button */
            case 2: ct->pauseButton = ct->tempPauseButton;
                    if (ct->tempPauseButton)
                        ct->tempPauseButton = false;
                    break; /* copy temp value to real value */
        }
        
        /* now unlock mutex */
        if (SDL_UnlockMutex(ct->pauseMutex) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, "controllers.c", "Couldn't unlock console pause mutex: %s\n", SDL_GetError());
        }
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "controllers.c", "Couldn't lock console pause mutex: %s\n", SDL_GetError());
    }

    return returnValue;
}

/* this function returns whether or not the pause button is pressed */
emubool controllers_handlePauseStatus(Controllers ct, emubyte action, emubool value)
{
    emubool returnValue = false;
    
    /* act upon specified action */
    switch (action) {
        case 0: returnValue = ct->pauseButton; break; /* read button */
        case 1: ct->pauseButton = value; break; /* write button */
    }

    return returnValue;
}

/* this function deals with the start button on the Game Gear */
emubyte controllers_handleTempStart(Controllers ct, emubyte action, emubyte value)
{
    emubyte returnValue = 0;
    
    /* lock mutex */
    if (SDL_LockMutex(ct->startMutex) == 0) {
        /* act upon specified action */
        switch (action) {
            case 0: returnValue = ct->tempStartButton; break; /* read port */
            case 1: ct->tempStartButton = (0x80 & ct->tempStartButton) | (0x7F & value); break; /* write port */
            case 2: ct->tempStartButton = 0x80 | (0x7F & ct->tempStartButton); break; /* unpress button */
            case 3: ct->tempStartButton = 0x7F & ct->tempStartButton; break; /* press button */
            case 4: ct->startButton = ct->tempStartButton; break; /* copy temp value to real value */
        }
        
        /* now unlock mutex */
        if (SDL_UnlockMutex(ct->startMutex) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, "controllers.c", "Couldn't unlock console start mutex: %s\n", SDL_GetError());
        }
    } else {
        __android_log_print(ANDROID_LOG_ERROR, "controllers.c", "Couldn't lock console start mutex: %s\n", SDL_GetError());
    }

    return returnValue;
}

/* this function deals with the start button on the Game Gear */
emubyte controllers_handleStart(Controllers ct, emubyte action, emubyte value)
{
    emubyte returnValue = 0;
    
    /* act upon specified action */
    switch (action) {
        case 0: returnValue = ct->startButton; break; /* read port */
        case 1: ct->startButton = (0x80 & ct->startButton) | (0x7F & value); break; /* write port */
        case 2: ct->startButton = 0x80 | (0x7F & ct->startButton); break; /* unpress button */
        case 3: ct->startButton = 0x7F & ct->startButton; break; /* press button */
    }

    if (ct->isGameGear) {
        if (ct->isJapanese)
            returnValue &= 0xBF;
        else
            returnValue |= 0x40;
    }
    
    return returnValue;
}

/* this function takes the event supplied values and copies them to the real locations - it is
   intended to be called once per frame */
void controllers_updateValues(Controllers ct)
{
    controllers_handleTempDC(ct, 2, 0);
    controllers_handleTempPauseStatus(ct, 2, true);
    controllers_handleTempStart(ct, 4, 0);
}

/* this function returns an emubyte pointer to the state of the Controllers object */
emubyte *controllers_saveState(Controllers ct)
{
    /* define variables */
    emuint controllersSize = 3 + 15; /* add extra 15 bytes for header and size */
    emuint marker = 0;

    /* allocate the memory */
    emubyte *controllersState = malloc(sizeof(emubyte) * controllersSize);
    if (controllersState == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "controllers.c", "Couldn't allocate memory for controllers state buffer...\n");
        return NULL;
    }

    /* fill first 15 bytes with header and state size */
    controllersState[marker++] = controllersSize & 0xFF;
    controllersState[marker++] = (controllersSize >> 8) & 0xFF;
    controllersState[marker++] = (controllersSize >> 16) & 0xFF;
    controllersState[marker++] = (controllersSize >> 24) & 0xFF;
    controllersState[marker++] = 'C';
    controllersState[marker++] = 'O';
    controllersState[marker++] = 'N';
    controllersState[marker++] = 'T';
    controllersState[marker++] = 'R';
    controllersState[marker++] = 'O';
    controllersState[marker++] = 'L';
    controllersState[marker++] = 'L';
    controllersState[marker++] = 'E';
    controllersState[marker++] = 'R';
    controllersState[marker++] = 'S';

    /* copy values to state buffer */
    controllersState[marker++] = ct->port3F;
    controllersState[marker++] = ct->portDC;
    controllersState[marker++] = ct->portDD;

    /* return controllers state */
    return controllersState;
}

/* this function returns the number of bytes required to create a Controllers object */
emuint controllers_getMemoryUsage(void)
{
    return (sizeof(struct Controllers) * sizeof(emubyte));
}