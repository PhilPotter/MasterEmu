/* MasterEmu specific datatypes file
   copyright Phil Potter, 2019 */

#ifndef DATATYPES
#define DATATYPES

#define CM_VERSION 0.1
#include <stdbool.h>
#include <stdint.h>

typedef unsigned char emubyte; /* for use with byte values */
typedef signed char signed_emubyte; /* for use with signed byte values */
typedef uint32_t emuint; /* for other uses */
typedef int32_t signed_emuint; /* signed version of the above */
typedef int64_t signed_emulong; /* for return values from some functions */
typedef uint64_t emulong; /* for unsigned return values from some functions */
typedef bool emubool; /* for boolean values */
typedef float emufloat; /* for floating point values */

typedef struct {
    emubyte value;
    emuint address;
} ArCheat;

typedef struct {
    emubyte value;
    emuint address;
    emubool cloakAndReferencePresent;
    emubyte cloak;
    emubyte reference;
} GgCheat;

typedef struct {
    emubool enabled;
    emuint cheatCount;
    ArCheat *cheats;
} ArCheatArray;

typedef struct {
    emubool enabled;
    emuint cheatCount;
    GgCheat *cheats;
} GgCheatArray;

#endif
