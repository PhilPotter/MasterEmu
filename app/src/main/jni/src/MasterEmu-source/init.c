/* MasterEmu initialisation source code file
   copyright Phil Potter, 2019 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include <time.h>
#include "init.h"
#include "../../SDL2-2.0.16/include/SDL.h"
#include "console.h"
#include "crc32_imp.h"
#include "util.h"

/* internal helper functions */
static int MasterEmuEventFilter(void *userdata, SDL_Event *event);
static void handleWindowResize(EmuBundle *eb, SDL_Collection s);
static void stopLogicThread(EmuBundle *eb);
static void startLogicThread(EmuBundle *eb);
JNIEXPORT void JNICALL Java_uk_co_philpotter_masteremu_PauseActivity_saveStateStub(JNIEnv *, jobject, jlong, jstring);
JNIEXPORT void JNICALL Java_uk_co_philpotter_masteremu_StateBrowser_loadStateStub(JNIEnv *, jobject, jlong, jstring);
JNIEXPORT void JNICALL Java_uk_co_philpotter_masteremu_PauseActivity_quitStub(JNIEnv *, jobject, jlong);
JNIEXPORT void JNICALL Java_org_libsdl_app_SDLActivity_onMasterEmuDown(JNIEnv *, jclass, jint);
JNIEXPORT void JNICALL Java_org_libsdl_app_SDLActivity_onMasterEmuUp(JNIEnv *, jclass, jint);

/* global cheat code array objects */
ArCheatArray arCheatArray;
GgCheatArray ggCheatArray;

static int copyOfUserEventCode = 0;

int start_emulator(JNIEnv *env, jclass cls, jobject obj, jbyteArray romData, jint romSize, jint params, jlongArray codesArray)
{
    /* define temp variables to store certain attributes */
    EmulatorContainer ec;
    ec.params = params;
    ec.noStretching = false;
    ec.isGameGear = false;
    ec.isCodemasters = false;
    ec.isPal = false;
    ec.touches.up = -1;
    ec.touches.down = -1;
    ec.touches.left = -1;
    ec.touches.right = -1;
    ec.touches.upLeft = -1;
    ec.touches.upRight = -1;
    ec.touches.downLeft = -1;
    ec.touches.downRight = -1;
    ec.touches.buttonOne = -1;
    ec.touches.buttonTwo = -1;
    ec.touches.pauseStart = -1;
    ec.touches.both = -1;
    ec.touches.nothing = -1;

    /* parse for ROM type */
    if ((ec.params & 0x20) == 0x20)
        ec.isGameGear = true;

    /* see if stretching is disabled */
    if ((ec.params & 0x40) == 0x40)
        ec.noStretching = true;

    /* load ROM by converting jbyteArray to usable form */
    jbyte *byteRomData = (*env)->GetByteArrayElements(env, romData, NULL);
    if (util_loadRom(&ec, byteRomData, romSize) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "init.c", "Couldn't load ROM...\n");
        return ERROR_UNABLE_TO_LOAD_ROM;
    }
    (*env)->ReleaseByteArrayElements(env, romData, byteRomData, JNI_ABORT);

    /* parse cheat codes into arrays, preparing where necessary */
    if (codesArray != NULL) {
        /* get number of codes and allocate enough memory to copy them over via JNI */
        signed_emuint numOfCodes = (*env)->GetArrayLength(env, codesArray);
        signed_emulong *nativeCodesArray = malloc(sizeof(signed_emulong) * numOfCodes);
        if (nativeCodesArray == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error allocating memory for nativeCodesArray, aborting...\n");
            return ERROR_ALLOCATING_MEMORY;
        }

        /* copy the codes */
        jlong *codesArrayData = (*env)->GetLongArrayElements(env, codesArray, NULL);
        memcpy(nativeCodesArray, codesArrayData, sizeof(signed_emulong) * numOfCodes);
        (*env)->ReleaseLongArrayElements(env, codesArray, codesArrayData, JNI_ABORT);

        /* calculate numbers of each type of code */
        signed_emuint actionReplayCount = 0;
        for (signed_emuint i = 0; i < numOfCodes; ++i) {
            emulong nativeCode = (emulong)nativeCodesArray[i];
            if ((nativeCode & 0x8000000000000000UL) == 0x8000000000000000UL)
                ++actionReplayCount;
        }
        signed_emuint gameGenieCount = numOfCodes - actionReplayCount;

        /* handle the Action Replay codes first */
        if (actionReplayCount > 0) {
            arCheatArray.enabled = true;
            arCheatArray.cheatCount = (emuint)actionReplayCount;
            arCheatArray.cheats = calloc(actionReplayCount, sizeof(ArCheat));
            if (arCheatArray.cheats == NULL) {
                __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error allocating memory for arCheatArray.cheats, aborting...\n");
                return ERROR_ALLOCATING_MEMORY;
            }

            for (signed_emuint i = 0, arMarker = 0; i < numOfCodes && arMarker != actionReplayCount; ++i) {
                emulong nativeCode = (emulong)nativeCodesArray[i];

                if ((nativeCode & 0x8000000000000000UL) == 0x8000000000000000UL) {
                    signed_emuint arAddress = (signed_emuint)((nativeCode >> 8) & 0xFFFFUL);

                    /* if code address is not in range, ignore it and move onto next code */
                    if (arAddress < 0xC000 || arAddress > 0xDFFF)
                        continue;

                    /* access relevant cheat, setting properties as appropriate */
                    arCheatArray.cheats[arMarker].address = (emuint)arAddress;
                    arCheatArray.cheats[arMarker].value = (emubyte)(nativeCode & 0xFFUL);

                    ++arMarker;
                }
            }
        }

        /* now handle Game Genie codes */
        if (gameGenieCount > 0) {
            ggCheatArray.enabled = true;
            ggCheatArray.cheatCount = (emuint)gameGenieCount;
            ggCheatArray.cheats = calloc(gameGenieCount, sizeof(GgCheat));
            if (ggCheatArray.cheats == NULL) {
                __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error allocating memory for ggCheatArray.cheats, aborting...\n");
                return ERROR_ALLOCATING_MEMORY;
            }

            for (signed_emuint i = 0, ggMarker = 0; i < numOfCodes && ggMarker != gameGenieCount; ++i) {
                emulong nativeCode = (emulong)nativeCodesArray[i];

                if ((nativeCode & 0x4000000000000000UL) == 0x4000000000000000UL) {
                    /* address is A letters in DDA-AAA-RRR layout - we need to chop the last digit and
                       move it to the start */
                    signed_emuint ggAddress = (signed_emuint)(((nativeCode >> 16) & 0xFFFUL) | ((nativeCode & 0xF000UL) ^ 0xF000UL));

                    /* if code address is not in range, ignore it and move onto next code */
                    if (ggAddress < 0 || ggAddress > 0xFFFF)
                        continue;

                    /* access relevant cheat, setting properties as appropriate */
                    ggCheatArray.cheats[ggMarker].address = (emuint)ggAddress;
                    ggCheatArray.cheats[ggMarker].value = (emubyte)(nativeCode >> 28);

                    /* now check if cloak and reference is enabled */
                    if ((nativeCode & 0x2000000000000000UL) == 0x2000000000000000UL) {
                        ggCheatArray.cheats[ggMarker].cloakAndReferencePresent = true;

                        /* cloak is 1st digit of RRR XORed with the 2nd - this is usually 8 in commercial codes for whatever reason */
                        ggCheatArray.cheats[ggMarker].cloak = (emubyte)(((nativeCode >> 8) ^ (nativeCode >> 4)) & 0xFUL);

                        /* getting reference is a bit trickier - first we make a byte composed of the 1st and 3rd digits of RRR */
                        emubyte tempRef = (emubyte)(((nativeCode >> 4) & 0xF0UL) | (nativeCode & 0xFUL));

                        /* then we rotate this byte right by 2 bits */
                        emubyte tempBits = (tempRef & 0x03) << 6;
                        tempRef = (tempRef >> 2) | tempBits;

                        /* now we XOR it with 0xBA */
                        ggCheatArray.cheats[ggMarker].reference = tempRef ^ 0xBA;
                    }

                    ++ggMarker;
                }
            }
        }

        /* free items that are no longer necessary */
        free(nativeCodesArray);
    }

    /* this initialises SDL */
    emubool largerButtons = false;
    if ((ec.params & 0x04) == 0x04)
        largerButtons = true;
    SDL_Collection s = util_setupSDL(ec.noStretching, ec.isGameGear, largerButtons, env, cls, obj, false);
    if (s == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error setting up SDL, aborting...\n");
        return ERROR_SETTING_UP_SDL;
    }

    /* setup quit variable */
    SDL_AtomicSet(&ec.quitVar, 0);

    /* deal with save state */
    emubyte *saveState;
    util_loadState(&ec, "current_state.mesav", &saveState);

    /* setup console */
    ec.consoleMemoryPointer = malloc(console_getWholeMemoryUsage());
    if (ec.consoleMemoryPointer == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error allocating console memory, aborting...");
        return ERROR_ALLOCATING_CONSOLE_MEMORY;
    }
    ec.console = createConsole(ec.romData, ec.romSize, ec.romChecksum, ec.isCodemasters, ec.isGameGear, ec.isPal, s->sourceRect, saveState, ec.params, ec.consoleMemoryPointer, 0);
    if (ec.console == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error creating console, aborting...");
        return ERROR_UNABLE_TO_CREATE_CONSOLE;
    }

    /* setup event filter */
    EmuBundle eb;
    eb.ec = &ec;
    eb.s = s;
    copyOfUserEventCode = eb.userEventType = SDL_RegisterEvents(3);

    /* create thread to run logic */
    SDL_AtomicSet(&eb.logicQuit, 0);
    eb.logicThread = SDL_CreateThread(LogicFunction, "logicThread", (void *)&eb);
    if (eb.logicThread == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error creating paint thread, aborting...");
        return ERROR_CREATING_LOGIC_THREAD;
    }
    SDL_DetachThread(eb.logicThread);

    /* set paint to default state of OK */
    SDL_AtomicSet(&eb.dontPaint, 0);

    /* set event filter function going */
    SDL_SetEventFilter(MasterEmuEventFilter, (void *)&eb);

    /* paint loop */
    SDL_Event e;
    signed_emuint waitStatus = 0;
    while ((waitStatus = SDL_WaitEvent(&e))) {
        if (e.type == eb.userEventType) {
            if (e.user.code == ACTION_PAINT)
                util_paintFrame((EmuBundle *)e.user.data1);
            else if (e.user.code == MASTEREMU_QUIT)
                break;
        }
    }

    if (waitStatus != 1)
        __android_log_print(ANDROID_LOG_ERROR, "MasterEmuDebug", "Error waiting on event: %s", SDL_GetError());

    __android_log_print(ANDROID_LOG_VERBOSE, "MasterEmuDebug", "Exiting emulator core...");

    /* stop logic thread */
    stopLogicThread(&eb);

    /* stop audio */
    console_stopAudio(ec.console);

    /* this cleans up the console */
    destroyConsole(ec.console);
    free((void *)ec.consoleMemoryPointer);

    /* this cleans up the code array objects' inner cheat arrays - called even if null as this is allowed in C spec */
    free(arCheatArray.cheats);
    free(ggCheatArray.cheats);

    /* this shuts down SDL */
    util_shutdownSDL(s, false);
    
    /* this deallocates the ROM data memory buffer */
    free((void *)(ec.romData));

	return ALL_GOOD;
}

/* this is the function which runs the logic, from a separate thread */
int LogicFunction(void *p) {
    #define PAL_CYCLES_PER_FRAME 70938
    #define NTSC_CYCLES_PER_FRAME 59659

    /* cast p to EmuBundle pointer */
    EmuBundle *eb = (EmuBundle *)p;

    /* start emulation loop here */
    emuint cyclesPerFrame = eb->ec->isPal ? PAL_CYCLES_PER_FRAME : NTSC_CYCLES_PER_FRAME;
    emuint cycles = 0;
    emuint nanoSecondsToCount = eb->ec->isPal ? 20000000 : 16666667;
    struct timespec t1, t2;

    while (SDL_AtomicGet(&eb->logicQuit) == 0) {
        clock_gettime(CLOCK_MONOTONIC, &t1);
        while ((cycles += console_executeInstruction(eb)) < cyclesPerFrame && SDL_AtomicGet(&eb->logicQuit) == 0)
            ;
        cycles -= cyclesPerFrame;
        do {
            clock_gettime(CLOCK_MONOTONIC, &t2);
        } while (t2.tv_sec == t1.tv_sec && t2.tv_nsec - t1.tv_nsec < nanoSecondsToCount && SDL_AtomicGet(&eb->logicQuit) == 0);
    }

    return 0;
}

/* this function lets us filter quit events in Android */
static int MasterEmuEventFilter(void *userdata, SDL_Event *event) {
    int returnVal = 1;
    EmuBundle *eb = (EmuBundle *)userdata;
    EmulatorContainer *ec = (*eb).ec;
    SDL_Collection s = (*eb).s;

    if ((*event).type == SDL_FINGERDOWN ||
        (*event).type == SDL_FINGERMOTION ||
        (*event).type == SDL_FINGERUP) {
        util_dealWithTouch(ec, s, event);
        returnVal = 0;
    } else if ((*event).type == eb->userEventType) {
        if ((*event).user.code == ACTION_DOWN || (*event).user.code == ACTION_UP) {

            signed_emuint keycode = (signed_emuint)((*event).user.data1);
            if ((*event).user.code == ACTION_UP && (keycode == KEYCODE_BUTTON_B || keycode == KEYCODE_BACK)) {
                returnVal = 0;
                JNIEnv *env = (JNIEnv*)SDL_AndroidGetJNIEnv();
                jobject obj = (jobject)SDL_AndroidGetActivity();
                jclass SDLActivity_class = (*env)->GetObjectClass(env, obj);

                /* start pause screen */
                stopLogicThread(eb);
                jmethodID mid = (*env)->GetMethodID(env, SDLActivity_class, "loadPauseMenu", "(JLjava/lang/String;)V");
                char checksumStr[9];
                if (sprintf(checksumStr, "%08x", (*ec).romChecksum) < 0) {
                    __android_log_print(ANDROID_LOG_ERROR, "init.c", "Couldn't convert ROM checksum to string...");
                }
                jstring statePath = (*env)->NewStringUTF(env, checksumStr);
                (*env)->CallVoidMethod(env, obj, mid, (jlong)ec, statePath);
                (*env)->DeleteLocalRef(env, obj);
                (*env)->DeleteLocalRef(env, SDLActivity_class);
            } else {
                util_dealWithButton(ec, event);
                returnVal = 0;
            }
        }
    } else if ((*event).type == SDL_APP_WILLENTERBACKGROUND) {

        /* deal with save state here */
        stopLogicThread(eb);
        if (util_saveState(ec, "current_state.mesav") != ALL_GOOD) {
            __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error saving state...");
            return ERROR_SAVING_STATE;
        }

        returnVal = 0;
    } else if ((*event).type == SDL_APP_DIDENTERFOREGROUND) {

        startLogicThread(eb);
        returnVal = 0;
    } else if ((*event).type == SDL_APP_TERMINATING) {
        /* terminate app here */
        util_handleQuit(copyOfUserEventCode);
        returnVal = 0;
    } else if ((*event).type == SDL_QUIT) {
        /* ignore this event */
        returnVal = 0;
    } else if ((*event).type == SDL_WINDOWEVENT) {
        /* deal with resize */
        if ((*event).window.event == SDL_WINDOWEVENT_RESIZED) {
            returnVal = 0;
            handleWindowResize(eb, s);
        }
    } else if ((*event).type == SDL_KEYUP) {
        /* check if it is back key */
        if ((*event).key.keysym.sym == SDLK_AC_BACK) {
            returnVal = 0;
            JNIEnv *env = (JNIEnv*)SDL_AndroidGetJNIEnv();
            jobject obj = (jobject)SDL_AndroidGetActivity();
            jclass SDLActivity_class = (*env)->GetObjectClass(env, obj);

            /* start pause screen */
            stopLogicThread(eb);
            jmethodID mid = (*env)->GetMethodID(env, SDLActivity_class, "loadPauseMenu", "(JLjava/lang/String;)V");
            char checksumStr[9];
            if (sprintf(checksumStr, "%08x", (*ec).romChecksum) < 0) {
                __android_log_print(ANDROID_LOG_ERROR, "init.c", "Couldn't convert ROM checksum to string...");
            }
            jstring statePath = (*env)->NewStringUTF(env, checksumStr);
            (*env)->CallVoidMethod(env, obj, mid, (jlong)ec, statePath);
            (*env)->DeleteLocalRef(env, obj);
            (*env)->DeleteLocalRef(env, SDLActivity_class);
        }
    }

    return returnVal;
}

/* this function stops the timer event from firing, which is what runs the emulator, in a different thread - it
   then deals with resetting some SDL parameters and then restarts the timer */
static void handleWindowResize(EmuBundle *eb, SDL_Collection s)
{
    /* emulator is now not running, we can make some changes safely as this thread will be the only one
       accessing the relevant structures */
    util_handleWindowResize(eb, s);
}

/* this function allows us to call the save state function from the Java side */
JNIEXPORT void JNICALL Java_uk_co_philpotter_masteremu_PauseActivity_saveStateStub(JNIEnv *env, jobject obj, jlong emulatorContainerPointer, jstring fileName)
{
    /* create compatible string */
    char *cFileName = (char *)(*env)->GetStringUTFChars(env, fileName, NULL);

    /* cast pointer back */
    EmulatorContainer *ec = (EmulatorContainer *)emulatorContainerPointer;

    /* get class and method id with JNI */
    jclass PauseActivity_class = (*env)->GetObjectClass(env, obj);
    jmethodID midSuccess = (*env)->GetMethodID(env, PauseActivity_class, "saveStateSucceeded", "()V");
    jmethodID midFailure = (*env)->GetMethodID(env, PauseActivity_class, "saveStateFailed", "()V");


    /* call save state function */
    if (util_saveState(ec, cFileName) == ALL_GOOD) {
        (*env)->CallVoidMethod(env, obj, midSuccess);
    } else {
        (*env)->CallVoidMethod(env, obj, midFailure);
    }

    /* free JNI stuff */
    (*env)->DeleteLocalRef(env, PauseActivity_class);
    (*env)->ReleaseStringUTFChars(env, fileName, cFileName);
}

/* this function allows us to load the state from the Java side */
JNIEXPORT void JNICALL Java_uk_co_philpotter_masteremu_StateBrowser_loadStateStub(JNIEnv *env, jobject obj, jlong emulatorContainerPointer, jstring fileName)
{
    /* create compatible string */
    char *cFileName = (char *)(*env)->GetStringUTFChars(env, fileName, NULL);

    /* cast pointer back */
    EmulatorContainer *ec = (EmulatorContainer *)emulatorContainerPointer;

    /* get class and method id with JNI */
    jclass StateBrowser_class = (*env)->GetObjectClass(env, obj);
    jmethodID midSuccess = (*env)->GetMethodID(env, StateBrowser_class, "loadStateSucceeded", "()V");
    jmethodID midFailure = (*env)->GetMethodID(env, StateBrowser_class, "loadStateFailed", "()V");

    /* load state */
    emubyte *saveState;
    if (util_loadState(ec, cFileName, &saveState) == ALL_GOOD) {
        (*env)->CallVoidMethod(env, obj, midSuccess);

        /* get source rect and audioId from old console */
        SDL_Rect *sourceRect = console_getSourceRect((*ec).console);
        emuint audioId = console_getAudioDeviceID((*ec).console);

        /* destroy console */
        destroyConsole((*ec).console);
        free((void *)(*ec).consoleMemoryPointer);

        /* recreate console, masking sRam only flag */
        emuint tempParams = (*ec).params;
        tempParams &= 0xFFFFFFFD;
        (*ec).consoleMemoryPointer = malloc(console_getWholeMemoryUsage());
        if ((*ec).consoleMemoryPointer == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error allocating console memory...");
        }
        (*ec).console = createConsole((*ec).romData, (*ec).romSize, (*ec).romChecksum, (*ec).isCodemasters, (*ec).isGameGear, (*ec).isPal, sourceRect, saveState, tempParams, (*ec).consoleMemoryPointer, audioId);
        if ((*ec).console == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error creating console...");
        }
    } else {
        (*env)->CallVoidMethod(env, obj, midFailure);
    }

    /* free JNI stuff */
    (*env)->DeleteLocalRef(env, StateBrowser_class);
    (*env)->ReleaseStringUTFChars(env, fileName, cFileName);
}

/* this function allows us to kill the emulator loop from the Java side */
JNIEXPORT void JNICALL Java_uk_co_philpotter_masteremu_PauseActivity_quitStub(JNIEnv *env, jobject obj, jlong emulatorContainerPointer)
{
    /* cast pointer back */
    EmulatorContainer *ec = (EmulatorContainer *)emulatorContainerPointer;

    /* quit emulator event loop */
    util_handleQuit(copyOfUserEventCode);
}

/* this function handles controller input when a button goes down */
JNIEXPORT void JNICALL Java_org_libsdl_app_SDLActivity_onMasterEmuDown(JNIEnv *env, jclass cls, jint keycode)
{
    /* define user event for updating display on main thread, and push it
       to event queue for processing */
    SDL_Event event;
    memset((void *)&event, 0, sizeof(event));
    event.user.code = ACTION_DOWN;

    /* this gets rid of compiler warnings about casting from jint to void * */
    jlong lKeycode = keycode;

    event.user.data1 = (void *)lKeycode;
    event.user.data2 = NULL;
    event.type = copyOfUserEventCode;
    SDL_PushEvent(&event);
}

/* this function handles controller input when a button goes up */
JNIEXPORT void JNICALL Java_org_libsdl_app_SDLActivity_onMasterEmuUp(JNIEnv *env, jclass cls, jint keycode)
{
    /* define user event for updating display on main thread, and push it
       to event queue for processing */
    SDL_Event event;
    memset((void *)&event, 0, sizeof(event));
    event.user.code = ACTION_UP;

    /* this gets rid of compiler warnings about casting from jint to void * */
    jlong lKeycode = keycode;

    event.user.data1 = (void *)lKeycode;
    event.user.data2 = NULL;
    event.type = copyOfUserEventCode;
    SDL_PushEvent(&event);
}

/* this function stops the logic thread and allows a suitable time to pass before returning */
static void stopLogicThread(EmuBundle *eb)
{
    SDL_AtomicSet(&eb->dontPaint, 1);
    SDL_AtomicSet(&eb->logicQuit, 1);
}

/* this function starts the logic thread */
static void startLogicThread(EmuBundle *eb)
{
    eb->logicThread = NULL;
    SDL_AtomicSet(&eb->dontPaint, 0);
    SDL_AtomicSet(&eb->logicQuit, 0);
    eb->logicThread = SDL_CreateThread(LogicFunction, "logicThread", (void *)eb);
    if (eb->logicThread == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "init.c", "Error creating paint thread, aborting...");
    }
    SDL_DetachThread(eb->logicThread);
}
