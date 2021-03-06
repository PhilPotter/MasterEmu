/* MasterEmu initialisation header file
   copyright Phil Potter, 2019 */

#ifndef INIT_MASTEREMU
#define INIT_MASTEREMU

#include <jni.h>
#define ALL_GOOD 0
#define ERROR_UNABLE_TO_LOAD_ROM 3
#define ERROR_UNABLE_TO_CREATE_CONSOLE 4
#define ERROR_CREATING_LOGIC_THREAD 5
#define ERROR_CREATING_PAINT_SEMAPHORE 6
#define ERROR_SETTING_UP_SDL 7
#define ERROR_GETTING_INTERNAL_PATH 9
#define ERROR_CONVERTING_CHECKSUM 10
#define ERROR_GETTING_ROMSTATE_PATH 11
#define ERROR_CLOSING_ROMSTATE_FOLDER 12
#define ERROR_CREATING_ROMSTATE_FOLDER 13
#define ERROR_GETTING_FINAL_ROMSTATE_PATH 14
#define ERROR_SAVING_STATE 15
#define ERROR_SEEKING_ON_STATE_FILE 16
#define ERROR_GETTING_STATE_FILE_SIZE 17
#define ERROR_ALLOCATING_MEMORY 18
#define ERROR_READING_STATE_TO_BUFFER 19
#define ERROR_CLOSING_ROMSTATE_FILE 20
#define ERROR_WRITING_STATE 21
#define ERROR_CREATING_ROMSTATE_FILE 22
#define ERROR_COMPILING_STATE 23
#define ERROR_ON_STATE_SANITY_CHECK 24
#define ERROR_ON_CHECKSUM_CHECK 25
#define ERROR_STATE_ROM_MISMATCH 26
#define ERROR_ALLOCATING_CONSOLE_MEMORY 27

int start_emulator(JNIEnv *env, jclass cls, jobject obj, jbyteArray romData, jint romSize, jint params, jlongArray codesArray);
int LogicFunction(void *p);

#endif
