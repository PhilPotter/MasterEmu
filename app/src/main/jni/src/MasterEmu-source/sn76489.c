/* MasterEmu SN76489 sound chip source code file
   copyright Phil Potter, 2024 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <android/log.h>
#include "../../SDL-release-2.30.2/include/SDL.h"
#include "sn76489.h"

#define PAL_SIZE 4422
#define NTSC_SIZE 4466

/* this struct models the SN76489 chip's internal state */
struct SN76489 {
    emubyte *volumeRegisters; /* this models models the volume registers */
    signed_emuint *toneAndNoiseRegisters; /* this models the tone and noise registers */
    signed_emuint *counters; /* this models the tone/noise counters */
    signed_emubyte *polarity; /* this models the polarity of each channel */
    emubyte noise; /* this models the noise bit */
    emubyte currentlyLatchedRegister; /* this determines which register is latched */
    emuint linearFeedbackShiftRegister; /* models linear feedback shift register of SN76489 */
    emuint z80Cycles; /* this helps keep the chip in sync with the Z80 */
    emubool volumeLatched; /* this determines whether a volume register is latched */
    emubool soundDisabled; /* this determines whether the sound chip 'stays silent' */
    emubool isGameGear; /* this determines whether we are in Game Gear mode */
    emubool isPal; /* this determines whether we are in PAL mode */

    /* the following stores pre and post filter buffers for both channels, and an output buffer too */
    emufloat *leftPreFilter;
    emufloat *rightPreFilter;
    emufloat *leftPostFilter;
    emufloat *rightPostFilter;
    emubyte *outputBuffer;
    emuint channelCount;

    /* Console reference (mainly handy for Game Gear mode) */
    Console ms;

    /* the following variables are for storing SDL related audio state */
    SDL_AudioDeviceID deviceId;
};

/* this function creates a new SN76489 object and returns a pointer to it */
SN76489 createSN76489(Console ms, emubyte *soundchipState, emubool soundDisabled, emubool isGameGear, emubool isPal, emubyte *wholePointer, emuint audioId)
{
    /* first, we allocate memory for an SN76489 struct */
    SN76489 s = (SN76489)wholePointer;
    wholePointer += sizeof(struct SN76489);

    /* assign console reference */
    s->ms = ms;

    /* disable sound if specified */
    s->soundDisabled = soundDisabled;

    /* set Game Gear and PAL bools */
    s->isGameGear = isGameGear;
    s->isPal = isPal;

    /* set all sub-pointers to NULL now */
    s->polarity = NULL;
    s->counters = NULL;
    s->toneAndNoiseRegisters = NULL;
    s->volumeRegisters = NULL;

    /* set deviceId to zero */
    s->deviceId = 0;

    /* setup volume registers */
    s->volumeRegisters = wholePointer;
    wholePointer += 4;
    s->volumeRegisters[0] = 0x0F;
    s->volumeRegisters[1] = 0x0F;
    s->volumeRegisters[2] = 0x0F;
    s->volumeRegisters[3] = 0x0F;

    /* setup tone/noise registers, initialising them all to zero */
    s->toneAndNoiseRegisters = (signed_emuint *)wholePointer;
    memset((void *)s->toneAndNoiseRegisters, 0, sizeof(signed_emuint) * 4);
    wholePointer += sizeof(signed_emuint) * 4;

    /* setup tone/noise counters, initialising them all to zero */
    s->counters = (signed_emuint *)wholePointer;
    memset((void *)s->counters, 0, sizeof(signed_emuint) * 4);
    wholePointer += sizeof(signed_emuint) * 4;

    /* setup polarities */
    s->polarity = (signed_emubyte *)wholePointer;
    wholePointer += sizeof(signed_emubyte) * 4;
    s->polarity[0] = 1;
    s->polarity[1] = 1;
    s->polarity[2] = 1;
    s->polarity[3] = 1;

    /* set initial state of linear feedback shift register and noise output */
    s->linearFeedbackShiftRegister = 0;
    s->noise = 0;

    /* set initially latched register to tone0 */
    s->currentlyLatchedRegister = 0;
    s->volumeLatched = false;

    /* set cycle count */
    s->z80Cycles = 0;

    /* allocate buffer areas */
    emuint channelSize = s->isPal ? PAL_SIZE : NTSC_SIZE;
    emuint divisor = s->isPal ? 201 : 203;
    s->channelCount = 0;
    s->leftPreFilter = (emufloat *)wholePointer;
    wholePointer += sizeof(emufloat) * 4466;
    s->rightPreFilter = (emufloat *)wholePointer;
    wholePointer += sizeof(emufloat) * 4466;
    s->leftPostFilter = (emufloat *)wholePointer;
    wholePointer += sizeof(emufloat) * 880;
    s->rightPostFilter = (emufloat *)wholePointer;
    wholePointer += sizeof(emufloat) * 880;
    s->outputBuffer = wholePointer;
    wholePointer += 3520;

    /* start audio channel */
    if (audioId != 0)
        s->deviceId = audioId;
    else if (soundchip_startAudio(s) == NULL)
        return NULL;

    /* set state if provided */
    if (soundchipState != NULL) {
        /* set main state of SN76489 */
        emuint marker = 13;
        s->volumeRegisters[0] = soundchipState[marker++];
        s->volumeRegisters[1] = soundchipState[marker++];
        s->volumeRegisters[2] = soundchipState[marker++];
        s->volumeRegisters[3] = soundchipState[marker++];
        s->toneAndNoiseRegisters[0] = (soundchipState[marker + 3] << 24) | (soundchipState[marker + 2] << 16) | (soundchipState[marker + 1] << 8) | soundchipState[marker];
        marker += 4;
        s->toneAndNoiseRegisters[1] = (soundchipState[marker + 3] << 24) | (soundchipState[marker + 2] << 16) | (soundchipState[marker + 1] << 8) | soundchipState[marker];
        marker += 4;
        s->toneAndNoiseRegisters[2] = (soundchipState[marker + 3] << 24) | (soundchipState[marker + 2] << 16) | (soundchipState[marker + 1] << 8) | soundchipState[marker];
        marker += 4;
        s->toneAndNoiseRegisters[3] = (soundchipState[marker + 3] << 24) | (soundchipState[marker + 2] << 16) | (soundchipState[marker + 1] << 8) | soundchipState[marker];
        marker += 4;
        s->counters[0] = (soundchipState[marker + 3] << 24) | (soundchipState[marker + 2] << 16) | (soundchipState[marker + 1] << 8) | soundchipState[marker];
        marker += 4;
        s->counters[1] = (soundchipState[marker + 3] << 24) | (soundchipState[marker + 2] << 16) | (soundchipState[marker + 1] << 8) | soundchipState[marker];
        marker += 4;
        s->counters[2] = (soundchipState[marker + 3] << 24) | (soundchipState[marker + 2] << 16) | (soundchipState[marker + 1] << 8) | soundchipState[marker];
        marker += 4;
        s->counters[3] = (soundchipState[marker + 3] << 24) | (soundchipState[marker + 2] << 16) | (soundchipState[marker + 1] << 8) | soundchipState[marker];
        marker += 4;
        s->polarity[0] = soundchipState[marker++];
        s->polarity[1] = soundchipState[marker++];
        s->polarity[2] = soundchipState[marker++];
        s->polarity[3] = soundchipState[marker++];
        s->noise = soundchipState[marker++];
        s->currentlyLatchedRegister = soundchipState[marker++];
        s->linearFeedbackShiftRegister = (soundchipState[marker + 3] << 24) | (soundchipState[marker + 2] << 16) | (soundchipState[marker + 1] << 8) | soundchipState[marker];
        marker += 4;
        s->z80Cycles = (soundchipState[marker + 3] << 24) | (soundchipState[marker + 2] << 16) | (soundchipState[marker + 1] << 8) | soundchipState[marker];
        marker += 4;
        if (soundchipState[marker++] == 1)
            s->volumeLatched = true;
        else
            s->volumeLatched = false;
    }

    /* return object */
    return s;
}

/* this function destroys the SN76489 object */
void destroySN76489(SN76489 s)
{
    ;
}

/* this function writs data to the SN76489 registers in different ways,
   depending on how the incoming byte is formatted */
void soundchip_soundWrite(SN76489 s, emubyte b)
{
    /* determine if incoming byte is a LATCH/DATA or a DATA byte */
    if ((b & 0x80) == 128) { /* byte is a LATCH/DATA byte */
        /* set new latch values */
        s->currentlyLatchedRegister = (b & 0x60) >> 5;
        if ((b & 0x10) == 0)
            s->volumeLatched = false;
        else
            s->volumeLatched = true;

        /* write lower 4 bits to newly latched register */
        if (s->volumeLatched) {
            s->volumeRegisters[s->currentlyLatchedRegister] = b & 0x0F;
        } else {
            if (s->currentlyLatchedRegister != 3) {
                s->toneAndNoiseRegisters[s->currentlyLatchedRegister] =
                  (s->toneAndNoiseRegisters[s->currentlyLatchedRegister] & 0x3F0) | (b & 0x0F);

                /* detect if current register value is inaudible and modify it to produce a silent wave */
                if (s->toneAndNoiseRegisters[s->currentlyLatchedRegister] < 6)
                    s->toneAndNoiseRegisters[s->currentlyLatchedRegister] = 0;
            } else {
                s->toneAndNoiseRegisters[s->currentlyLatchedRegister] = b & 0x0F;
                s->linearFeedbackShiftRegister = 0x8000;
            }
        }
    } else { /* byte is a DATA byte */
        /* write the lower 6 bits of the byte to the upper 6 bits of the latched
           register, if it's a tone register, else write the lower 4 bits to
           the low 4 bits of the relevant register */
        if (s->volumeLatched) { 
            /* volume register latched so we just write lower 4 bits */
            s->volumeRegisters[s->currentlyLatchedRegister] = b & 0x0F;
        } else {
            /* tone or noise register is latched, so we act accordingly */
            if (s->currentlyLatchedRegister != 3) {
                s->toneAndNoiseRegisters[s->currentlyLatchedRegister] =
                  ((b << 4) & 0x3F0) | (s->toneAndNoiseRegisters[s->currentlyLatchedRegister] & 0x0F);

                /* detect if current register value is inaudible and modify it to produce a silent wave */
                if (s->toneAndNoiseRegisters[s->currentlyLatchedRegister] < 6)
                    s->toneAndNoiseRegisters[s->currentlyLatchedRegister] = 0;
            } else {
                s->toneAndNoiseRegisters[s->currentlyLatchedRegister] = b & 0x0F;
                s->linearFeedbackShiftRegister = 0x8000;
            }
        }
    }
}

/* this function runs the sound chip for the relevant number of cycles */
void soundchip_executeCycles(SN76489 s, emuint c)
{
    s->z80Cycles = s->z80Cycles + c;
    if (s->z80Cycles >= 16) {
        /* reset cycle count, compensating if number of cycles is over 16 */
        s->z80Cycles = s->z80Cycles - 16;

        /* begin executing one SN76489 cycle */

        /* deal with noise register counter */
        if (s->counters[3] <= 0) {
            /* reload noise counter from register */
            switch (s->toneAndNoiseRegisters[3] & 0x03) {
                case 0: s->counters[3] = 0x10; break;
                case 1: s->counters[3] = 0x20; break;
                case 2: s->counters[3] = 0x40; break;
                case 3: s->counters[3] = s->toneAndNoiseRegisters[2]; break;
            }

            /* deal with polarity shift */
            if (s->polarity[3] == -1) {
                /* polarity changes from -1 to +1, so LFSR is shifted right -
                   shifted bit is output, and bit 15 is calculated */
                s->polarity[3] = 1;
                s->noise = s->linearFeedbackShiftRegister & 0x01;

                /* calculate new bit 15 */
                emubyte inputBit;
                if ((s->toneAndNoiseRegisters[3] & 0x04) == 4) {
                    /* tap bits 0 and 3 */
                    emubyte bit0 = s->linearFeedbackShiftRegister & 0x01;
                    emubyte bit3 = (s->linearFeedbackShiftRegister & 0x08) >> 3;
                    inputBit = bit0 ^ bit3;
                } else {
                    inputBit = s->linearFeedbackShiftRegister & 0x01;
                }

                s->linearFeedbackShiftRegister = s->linearFeedbackShiftRegister >> 1;
                s->linearFeedbackShiftRegister = (inputBit << 15) |
                                                 (s->linearFeedbackShiftRegister & 0x7FFF);
            } else {
                s->polarity[3] = -1;
            }
        }

        /* calculate tone values */
        signed_emubyte tone0 = s->polarity[0];
        signed_emubyte tone1 = s->polarity[1];
        signed_emubyte tone2 = s->polarity[2];

        /* get volume for each channel */
        emubyte volume0 = ~(s->volumeRegisters[0]) & 0x0F;
        emubyte volume1 = ~(s->volumeRegisters[1]) & 0x0F;
        emubyte volume2 = ~(s->volumeRegisters[2]) & 0x0F;
        emubyte volume3 = ~(s->volumeRegisters[3]) & 0x0F;

        signed_emuint output0 = 0;
        switch (volume0) {
            case 1: output0 = tone0 * 1304; break;
            case 2: output0 = tone0 * 1642; break;
            case 3: output0 = tone0 * 2067; break;
            case 4: output0 = tone0 * 2603; break;
            case 5: output0 = tone0 * 3277; break;
            case 6: output0 = tone0 * 4125; break;
            case 7: output0 = tone0 * 5193; break;
            case 8: output0 = tone0 * 6568; break;
            case 9: output0 = tone0 * 8231; break;
            case 10: output0 = tone0 * 10362; break;
            case 11: output0 = tone0 * 13045; break;
            case 12: output0 = tone0 * 16422; break;
            case 13: output0 = tone0 * 20675; break;
            case 14: output0 = tone0 * 26028; break;
            case 15: output0 = tone0 * 32767; break;
            default: output0 = 0; break;
        }
        signed_emuint output1 = 0;
        switch (volume1) {
            case 1: output1 = tone1 * 1304; break;
            case 2: output1 = tone1 * 1642; break;
            case 3: output1 = tone1 * 2067; break;
            case 4: output1 = tone1 * 2603; break;
            case 5: output1 = tone1 * 3277; break;
            case 6: output1 = tone1 * 4125; break;
            case 7: output1 = tone1 * 5193; break;
            case 8: output1 = tone1 * 6568; break;
            case 9: output1 = tone1 * 8231; break;
            case 10: output1 = tone1 * 10362; break;
            case 11: output1 = tone1 * 13045; break;
            case 12: output1 = tone1 * 16422; break;
            case 13: output1 = tone1 * 20675; break;
            case 14: output1 = tone1 * 26028; break;
            case 15: output1 = tone1 * 32767; break;
            default: output1 = 0; break;
        }
        signed_emuint output2 = 0;
        switch (volume2) {
            case 1: output2 = tone2 * 1304; break;
            case 2: output2 = tone2 * 1642; break;
            case 3: output2 = tone2 * 2067; break;
            case 4: output2 = tone2 * 2603; break;
            case 5: output2 = tone2 * 3277; break;
            case 6: output2 = tone2 * 4125; break;
            case 7: output2 = tone2 * 5193; break;
            case 8: output2 = tone2 * 6568; break;
            case 9: output2 = tone2 * 8231; break;
            case 10: output2 = tone2 * 10362; break;
            case 11: output2 = tone2 * 13045; break;
            case 12: output2 = tone2 * 16422; break;
            case 13: output2 = tone2 * 20675; break;
            case 14: output2 = tone2 * 26028; break;
            case 15: output2 = tone2 * 32767; break;
            default: output2 = 0; break;
        }
        signed_emuint output3 = 0;
        switch (volume3) {
            case 1: output3 = s->noise * 1304; break;
            case 2: output3 = s->noise * 1642; break;
            case 3: output3 = s->noise * 2067; break;
            case 4: output3 = s->noise * 2603; break;
            case 5: output3 = s->noise * 3277; break;
            case 6: output3 = s->noise * 4125; break;
            case 7: output3 = s->noise * 5193; break;
            case 8: output3 = s->noise * 6568; break;
            case 9: output3 = s->noise * 8231; break;
            case 10: output3 = s->noise * 10362; break;
            case 11: output3 = s->noise * 13045; break;
            case 12: output3 = s->noise * 16422; break;
            case 13: output3 = s->noise * 20675; break;
            case 14: output3 = s->noise * 26028; break;
            case 15: output3 = s->noise * 32767; break;
            default: output3 = 0; break;
        }

        /* now merge into one sample or determine output channels based on GG reg */
        signed_emuint leftOutput = 0, rightOutput = 0;
        if (s->isGameGear) {
            emubyte channelBits = console_readPSGReg(s->ms);
            emuint leftChannels = 0, rightChannels = 0, leftMultiplyBy = 0, rightMultiplyBy = 0;

            /* determine left output value */
            if ((channelBits & 0x80) != 0) {
                leftOutput += output3;
                ++leftChannels;
            }
            if ((channelBits & 0x40) != 0) {
                leftOutput += output2;
                ++leftChannels;
            }
            if ((channelBits & 0x20) != 0) {
                leftOutput += output1;
                ++leftChannels;
            }
            if ((channelBits & 0x10) != 0) {
                leftOutput += output0;
                ++leftChannels;
            }
            leftOutput /= 2;
            if (leftOutput > 32767)
                leftOutput = 32767;
            else if (leftOutput < -32768)
                leftOutput = -32768;

            /* determine right output value */
            if ((channelBits & 0x08) != 0) {
                rightOutput += output3;
                ++rightChannels;
            }
            if ((channelBits & 0x04) != 0) {
                rightOutput += output2;
                ++rightChannels;
            }
            if ((channelBits & 0x02) != 0) {
                rightOutput += output1;
                ++rightChannels;
            }
            if ((channelBits & 0x01) != 0) {
                rightOutput += output0;
                ++rightChannels;
            }
            rightOutput /= 2;
            if (rightOutput > 32767)
                rightOutput = 32767;
            else if (rightOutput < -32768)
                rightOutput = -32768;

        } else {
            leftOutput = (output0 + output1 + output2 + output3) / 2;
            if (leftOutput > 32767)
                leftOutput = 32767;
            else if (leftOutput < -32768)
                leftOutput = -32768;
            rightOutput = leftOutput;
        }

        /* store in pre-filter buffer, pausing if we are on right sample */
        emuint channelSize = s->isPal ? PAL_SIZE : NTSC_SIZE;
        s->leftPreFilter[s->channelCount] = leftOutput;
        s->rightPreFilter[s->channelCount] = rightOutput;
        ++s->channelCount;

        if (s->channelCount == channelSize) {

            /* this is where we interpolate and output to the sound card */
            emufloat decimationRatio = s->isPal ? 201.0f : 203.0f;
            emufloat expansionRatio = 40.0f;

            /* run interpolation */
            emuint i;
            for (i = 0; i < 880; ++i) {
                emufloat samplingPos = i * (decimationRatio / expansionRatio);
                emuint samplingIndex = (emuint)samplingPos;
                emufloat difference = samplingPos - samplingIndex;
                s->leftPostFilter[i] = (1.0f - difference) * s->leftPreFilter[samplingIndex] +
                                       difference * s->leftPreFilter[samplingIndex + 1];
                s->rightPostFilter[i] = (1.0f - difference) * s->rightPreFilter[samplingIndex] +
                                        difference * s->rightPreFilter[samplingIndex + 1];
            }

            /* copy to output buffer */
            for (i = 0; i < 880; ++i) {
                signed_emuint finalLeftOutput = s->leftPostFilter[i];
                signed_emuint finalRightOutput = s->rightPostFilter[i];

                s->outputBuffer[i*4] = (emubyte)(finalLeftOutput & 0xFF);
                s->outputBuffer[(i*4)+1] = (emubyte)((finalLeftOutput >> 8) & 0xFF);
                s->outputBuffer[(i*4)+2] = (emubyte)(finalRightOutput & 0xFF);
                s->outputBuffer[(i*4)+3] = (emubyte)((finalRightOutput >> 8) & 0xFF);
            }

            /* output */
            if (!s->soundDisabled)
                SDL_QueueAudio(s->deviceId, (const void *)s->outputBuffer, 3520);

            /* reset channelCount */
            s->channelCount = 0;
        }

        /* now decrement each channel's counter */
        --s->counters[0];
        --s->counters[1];
        --s->counters[2];
        --s->counters[3];

        /* check if counter is 0 and reconfigure accordingly - this also accounts
           for if the associated register value is set to 0, and forces a polarity
           of 1 in this instance */
        emubyte i;
        for (i = 0; i < 3; ++i) {
            if (s->counters[i] <= 0) {
                s->counters[i] = s->toneAndNoiseRegisters[i];
                if (s->polarity[i] == 1 && s->toneAndNoiseRegisters[i] != 0)
                    s->polarity[i] = -1;
                else
                    s->polarity[i] = 1;
            }
        }
    }
}

/* this function returns an emubyte pointer to the state of the SN76489 */
emubyte *soundchip_saveState(SN76489 s)
{
    /* define variables */
    emuint soundchipSize = 59 + 13; /* add extra 13 bytes for header and size */
    emuint marker = 0;

    /* allocate the memory */
    emubyte *soundchipState = malloc(sizeof(emubyte) * soundchipSize);
    if (soundchipState == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "sn76489.c", "Couldn't allocate memory for soundchip state buffer...\n");
        return NULL;
    }

    /* fill first 13 bytes with header and state size */
    soundchipState[marker++] = soundchipSize & 0xFF;
    soundchipState[marker++] = (soundchipSize >> 8) & 0xFF;
    soundchipState[marker++] = (soundchipSize >> 16) & 0xFF;
    soundchipState[marker++] = (soundchipSize >> 24) & 0xFF;
    soundchipState[marker++] = 'S';
    soundchipState[marker++] = 'O';
    soundchipState[marker++] = 'U';
    soundchipState[marker++] = 'N';
    soundchipState[marker++] = 'D';
    soundchipState[marker++] = 'C';
    soundchipState[marker++] = 'H';
    soundchipState[marker++] = 'I';
    soundchipState[marker++] = 'P';

    /* copy volume register values to state buffer */
    soundchipState[marker++] = s->volumeRegisters[0];
    soundchipState[marker++] = s->volumeRegisters[1];
    soundchipState[marker++] = s->volumeRegisters[2];
    soundchipState[marker++] = s->volumeRegisters[3];

    /* copy tone and noise register values to state buffer */
    soundchipState[marker++] = s->toneAndNoiseRegisters[0] & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[0] >> 8) & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[0] >> 16) & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[0] >> 24) & 0xFF;
    soundchipState[marker++] = s->toneAndNoiseRegisters[1] & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[1] >> 8) & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[1] >> 16) & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[1] >> 24) & 0xFF;
    soundchipState[marker++] = s->toneAndNoiseRegisters[2] & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[2] >> 8) & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[2] >> 16) & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[2] >> 24) & 0xFF;
    soundchipState[marker++] = s->toneAndNoiseRegisters[3] & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[3] >> 8) & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[3] >> 16) & 0xFF;
    soundchipState[marker++] = (s->toneAndNoiseRegisters[3] >> 24) & 0xFF;

    /* copy tone and noise counter values to state buffer */
    soundchipState[marker++] = s->counters[0] & 0xFF;
    soundchipState[marker++] = (s->counters[0] >> 8) & 0xFF;
    soundchipState[marker++] = (s->counters[0] >> 16) & 0xFF;
    soundchipState[marker++] = (s->counters[0] >> 24) & 0xFF;
    soundchipState[marker++] = s->counters[1] & 0xFF;
    soundchipState[marker++] = (s->counters[1] >> 8) & 0xFF;
    soundchipState[marker++] = (s->counters[1] >> 16) & 0xFF;
    soundchipState[marker++] = (s->counters[1] >> 24) & 0xFF;
    soundchipState[marker++] = s->counters[2] & 0xFF;
    soundchipState[marker++] = (s->counters[2] >> 8) & 0xFF;
    soundchipState[marker++] = (s->counters[2] >> 16) & 0xFF;
    soundchipState[marker++] = (s->counters[2] >> 24) & 0xFF;
    soundchipState[marker++] = s->counters[3] & 0xFF;
    soundchipState[marker++] = (s->counters[3] >> 8) & 0xFF;
    soundchipState[marker++] = (s->counters[3] >> 16) & 0xFF;
    soundchipState[marker++] = (s->counters[3] >> 24) & 0xFF;

    /* copy polarity counter values to state buffer */
    soundchipState[marker++] = s->polarity[0];
    soundchipState[marker++] = s->polarity[1];
    soundchipState[marker++] = s->polarity[2];
    soundchipState[marker++] = s->polarity[3];

    /* copy other values to state buffer */
    soundchipState[marker++] = s->noise;
    soundchipState[marker++] = s->currentlyLatchedRegister;
    soundchipState[marker++] = s->linearFeedbackShiftRegister & 0xFF;
    soundchipState[marker++] = (s->linearFeedbackShiftRegister >> 8) & 0xFF;
    soundchipState[marker++] = (s->linearFeedbackShiftRegister >> 16) & 0xFF;
    soundchipState[marker++] = (s->linearFeedbackShiftRegister >> 24) & 0xFF;
    soundchipState[marker++] = s->z80Cycles & 0xFF;
    soundchipState[marker++] = (s->z80Cycles >> 8) & 0xFF;
    soundchipState[marker++] = (s->z80Cycles >> 16) & 0xFF;
    soundchipState[marker++] = (s->z80Cycles >> 24) & 0xFF;
    if (s->volumeLatched)
        soundchipState[marker++] = 1;
    else
        soundchipState[marker++] = 0;

    /* fill in buffer with zeros - this version no longer uses Buffer objects */
    soundchipState[marker++] = 0;
    soundchipState[marker++] = 0;
    soundchipState[marker++] = 0;
    soundchipState[marker++] = 0;
    soundchipState[marker++] = 0;
    soundchipState[marker++] = 0;
    soundchipState[marker++] = 0;
    soundchipState[marker++] = 0;

    /* return soundchip state */
    return soundchipState;
}

/* this function stops the audio if an SDL audio device is currently open */
void soundchip_stopAudio(SN76489 s)
{
    /* stop playback and close audio device */
    if (s->deviceId != 0) {
        SDL_PauseAudioDevice(s->deviceId, 1);
        SDL_CloseAudioDevice(s->deviceId);
        s->deviceId = 0;
    }
}

/* this function starts the SDL audio channel */
SN76489 soundchip_startAudio(SN76489 s)
{
    /* initialise the (real) audio device */
    SDL_AudioSpec desiredFormat, receivedFormat;
    desiredFormat.freq = 44100;
    desiredFormat.format = AUDIO_S16LSB;
    desiredFormat.channels = 2;
    desiredFormat.samples = 2048;
    desiredFormat.callback = NULL;
    desiredFormat.userdata = NULL;
    if ((s->deviceId = SDL_OpenAudioDevice(NULL, 0, &desiredFormat, &receivedFormat, 0)) == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "sn76489.c", "Couldn't open audio device: %s", SDL_GetError());
        return NULL;
    }
    SDL_PauseAudioDevice(s->deviceId, 0);

    return s;
}

/* this function returns the total number of bytes requires by an SN76489 object */
emuint soundchip_getMemoryUsage(void)
{
    return (sizeof(struct SN76489) * sizeof(emubyte)) + /* volume registers */ (sizeof(emubyte) * 4) +
    /* tone and noise registers */ (sizeof(signed_emuint) * 4) + /* counters */ (sizeof(signed_emuint) * 4) +
    /* polarity */ (sizeof(signed_emubyte) * 4) + /* pre filter buffers */ (sizeof(emufloat) * 4466 * 2) +
    /* post filter buffers */ (sizeof(emufloat) * 880 * 2) + /* output buffer */ (sizeof(emubyte) * 3520);
}

/* this function returns the SDL AudioDeviceID */
emuint soundchip_getAudioDeviceID(SN76489 s)
{
    return s->deviceId;
}