/* MasterEmu utility functions source code file
   copyright Phil Potter, 2024 */

#include <stdlib.h>
#include <math.h>
#include <android/log.h>
#include <sys/stat.h>
#include <string.h>
#include "crc32_imp.h"
#include "util.h"
#include "init.h"
#include "../../SDL_image-release-2.8.2/include/SDL_image.h"

/* this function deals with setting up SDL and initialising all needed structures */
SDL_Collection util_setupSDL(JNIEnv *env, jclass cls, jobject obj, EmulatorContainer *ec, emubool noStretching, emubool isGameGear, emubool largerButtons, emubool fromResume)
{
    /* define variables */
    SDL_Collection s;

    if ((s = malloc(sizeof(struct SDL_Collection))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate memory for SDL_Collection struct...\n");
        return NULL;
    }

    /* initialise SDL */
    if (!fromResume) {
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't initialise SDL: %s\n",
                                SDL_GetError());
            return NULL;
        }
    }

    /* initialise game controller if there is one - take first four game controllers */
    memset(s->gameControllers, 0, sizeof(s->gameControllers));
    int controllerCount = 0, numOfJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numOfJoysticks && controllerCount < 4; i++) {
        if (SDL_IsGameController(i)) {
            s->gameControllers[controllerCount] = SDL_GameControllerOpen(i);
            if (s->gameControllers[controllerCount] == NULL) {
                __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't initialise game controller: %s\n", SDL_GetError());
                return NULL;
            }
            controllerCount++;
        }
    }

    /* setup source SDL rectangle */
    if ((s->sourceRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate source SDL_Rect struct...\n");
        return NULL;
    }
    if (isGameGear) {
        s->sourceRect->x = 48;
        s->sourceRect->y = 24;
        s->sourceRect->w = 160;
        s->sourceRect->h = 144;
    }
    else {
        s->sourceRect->x = 8;
        s->sourceRect->y = 0;
        s->sourceRect->w = 248;
        s->sourceRect->h = 192;
    }

    /* setup double buffer - zero out the memory in case we are using controller mapping mode */
    if ((s->displayFrame = calloc(1, sizeof(emubyte) * 184320)) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate space for display frame buffer...");
        return NULL;
    }

    /* setup window SDL rectangle */
    SDL_DisplayMode m;
    if (SDL_GetCurrentDisplayMode(0, &m) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't get SDL display mode...\n");
        return NULL;
    }
    if ((s->windowRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate window SDL_Rect struct...\n");
        return NULL;
    }
    s->screenWidth = m.w;
    s->screenHeight = m.h;

    /* set render scale hint */
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    /* create SDL window */
    s->window = SDL_CreateWindow("MasterEmu",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        m.w, m.h, 0);
    if (s->window == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create SDL window: %s\n", SDL_GetError());
        return NULL;
    }

    /* create SDL renderer */
    s->renderer = SDL_CreateRenderer(s->window, -1, (SDL_RENDERER_ACCELERATED
        | SDL_RENDERER_PRESENTVSYNC
        | SDL_RENDERER_TARGETTEXTURE));
    if (s->renderer == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create SDL final renderer: %s\n", SDL_GetError());
        return NULL;
    }

    /* create SDL streaming texture */
    s->texture = SDL_CreateTexture(s->renderer,
        SDL_PIXELFORMAT_BGR24,
        SDL_TEXTUREACCESS_STREAMING,
        256, 240);
    if (s->texture == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create SDL streaming texture: %s\n", SDL_GetError());
        return NULL;
    }

    /* setup controller rectangles and textures */
    SDL_RWops *dpadFile = SDL_RWFromFile("dpad.png", "rb");
    if (dpadFile == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open dpad image: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_RWops *buttonsFile = SDL_RWFromFile("action_buttons.png", "rb");
    if (buttonsFile == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open buttons image: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_RWops *pauseFile = SDL_RWFromFile("start_pause.png", "rb");
    if (pauseFile == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open pause button image: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_RWops *pressedFile = SDL_RWFromFile("main_press.png", "rb");
    if (pressedFile == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open pressed button image: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_RWops *pausePressedFile = SDL_RWFromFile("pause_press.png", "rb");
    if (pausePressedFile == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open pause pressed image: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_RWops *actionLabelFile = NULL;
    SDL_RWops *pauseLabelFile = NULL;
    if (isGameGear) {
        actionLabelFile = SDL_RWFromFile("gg_action_label.png", "rb");
        if (actionLabelFile == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open action label image: %s\n", SDL_GetError());
            return NULL;
        }
        pauseLabelFile = SDL_RWFromFile("gg_pause_label.png", "rb");
        if (pauseLabelFile == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open pause label image: %s\n", SDL_GetError());
            return NULL;
        }
    } else {
        actionLabelFile = SDL_RWFromFile("ms_action_label.png", "rb");
        if (actionLabelFile == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open action label image: %s\n", SDL_GetError());
            return NULL;
        }
        pauseLabelFile = SDL_RWFromFile("ms_pause_label.png", "rb");
        if (pauseLabelFile == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open pause label image: %s\n", SDL_GetError());
            return NULL;
        }
    }
    SDL_RWops *bothLabelFile = SDL_RWFromFile("both_label.png", "rb");
    if (bothLabelFile == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open both label image: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_RWops *backFile = SDL_RWFromFile("back.png", "rb");
    if (backFile == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't open back image: %s\n", SDL_GetError());
        return NULL;
    }

    s->dpadTexture = IMG_LoadTextureTyped_RW(s->renderer, dpadFile, 1, "PNG");
    if (s->dpadTexture == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't load dpad PNG texture: %s\n", SDL_GetError());
        return NULL;
    }
    s->buttonsTexture = IMG_LoadTextureTyped_RW(s->renderer, buttonsFile, 1, "PNG");
    if (s->buttonsTexture == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't load buttons PNG texture: %s\n", SDL_GetError());
        return NULL;
    }
    s->pauseTexture = IMG_LoadTextureTyped_RW(s->renderer, pauseFile, 1, "PNG");
    if (s->pauseTexture == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't load pause button PNG texture: %s\n", SDL_GetError());
        return NULL;
    }
    s->pressedTexture = IMG_LoadTextureTyped_RW(s->renderer, pressedFile, 1, "PNG");
    if (s->pressedTexture == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't load pressed button PNG texture: %s\n", SDL_GetError());
        return NULL;
    }
    s->pausePressedTexture = IMG_LoadTextureTyped_RW(s->renderer, pausePressedFile, 1, "PNG");
    if (s->pausePressedTexture == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't load pause pressed PNG texture: %s\n", SDL_GetError());
        return NULL;
    }
    s->pauseLabelTexture = IMG_LoadTextureTyped_RW(s->renderer, pauseLabelFile, 1, "PNG");
    if (s->pauseLabelTexture == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't load pause label PNG texture: %s\n", SDL_GetError());
        return NULL;
    }
    s->actionLabelTexture = IMG_LoadTextureTyped_RW(s->renderer, actionLabelFile, 1, "PNG");
    if (s->actionLabelTexture == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't load action label PNG texture: %s\n", SDL_GetError());
        return NULL;
    }
    s->bothLabelTexture = IMG_LoadTextureTyped_RW(s->renderer, bothLabelFile, 1, "PNG");
    if (s->bothLabelTexture == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't load both label PNG texture: %s\n", SDL_GetError());
        return NULL;
    }
    s->backTexture = IMG_LoadTextureTyped_RW(s->renderer, backFile, 1, "PNG");
    if (s->backTexture == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't load back PNG texture: %s\n", SDL_GetError());
        return NULL;
    }

    if ((s->dpadRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate dpad SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->buttonsRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate buttons SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->buttonOneRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate button one SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->buttonTwoRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate button two SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->pauseRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate pause button SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->upRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate up SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->downRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate down SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->leftRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate left SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->rightRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate right SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->upLeftRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate upLeft SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->upRightRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate upRight SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->downLeftRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate downLeft SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->downRightRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate downRight SDL_Rect struct...\n");
        return NULL;
    }
    if ((s->bothRect = malloc(sizeof(SDL_Rect))) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate both SDL_Rect struct...\n");
        return NULL;
    }

    jmethodID mid = (*env)->GetMethodID(env, cls, "getPixelsForOneInch", "()I");
    if (mid == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't get JNI method ID for SDLActivity.getPixelsForOneInch()\n");
        return NULL;
    }
    s->pixelsForOneInch = (*env)->CallIntMethod(env, obj, mid);

    if (largerButtons)
        s->pixelsForOneInch *= LARGER_BUTTONS_FACTOR;

    /* set initial layout of buttons + game window */
    if (util_handleWindowResize(ec, s, true)) {
        return NULL;
    }

    /* return SDL_Collection object */
    return s;
}

/* this function cleans up SDL before exiting */
void util_shutdownSDL(SDL_Collection s, emubool fromResume)
{
    /* free rectangles */
    free((void *)s->sourceRect);
    free((void *)s->windowRect);
    free((void *)s->dpadRect);
    free((void *)s->buttonsRect);
    free((void *)s->buttonOneRect);
    free((void *)s->buttonTwoRect);
    free((void *)s->pauseRect);
    free((void *)s->upRect);
    free((void *)s->downRect);
    free((void *)s->leftRect);
    free((void *)s->rightRect);
    free((void *)s->upLeftRect);
    free((void *)s->upRightRect);
    free((void *)s->downLeftRect);
    free((void *)s->downRightRect);
    free((void *)s->bothRect);

    /* cleanup SDL streaming texture */
    SDL_DestroyTexture(s->texture);

    /* cleanup controller textures */
    SDL_DestroyTexture(s->dpadTexture);
    SDL_DestroyTexture(s->buttonsTexture);
    SDL_DestroyTexture(s->pauseTexture);
    SDL_DestroyTexture(s->pressedTexture);
    SDL_DestroyTexture(s->pausePressedTexture);
    SDL_DestroyTexture(s->pauseLabelTexture);
    SDL_DestroyTexture(s->actionLabelTexture);
    SDL_DestroyTexture(s->bothLabelTexture);
    SDL_DestroyTexture(s->backTexture);

    /* cleanup SDL renderer */
    SDL_DestroyRenderer(s->renderer);

    /* cleanup SDL window */
    SDL_DestroyWindow(s->window);

    /* cleanup game controllers */
    for (int i = 0; i < 4; i++) {
        if (s->gameControllers[i] != NULL) {
            SDL_GameControllerClose(s->gameControllers[i]);
        } else {
            // All will be NULL after this point
            break;
        }
    }

    /* cleanup SDL itself */
    if (!fromResume)
        SDL_Quit();

    /* deallocate memory for display frame buffer */
    free((void *)s->displayFrame);

    /* deallocate memory for SDL collection object */
    free((void *)s);
}

/* this function is passed a CRC-32 checksum which it compares against an internal list of
   Codemasters games - if it matches any, then it returns true, else it returns false */
emubool util_isCodemastersROM(emuint checksum)
{
    emubool returnVal = false;

    switch (checksum) {
        case 0x8813514B: /* The Excellent Dizzy Collection (proto) */
        case 0xB9664AE1: /* Fantastic Dizzy (Europe) */
        case 0xA577CE46: /* Micro Machines (Europe) */
        case 0xEA5C3A6F: /* Dinobasher (Europe) (proto) */
        case 0xAA140C9C: /* The Excellent Dizzy Collection */
        case 0x29822980: /* Cosmic Spacehead */
        case 0x72981057: /* CJ Elephant Hunter (GG) */
        case 0x152F0DCC: /* Dropzone (GG) */
        case 0x5E53C7F7: /* Ernie Els Golf (GG) */
        case 0xC888222B: /* Fantastic Dizzy (Alternative) */
        case 0xDBE8895C: /* Micro Machines 2 - Turbo Tournament (GG) */
        case 0xF7C524F6: /* Micro Machines (GG) */
        case 0xC1756BEE: /* Pete Sampras Tennis (GG) */
        case 0xD9A7F170: /* S.S. Lucifer - Man Overboard! (GG) */
        case 0x6CAA625B: /* Cosmic Spacehead (GG) */
            returnVal = true; break;
    }

    return returnVal;
}

/* this function is passed a CRC-32 checksum which it compares against an internal list of
   PAL only games - if it matches any, then it returns true, else it returns false */
emubool util_isPalOnlyROM(emuint checksum)
{
    emubool returnVal = false;

    switch (checksum) {
        case 0x8813514B: /* The Excellent Dizzy Collection (proto) */
        case 0xB9664AE1: /* Fantastic Dizzy (Europe) */
        case 0xAA140C9C: /* The Excellent Dizzy Collection */
        case 0xA577CE46: /* Micro Machines (Europe) */
        case 0x2B7F7447: /* 007 James Bond - The Duel */
        case 0x72420F38: /* Addams Family */
        case 0xE5FF50D8: /* Back to the Future 2 */
        case 0x2D48C1D3: /* Back to the Future 3 */
        case 0xD1CC08EE: /* Bart vs Space Mutants */
        case 0xC0E25D62: /* California Games 2 */
        case 0x85CFC9C9: /* Chase H.Q. */
        case 0x29822980: /* Cosmic Spacehead */
        case 0x6C1433F9: /* Desert Strike */
        case 0xC9DBF936: /* Home Alone */
        case 0x0CA95637: /* Laser Ghost */
        case 0xC660FF34: /* The New Zealand Story */
        case 0x205CAAE8: /* Operation Wolf */
        case 0x0047B615: /* Predator 2 */
        case 0x9F951756: /* Robocop 3 */
        case 0xF8176918: /* Sensible Soccer */
        case 0x1575581D: /* Shadow of the Beast */
        case 0x5B3B922C: /* Sonic the Hedgehog 2 */
        case 0xD6F2BFCA: /* Sonic the Hedgehog 2 (v1.1) */
        case 0xCA1D3752: /* Space Harrier */
        case 0x5C205EE1: /* Xenon 2 */
            returnVal = true; break;
    }

    return returnVal;
}

/* this function redraws the frame */
void util_paintFrame(EmuBundle *eb)
{
    /* check we can continue */
    if (SDL_AtomicGet(&eb->dontPaint))
        return;

    /* separate out EmuContainer and SDL_Collection */
    EmulatorContainer *ec = eb->ec;
    SDL_Collection s = eb->s;

    /* fetch relevant parameters */
    emubool noButtons = (ec->params >> 3) & 0x01;

    /* lock texture */
    int pitch = 0;
    void *pixels = NULL;
    if (SDL_LockTexture(s->texture, NULL, &pixels, &pitch) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't lock texture: %s\n", SDL_GetError());
        return;
    }

    /* check pitch is correct size */
    if (pitch != 768) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "incorrect pitch size...\n");
        return;
    }

    /* copy pixels from display frame buffer to our SDL buffer */
    memcpy((void *)pixels, (void *)s->displayFrame, 184320);

    /* unlock texture */
    SDL_UnlockTexture(s->texture);

    /* present pixels */
    SDL_RenderClear(s->renderer);
    SDL_RenderCopy(s->renderer, s->texture, s->sourceRect, s->windowRect);
    if (!ec->showBack) {
        if (!noButtons) {
            SDL_RenderCopy(s->renderer, s->dpadTexture, NULL, s->dpadRect);
            SDL_RenderCopy(s->renderer, s->buttonsTexture, NULL, s->buttonsRect);
            SDL_RenderCopy(s->renderer, s->pauseTexture, NULL, s->pauseRect);
            SDL_RenderCopy(s->renderer, s->pauseTexture, NULL, s->bothRect);
            SDL_RenderCopy(s->renderer, s->actionLabelTexture, NULL, s->buttonsRect);
            SDL_RenderCopy(s->renderer, s->pauseLabelTexture, NULL, s->pauseRect);
            SDL_RenderCopy(s->renderer, s->bothLabelTexture, NULL, s->bothRect);
        }

        /* this section paints extra textures if buttons are pressed */
        if (!noButtons) {
            if ((*ec).touches.up != -1)
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->upRect);
            if ((*ec).touches.down != -1)
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->downRect);
            if ((*ec).touches.left != -1)
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->leftRect);
            if ((*ec).touches.right != -1)
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->rightRect);
            if ((*ec).touches.upLeft != -1)
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->upLeftRect);
            if ((*ec).touches.upRight != -1)
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->upRightRect);
            if ((*ec).touches.downLeft != -1)
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->downLeftRect);
            if ((*ec).touches.downRight != -1)
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->downRightRect);
            if ((*ec).touches.buttonOne != -1)
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->buttonOneRect);
            if ((*ec).touches.buttonTwo != -1)
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->buttonTwoRect);
            if ((*ec).touches.pauseStart != -1)
                SDL_RenderCopy(s->renderer, s->pausePressedTexture, NULL, s->pauseRect);
            if ((*ec).touches.both != -1) {
                SDL_RenderCopy(s->renderer, s->pausePressedTexture, NULL, s->bothRect);
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->buttonOneRect);
                SDL_RenderCopy(s->renderer, s->pressedTexture, NULL, s->buttonTwoRect);
            }
        }
    } else {
        SDL_RenderCopy(s->renderer, s->backTexture, NULL, s->dpadRect);
    }

    SDL_RenderPresent(s->renderer);
}

/* this function loads the ROM file */
EmulatorContainer *util_loadRom(EmulatorContainer *ec, signed_emubyte *romData, emuint romSize)
{
    /* allocate appropriate memory for romData and copy data from romData */
    if ((ec->romData = malloc((size_t)romSize)) == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate memory for ROM data...\n");
        return NULL;
    }
    if (memcpy((void *)ec->romData, (void *)romData, (size_t)romSize) != ec->romData) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't copy ROM data to memory buffer...\n");
        return NULL;
    }

    /* set size */
    ec->romSize = romSize;

    /* get CRC-32 checksum of ROM data */
    ec->romChecksum = calculateCRC32(ec->romData, ec->romSize);

    /* determine if ROM file uses Codemasters mapper */
    ec->isCodemasters = util_isCodemastersROM(ec->romChecksum);

    /* determine if ROM is only works correctly with PAL consoles */
    if (!ec->isGameGear)
        ec->isPal = util_isPalOnlyROM(ec->romChecksum);

    /* determine if ROM runs in SMS-GG mode if a Game Gear ROM */
    if (ec->isGameGear) {
        if (util_isSmsGgROM(ec->romChecksum)) {
            ec->isGameGear = false;
        }
    }

    return ec;
}

/* this function compares the checksum against a list, and sets the ROM to run in Master System
   mode if it matches */
emubool util_isSmsGgROM(emuint checksum)
{
    /* define variables */
    emubool returnVal = false;

     /* 
        WWF - Steel Cage Challenge.gg : da8e95a9 */

    switch (checksum) {
        case 0x44FBE8F6: /* Chase H.Q. */
        case 0xC888222B: /* Fantastic Dizzy */
        case 0x8813514B: /* Excellent Dizzy Collection */
        case 0x76C5BDFB: /* Jang Pung II */
        case 0x59840FD6: /* Castle of Illusion feat. Mickey Mouse */
        case 0xA2F9C7AF: /* Olympic Gold */
        case 0xEDA59FFC: /* Out-Run Europa */
        case 0xE5F789B9: /* Predator 2 */
        case 0x311D2863: /* Prince of Persia */
        case 0x56201996: /* R.C. Grand Prix */
        case 0x9C76FB3A: /* Rastan Saga */
        case 0x9FA727A0: /* Street Hero (Proto 0) */
        case 0xFB481971: /* Street Hero (Proto 1) */
        case 0x10DBBEF4: /* Super Kick Off */
        case 0xDA8E95A9: /* WWF - Steel Cage Challenge */
            returnVal = true; break;
    }

    return returnVal;
}

/* this function deals with detecting which button was touched/untouched */
void util_dealWithTouch(EmulatorContainer *ec, SDL_Collection s, SDL_Event *event)
{
    /* get X and Y coordinates of touch, as well as the finger ID */
    float x = (*event).tfinger.x * s->usableScreenWidth;
    float y = (*event).tfinger.y * s->usableScreenHeight;
    SDL_Rect touchLocation;
    touchLocation.x = x;
    touchLocation.y = y;
    touchLocation.w = 1;
    touchLocation.h = 1;
    SDL_FingerID fingerId = (*event).tfinger.fingerId;

    /* test if finger is up or down */
    emubyte tempPort = 0;
    switch ((*event).type) {
        case SDL_FINGERDOWN: /* test which button is being pressed (if any) */
                             if (SDL_HasIntersection(&touchLocation, s->upRect) == SDL_TRUE) {
                                 if ((*ec).touches.up == -1) {
                                     (*ec).touches.up = fingerId;
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xFE);
                                 }
                             } else if (SDL_HasIntersection(&touchLocation, s->downRect) == SDL_TRUE) {
                                 if ((*ec).touches.down == -1) {
                                     (*ec).touches.down = fingerId;
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xFD);
                                 }
                             } else if (SDL_HasIntersection(&touchLocation, s->leftRect) == SDL_TRUE) {
                                 if ((*ec).touches.left == -1) {
                                     (*ec).touches.left = fingerId;
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xFB);
                                 }
                             } else if (SDL_HasIntersection(&touchLocation, s->rightRect) == SDL_TRUE) {
                                 if ((*ec).touches.right == -1) {
                                     (*ec).touches.right = fingerId;
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xF7);
                                 }
                             } else if (SDL_HasIntersection(&touchLocation, s->upLeftRect) == SDL_TRUE) {
                                 if ((*ec).touches.upLeft == -1) {
                                     (*ec).touches.upLeft = fingerId;
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xFE);
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xFB);
                                 }
                             } else if (SDL_HasIntersection(&touchLocation, s->upRightRect) == SDL_TRUE) {
                                 if ((*ec).touches.upRight == -1) {
                                     (*ec).touches.upRight = fingerId;
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xFE);
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xF7);
                                 }
                             } else if (SDL_HasIntersection(&touchLocation, s->downLeftRect) == SDL_TRUE) {
                                 if ((*ec).touches.downLeft == -1) {
                                     (*ec).touches.downLeft = fingerId;
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xFD);
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xFB);
                                 }
                             } else if (SDL_HasIntersection(&touchLocation, s->downRightRect) == SDL_TRUE) {
                                 if ((*ec).touches.downRight == -1) {
                                     (*ec).touches.downRight = fingerId;
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xFD);
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xF7);
                                 }
                             } else if (SDL_HasIntersection(&touchLocation, s->pauseRect) == SDL_TRUE) {
                                 if ((*ec).touches.pauseStart == -1) {
                                     (*ec).touches.pauseStart = fingerId;
                                     if ((*ec).isGameGear) {
                                         console_handleTempStart((*ec).console, 3, 0);
                                     }
                                 }
                             } else if (SDL_HasIntersection(&touchLocation, s->bothRect) == SDL_TRUE) {
                                 if ((*ec).touches.both == -1) {
                                     (*ec).touches.both = fingerId;
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xEF);
                                     tempPort = console_handleTempDC((*ec).console, 0, 0);
                                     console_handleTempDC((*ec).console, 1, tempPort & 0xDF);
                                 }
                             } else {
                                 /* this is where we do a bit of math */
                                 float buttonOneX = s->buttonsRect->x + (s->buttonsRect->w / 4);
                                 float buttonOneY = s->buttonsRect->y + (s->buttonsRect->h / 2);
                                 float buttonTwoX = s->buttonsRect->x + (s->buttonsRect->w * 0.75);
                                 float buttonTwoY = buttonOneY;

                                 /* button radii */
                                 float buttonOneRadius = (s->buttonsRect->w / 4) * 0.9;
                                 float buttonTwoRadius = buttonOneRadius;

                                 /* button one calculation */
                                 signed_emuint tempX = abs((int)(x - buttonOneX));
                                 signed_emuint tempY = abs((int)(y - buttonOneY));
                                 tempX = tempX * tempX;
                                 tempY = tempY * tempY;
                                 float distanceToButtonOne = sqrtf((float)(tempX + tempY));

                                 /* button two calculation */
                                 tempX = abs((int)(x - buttonTwoX));
                                 tempY = abs((int)(y - buttonTwoY));
                                 tempX = tempX * tempX;
                                 tempY = tempY * tempY;
                                 float distanceToButtonTwo = sqrtf((float)(tempX + tempY));

                                 /* now run checks */
                                 if (distanceToButtonOne < buttonOneRadius) {
                                     if ((*ec).touches.buttonOne == -1) {
                                         (*ec).touches.buttonOne = fingerId;
                                         tempPort = console_handleTempDC((*ec).console, 0, 0);
                                         console_handleTempDC((*ec).console, 1, tempPort & 0xEF);
                                     }
                                 } else if (distanceToButtonTwo < buttonTwoRadius) {
                                     if ((*ec).touches.buttonTwo == -1) {
                                         (*ec).touches.buttonTwo = fingerId;
                                         tempPort = console_handleTempDC((*ec).console, 0, 0);
                                         console_handleTempDC((*ec).console, 1, tempPort & 0xDF);
                                     }
                                 } else {
                                     if ((*ec).touches.nothing == -1) {
                                         (*ec).touches.nothing = fingerId;
                                     }
                                 }
                             }
                             break;
        case SDL_FINGERMOTION: /* this checks if we have moved onto another button whilst still pressing down */
                               /* check if we have moved off the original buton */
                               {
                               emubool movedOff = false;

                               if ((*ec).touches.up == fingerId) {
                                   if (SDL_HasIntersection(&touchLocation, s->upRect) == SDL_FALSE) {
                                       (*ec).touches.up = -1;
                                       movedOff = true;
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x01);
                                   }
                               } else if ((*ec).touches.down == fingerId) {
                                   if (SDL_HasIntersection(&touchLocation, s->downRect) == SDL_FALSE) {
                                       (*ec).touches.down = -1;
                                       movedOff = true;
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x02);
                                   }
                               } else if ((*ec).touches.left == fingerId) {
                                   if (SDL_HasIntersection(&touchLocation, s->leftRect) == SDL_FALSE) {
                                       (*ec).touches.left = -1;
                                       movedOff = true;
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x04);
                                   }
                               } else if ((*ec).touches.right == fingerId) {
                                   if (SDL_HasIntersection(&touchLocation, s->rightRect) == SDL_FALSE) {
                                       (*ec).touches.right = -1;
                                       movedOff = true;
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x08);
                                   }
                               } else if ((*ec).touches.upLeft == fingerId) {
                                   if (SDL_HasIntersection(&touchLocation, s->upLeftRect) == SDL_FALSE) {
                                       (*ec).touches.upLeft = -1;
                                       movedOff = true;
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x01);
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x04);
                                   }
                               } else if ((*ec).touches.upRight == fingerId) {
                                   if (SDL_HasIntersection(&touchLocation, s->upRightRect) == SDL_FALSE) {
                                       (*ec).touches.upRight = -1;
                                       movedOff = true;
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x01);
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x08);
                                   }
                               } else if ((*ec).touches.downLeft == fingerId) {
                                   if (SDL_HasIntersection(&touchLocation, s->downLeftRect) == SDL_FALSE) {
                                       (*ec).touches.downLeft = -1;
                                       movedOff = true;
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x02);
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x04);
                                   }
                               } else if ((*ec).touches.downRight == fingerId) {
                                   if (SDL_HasIntersection(&touchLocation, s->downRightRect) == SDL_FALSE) {
                                       (*ec).touches.downRight = -1;
                                       movedOff = true;
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x02);
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x08);
                                   }
                               } else if ((*ec).touches.pauseStart == fingerId) {
                                   if (SDL_HasIntersection(&touchLocation, s->pauseRect) == SDL_FALSE) {
                                       (*ec).touches.pauseStart = -1;
                                       movedOff = true;
                                       if ((*ec).isGameGear) {
                                           console_handleTempStart((*ec).console, 2, 0);
                                       } else {
                                           tempPort = console_handleTempPauseStatus((*ec).console, 1, true);
                                       }
                                   }
                               } else if ((*ec).touches.both == fingerId) {
                                   if (SDL_HasIntersection(&touchLocation, s->bothRect) == SDL_FALSE) {
                                       (*ec).touches.both = -1;
                                       movedOff = true;
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x10);
                                       tempPort = console_handleTempDC((*ec).console, 0, 0);
                                       console_handleTempDC((*ec).console, 1, tempPort | 0x20);
                                   }
                               } else {
                                   /* this is where we do a bit of math */
                                   float buttonOneX = s->buttonsRect->x + (s->buttonsRect->w / 4);
                                   float buttonOneY = s->buttonsRect->y + (s->buttonsRect->h / 2);
                                   float buttonTwoX = s->buttonsRect->x + (s->buttonsRect->w * 0.75);
                                   float buttonTwoY = buttonOneY;

                                   /* button radii */
                                   float buttonOneRadius = (s->buttonsRect->w / 4) * 0.9;
                                   float buttonTwoRadius = buttonOneRadius;

                                   /* button one calculation */
                                   signed_emuint tempX = abs((int)(x - buttonOneX));
                                   signed_emuint tempY = abs((int)(y - buttonOneY));
                                   tempX = tempX * tempX;
                                   tempY = tempY * tempY;
                                   float distanceToButtonOne = sqrtf((float)(tempX + tempY));

                                   /* button two calculation */
                                   tempX = abs((int)(x - buttonTwoX));
                                   tempY = abs((int)(y - buttonTwoY));
                                   tempX = tempX * tempX;
                                   tempY = tempY * tempY;
                                   float distanceToButtonTwo = sqrtf((float)(tempX + tempY));

                                   /* now run checks */
                                   if ((*ec).touches.buttonOne == fingerId) {
                                       if (distanceToButtonOne > buttonOneRadius) {
                                           (*ec).touches.buttonOne = -1;
                                           movedOff = true;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort | 0x10);
                                       }
                                   } else if ((*ec).touches.buttonTwo == fingerId) {
                                       if (distanceToButtonTwo > buttonTwoRadius) {
                                           (*ec).touches.buttonTwo = -1;
                                           movedOff = true;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort | 0x20);
                                       }
                                   } else if ((*ec).touches.nothing == fingerId) {
                                       if (SDL_HasIntersection(&touchLocation, s->upRect) == SDL_TRUE ||
                                           SDL_HasIntersection(&touchLocation, s->downRect) == SDL_TRUE ||
                                           SDL_HasIntersection(&touchLocation, s->leftRect) == SDL_TRUE ||
                                           SDL_HasIntersection(&touchLocation, s->rightRect) == SDL_TRUE ||
                                           SDL_HasIntersection(&touchLocation, s->upLeftRect) == SDL_TRUE ||
                                           SDL_HasIntersection(&touchLocation, s->upRightRect) == SDL_TRUE ||
                                           SDL_HasIntersection(&touchLocation, s->downLeftRect) == SDL_TRUE ||
                                           SDL_HasIntersection(&touchLocation, s->downRightRect) == SDL_TRUE ||
                                           SDL_HasIntersection(&touchLocation, s->pauseRect) == SDL_TRUE ||
                                           SDL_HasIntersection(&touchLocation, s->bothRect) == SDL_TRUE ||
                                           distanceToButtonOne < buttonOneRadius ||
                                           distanceToButtonTwo < buttonTwoRadius) {
                                           (*ec).touches.nothing = -1;
                                           movedOff = true;
                                       }
                                   }
                               }

                               /* now check if we are somewhere else */
                               if (movedOff) {
                                   if (SDL_HasIntersection(&touchLocation, s->upRect) == SDL_TRUE) {
                                       if ((*ec).touches.up == -1) {
                                           (*ec).touches.up = fingerId;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xFE);
                                       }
                                   } else if (SDL_HasIntersection(&touchLocation, s->downRect) == SDL_TRUE) {
                                       if ((*ec).touches.down == -1) {
                                           (*ec).touches.down = fingerId;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xFD);
                                       }
                                   } else if (SDL_HasIntersection(&touchLocation, s->leftRect) == SDL_TRUE) {
                                       if ((*ec).touches.left == -1) {
                                           (*ec).touches.left = fingerId;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xFB);
                                       }
                                   } else if (SDL_HasIntersection(&touchLocation, s->rightRect) == SDL_TRUE) {
                                       if ((*ec).touches.right == -1) {
                                           (*ec).touches.right = fingerId;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xF7);
                                       }
                                   } else if (SDL_HasIntersection(&touchLocation, s->upLeftRect) == SDL_TRUE) {
                                       if ((*ec).touches.upLeft == -1) {
                                           (*ec).touches.upLeft = fingerId;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xFE);
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xFB);
                                       }
                                   } else if (SDL_HasIntersection(&touchLocation, s->upRightRect) == SDL_TRUE) {
                                       if ((*ec).touches.upRight == -1) {
                                           (*ec).touches.upRight = fingerId;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xFE);
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xF7);
                                       }
                                   } else if (SDL_HasIntersection(&touchLocation, s->downLeftRect) == SDL_TRUE) {
                                       if ((*ec).touches.downLeft == -1) {
                                           (*ec).touches.downLeft = fingerId;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xFD);
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xFB);
                                       }
                                   } else if (SDL_HasIntersection(&touchLocation, s->downRightRect) == SDL_TRUE) {
                                       if ((*ec).touches.downRight == -1) {
                                           (*ec).touches.downRight = fingerId;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xFD);
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xF7);
                                       }
                                   } else if (SDL_HasIntersection(&touchLocation, s->pauseRect) == SDL_TRUE) {
                                       if ((*ec).touches.pauseStart == -1) {
                                           (*ec).touches.pauseStart = fingerId;
                                           if ((*ec).isGameGear) {
                                               console_handleTempStart((*ec).console, 3, 0);
                                           }
                                       }
                                   } else if (SDL_HasIntersection(&touchLocation, s->bothRect) == SDL_TRUE) {
                                       if ((*ec).touches.both == -1) {
                                           (*ec).touches.both = fingerId;
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xEF);
                                           tempPort = console_handleTempDC((*ec).console, 0, 0);
                                           console_handleTempDC((*ec).console, 1, tempPort & 0xDF);
                                       }
                                   } else {
                                       /* this is where we do a bit of math */
                                       float buttonOneX = s->buttonsRect->x + (s->buttonsRect->w / 4);
                                       float buttonOneY = s->buttonsRect->y + (s->buttonsRect->h / 2);
                                       float buttonTwoX = s->buttonsRect->x + (s->buttonsRect->w * 0.75);
                                       float buttonTwoY = buttonOneY;

                                       /* button radii */
                                       float buttonOneRadius = (s->buttonsRect->w / 4) * 0.9;
                                       float buttonTwoRadius = buttonOneRadius;

                                       /* button one calculation */
                                       signed_emuint tempX = abs((int)(x - buttonOneX));
                                       signed_emuint tempY = abs((int)(y - buttonOneY));
                                       tempX = tempX * tempX;
                                       tempY = tempY * tempY;
                                       float distanceToButtonOne = sqrtf((float)(tempX + tempY));

                                       /* button two calculation */
                                       tempX = abs((int)(x - buttonTwoX));
                                       tempY = abs((int)(y - buttonTwoY));
                                       tempX = tempX * tempX;
                                       tempY = tempY * tempY;
                                       float distanceToButtonTwo = sqrtf((float)(tempX + tempY));

                                       /* now run checks */
                                       if (distanceToButtonOne < buttonOneRadius) {
                                           if ((*ec).touches.buttonOne == -1) {
                                               (*ec).touches.buttonOne = fingerId;
                                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                                               console_handleTempDC((*ec).console, 1, tempPort & 0xEF);
                                           }
                                       } else if (distanceToButtonTwo < buttonTwoRadius) {
                                           if ((*ec).touches.buttonTwo == -1) {
                                               (*ec).touches.buttonTwo = fingerId;
                                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                                               console_handleTempDC((*ec).console, 1, tempPort & 0xDF);
                                           }
                                       } else {
                                           if ((*ec).touches.nothing == -1) {
                                               (*ec).touches.nothing = fingerId;
                                           }
                                       }
                                   }
                               }}
                               break;
        case SDL_FINGERUP: /* now check whether this finger up event corresponds to a previous button press */
                           if ((*ec).touches.up != -1 &&
                               (*ec).touches.up == fingerId) {
                               (*ec).touches.up = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x01);
                           } else if ((*ec).touches.down != -1 &&
                                      (*ec).touches.down == fingerId) {
                               (*ec).touches.down = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x02);
                           } else if ((*ec).touches.left != -1 &&
                                      (*ec).touches.left == fingerId) {
                               (*ec).touches.left = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x04);
                           } else if ((*ec).touches.right != -1 &&
                                      (*ec).touches.right == fingerId) {
                               (*ec).touches.right = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x08);
                           } else if ((*ec).touches.upLeft != -1 &&
                                      (*ec).touches.upLeft == fingerId) {
                               (*ec).touches.upLeft = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x01);
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x04);
                           } else if ((*ec).touches.upRight != -1 &&
                                      (*ec).touches.upRight == fingerId) {
                               (*ec).touches.upRight = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x01);
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x08);
                           } else if ((*ec).touches.downLeft != -1 &&
                                      (*ec).touches.downLeft == fingerId) {
                               (*ec).touches.downLeft = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x02);
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x04);
                           } else if ((*ec).touches.downRight != -1 &&
                                      (*ec).touches.downRight == fingerId) {
                               (*ec).touches.downRight = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x02);
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x08);
                           } else if ((*ec).touches.pauseStart != -1 &&
                                      (*ec).touches.pauseStart == fingerId) {
                               (*ec).touches.pauseStart = -1;
                               if ((*ec).isGameGear) {
                                   console_handleTempStart((*ec).console, 2, 0);
                               } else {
                                   tempPort = console_handleTempPauseStatus((*ec).console, 1, true);
                               }
                           } else if ((*ec).touches.both != -1 &&
                                      (*ec).touches.both == fingerId) {
                               (*ec).touches.both = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x10);
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x20);
                           } else if ((*ec).touches.buttonOne != -1 &&
                                      (*ec).touches.buttonOne == fingerId) {
                               (*ec).touches.buttonOne = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x10);
                           } else if ((*ec).touches.buttonTwo != -1 &&
                                      (*ec).touches.buttonTwo == fingerId) {
                               (*ec).touches.buttonTwo = -1;
                               tempPort = console_handleTempDC((*ec).console, 0, 0);
                               console_handleTempDC((*ec).console, 1, tempPort | 0x20);
                           } else if ((*ec).touches.nothing != -1 &&
                                      (*ec).touches.nothing == fingerId) {
                               (*ec).touches.nothing = -1;
                           }
                           break;
        default: break;
    }

}

/* this function sends an event which quits the emulator */
void util_handleQuit(emuint userEventCode)
{
    /* define user event for quitting the event loop */
    SDL_Event event;
    memset((void *)&event, 0, sizeof(event));
    event.user.code = MASTEREMU_QUIT;
    event.type = userEventCode;
    SDL_PushEvent(&event);
}

/* this function deals with resize events and is also called during SDL setup */
emuint util_handleWindowResize(EmulatorContainer *ec, SDL_Collection s, emubool wipeTouches)
{
    /* determine new screen resolution */
    SDL_DisplayMode m;
    if (SDL_GetCurrentDisplayMode(0, &m) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't get SDL display mode...\n");
        return ERROR_SIZING_WINDOW;
    }

    /* setup window SDL rectangle */
    s->screenWidth = m.w;
    s->screenHeight = m.h;

    /* determine usable screen space */
    if (SDL_GetRendererOutputSize(s->renderer, &s->usableScreenWidth, &s->usableScreenHeight)) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't get usable screen space...\n");
        return ERROR_SIZING_WINDOW;
    }
    s->windowRect->x = 0;
    s->windowRect->y = 0;
    if (s->usableScreenWidth < s->usableScreenHeight) {
        s->windowRect->w = s->usableScreenWidth;
        s->windowRect->h = s->usableScreenWidth * 0.75;
    } else {
        if (ec->noStretching) {
            s->windowRect->w = s->usableScreenHeight * 1.3333;
            s->windowRect->h = s->usableScreenHeight;
            s->windowRect->x = (s->usableScreenWidth - s->windowRect->w) / 2;
        } else {
            s->windowRect->w = s->usableScreenWidth;
            s->windowRect->h = s->usableScreenHeight;
        }
    }

    /* redo rect parameters for new screen layout */
    s->dpadRect->w = s->pixelsForOneInch * 1;
    s->dpadRect->h = s->dpadRect->w;
    s->buttonsRect->w = s->pixelsForOneInch * 1;
    s->buttonsRect->h = s->buttonsRect->w / 2;
    s->buttonOneRect->w = s->buttonTwoRect->w = s->buttonsRect->w / 2;
    s->buttonOneRect->h = s->buttonTwoRect->h = s->buttonsRect->h;
    s->pauseRect->w = s->pixelsForOneInch * 0.4;
    s->pauseRect->h = s->pauseRect->w / 2;
    s->bothRect->w = s->pauseRect->w;
    s->bothRect->h = s->pauseRect->h;

    s->dpadRect->x = 0;
    s->dpadRect->y = s->usableScreenHeight - s->dpadRect->h;
    s->buttonsRect->x = s->usableScreenWidth - s->buttonsRect->w;
    s->buttonsRect->y = s->usableScreenHeight - s->buttonsRect->h;
    s->buttonOneRect->x = s->buttonsRect->x;
    s->buttonTwoRect->x = s->buttonsRect->x + s->buttonOneRect->w;
    s->buttonOneRect->y = s->buttonTwoRect->y = s->buttonsRect->y;
    s->pauseRect->x = s->buttonsRect->x + ((s->buttonsRect->w - (s->pauseRect->w * 2)) / 3);
    s->pauseRect->y = s->buttonsRect->y - s->pauseRect->h - (s->pixelsForOneInch * 0.125);
    s->bothRect->x = s->pauseRect->x + s->pauseRect->w + ((s->buttonsRect->w - (s->bothRect->w * 2)) / 3);
    s->bothRect->y = s->pauseRect->y;

    s->upRect->w = s->dpadRect->w / 3;
    s->upRect->h = s->upRect->w;
    s->downRect->w = s->upRect->w;
    s->downRect->h = s->upRect->w;
    s->leftRect->w = s->upRect->w;
    s->leftRect->h = s->upRect->w;
    s->rightRect->w = s->upRect->w;
    s->rightRect->h = s->upRect->w;
    s->upLeftRect->w = s->upRect->w;
    s->upLeftRect->h = s->upRect->w;
    s->upRightRect->w = s->upRect->w;
    s->upRightRect->h = s->upRect->w;
    s->downLeftRect->w = s->upRect->w;
    s->downLeftRect->h = s->upRect->w;
    s->downRightRect->w = s->upRect->w;
    s->downRightRect->h = s->upRect->w;

    s->upRect->x = s->dpadRect->x + s->upRect->w;
    s->upRect->y = s->dpadRect->y;
    s->downRect->x = s->dpadRect->x + s->downRect->w;
    s->downRect->y = s->dpadRect->y + (s->downRect->h * 2);
    s->leftRect->x = s->dpadRect->x;
    s->leftRect->y = s->dpadRect->y + s->leftRect->h;
    s->rightRect->x = s->dpadRect->x + (s->rightRect->w * 2);
    s->rightRect->y = s->dpadRect->y + s->rightRect->h;
    s->upLeftRect->x = s->dpadRect->x;
    s->upLeftRect->y = s->dpadRect->y;
    s->upRightRect->x = s->dpadRect->x + (s->upRightRect->w * 2);
    s->upRightRect->y = s->dpadRect->y;
    s->downLeftRect->x = s->dpadRect->x;
    s->downLeftRect->y = s->dpadRect->y + (s->downLeftRect->h * 2);
    s->downRightRect->x = s->dpadRect->x + (s->downRightRect->w * 2);
    s->downRightRect->y = s->dpadRect->y + (s->downRightRect->h * 2);

    /* reset all touches */
    if (wipeTouches) {
        (*ec).touches.up = -1;
        (*ec).touches.down = -1;
        (*ec).touches.left = -1;
        (*ec).touches.right = -1;
        (*ec).touches.upLeft = -1;
        (*ec).touches.upRight = -1;
        (*ec).touches.downLeft = -1;
        (*ec).touches.downRight = -1;
        (*ec).touches.buttonOne = -1;
        (*ec).touches.buttonTwo = -1;
        (*ec).touches.pauseStart = -1;
        (*ec).touches.both = -1;
    }

    return ALL_GOOD;
}

/* this function saves the state of the emulator */
emuint util_saveState(EmulatorContainer *ec, char *fileName)
{
    /* define variables */
    emubyte *saveState = NULL;

    /* determine where to save state file */
    char *internalPath = (char *)SDL_AndroidGetInternalStoragePath();
    if (internalPath == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't get internal storage path...");
        return ERROR_GETTING_INTERNAL_PATH;
    }

    /* now we create a subdirectory using the checksum value if it doesn't exist */
    char checksumStr[9];
    if (sprintf(checksumStr, "%08x", (*ec).romChecksum) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't convert ROM checksum to string...");
        return ERROR_CONVERTING_CHECKSUM;
    }

    /* now we formulate directory path for this ROM */
    char directoryPath[strlen(internalPath) + /* length of internal storage path */
                       1 + /* forward slash */
                       8 + /* checksum string */
                       1 /* null terminator */];
    if (sprintf(directoryPath, "%s/%s", internalPath, checksumStr) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create ROM state directory path...");
        return ERROR_GETTING_ROMSTATE_PATH;
    }

    /* check if directory exists, and if it doesn't, create it */
    FILE *folder = fopen(directoryPath, "rb");
    if (folder == NULL) {
        __android_log_print(ANDROID_LOG_VERBOSE, "util.c", "Creating new save state folder for this ROM at %s", directoryPath);
        if (mkdir(directoryPath, 0777) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create ROM state directory path...");
            return ERROR_CREATING_ROMSTATE_FOLDER;
        }
    } else {
        if (fclose(folder) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't close ROM state directory path...");
            return ERROR_CLOSING_ROMSTATE_FOLDER;
        }
    }

    /* now create filename string */
    char filePath[strlen(directoryPath) + /* length of directory path */
                  1 + /* forward slash */
                  strlen(fileName) + /* name of save state file */
                  1 /* null terminator */];
    if (sprintf(filePath, "%s/%s", directoryPath, fileName) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create ROM state directory path...");
        return ERROR_GETTING_FINAL_ROMSTATE_PATH;
    }

    /* save current state */
    saveState = console_saveState((*ec).console);
    if (saveState == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't get save state...\n");
        return ERROR_COMPILING_STATE;
    }
    emuint saveStateSize = (saveState[7] << 24) | (saveState[6] << 16) | (saveState[5] << 8) | saveState[4];
    FILE *saveStateFile = fopen(filePath, "wb");
    if (saveStateFile == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create save state file...\n");
        return ERROR_CREATING_ROMSTATE_FILE;
    }
    if (fwrite((void *)saveState, saveStateSize, 1, saveStateFile) != 1) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't write to save state file...\n");
        return ERROR_WRITING_STATE;
    }
    free((void *)saveState);
    if (fclose(saveStateFile)) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't close save state file...\n");
        return ERROR_CLOSING_ROMSTATE_FILE;
    }
    __android_log_print(ANDROID_LOG_VERBOSE, "util.c", "Created/updated save state file at %s", filePath);

    return ALL_GOOD;
}

/* this function loads the state of the emulator */
emuint util_loadState(EmulatorContainer *ec, char *fileName, emubyte **saveState)
{
    /* set pointer to NULL initially */
    *saveState = NULL;

    /* determine where to get state file from */
    char *internalPath = (char *)SDL_AndroidGetInternalStoragePath();
    if (internalPath == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't get internal storage path...");
        return ERROR_GETTING_INTERNAL_PATH;
    }

    /* now we find the subdirectory using the checksum value */
    char checksumStr[9];
    if (sprintf(checksumStr, "%08x", (*ec).romChecksum) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't convert ROM checksum to string...");
        return ERROR_CONVERTING_CHECKSUM;
    }

    /* now we formulate directory path for this ROM */
    char directoryPath[strlen(internalPath) + /* length of internal storage path */
                       1 + /* forward slash */
                       8 + /* checksum string */
                       1 /* null terminator */];
    if (sprintf(directoryPath, "%s/%s", internalPath, checksumStr) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create ROM state directory path...");
        return ERROR_GETTING_ROMSTATE_PATH;
    }

    /* now create filename string */
    char filePath[strlen(directoryPath) + /* length of directory path */
                  1 + /* forward slash */
                  strlen(fileName) + /* name of save state file */
                  1 /* null terminator */];
    if (sprintf(filePath, "%s/%s", directoryPath, fileName) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create ROM state directory path...");
        return ERROR_GETTING_FINAL_ROMSTATE_PATH;
    }

    /* check if a save state if present and load and perform sanity checks if so */
    FILE *saveStateFile = fopen(filePath, "rb");
    if (saveStateFile != NULL) {
        /* find save state file size */
        signed_emulong saveStateFileSize;
        if (fseek(saveStateFile, 0, SEEK_END)) { /* potentially non-portable */
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't seek to end of save state file...\n");
            return ERROR_SEEKING_ON_STATE_FILE;
        }
        if ((saveStateFileSize = ftell(saveStateFile)) == -1) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't get save state file size...\n");
            return ERROR_GETTING_STATE_FILE_SIZE;
        }
        if (fseek(saveStateFile, 0, SEEK_SET)) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't seek to beginning of save state file...\n");
            return ERROR_SEEKING_ON_STATE_FILE;
        }

        /* read save state file into memory buffer */
        if ((*saveState = malloc((size_t)saveStateFileSize)) == NULL) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't allocate memory for save state data...\n");
            return ERROR_ALLOCATING_MEMORY;
        }
        if (fread((void *)*saveState, (size_t)saveStateFileSize, 1, saveStateFile) != 1) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't copy save state data from file to memory buffer...\n");
            return ERROR_READING_STATE_TO_BUFFER;
        }

        /* close save state file */
        if (fclose(saveStateFile) != 0) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't close save state file...\n");
            return ERROR_CLOSING_ROMSTATE_FILE;
        }

        /* now run a sanity check on the size, making sure it matches */
        emuint saveStateSize = ((*saveState)[7] << 24) | ((*saveState)[6] << 16) | ((*saveState)[5] << 8) | (*saveState)[4];
        if (saveStateSize != saveStateFileSize) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "File size in save state header does not match actual file size...\n");
            return ERROR_ON_STATE_SANITY_CHECK;
        }

        /* now check the checksum of the save state is correct */
        emuint checksum1 = calculateCRC32(*saveState, saveStateSize - 4);
        emuint checksum2 = ((*saveState)[saveStateSize - 1] << 24) | ((*saveState)[saveStateSize - 2] << 16) | ((*saveState)[saveStateSize - 3] << 8) | (*saveState)[saveStateSize - 4];
        if (checksum1 != checksum2) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "Save state file checksum does not match computed checksum...\n");
            return ERROR_ON_CHECKSUM_CHECK;
        }

        /* now check the save state ROM checksum is for the game we are loading */
        emuint checksum3 = ((*saveState)[49438] << 24) | ((*saveState)[49437] << 16) | ((*saveState)[49436] << 8) | (*saveState)[49435];
        if ((*ec).romChecksum != checksum3) {
            __android_log_print(ANDROID_LOG_ERROR, "util.c", "This save state file is not for the ROM you have loaded...\n");
            return ERROR_STATE_ROM_MISMATCH;
        }

        __android_log_print(ANDROID_LOG_VERBOSE, "util.c", "Loaded save state file from %s", filePath);
    }

    return ALL_GOOD;
}

/* this function deals with detecting which button was pressed on a physical controller */
void util_dealWithButtons(EmuBundle *eb)
{
    /* first, get controller state, but only if one is enabled */
    if (eb->s->gameControllers[0] == NULL)
        return;

    util_pollAndSetControllerState(eb);

    /* store pointers locally for easier reference */
    ButtonMapping *bm = &eb->ec->buttonMapping;
    CurrentControllerState *ccs = &eb->ec->currentControllerState;
    Console c = eb->ec->console;
    emubool isGameGear = eb->ec->isGameGear;

    /* read current DC register state */
    emubyte tempDC = console_handleTempDC(c, 0, 0);
    emubyte originalTempDC = tempDC;

    /* now merge/mask bits into it as appropriate */
    if (ccs->buttonArray[bm->up])
        tempDC &= 0xFE;
    else
        tempDC |= 0x01;

    if (ccs->buttonArray[bm->down])
        tempDC &= 0xFD;
    else
        tempDC |= 0x02;

    if (ccs->buttonArray[bm->left])
        tempDC &= 0xFB;
    else
        tempDC |= 0x04;

    if (ccs->buttonArray[bm->right])
        tempDC &= 0xF7;
    else
        tempDC |= 0x08;

    if (ccs->buttonArray[bm->buttonOne])
        tempDC &= 0xEF;
    else
        tempDC |= 0x10;

    if (ccs->buttonArray[bm->buttonTwo])
        tempDC &= 0xDF;
    else
        tempDC |= 0x20;

    /* write DC value if it has changed */
    if (tempDC != originalTempDC)
        console_handleTempDC(c, 1, tempDC);

    /* now handle pause/start button */
    if (isGameGear) {
        if (ccs->buttonArray[bm->pauseStart])
            console_handleTempStart(c, 3, 0);
        else
            console_handleTempStart(c, 2, 0);
    } else {
        /* use our atomic flag to stop this button being pressed again
           until it is released or we can trigger a flurry of NMIs and
           jittery pause/unpause behaviour */
        if (ccs->buttonArray[bm->pauseStart] && !SDL_AtomicGet(&ccs->noPauseAllowed)) {
            console_handleTempPauseStatus(c, 1, true);
            SDL_AtomicSet(&ccs->noPauseAllowed, 1);
        }
    }

    /* handle back button */
    if (ccs->buttonArray[bm->back])
        init_loadPauseMenu(eb);
}

/* this function pushes an event to the queue that triggers a screen repaint, and also is
   where we poll the state of the controller if one is attached */
void util_triggerPainting(EmuBundle *eb)
{
    /* mirror VDP buffer to display frame buffer - should help prevent tearing */
    console_handleFrame(eb->ec->console, (void *)eb->s->displayFrame);

    /* poll controller here */
    util_dealWithButtons(eb);

    /* create event and push it to event queue */
    SDL_Event e;
    memset((void *)&e, 0, sizeof(e));
    e.type = eb->userEventType;
    e.user.code = ACTION_PAINT; /* leet */
    e.user.data1 = (void *)eb;
    SDL_PushEvent(&e);
}

/* this function pushes an event to the queue that triggers a screen repaint for controller remapping mode */
void util_triggerRemapPainting(EmuBundle *eb)
{
    /* create event and push it to event queue */
    SDL_Event e;
    memset((void *)&e, 0, sizeof(e));
    e.type = eb->userEventType;
    e.user.code = ACTION_PAINT; /* leet */
    e.user.data1 = (void *)eb;
    SDL_PushEvent(&e);
}



/* this function loads the default button mappings */
void util_loadDefaultButtonMapping(EmulatorContainer *ec)
{
    ec->buttonMapping.up = SDL_CONTROLLER_BUTTON_DPAD_UP;
    ec->buttonMapping.down = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    ec->buttonMapping.left = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    ec->buttonMapping.right = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    ec->buttonMapping.buttonOne = SDL_CONTROLLER_BUTTON_A;
    ec->buttonMapping.buttonTwo = SDL_CONTROLLER_BUTTON_X;
    ec->buttonMapping.pauseStart = SDL_CONTROLLER_BUTTON_START;
    ec->buttonMapping.back = SDL_CONTROLLER_BUTTON_B;
}

/* this function loads a button mapping file if one exists, thus changing the layout of
 * the physical controller in use */
void util_loadButtonMapping(EmulatorContainer *ec)
{
    /* determine where to get mapping file from */
    char *internalPath = (char *)SDL_AndroidGetInternalStoragePath();
    if (internalPath == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't get internal storage path...");
        return;
    }

    /* now we formulate path for the mapping file */
    char buttonMappingPath[strlen(internalPath) + /* length of internal storage path */
                           1 + /* forward slash */
                           strlen("button_mapping.ini") +
                           1 /* null terminator */];

    /* form actual path now */
    if (sprintf(buttonMappingPath, "%s/%s", internalPath, "button_mapping.ini") < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create button mapping path...");
        return;
    }

    /* check if a button mapping file is present, and apply it if so - assume correct layout
     * due to internal nature */
    FILE *buttonMappingFile = fopen(buttonMappingPath, "rb");
    if (buttonMappingFile != NULL) {
        /* temporary local array for holding each line on the stack before conversion */
        char numStr[10];
        int num;

        /* read each line, and assign it to correct button in struct */
        for (int i = 0; i < 8; i++) {
            /* read into local string */
            char *numPtr = numStr;
            int c;
            while ((c = fgetc(buttonMappingFile)) != '\n' && c != EOF) {
                *numPtr++ = (char)c;
            }

            /* basic error check, should never get EOF, set default state and return */
            if (c == EOF) {
                fclose(buttonMappingFile);
                util_loadDefaultButtonMapping(ec);
                __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't read button mapping file...\n");
                return;
            }

            *numPtr = '\0';

            /* convert string to actual number, and assign it */
            num = atoi(numStr);
            switch (i) {
            case 0:
                ec->buttonMapping.up = num;
                break;
            case 1:
                ec->buttonMapping.down = num;
                break;
            case 2:
                ec->buttonMapping.left = num;
                break;
            case 3:
                ec->buttonMapping.right = num;
                break;
            case 4:
                ec->buttonMapping.buttonOne = num;
                break;
            case 5:
                ec->buttonMapping.buttonTwo = num;
                break;
            case 6:
                ec->buttonMapping.pauseStart = num;
                break;
            case 7:
                ec->buttonMapping.back = num;
                break;
            }
        }
        fclose(buttonMappingFile);
        __android_log_print(ANDROID_LOG_VERBOSE, "util.c", "Loaded button mapping file from %s", buttonMappingPath);
    }
}

/* this function writes a button mapping file */
void util_writeButtonMapping(emuint *mappings, emuint size)
{
    /* determine where to save mapping file to */
    char *internalPath = (char *)SDL_AndroidGetInternalStoragePath();
    if (internalPath == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't get internal storage path...");
        return;
    }

    /* now we formulate path for the mapping file */
    char buttonMappingPath[strlen(internalPath) + /* length of internal storage path */
                           1 + /* forward slash */
                           strlen("button_mapping.ini") +
                           1 /* null terminator */];

    /* form actual path now */
    if (sprintf(buttonMappingPath, "%s/%s", internalPath, "button_mapping.ini") < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "util.c", "Couldn't create button mapping path...");
        return;
    }

    /* create the mapping file and write the mappings to it */
    FILE *buttonMappingFile = fopen(buttonMappingPath, "wb");
    if (buttonMappingFile != NULL) {
        /* write mappings */
        for (emuint i = 0; i < size; i++)
            fprintf(buttonMappingFile, "%u\n", mappings[i]);

        fclose(buttonMappingFile);
        __android_log_print(ANDROID_LOG_VERBOSE, "util.c", "Wrote button mapping file to %s", buttonMappingPath);
    }
}

/* this function polls for and sets the state of all buttons we care about */
void util_pollAndSetControllerState(EmuBundle *eb)
{
    /* return here if no controller was detected on start */
    if (eb->s->gameControllers[0] == NULL)
        return;

    /* get most recent state of left joystick (for all controllers) */
    int xAxis = SDL_AtomicGet(&eb->ec->currentControllerState.xAxis);
    int yAxis = SDL_AtomicGet(&eb->ec->currentControllerState.yAxis);

    /* poll from 0 to SDL_CONTROLLER_BUTTON_MAX-1 as these are the SDL buttons we care about */
    for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
        emubyte buttonState = 0;

        /* poll every connected controller for the state of this button */
        for (int j = 0; j < 4; j++) {
            if (eb->s->gameControllers[j] != NULL) {
                SDL_GameControllerButton button = (SDL_GameControllerButton)i;

                /* check analogue stick if relevant - simplistic for now, no Pythagoras etc.
                 * and simple deadzone of +/- 8,000 */
                switch (button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP:
                    if (yAxis < -8000)
                        buttonState |= 1;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    if (yAxis > 8000)
                        buttonState |= 1;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    if (xAxis < -8000)
                        buttonState |= 1;
                    break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    if (xAxis > 8000)
                        buttonState |= 1;
                    break;
                }

                buttonState |= SDL_GameControllerGetButton(eb->s->gameControllers[j], button);
            } else {
                break;
            }
        }

        /* now set our internal controller state */
        if (buttonState) {
            eb->ec->currentControllerState.buttonArray[i] = true;
        } else {
            eb->ec->currentControllerState.buttonArray[i] = false;
        }
    }
}