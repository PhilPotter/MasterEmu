/* MasterEmu console header file
   copyright Phil Potter, 2019 */

#ifndef UTIL_INCLUDE
#define UTIL_INCLUDE
#include <stdio.h>
#include <jni.h>
#include "../../SDL-release-2.24.0/include/SDL.h"
#include "datatypes.h"

#define LARGER_BUTTONS_FACTOR 1.3

#include "console.h"

/* this struct helps us keep all the SDL stuff together */
struct SDL_Collection {
    SDL_GameController *gameControllers[4];
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_Texture *dpadTexture;
    SDL_Texture *buttonsTexture;
    SDL_Texture *pauseTexture;
    SDL_Texture *pressedTexture;
    SDL_Texture *pausePressedTexture;
    SDL_Texture *pauseLabelTexture;
    SDL_Texture *bothLabelTexture;
    SDL_Texture *actionLabelTexture;
    SDL_Texture *backTexture;
    SDL_Rect *sourceRect;
    SDL_Rect *windowRect;
    SDL_Rect *dpadRect;
    SDL_Rect *upRect;
    SDL_Rect *downRect;
    SDL_Rect *leftRect;
    SDL_Rect *rightRect;
    SDL_Rect *upLeftRect;
    SDL_Rect *upRightRect;
    SDL_Rect *downLeftRect;
    SDL_Rect *downRightRect;
    SDL_Rect *buttonsRect;
    SDL_Rect *buttonOneRect;
    SDL_Rect *buttonTwoRect;
    SDL_Rect *pauseRect;
    SDL_Rect *bothRect;
    emuint screenWidth;
    emuint screenHeight;
    int pixelsForOneInch;
    emubyte *displayFrame;
};
typedef struct SDL_Collection *SDL_Collection;

struct Touches {
    SDL_FingerID up;
    SDL_FingerID down;
    SDL_FingerID left;
    SDL_FingerID right;
    SDL_FingerID upLeft;
    SDL_FingerID upRight;
    SDL_FingerID downLeft;
    SDL_FingerID downRight;
    SDL_FingerID buttonOne;
    SDL_FingerID buttonTwo;
    SDL_FingerID pauseStart;
    SDL_FingerID both;
    SDL_FingerID nothing;
};
typedef struct Touches Touches;

struct ButtonMapping {
    emuint up;
    emuint down;
    emuint left;
    emuint right;
    emuint buttonOne;
    emuint buttonTwo;
    emuint pauseStart;
    emuint back;
};
typedef struct ButtonMapping ButtonMapping;

struct CurrentControllerState {
    /* only some of these buttons are actually polled for - we just store
       them like this for easy indexing */
    SDL_atomic_t xAxis;
    SDL_atomic_t yAxis;
    emubool buttonArray[SDL_CONTROLLER_BUTTON_MAX];
};
typedef struct CurrentControllerState CurrentControllerState;

struct EmulatorContainer {
    emubool noStretching;
    emubool isGameGear;
    emubool isCodemasters;
    emubool isPal;
    emuint params;
    Console console;
    emubyte *romData;
    signed_emulong romSize;
    emuint romChecksum;
    Touches touches;
    ButtonMapping buttonMapping;
    CurrentControllerState currentControllerState;
    emubool showBack;
    SDL_mutex *remappingWaitMutex;
    SDL_cond *remappingCondVar;
    emubyte *consoleMemoryPointer;
};
typedef struct EmulatorContainer EmulatorContainer;

struct EmuBundle {
    EmulatorContainer *ec;
    SDL_Collection s;
    SDL_Thread *logicThread;
    SDL_atomic_t logicQuit;
    SDL_atomic_t dontPaint;
    signed_emuint userEventType;
};
typedef struct EmuBundle EmuBundle;

/* custom definitions for MasterEmu specific events */
#define ACTION_PAINT 1337
#define MASTEREMU_QUIT 1338

/* function declarations */
SDL_Collection util_setupSDL(emubool noStretching, emubool isGameGear, emubool largerButtons, JNIEnv *env, jclass cls, jobject obj, emubool fromResume); /* this sets up SDL */
void util_shutdownSDL(SDL_Collection s, emubool fromResume); /* this shuts down SDL */
emubool util_isCodemastersROM(emuint checksum); /* this tells whether the ROM uses a Codemasters mapper */
emubool util_isPalOnlyROM(emuint checksum); /* this tells us if the ROM is PAL only */
emubool util_isSmsGgROM(emuint checksum); /* this tells us if the Game Gear ROM is to run in Master System mode */
void util_handleQuit(emuint userEventCode); /* this lets us quit the emulator */
void util_paintFrame(EmuBundle *eb); /* this paints one frame for us */
EmulatorContainer *util_loadRom(EmulatorContainer *ec, signed_emubyte *romData, emuint romSize); /* this loads the ROM from its file */
void util_handleWindowResize(EmuBundle *eb, SDL_Collection s); /* this deals with window resizing */
void util_dealWithTouch(EmulatorContainer *ec, SDL_Collection s, SDL_Event *event); /* this deals with touch events */
emuint util_saveState(EmulatorContainer *ec, char *fileName); /* this saves the state of the emulator to a file */
emuint util_loadState(EmulatorContainer *ec, char *fileName, emubyte **saveState); /* this loads the state of the emulator to memory */
void util_dealWithButtons(EmuBundle *eb); /* this handles button presses from physical controllers */
void util_triggerPainting(EmuBundle *eb); /* this pushes an event to the queue to redraw the screen */
void util_triggerRemapPainting(EmuBundle *eb); /* this pushes an event to the queue to redraw the screen in controller remapping mode */
void util_loadButtonMapping(EmulatorContainer *ec); /* this loads a button mapping file if one exists */
void util_loadDefaultButtonMapping(EmulatorContainer *ec); /* this loads the default button mappings */
void util_writeButtonMapping(emuint *mappings, emuint size); /* this writes the button mapping to the correct file location */
void util_pollAndSetControllerState(EmuBundle *eb); /* this polls for and sets the state of all buttons we care about */

#endif
