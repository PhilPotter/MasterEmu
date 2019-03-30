/* MasterEmu Z80 source code file
   copyright Phil Potter, 2019 */

#include <stdlib.h>
#include <android/log.h>
#include "z80.h"
#include "console.h"

/* the following enum describes arguments that can be passed to various functions
   to make them behave differently - this cuts down on boilerplate code */
enum cpuVal { A, B, C, D, E, H, L, n, IXd, IYd, aHL, IXh, IXl, IYh, IYl,
              a_nn, nn, SP, R, I, HL, BC, DE, aBC, aDE, IX, IY, AF, M,
              NC, NZ, P, PE, PO, Z, aSP, aC, AFShadow, a_n };
enum calcType { subtraction = 128, addition = 0, subtraction16 =  32768, addition16 = 0 };
typedef enum cpuVal cpuVal;
typedef enum calcType calcType;

/* miscellaneous static function declarations for use within file 
   - see each function's comment for its purpose */
static void writeToMemory(Z80 z, emuint address, emubyte data);
static emubyte readFromMemory(Z80 z, emuint address);
static void writeToIO(Z80 z, emuint address, emubyte data);
static emubyte readFromIO(Z80 z, emuint address);
static void setSFlag(Z80 z, emubool value);
static void setZFlag(Z80 z, emubool value);
static void setHFlag(Z80 z, emubool value);
static void setPVFlag(Z80 z, emubool value);
static void setNFlag(Z80 z, emubool value);
static void setCFlag(Z80 z, emubool value);
static void writeProgramCounter(Z80 z, emuint address);
static emuint readProgramCounter(Z80 z);
static void incrementProgramCounter(Z80 z);
static void decrementProgramCounter(Z80 z);
static void incrementRefreshRegister(Z80 z);
static void writeStackPointer(Z80 z, emuint address);
static emuint readStackPointer(Z80 z);
static void incrementStackPointer(Z80 z);
static void decrementStackPointer(Z80 z);
static void calculatePVFlag(Z80 z, emubyte temp);
static void calculateZFlag(Z80 z, emubyte temp);
static void calculateSFlag(Z80 z, emubyte temp);
static void calculateHFlag(Z80 z, emubyte operand1, emubyte operand2, emuint result);
static void calculateCFlag(Z80 z, emubyte operand, emuint result);
static void calculatePVOverflow(Z80 z, emubyte operand, emuint result, calcType type);
static void calculateZFlag16(Z80 z, emuint temp);
static void calculateSFlag16(Z80 z, emuint temp);
static void calculateHFlag16(Z80 z, emuint tempHL, emuint operand, emuint result);
static void calculateCFlag16(Z80 z, emuint tempHL, emuint operand, emuint result);
static void calculatePVOverflow16(Z80 z, emuint tempHL, emuint operand, emuint result, calcType type);

/* static function declarations for Z80 instructions
   - see each function's comment for its purpose */
static void NOP(Z80 z);
static void DAA(Z80 z);
static void EI(Z80 z);
static void DI(Z80 z);
static void HALT(Z80 z);
static void IM(Z80 z, emubyte mode);
static void XOR(Z80 z, cpuVal value);
static void BIT(Z80 z, emubyte bit, cpuVal value);
static void RST(Z80 z, emubyte vector);
static void EXX(Z80 z);
static void SET(Z80 z, emubyte bit, cpuVal value);
static void RES(Z80 z, emubyte bit, cpuVal value);
static void LD_8Bit(Z80 z, cpuVal destination, cpuVal source);
static void LD_16Bit(Z80 z, cpuVal destination, cpuVal source, emubyte longer);
static void OR(Z80 z, cpuVal value);
static void SUB(Z80 z, cpuVal value);
static void SBC(Z80 z, cpuVal value);
static void PUSH(Z80 z, cpuVal value);
static void POP(Z80 z, cpuVal value);
static void RET_CONDITIONAL(Z80 z, cpuVal value);
static void RET(Z80 z);
static void ADC(Z80 z, cpuVal value);
static void ADD(Z80 z, cpuVal valueone, cpuVal valuetwo);
static void AND(Z80 z, cpuVal value);
static void CALL_CONDITIONAL(Z80 z, cpuVal value);
static void CALL(Z80 z);
static void CCF(Z80 z);
static void CP(Z80 z, cpuVal value);
static void CPD(Z80 z);
static void CPDR(Z80 z);
static void CPI(Z80 z);
static void CPIR(Z80 z);
static void CPL(Z80 z);
static void NEG(Z80 z);
static void LDD(Z80 z);
static void LDI(Z80 z);
static void LDDR(Z80 z);
static void LDIR(Z80 z);
static void DJNZ(Z80 z);
static void EX(Z80 z, cpuVal valueone, cpuVal valuetwo);
static void DEC(Z80 z, cpuVal value);
static void INC(Z80 z, cpuVal value);
static void IN(Z80 z, cpuVal destination, cpuVal source);
static void OUT(Z80 z, cpuVal destination, cpuVal source);
static void IND(Z80 z);
static void INDR(Z80 z);
static void INI(Z80 z);
static void INIR(Z80 z);
static void OUTD(Z80 z);
static void OTDR(Z80 z);
static void OUTI(Z80 z);
static void OTIR(Z80 z);
static void JP_CONDITIONAL(Z80 z, cpuVal value);
static void JP(Z80 z, cpuVal value);
static void JR_CONDITIONAL(Z80 z, cpuVal value);
static void JR(Z80 z);
static void RETI(Z80 z);
static void RETN(Z80 z);
static void RL(Z80 z, cpuVal value);
static void RLA(Z80 z);
static void RLC(Z80 z, cpuVal value);
static void RLCA(Z80 z);
static void RLD(Z80 z);
static void RR(Z80 z, cpuVal value);
static void RRA(Z80 z);
static void RRC(Z80 z, cpuVal value);
static void RRCA(Z80 z);
static void RRD(Z80 z);
static void SLA(Z80 z, cpuVal value);
static void SLL(Z80 z, cpuVal value);
static void SRA(Z80 z, cpuVal value);
static void SRL(Z80 z, cpuVal value);
static void SCF(Z80 z);

/* this struct models the Z80's internal state */
struct Z80 {
    /* pointer to main console object */
    Console ms;

    /* interrupt related attributes */
    emubyte nmi; /* signals a non-maskable interrupt if set to 1 */
    emubyte iff1; /* first interrupt flip-flop register */
    emubyte iff2; /* second interrupt flip-flop register */
    emubyte halt; /* if set to 1, Z80 is halted */
    emubyte intMode; /* shows interrupt mode processor is set to */
    emubool followingInstruction; /* stops maskable interrupts in the instruction
                                     immediately following EI */

    /* Z80 main 8 bit registers */
    emubyte regA;
    emubyte regB;
    emubyte regC;
    emubyte regD;
    emubyte regE;
    emubyte regH;
    emubyte regL;
    emubyte regF;

    /* Z80 shadow 8 bit registers */
    emubyte regAShadow;
    emubyte regBShadow;
    emubyte regCShadow;
    emubyte regDShadow;
    emubyte regEShadow;
    emubyte regHShadow;
    emubyte regLShadow;
    emubyte regFShadow;

    /* miscellaneous registers */
    emuint regIX; /* 16 bit register */
    emuint regIY; /* 16 bit register */
    emuint regSP; /* 16 bit register (stack pointer) */
    emubyte regI; /* 8 bit register (interrupt vector) */
    emubyte regR; /* 8 bit register (refresh register) */
    emuint regPC; /* 16 bit register (program counter) */

    /* miscellaneous attributes */
    emuint cycles; /* counts number of cycles executed */
    emubool ddStub; /* this allows long sequences of DD */
    emubool fdStub; /* this allows long sequences of FD */
    emuint interruptCounter; /* this allows us to delay the servicing of maskable interrupts */
    emubool interruptPending; /* this also allows us to delay the servicing of maskable interrupts */
};

/* this function creates a new Z80 object and returns a pointer to it */
Z80 createZ80(Console ms, emubyte *z80State, emubyte *wholePointer)
{
    /* first, we allocate memory for a Z80 struct */
    Z80 z = (Z80)wholePointer;
 
    /* set Console reference to point to parent Master System object */
    z->ms = ms;

    /* initialise cycle counter to zero */
    z->cycles = 0;

    /* interrupt related initialisations */
    z->nmi = 0;
    z->iff1 = 0;
    z->iff2 = 0;
    z->halt = 0;
    z->intMode = 1;
    z->followingInstruction = false;
    z->interruptCounter = 0;
    z->interruptPending = false;

    /* set main register set */
    z->regA = 0;
    z->regB = 0;
    z->regC = 0;
    z->regD = 0;
    z->regE = 0;
    z->regH = 0;
    z->regL = 0;
    z->regF = 0;

    /* set shadow register set */
    z->regAShadow = 0;
    z->regBShadow = 0;
    z->regCShadow = 0;
    z->regDShadow = 0;
    z->regEShadow = 0;
    z->regHShadow = 0;
    z->regLShadow = 0;
    z->regFShadow = 0;

    /* set other registers */
    z->regIX = 0;
    z->regIY = 0;
    z->regSP = 0xDFF0; /* this seems to be the default stack pointer for a few games */
    z->regI = 0;
    z->regR = 0;
    z->regPC = 0;

    /* setup stub variables */
    z->ddStub = false;
    z->fdStub = false;

    /* set state if present */
    if (z80State != NULL) {
        emuint marker = 4;
        if (z80State[marker] != 'Z' && z80State[marker + 1] != '8' && z80State[marker + 2] != '0') {
            z->interruptPending = z80State[marker];
            z->interruptCounter = (z80State[marker + 2] << 8) | z80State[marker + 1];
        }
        marker += 3;
        z->nmi = z80State[marker++];
        z->iff1 = z80State[marker++];
        z->iff2 = z80State[marker++];
        z->halt = z80State[marker++];
        z->intMode = z80State[marker++];
        if (z80State[marker++] == 1)
            z->followingInstruction = true;
        else
            z->followingInstruction = false;
        z->regA = z80State[marker++];
        z->regB = z80State[marker++];
        z->regC = z80State[marker++];
        z->regD = z80State[marker++];
        z->regE = z80State[marker++];
        z->regH = z80State[marker++];
        z->regL = z80State[marker++];
        z->regF = z80State[marker++];
        z->regAShadow = z80State[marker++];
        z->regBShadow = z80State[marker++];
        z->regCShadow = z80State[marker++];
        z->regDShadow = z80State[marker++];
        z->regEShadow = z80State[marker++];
        z->regHShadow = z80State[marker++];
        z->regLShadow = z80State[marker++];
        z->regFShadow = z80State[marker++];
        z->regIX = (z80State[marker + 1] << 8) | z80State[marker];
        marker += 2;
        z->regIY = (z80State[marker + 1] << 8) | z80State[marker];
        marker += 2;
        z->regSP = (z80State[marker + 1] << 8) | z80State[marker];
        marker += 2;
        z->regI = z80State[marker++];
        z->regR = z80State[marker++];
        z->regPC = (z80State[marker + 1] << 8) | z80State[marker];
        marker += 2;
        if (z80State[marker++] == 1)
            z->ddStub = true;
        else
            z->ddStub = false;
        if (z80State[marker++] == 1)
            z->fdStub = true;
        else
            z->fdStub = false;
    }

    /* return Z80 object */
    return z;
}

/* this function destroys the Z80 object */
void destroyZ80(Z80 z)
{
    ;
}

/* this is the main execute block of the Z80 core, fetching instructions and executing them
   accordingly, then returning the correct number of cycles */
emuint Z80_executeInstruction(Z80 z)
{
    /* initialise cycle count to zero and setup some other local variables */
    z->cycles = 0;
    emubyte opcode = 0, cbOpcode = 0, ddOpcode = 0, edOpcode = 0, fdOpcode = 0;
    
    /* this checks for non-maskable interrupts every instruction */
    if (console_checkNmi(z->ms))
        z->nmi = 1;

    /* handle non-maskable interrupt if received */
    if (z->nmi) {
        /* allow maskables immediately following return from this NMI, regardless
           of whether or not they were blocked for the following instruction */
        z->followingInstruction = false;

        /* end HALT, reset NMI and INT variable, and store IFF1 in IFF2 */
        console_interruptHandled(z->ms);
        z->nmi = 0;
        z->iff2 = z->iff1;
        z->iff1 = 0;
        z->halt = 0;

        /* store program counter on the stack */
        decrementStackPointer(z);
        writeToMemory(z, readStackPointer(z), (readProgramCounter(z) >> 8));
        decrementStackPointer(z);
        writeToMemory(z, readStackPointer(z), (readProgramCounter(z) & 0xFF));

        /* change program counter to NMI location and set relevant cycle count */
        writeProgramCounter(z, 0x66);
        z->cycles = 11;
    }
    /* this checks for normal interrupts if there is no NMI, as NMI takes priority */
    else if ((z->interruptPending = console_checkInterrupt(z->ms)) && (z->iff1) && !z->followingInstruction && z->interruptCounter > 100) {
        /* decides what to do based on the interupt mode */
        switch (z->intMode) {
            case 0: break; /* we don't use mode 0 on the master system */
            case 1: /* mode 1 is the master system mode */
            {
                /* reset counter */
                z->interruptCounter = 0;

                /* end HALT and reset INT variables */
                console_interruptHandled(z->ms);
                z->halt = 0;
                z->iff1 = 0;
                z->iff2 = 0;

                /* store program counter on the stack */
                decrementStackPointer(z);
                writeToMemory(z, readStackPointer(z), (readProgramCounter(z) >> 8));
                decrementStackPointer(z);
                writeToMemory(z, readStackPointer(z), (readProgramCounter(z) & 0xFF));

                /* change program counter to INT location and set relevant cycle count */
                writeProgramCounter(z, 0x38);
                z->cycles = 13;

            } break;
            case 2: break; /* we don't use mode 2 on the master system */
        }
    }
    /* this checks for a HALT condition */
    else if (z->halt) {
        /* this enable maskable interrupts to run following a HALT */
        z->followingInstruction = false;

        /* execute NOPs continually in the HALT state and top up
           the memory refresh register */
        NOP(z);
        incrementRefreshRegister(z);
    }
    /* fetch/process opcodes as normal */
    else {
        /* this enables maskable interrupts to run following any other
           instruction (except EI) */
        z->followingInstruction = false;

        /* load instruction opcode using program counter */
        if (!(z->ddStub || z->fdStub)) {
            opcode = readFromMemory(z, readProgramCounter(z));
            incrementProgramCounter(z);
        } else if (z->ddStub) {
            z->ddStub = false;
            opcode = 0xDD;
        } else {
            z->fdStub = false;
            opcode = 0xFD;
        }

        /* increment refresh register and call relevant instruction function */
        incrementRefreshRegister(z);
        switch (opcode) {
            /* this is for the '8-bit load' group of instructions with
               only one opcode */
            case 0x7F: LD_8Bit(z, A, A); break;
            case 0x78: LD_8Bit(z, A, B); break;
            case 0x79: LD_8Bit(z, A, C); break;
            case 0x7A: LD_8Bit(z, A, D); break;
            case 0x7B: LD_8Bit(z, A, E); break;
            case 0x7C: LD_8Bit(z, A, H); break;
            case 0x7D: LD_8Bit(z, A, L); break;
            case 0x47: LD_8Bit(z, B, A); break;
            case 0x40: LD_8Bit(z, B, B); break;
            case 0x41: LD_8Bit(z, B, C); break;
            case 0x42: LD_8Bit(z, B, D); break;
            case 0x43: LD_8Bit(z, B, E); break;
            case 0x44: LD_8Bit(z, B, H); break;
            case 0x45: LD_8Bit(z, B, L); break;
            case 0x4F: LD_8Bit(z, C, A); break;
            case 0x48: LD_8Bit(z, C, B); break;
            case 0x49: LD_8Bit(z, C, C); break;
            case 0x4A: LD_8Bit(z, C, D); break;
            case 0x4B: LD_8Bit(z, C, E); break;
            case 0x4C: LD_8Bit(z, C, H); break;
            case 0x4D: LD_8Bit(z, C, L); break;
            case 0x57: LD_8Bit(z, D, A); break;
            case 0x50: LD_8Bit(z, D, B); break;
            case 0x51: LD_8Bit(z, D, C); break;
            case 0x52: LD_8Bit(z, D, D); break;
            case 0x53: LD_8Bit(z, D, E); break;
            case 0x54: LD_8Bit(z, D, H); break;
            case 0x55: LD_8Bit(z, D, L); break;
            case 0x5F: LD_8Bit(z, E, A); break;
            case 0x58: LD_8Bit(z, E, B); break;
            case 0x59: LD_8Bit(z, E, C); break;
            case 0x5A: LD_8Bit(z, E, D); break;
            case 0x5B: LD_8Bit(z, E, E); break;
            case 0x5C: LD_8Bit(z, E, H); break;
            case 0x5D: LD_8Bit(z, E, L); break;
            case 0x67: LD_8Bit(z, H, A); break;
            case 0x60: LD_8Bit(z, H, B); break;
            case 0x61: LD_8Bit(z, H, C); break;
            case 0x62: LD_8Bit(z, H, D); break;
            case 0x63: LD_8Bit(z, H, E); break;
            case 0x64: LD_8Bit(z, H, H); break;
            case 0x65: LD_8Bit(z, H, L); break;
            case 0x6F: LD_8Bit(z, L, A); break;
            case 0x68: LD_8Bit(z, L, B); break;
            case 0x69: LD_8Bit(z, L, C); break;
            case 0x6A: LD_8Bit(z, L, D); break;
            case 0x6B: LD_8Bit(z, L, E); break;
            case 0x6C: LD_8Bit(z, L, H); break;
            case 0x6D: LD_8Bit(z, L, L); break;
            case 0x3E: LD_8Bit(z, A, n); break;
            case 0x06: LD_8Bit(z, B, n); break;
            case 0x0E: LD_8Bit(z, C, n); break;
            case 0x16: LD_8Bit(z, D, n); break;
            case 0x1E: LD_8Bit(z, E, n); break;
            case 0x26: LD_8Bit(z, H, n); break;
            case 0x2E: LD_8Bit(z, L, n); break;
            case 0x7E: LD_8Bit(z, A, aHL); break;
            case 0x46: LD_8Bit(z, B, aHL); break;
            case 0x4E: LD_8Bit(z, C, aHL); break;
            case 0x56: LD_8Bit(z, D, aHL); break;
            case 0x5E: LD_8Bit(z, E, aHL); break;
            case 0x66: LD_8Bit(z, H, aHL); break;
            case 0x6E: LD_8Bit(z, L, aHL); break;
            case 0x77: LD_8Bit(z, aHL, A); break;
            case 0x70: LD_8Bit(z, aHL, B); break;
            case 0x71: LD_8Bit(z, aHL, C); break;
            case 0x72: LD_8Bit(z, aHL, D); break;
            case 0x73: LD_8Bit(z, aHL, E); break;
            case 0x74: LD_8Bit(z, aHL, H); break;
            case 0x75: LD_8Bit(z, aHL, L); break;
            case 0x36: LD_8Bit(z, aHL, n); break;
            case 0x0A: LD_8Bit(z, A, aBC); break;
            case 0x1A: LD_8Bit(z, A, aDE); break;
            case 0x3A: LD_8Bit(z, A, a_nn); break;
            case 0x02: LD_8Bit(z, aBC, A); break;
            case 0x12: LD_8Bit(z, aDE, A); break;
            case 0x32: LD_8Bit(z, a_nn, A); break;

            /* this is for the '16-bit load' group of instructions with
               only one opcode */
            case 0x01: LD_16Bit(z, BC, nn, 0); break;
            case 0x11: LD_16Bit(z, DE, nn, 0); break;
            case 0x21: LD_16Bit(z, HL, nn, 0); break;
            case 0x31: LD_16Bit(z, SP, nn, 0); break;
            case 0x2A: LD_16Bit(z, HL, a_nn, 0); break;
            case 0x22: LD_16Bit(z, a_nn, HL, 0); break;
            case 0xF9: LD_16Bit(z, SP, HL, 0); break;
            case 0xC5: PUSH(z, BC); break;
            case 0xD5: PUSH(z, DE); break;
            case 0xE5: PUSH(z, HL); break;
            case 0xF5: PUSH(z, AF); break;
            case 0xC1: POP(z, BC); break;
            case 0xD1: POP(z, DE); break;
            case 0xE1: POP(z, HL); break;
            case 0xF1: POP(z, AF); break;

            /* this is for the 'exchange, block transfer, and search'
               of instructions with only one opcode */
            case 0xEB: EX(z, DE, HL); break;
            case 0x08: EX(z, AF, AFShadow); break;
            case 0xD9: EXX(z); break;
            case 0xE3: EX(z, aSP, HL); break;

            /* this is for the '8-bit arithmetic group' of
               instructions with only one opcode */
            case 0x87: ADD(z, A, A); break;
            case 0x80: ADD(z, A, B); break;
            case 0x81: ADD(z, A, C); break;
            case 0x82: ADD(z, A, D); break;
            case 0x83: ADD(z, A, E); break;
            case 0x84: ADD(z, A, H); break;
            case 0x85: ADD(z, A, L); break;
            case 0xC6: ADD(z, A, n); break;
            case 0x86: ADD(z, A, aHL); break;
            case 0x88: ADC(z, B); break;
            case 0x89: ADC(z, C); break;
            case 0x8A: ADC(z, D); break;
            case 0x8B: ADC(z, E); break;
            case 0x8C: ADC(z, H); break;
            case 0x8D: ADC(z, L); break;
            case 0x8F: ADC(z, A); break;
            case 0xCE: ADC(z, n); break;
            case 0x8E: ADC(z, aHL); break;
            case 0x90: SUB(z, B); break;
            case 0x91: SUB(z, C); break;
            case 0x92: SUB(z, D); break;
            case 0x93: SUB(z, E); break;
            case 0x94: SUB(z, H); break;
            case 0x95: SUB(z, L); break;
            case 0x97: SUB(z, A); break;
            case 0xD6: SUB(z, n); break;
            case 0x96: SUB(z, aHL); break;
            case 0x98: SBC(z, B); break;
            case 0x99: SBC(z, C); break;
            case 0x9A: SBC(z, D); break;
            case 0x9B: SBC(z, E); break;
            case 0x9C: SBC(z, H); break;
            case 0x9D: SBC(z, L); break;
            case 0x9F: SBC(z, A); break;
            case 0xDE: SBC(z, n); break;
            case 0x9E: SBC(z, aHL); break;
            case 0xA0: AND(z, B); break;
            case 0xA1: AND(z, C); break;
            case 0xA2: AND(z, D); break;
            case 0xA3: AND(z, E); break;
            case 0xA4: AND(z, H); break;
            case 0xA5: AND(z, L); break;
            case 0xA7: AND(z, A); break;
            case 0xE6: AND(z, n); break;
            case 0xA6: AND(z, aHL); break;
            case 0xB0: OR(z, B); break;
            case 0xB1: OR(z, C); break;
            case 0xB2: OR(z, D); break;
            case 0xB3: OR(z, E); break;
            case 0xB4: OR(z, H); break;
            case 0xB5: OR(z, L); break;
            case 0xB7: OR(z, A); break;
            case 0xF6: OR(z, n); break;
            case 0xB6: OR(z, aHL); break;
            case 0xA8: XOR(z, B); break;
            case 0xA9: XOR(z, C); break;
            case 0xAA: XOR(z, D); break;
            case 0xAB: XOR(z, E); break;
            case 0xAC: XOR(z, H); break;
            case 0xAD: XOR(z, L); break;
            case 0xAF: XOR(z, A); break;
            case 0xEE: XOR(z, n); break;
            case 0xAE: XOR(z, aHL); break;
            case 0xB8: CP(z, B); break;
            case 0xB9: CP(z, C); break;
            case 0xBA: CP(z, D); break;
            case 0xBB: CP(z, E); break;
            case 0xBC: CP(z, H); break;
            case 0xBD: CP(z, L); break;
            case 0xBF: CP(z, A); break;
            case 0xFE: CP(z, n); break;
            case 0xBE: CP(z, aHL); break;
            case 0x3C: INC(z, A); break;
            case 0x04: INC(z, B); break;
            case 0x0C: INC(z, C); break;
            case 0x14: INC(z, D); break;
            case 0x1C: INC(z, E); break;
            case 0x24: INC(z, H); break;
            case 0x2C: INC(z, L); break;
            case 0x34: INC(z, aHL); break;
            case 0x05: DEC(z, B); break;
            case 0x0D: DEC(z, C); break;
            case 0x15: DEC(z, D); break;
            case 0x1D: DEC(z, E); break;
            case 0x25: DEC(z, H); break;
            case 0x2D: DEC(z, L); break;
            case 0x3D: DEC(z, A); break;
            case 0x35: DEC(z, aHL); break;

            /* this is for the 'general purpose arithmetic and CPU
               control group' of instructions with only one opcode */
            case 0x27: DAA(z); break;
            case 0x2F: CPL(z); break;
            case 0x3F: CCF(z); break;
            case 0x37: SCF(z); break;
            case 0x00: NOP(z); break;
            case 0x76: HALT(z); break;
            case 0xF3: DI(z); break;
            case 0xFB: EI(z); break;

            /* this is for the '16-bit arithmetic group' of
               instructions with only one opcode */
            case 0x09: ADD(z, HL, BC); break;
            case 0x19: ADD(z, HL, DE); break;
            case 0x29: ADD(z, HL, HL); break;
            case 0x39: ADD(z, HL, SP); break;
            case 0x03: INC(z, BC); break;
            case 0x13: INC(z, DE); break;
            case 0x23: INC(z, HL); break;
            case 0x33: INC(z, SP); break;
            case 0x0B: DEC(z, BC); break;
            case 0x1B: DEC(z, DE); break;
            case 0x2B: DEC(z, HL); break;
            case 0x3B: DEC(z, SP); break;

            /* this is for the 'rotate and shift group' of instructions
               with only one opcode */
            case 0x07: RLCA(z); break;
            case 0x17: RLA(z); break;
            case 0x0F: RRCA(z); break;
            case 0x1F: RRA(z); break;

            /* this is for the 'jump group' of instructions with only
               one opcode */
            case 0xC3: JP(z, nn); break;
            case 0xC2: JP_CONDITIONAL(z, NZ); break;
            case 0xCA: JP_CONDITIONAL(z, Z); break;
            case 0xD2: JP_CONDITIONAL(z, NC); break;
            case 0xDA: JP_CONDITIONAL(z, C); break;
            case 0xE2: JP_CONDITIONAL(z, PO); break;
            case 0xEA: JP_CONDITIONAL(z, PE); break;
            case 0xF2: JP_CONDITIONAL(z, P); break;
            case 0xFA: JP_CONDITIONAL(z, M); break;
            case 0x18: JR(z); break;
            case 0x38: JR_CONDITIONAL(z, C); break;
            case 0x30: JR_CONDITIONAL(z, NC); break;
            case 0x28: JR_CONDITIONAL(z, Z); break;
            case 0x20: JR_CONDITIONAL(z, NZ); break;
            case 0xE9: JP(z, HL); break;
            case 0x10: DJNZ(z); break;

            /* this is for the 'call and return group' of instructions
               with only one opcode */
            case 0xCD: CALL(z); break;
            case 0xC4: CALL_CONDITIONAL(z, NZ); break;
            case 0xCC: CALL_CONDITIONAL(z, Z); break;
            case 0xD4: CALL_CONDITIONAL(z, NC); break;
            case 0xDC: CALL_CONDITIONAL(z, C); break;
            case 0xE4: CALL_CONDITIONAL(z, PO); break;
            case 0xEC: CALL_CONDITIONAL(z, PE); break;
            case 0xF4: CALL_CONDITIONAL(z, P); break;
            case 0xFC: CALL_CONDITIONAL(z, M); break;
            case 0xC9: RET(z); break;
            case 0xC0: RET_CONDITIONAL(z, NZ); break;
            case 0xC8: RET_CONDITIONAL(z, Z); break;
            case 0xD0: RET_CONDITIONAL(z, NC); break;
            case 0xD8: RET_CONDITIONAL(z, C); break;
            case 0xE0: RET_CONDITIONAL(z, PO); break;
            case 0xE8: RET_CONDITIONAL(z, PE); break;
            case 0xF0: RET_CONDITIONAL(z, P); break;
            case 0xF8: RET_CONDITIONAL(z, M); break;
            case 0xC7: RST(z, 0x00); break;
            case 0xCF: RST(z, 0x08); break;
            case 0xD7: RST(z, 0x10); break;
            case 0xDF: RST(z, 0x18); break;
            case 0xE7: RST(z, 0x20); break;
            case 0xEF: RST(z, 0x28); break;
            case 0xF7: RST(z, 0x30); break;
            case 0xFF: RST(z, 0x38); break;

            /* this is for the 'input and output group' of instructions with
               only one opcode */
            case 0xDB: IN(z, A, a_n); break;
            case 0xD3: OUT(z, a_n, A); break;

            /* this is for instructions following the CB opcode */
            case 0xCB:
            {
                cbOpcode = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                switch (cbOpcode)
                {
                    /* this is for the 'rotate and shift group' of instructions
                       with multi-opcodes */
                    case 0x00: RLC(z, B); break;
                    case 0x01: RLC(z, C); break;
                    case 0x02: RLC(z, D); break;
                    case 0x03: RLC(z, E); break;
                    case 0x04: RLC(z, H); break;
                    case 0x05: RLC(z, L); break;
                    case 0x07: RLC(z, A); break;
                    case 0x06: RLC(z, aHL); break;
                    case 0x10: RL(z, B); break;
                    case 0x11: RL(z, C); break;
                    case 0x12: RL(z, D); break;
                    case 0x13: RL(z, E); break;
                    case 0x14: RL(z, H); break;
                    case 0x15: RL(z, L); break;
                    case 0x17: RL(z, A); break;
                    case 0x16: RL(z, aHL); break;
                    case 0x08: RRC(z, B); break;
                    case 0x09: RRC(z, C); break;
                    case 0x0A: RRC(z, D); break;
                    case 0x0B: RRC(z, E); break;
                    case 0x0C: RRC(z, H); break;
                    case 0x0D: RRC(z, L); break;
                    case 0x0F: RRC(z, A); break;
                    case 0x0E: RRC(z, aHL); break;
                    case 0x18: RR(z, B); break;
                    case 0x19: RR(z, C); break;
                    case 0x1A: RR(z, D); break;
                    case 0x1B: RR(z, E); break;
                    case 0x1C: RR(z, H); break;
                    case 0x1D: RR(z, L); break;
                    case 0x1F: RR(z, A); break;
                    case 0x1E: RR(z, aHL); break;
                    case 0x20: SLA(z, B); break;
                    case 0x21: SLA(z, C); break;
                    case 0x22: SLA(z, D); break;
                    case 0x23: SLA(z, E); break;
                    case 0x24: SLA(z, H); break;
                    case 0x25: SLA(z, L); break;
                    case 0x27: SLA(z, A); break;
                    case 0x26: SLA(z, aHL); break;
                    case 0x28: SRA(z, B); break;
                    case 0x29: SRA(z, C); break;
                    case 0x2A: SRA(z, D); break;
                    case 0x2B: SRA(z, E); break;
                    case 0x2C: SRA(z, H); break;
                    case 0x2D: SRA(z, L); break;
                    case 0x2F: SRA(z, A); break;
                    case 0x2E: SRA(z, aHL); break;
                    case 0x38: SRL(z, B); break;
                    case 0x39: SRL(z, C); break;
                    case 0x3A: SRL(z, D); break;
                    case 0x3B: SRL(z, E); break;
                    case 0x3C: SRL(z, H); break;
                    case 0x3D: SRL(z, L); break;
                    case 0x3F: SRL(z, A); break;
                    case 0x3E: SRL(z, aHL); break;
                    case 0x30: SLL(z, B); break; /* undocumented */
                    case 0x31: SLL(z, C); break; /* undocumented */
                    case 0x32: SLL(z, D); break; /* undocumented */
                    case 0x33: SLL(z, E); break; /* undocumented */
                    case 0x34: SLL(z, H); break; /* undocumented */
                    case 0x35: SLL(z, L); break; /* undocumented */
                    case 0x36: SLL(z, aHL); break; /* undocumented */
                    case 0x37: SLL(z, A); break; /* undocumented */

                    /* this is for the 'bit set, reset, and test group' of
                       instructions with multi-opcodes */
                    case 0x40: BIT(z, 0, B); break;
                    case 0x41: BIT(z, 0, C); break;
                    case 0x42: BIT(z, 0, D); break;
                    case 0x43: BIT(z, 0, E); break;
                    case 0x44: BIT(z, 0, H); break;
                    case 0x45: BIT(z, 0, L); break;
                    case 0x47: BIT(z, 0, A); break;
                    case 0x48: BIT(z, 1, B); break;
                    case 0x49: BIT(z, 1, C); break;
                    case 0x4A: BIT(z, 1, D); break;
                    case 0x4B: BIT(z, 1, E); break;
                    case 0x4C: BIT(z, 1, H); break;
                    case 0x4D: BIT(z, 1, L); break;
                    case 0x4F: BIT(z, 1, A); break;
                    case 0x50: BIT(z, 2, B); break;
                    case 0x51: BIT(z, 2, C); break;
                    case 0x52: BIT(z, 2, D); break;
                    case 0x53: BIT(z, 2, E); break;
                    case 0x54: BIT(z, 2, H); break;
                    case 0x55: BIT(z, 2, L); break;
                    case 0x57: BIT(z, 2, A); break;
                    case 0x58: BIT(z, 3, B); break;
                    case 0x59: BIT(z, 3, C); break;
                    case 0x5A: BIT(z, 3, D); break;
                    case 0x5B: BIT(z, 3, E); break;
                    case 0x5C: BIT(z, 3, H); break;
                    case 0x5D: BIT(z, 3, L); break;
                    case 0x5F: BIT(z, 3, A); break;
                    case 0x60: BIT(z, 4, B); break;
                    case 0x61: BIT(z, 4, C); break;
                    case 0x62: BIT(z, 4, D); break;
                    case 0x63: BIT(z, 4, E); break;
                    case 0x64: BIT(z, 4, H); break;
                    case 0x65: BIT(z, 4, L); break;
                    case 0x67: BIT(z, 4, A); break;
                    case 0x68: BIT(z, 5, B); break;
                    case 0x69: BIT(z, 5, C); break;
                    case 0x6A: BIT(z, 5, D); break;
                    case 0x6B: BIT(z, 5, E); break;
                    case 0x6C: BIT(z, 5, H); break;
                    case 0x6D: BIT(z, 5, L); break;
                    case 0x6F: BIT(z, 5, A); break;
                    case 0x70: BIT(z, 6, B); break;
                    case 0x71: BIT(z, 6, C); break;
                    case 0x72: BIT(z, 6, D); break;
                    case 0x73: BIT(z, 6, E); break;
                    case 0x74: BIT(z, 6, H); break;
                    case 0x75: BIT(z, 6, L); break;
                    case 0x77: BIT(z, 6, A); break;
                    case 0x78: BIT(z, 7, B); break;
                    case 0x79: BIT(z, 7, C); break;
                    case 0x7A: BIT(z, 7, D); break;
                    case 0x7B: BIT(z, 7, E); break;
                    case 0x7C: BIT(z, 7, H); break;
                    case 0x7D: BIT(z, 7, L); break;
                    case 0x7F: BIT(z, 7, A); break;
                    case 0x46: BIT(z, 0, aHL); break;
                    case 0x4E: BIT(z, 1, aHL); break;
                    case 0x56: BIT(z, 2, aHL); break;
                    case 0x5E: BIT(z, 3, aHL); break;
                    case 0x66: BIT(z, 4, aHL); break;
                    case 0x6E: BIT(z, 5, aHL); break;
                    case 0x76: BIT(z, 6, aHL); break;
                    case 0x7E: BIT(z, 7, aHL); break;
                    case 0xC0: SET(z, 0, B); break;
                    case 0xC1: SET(z, 0, C); break;
                    case 0xC2: SET(z, 0, D); break;
                    case 0xC3: SET(z, 0, E); break;
                    case 0xC4: SET(z, 0, H); break;
                    case 0xC5: SET(z, 0, L); break;
                    case 0xC7: SET(z, 0, A); break;
                    case 0xC8: SET(z, 1, B); break;
                    case 0xC9: SET(z, 1, C); break;
                    case 0xCA: SET(z, 1, D); break;
                    case 0xCB: SET(z, 1, E); break;
                    case 0xCC: SET(z, 1, H); break;
                    case 0xCD: SET(z, 1, L); break;
                    case 0xCF: SET(z, 1, A); break;
                    case 0xD0: SET(z, 2, B); break;
                    case 0xD1: SET(z, 2, C); break;
                    case 0xD2: SET(z, 2, D); break;
                    case 0xD3: SET(z, 2, E); break;
                    case 0xD4: SET(z, 2, H); break;
                    case 0xD5: SET(z, 2, L); break;
                    case 0xD7: SET(z, 2, A); break;
                    case 0xD8: SET(z, 3, B); break;
                    case 0xD9: SET(z, 3, C); break;
                    case 0xDA: SET(z, 3, D); break;
                    case 0xDB: SET(z, 3, E); break;
                    case 0xDC: SET(z, 3, H); break;
                    case 0xDD: SET(z, 3, L); break;
                    case 0xDF: SET(z, 3, A); break;
                    case 0xE0: SET(z, 4, B); break;
                    case 0xE1: SET(z, 4, C); break;
                    case 0xE2: SET(z, 4, D); break;
                    case 0xE3: SET(z, 4, E); break;
                    case 0xE4: SET(z, 4, H); break;
                    case 0xE5: SET(z, 4, L); break;
                    case 0xE7: SET(z, 4, A); break;
                    case 0xE8: SET(z, 5, B); break;
                    case 0xE9: SET(z, 5, C); break;
                    case 0xEA: SET(z, 5, D); break;
                    case 0xEB: SET(z, 5, E); break;
                    case 0xEC: SET(z, 5, H); break;
                    case 0xED: SET(z, 5, L); break;
                    case 0xEF: SET(z, 5, A); break;
                    case 0xF0: SET(z, 6, B); break;
                    case 0xF1: SET(z, 6, C); break;
                    case 0xF2: SET(z, 6, D); break;
                    case 0xF3: SET(z, 6, E); break;
                    case 0xF4: SET(z, 6, H); break;
                    case 0xF5: SET(z, 6, L); break;
                    case 0xF7: SET(z, 6, A); break;
                    case 0xF8: SET(z, 7, B); break;
                    case 0xF9: SET(z, 7, C); break;
                    case 0xFA: SET(z, 7, D); break;
                    case 0xFB: SET(z, 7, E); break;
                    case 0xFC: SET(z, 7, H); break;
                    case 0xFD: SET(z, 7, L); break;
                    case 0xFF: SET(z, 7, A); break;
                    case 0xC6: SET(z, 0, aHL); break;
                    case 0xCE: SET(z, 1, aHL); break;
                    case 0xD6: SET(z, 2, aHL); break;
                    case 0xDE: SET(z, 3, aHL); break;
                    case 0xE6: SET(z, 4, aHL); break;
                    case 0xEE: SET(z, 5, aHL); break;
                    case 0xF6: SET(z, 6, aHL); break;
                    case 0xFE: SET(z, 7, aHL); break;
                    case 0x80: RES(z, 0, B); break;
                    case 0x81: RES(z, 0, C); break;
                    case 0x82: RES(z, 0, D); break;
                    case 0x83: RES(z, 0, E); break;
                    case 0x84: RES(z, 0, H); break;
                    case 0x85: RES(z, 0, L); break;
                    case 0x87: RES(z, 0, A); break;
                    case 0x88: RES(z, 1, B); break;
                    case 0x89: RES(z, 1, C); break;
                    case 0x8A: RES(z, 1, D); break;
                    case 0x8B: RES(z, 1, E); break;
                    case 0x8C: RES(z, 1, H); break;
                    case 0x8D: RES(z, 1, L); break;
                    case 0x8F: RES(z, 1, A); break;
                    case 0x90: RES(z, 2, B); break;
                    case 0x91: RES(z, 2, C); break;
                    case 0x92: RES(z, 2, D); break;
                    case 0x93: RES(z, 2, E); break;
                    case 0x94: RES(z, 2, H); break;
                    case 0x95: RES(z, 2, L); break;
                    case 0x97: RES(z, 2, A); break;
                    case 0x98: RES(z, 3, B); break;
                    case 0x99: RES(z, 3, C); break;
                    case 0x9A: RES(z, 3, D); break;
                    case 0x9B: RES(z, 3, E); break;
                    case 0x9C: RES(z, 3, H); break;
                    case 0x9D: RES(z, 3, L); break;
                    case 0x9F: RES(z, 3, A); break;
                    case 0xA0: RES(z, 4, B); break;
                    case 0xA1: RES(z, 4, C); break;
                    case 0xA2: RES(z, 4, D); break;
                    case 0xA3: RES(z, 4, E); break;
                    case 0xA4: RES(z, 4, H); break;
                    case 0xA5: RES(z, 4, L); break;
                    case 0xA7: RES(z, 4, A); break;
                    case 0xA8: RES(z, 5, B); break;
                    case 0xA9: RES(z, 5, C); break;
                    case 0xAA: RES(z, 5, D); break;
                    case 0xAB: RES(z, 5, E); break;
                    case 0xAC: RES(z, 5, H); break;
                    case 0xAD: RES(z, 5, L); break;
                    case 0xAF: RES(z, 5, A); break;
                    case 0xB0: RES(z, 6, B); break;
                    case 0xB1: RES(z, 6, C); break;
                    case 0xB2: RES(z, 6, D); break;
                    case 0xB3: RES(z, 6, E); break;
                    case 0xB4: RES(z, 6, H); break;
                    case 0xB5: RES(z, 6, L); break;
                    case 0xB7: RES(z, 6, A); break;
                    case 0xB8: RES(z, 7, B); break;
                    case 0xB9: RES(z, 7, C); break;
                    case 0xBA: RES(z, 7, D); break;
                    case 0xBB: RES(z, 7, E); break;
                    case 0xBC: RES(z, 7, H); break;
                    case 0xBD: RES(z, 7, L); break;
                    case 0xBF: RES(z, 7, A); break;
                    case 0x86: RES(z, 0, aHL); break;
                    case 0x8E: RES(z, 1, aHL); break;
                    case 0x96: RES(z, 2, aHL); break;
                    case 0x9E: RES(z, 3, aHL); break;
                    case 0xA6: RES(z, 4, aHL); break;
                    case 0xAE: RES(z, 5, aHL); break;
                    case 0xB6: RES(z, 6, aHL); break;
                    case 0xBE: RES(z, 7, aHL); break;
                }
            } break;

            /* this is for instructions following the DD opcode */
            case 0xDD:
            {
                ddOpcode = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                switch (ddOpcode)
                {
                    /* this is a stub for handling the appearance of long sequences
                       of DD or FD */
                    case 0xDD: z->ddStub = true; break;
                    case 0xFD: z->fdStub = true; break;

                    /* this is for multi-opcode 8-bit load instructions */
                    case 0x7E: LD_8Bit(z, A, IXd); break;
                    case 0x46: LD_8Bit(z, B, IXd); break;
                    case 0x4E: LD_8Bit(z, C, IXd); break;
                    case 0x56: LD_8Bit(z, D, IXd); break;
                    case 0x5E: LD_8Bit(z, E, IXd); break;
                    case 0x66: LD_8Bit(z, H, IXd); break;
                    case 0x6E: LD_8Bit(z, L, IXd); break;
                    case 0x77: LD_8Bit(z, IXd, A); break;
                    case 0x70: LD_8Bit(z, IXd, B); break;
                    case 0x71: LD_8Bit(z, IXd, C); break;
                    case 0x72: LD_8Bit(z, IXd, D); break;
                    case 0x73: LD_8Bit(z, IXd, E); break;
                    case 0x74: LD_8Bit(z, IXd, H); break;
                    case 0x75: LD_8Bit(z, IXd, L); break;
                    case 0x36: LD_8Bit(z, IXd, n); break;
                    case 0x26: LD_8Bit(z, IXh, n); break; /* undocumented */
                    case 0x2E: LD_8Bit(z, IXl, n); break; /* undocumented */
                    case 0x44: LD_8Bit(z, B, IXh); break; /* undocumented */
                    case 0x45: LD_8Bit(z, B, IXl); break; /* undocumented */
                    case 0x4C: LD_8Bit(z, C, IXh); break; /* undocumented */
                    case 0x4D: LD_8Bit(z, C, IXl); break; /* undocumented */
                    case 0x54: LD_8Bit(z, D, IXh); break; /* undocumented */
                    case 0x55: LD_8Bit(z, D, IXl); break; /* undocumented */
                    case 0x5C: LD_8Bit(z, E, IXh); break; /* undocumented */
                    case 0x5D: LD_8Bit(z, E, IXl); break; /* undocumented */
                    case 0x60: LD_8Bit(z, IXh, B); break; /* undocumented */
                    case 0x61: LD_8Bit(z, IXh, C); break; /* undocumented */
                    case 0x62: LD_8Bit(z, IXh, D); break; /* undocumented */
                    case 0x63: LD_8Bit(z, IXh, E); break; /* undocumented */
                    case 0x64: LD_8Bit(z, IXh, IXh); break; /* undocumented */
                    case 0x65: LD_8Bit(z, IXh, IXl); break; /* undocumented */
                    case 0x67: LD_8Bit(z, IXh, A); break; /* undocumented */
                    case 0x68: LD_8Bit(z, IXl, B); break; /* undocumented */
                    case 0x69: LD_8Bit(z, IXl, C); break; /* undocumented */
                    case 0x6A: LD_8Bit(z, IXl, D); break; /* undocumented */
                    case 0x6B: LD_8Bit(z, IXl, E); break; /* undocumented */
                    case 0x6C: LD_8Bit(z, IXl, IXh); break; /* undocumented */
                    case 0x6D: LD_8Bit(z, IXl, IXl); break; /* undocumented */
                    case 0x6F: LD_8Bit(z, IXl, A); break; /* undocumented */
                    case 0x7C: LD_8Bit(z, A, IXh); break; /* undocumented */
                    case 0x7D: LD_8Bit(z, A, IXl); break; /* undocumented */
                    case 0x7F: LD_8Bit(z, A, A); break; /* undocumented call */
                    case 0x78: LD_8Bit(z, A, B); break; /* undocumented call */
                    case 0x79: LD_8Bit(z, A, C); break; /* undocumented call */
                    case 0x7A: LD_8Bit(z, A, D); break; /* undocumented call */
                    case 0x7B: LD_8Bit(z, A, E); break; /* undocumented call */
                    case 0x47: LD_8Bit(z, B, A); break; /* undocumented call */
                    case 0x40: LD_8Bit(z, B, B); break; /* undocumented call */
                    case 0x41: LD_8Bit(z, B, C); break; /* undocumented call */
                    case 0x42: LD_8Bit(z, B, D); break; /* undocumented call */
                    case 0x43: LD_8Bit(z, B, E); break; /* undocumented call */
                    case 0x4F: LD_8Bit(z, C, A); break; /* undocumented call */
                    case 0x48: LD_8Bit(z, C, B); break; /* undocumented call */
                    case 0x49: LD_8Bit(z, C, C); break; /* undocumented call */
                    case 0x4A: LD_8Bit(z, C, D); break; /* undocumented call */
                    case 0x4B: LD_8Bit(z, C, E); break; /* undocumented call */
                    case 0x57: LD_8Bit(z, D, A); break; /* undocumented call */
                    case 0x50: LD_8Bit(z, D, B); break; /* undocumented call */
                    case 0x51: LD_8Bit(z, D, C); break; /* undocumented call */
                    case 0x52: LD_8Bit(z, D, D); break; /* undocumented call */
                    case 0x53: LD_8Bit(z, D, E); break; /* undocumented call */
                    case 0x5F: LD_8Bit(z, E, A); break; /* undocumented call */
                    case 0x58: LD_8Bit(z, E, B); break; /* undocumented call */
                    case 0x59: LD_8Bit(z, E, C); break; /* undocumented call */
                    case 0x5A: LD_8Bit(z, E, D); break; /* undocumented call */
                    case 0x5B: LD_8Bit(z, E, E); break; /* undocumented call */

                    /* this is for multi-opcode 16-bit load instructions */
                    case 0x21: LD_16Bit(z, IX, nn, 0); break;
                    case 0x2A: LD_16Bit(z, IX, a_nn, 0); break;
                    case 0x22: LD_16Bit(z, a_nn, IX, 0); break;
                    case 0xF9: LD_16Bit(z, SP, IX, 0); break;
                    case 0xE5: PUSH(z, IX); break;
                    case 0xE1: POP(z, IX); break;

                    /* this is for the 'exchange, block transfer, and search'
                       of instructions with multi-opcodes */
                    case 0xE3: EX(z, aSP, IX); break;

                    /* this is for the '8-bit arithmetic group' of
                       instructions with multi-opcodes */
                    case 0x86: ADD(z, A, IXd); break;
                    case 0x8E: ADC(z, IXd); break;
                    case 0x96: SUB(z, IXd); break;
                    case 0x9E: SBC(z, IXd); break;
                    case 0xA6: AND(z, IXd); break;
                    case 0xB6: OR(z, IXd); break;
                    case 0xAE: XOR(z, IXd); break;
                    case 0xBE: CP(z, IXd); break;
                    case 0x34: INC(z, IXd); break;
                    case 0x35: DEC(z, IXd); break;
                    case 0x24: INC(z, IXh); break; /* undocumented */
                    case 0x25: DEC(z, IXh); break; /* undocumented */
                    case 0x2C: INC(z, IXl); break; /* undocumented */
                    case 0x2D: DEC(z, IXl); break; /* undocumented */
                    case 0x84: ADD(z, A, IXh); break; /* undocumented */
                    case 0x85: ADD(z, A, IXl); break; /* undocumented */
                    case 0x8C: ADC(z, IXh); break; /* undocumented */
                    case 0x8D: ADC(z, IXl); break; /* undocumented */
                    case 0x94: SUB(z, IXh); break; /* undocumented */
                    case 0x95: SUB(z, IXl); break; /* undocumented */
                    case 0x9C: SBC(z, IXh); break; /* undocumented */
                    case 0x9D: SBC(z, IXl); break; /* undocumented */
                    case 0xA4: AND(z, IXh); break; /* undocumented */
                    case 0xA5: AND(z, IXl); break; /* undocumented */
                    case 0xAC: XOR(z, IXh); break; /* undocumented */
                    case 0xAD: XOR(z, IXl); break; /* undocumented */
                    case 0xB4: OR(z, IXh); break; /* undocumented */
                    case 0xB5: OR(z, IXl); break; /* undocumented */
                    case 0xBC: CP(z, IXh); break; /* undocumented */
                    case 0xBD: CP(z, IXl); break; /* undocumented */

                    /* this is for the '16-bit arithmetic group' of
                       instructions with multi-opcodes */
                    case 0x09: ADD(z, IX, BC); break;
                    case 0x19: ADD(z, IX, DE); break;
                    case 0x29: ADD(z, IX, IX); break;
                    case 0x39: ADD(z, IX, SP); break;
                    case 0x23: INC(z, IX); break;
                    case 0x2B: DEC(z, IX); break;

                    /* this is for the 'jump group' of instructions with
                       multi-opcodes */
                    case 0xE9: JP(z, IX); break;

                    /* this is for instructions following the DDCB opcode */
                    case 0xCB:
                    {
                        incrementProgramCounter(z);
                        cbOpcode = readFromMemory(z, readProgramCounter(z));
                        incrementProgramCounter(z);
                        switch (cbOpcode)
                        {
                            /* this is for the 'rotate and shift group' of instructions
                               with multi-opcodes */
                            case 0x06: RLC(z, IXd); break;
                            case 0x16: RL(z, IXd); break;
                            case 0x0E: RRC(z, IXd); break;
                            case 0x1E: RR(z, IXd); break;
                            case 0x26: SLA(z, IXd); break;
                            case 0x2E: SRA(z, IXd); break;
                            case 0x3E: SRL(z, IXd); break;
                            case 0x36: SLL(z, IXd); break; /* undocumented */

                            /* this is for the 'bit set, reset, and test group' of
                               instructions with multi-opcodes */
                            case 0x46: BIT(z, 0, IXd); break;
                            case 0x4E: BIT(z, 1, IXd); break;
                            case 0x56: BIT(z, 2, IXd); break;
                            case 0x5E: BIT(z, 3, IXd); break;
                            case 0x66: BIT(z, 4, IXd); break;
                            case 0x6E: BIT(z, 5, IXd); break;
                            case 0x76: BIT(z, 6, IXd); break;
                            case 0x7E: BIT(z, 7, IXd); break;
                            case 0xC6: SET(z, 0, IXd); break;
                            case 0xCE: SET(z, 1, IXd); break;
                            case 0xD6: SET(z, 2, IXd); break;
                            case 0xDE: SET(z, 3, IXd); break;
                            case 0xE6: SET(z, 4, IXd); break;
                            case 0xEE: SET(z, 5, IXd); break;
                            case 0xF6: SET(z, 6, IXd); break;
                            case 0xFE: SET(z, 7, IXd); break;
                            case 0x86: RES(z, 0, IXd); break;
                            case 0x8E: RES(z, 1, IXd); break;
                            case 0x96: RES(z, 2, IXd); break;
                            case 0x9E: RES(z, 3, IXd); break;
                            case 0xA6: RES(z, 4, IXd); break;
                            case 0xAE: RES(z, 5, IXd); break;
                            case 0xB6: RES(z, 6, IXd); break;
                            case 0xBE: RES(z, 7, IXd); break;
                        }
                    } break;
                }
            } break;

            /* this is for instructions following the ED opcode */
            case 0xED:
            {
                edOpcode = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                switch (edOpcode)
                {
                    /* this is for multi-opcode 8-bit load instructions */
                    case 0x57: LD_8Bit(z, A, I); break;
                    case 0x5F: LD_8Bit(z, A, R); break;
                    case 0x47: LD_8Bit(z, I, A); break;
                    case 0x4F: LD_8Bit(z, R, A); break;

                    /* this is for multi-opcode 16-bit load instructions */
                    case 0x4B: LD_16Bit(z, BC, a_nn, 0); break;
                    case 0x5B: LD_16Bit(z, DE, a_nn, 0); break;
                    case 0x6B: LD_16Bit(z, HL, a_nn, 1); break;
                    case 0x7B: LD_16Bit(z, SP, a_nn, 0); break;
                    case 0x43: LD_16Bit(z, a_nn, BC, 0); break;
                    case 0x53: LD_16Bit(z, a_nn, DE, 0); break;
                    case 0x63: LD_16Bit(z, a_nn, HL, 1); break;
                    case 0x73: LD_16Bit(z, a_nn, SP, 0); break;

                    /* this is for the 'exchange, block transfer, and search'
                       of instructions with multi-opcodes */
                    case 0xA0: LDI(z); break;
                    case 0xB0: LDIR(z); break;
                    case 0xA8: LDD(z); break;
                    case 0xB8: LDDR(z); break;
                    case 0xA1: CPI(z); break;
                    case 0xB1: CPIR(z); break;
                    case 0xA9: CPD(z); break;
                    case 0xB9: CPDR(z); break;

                    /* this is for multi-opcode Call and Return instructions */
                    case 0x45: RETN(z); break;

                    /* this is for the 'general purpose arithmetic and CPU
                       control group' of instructions with multi-opcodes */
                    case 0x44: NEG(z); break;
                    case 0x46: IM(z, 0); break;
                    case 0x56: IM(z, 1); break;
                    case 0x5E: IM(z, 2); break;

                    /* this is for the '16-bit arithmetic group' of
                       instructions with multi-opcodes */
                    case 0x4A: ADC(z, BC); break;
                    case 0x5A: ADC(z, DE); break;
                    case 0x6A: ADC(z, HL); break;
                    case 0x7A: ADC(z, SP); break;
                    case 0x42: SBC(z, BC); break;
                    case 0x52: SBC(z, DE); break;
                    case 0x62: SBC(z, HL); break;
                    case 0x72: SBC(z, SP); break;

                    /* this is for the 'rotate and shift group' of instructions
                       with multi-opcodes */
                    case 0x6F: RLD(z); break;
                    case 0x67: RRD(z); break;

                    /* this is for the 'call and return group' of instructions
                       with multi-opcodes */
                    case 0x4D: RETI(z); break;

                    /* this is for the 'input and output group' of instructions
                       with multi-opcodes */
                    case 0x40: IN(z, B, aC); break;
                    case 0x48: IN(z, C, aC); break;
                    case 0x50: IN(z, D, aC); break;
                    case 0x58: IN(z, E, aC); break;
                    case 0x60: IN(z, H, aC); break;
                    case 0x68: IN(z, L, aC); break;
                    case 0x78: IN(z, A, aC); break;
                    case 0xA2: INI(z); break;
                    case 0xB2: INIR(z); break;
                    case 0xAA: IND(z); break;
                    case 0xBA: INDR(z); break;
                    case 0x41: OUT(z, aC, B); break;
                    case 0x49: OUT(z, aC, C); break;
                    case 0x51: OUT(z, aC, D); break;
                    case 0x59: OUT(z, aC, E); break;
                    case 0x61: OUT(z, aC, H); break;
                    case 0x69: OUT(z, aC, L); break;
                    case 0x79: OUT(z, aC, A); break;
                    case 0xA3: OUTI(z); break;
                    case 0xB3: OTIR(z); break;
                    case 0xAB: OUTD(z); break;
                    case 0xBB: OTDR(z); break;
                }
            } break;

            /* this is for instructions following the FD opcode */
            case 0xFD:
            {
                fdOpcode = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                switch (fdOpcode)
                {
                    /* this is a stub for handling the appearance of long sequences
                       of DD or FD */
                    case 0xDD: z->ddStub = true; break;
                    case 0xFD: z->fdStub = true; break;

                    /* this is for multi-opcode 8-bit load instructions */
                    case 0x7E: LD_8Bit(z, A, IYd); break;
                    case 0x46: LD_8Bit(z, B, IYd); break;
                    case 0x4E: LD_8Bit(z, C, IYd); break;
                    case 0x56: LD_8Bit(z, D, IYd); break;
                    case 0x5E: LD_8Bit(z, E, IYd); break;
                    case 0x66: LD_8Bit(z, H, IYd); break;
                    case 0x6E: LD_8Bit(z, L, IYd); break;
                    case 0x77: LD_8Bit(z, IYd, A); break;
                    case 0x70: LD_8Bit(z, IYd, B); break;
                    case 0x71: LD_8Bit(z, IYd, C); break;
                    case 0x72: LD_8Bit(z, IYd, D); break;
                    case 0x73: LD_8Bit(z, IYd, E); break;
                    case 0x74: LD_8Bit(z, IYd, H); break;
                    case 0x75: LD_8Bit(z, IYd, L); break;
                    case 0x36: LD_8Bit(z, IYd, n); break;
                    case 0x26: LD_8Bit(z, IYh, n); break; /* undocumented */
                    case 0x2E: LD_8Bit(z, IYl, n); break; /* undocumented */
                    case 0x44: LD_8Bit(z, B, IYh); break; /* undocumented */
                    case 0x45: LD_8Bit(z, B, IYl); break; /* undocumented */
                    case 0x4C: LD_8Bit(z, C, IYh); break; /* undocumented */
                    case 0x4D: LD_8Bit(z, C, IYl); break; /* undocumented */
                    case 0x54: LD_8Bit(z, D, IYh); break; /* undocumented */
                    case 0x55: LD_8Bit(z, D, IYl); break; /* undocumented */
                    case 0x5C: LD_8Bit(z, E, IYh); break; /* undocumented */
                    case 0x5D: LD_8Bit(z, E, IYl); break; /* undocumented */
                    case 0x60: LD_8Bit(z, IYh, B); break; /* undocumented */
                    case 0x61: LD_8Bit(z, IYh, C); break; /* undocumented */
                    case 0x62: LD_8Bit(z, IYh, D); break; /* undocumented */
                    case 0x63: LD_8Bit(z, IYh, E); break; /* undocumented */
                    case 0x64: LD_8Bit(z, IYh, IYh); break; /* undocumented */
                    case 0x65: LD_8Bit(z, IYh, IYl); break; /* undocumented */
                    case 0x67: LD_8Bit(z, IYh, A); break; /* undocumented */
                    case 0x68: LD_8Bit(z, IYl, B); break; /* undocumented */
                    case 0x69: LD_8Bit(z, IYl, C); break; /* undocumented */
                    case 0x6A: LD_8Bit(z, IYl, D); break; /* undocumented */
                    case 0x6B: LD_8Bit(z, IYl, E); break; /* undocumented */
                    case 0x6C: LD_8Bit(z, IYl, IYh); break; /* undocumented */
                    case 0x6D: LD_8Bit(z, IYl, IYl); break; /* undocumented */
                    case 0x6F: LD_8Bit(z, IYl, A); break; /* undocumented */
                    case 0x7C: LD_8Bit(z, A, IYh); break; /* undocumented */
                    case 0x7D: LD_8Bit(z, A, IYl); break; /* undocumented */
                    case 0x7F: LD_8Bit(z, A, A); break; /* undocumented call */
                    case 0x78: LD_8Bit(z, A, B); break; /* undocumented call */
                    case 0x79: LD_8Bit(z, A, C); break; /* undocumented call */
                    case 0x7A: LD_8Bit(z, A, D); break; /* undocumented call */
                    case 0x7B: LD_8Bit(z, A, E); break; /* undocumented call */
                    case 0x47: LD_8Bit(z, B, A); break; /* undocumented call */
                    case 0x40: LD_8Bit(z, B, B); break; /* undocumented call */
                    case 0x41: LD_8Bit(z, B, C); break; /* undocumented call */
                    case 0x42: LD_8Bit(z, B, D); break; /* undocumented call */
                    case 0x43: LD_8Bit(z, B, E); break; /* undocumented call */
                    case 0x4F: LD_8Bit(z, C, A); break; /* undocumented call */
                    case 0x48: LD_8Bit(z, C, B); break; /* undocumented call */
                    case 0x49: LD_8Bit(z, C, C); break; /* undocumented call */
                    case 0x4A: LD_8Bit(z, C, D); break; /* undocumented call */
                    case 0x4B: LD_8Bit(z, C, E); break; /* undocumented call */
                    case 0x57: LD_8Bit(z, D, A); break; /* undocumented call */
                    case 0x50: LD_8Bit(z, D, B); break; /* undocumented call */
                    case 0x51: LD_8Bit(z, D, C); break; /* undocumented call */
                    case 0x52: LD_8Bit(z, D, D); break; /* undocumented call */
                    case 0x53: LD_8Bit(z, D, E); break; /* undocumented call */
                    case 0x5F: LD_8Bit(z, E, A); break; /* undocumented call */
                    case 0x58: LD_8Bit(z, E, B); break; /* undocumented call */
                    case 0x59: LD_8Bit(z, E, C); break; /* undocumented call */
                    case 0x5A: LD_8Bit(z, E, D); break; /* undocumented call */
                    case 0x5B: LD_8Bit(z, E, E); break; /* undocumented call */

                    /* this is for multi-opcode 16-bit load instructions */
                    case 0x21: LD_16Bit(z, IY, nn, 0); break;
                    case 0x2A: LD_16Bit(z, IY, a_nn, 0); break;
                    case 0x22: LD_16Bit(z, a_nn, IY, 0); break;
                    case 0xF9: LD_16Bit(z, SP, IY, 0); break;
                    case 0xE5: PUSH(z, IY); break;
                    case 0xE1: POP(z, IY); break;

                    /* this is for the 'exchange, block transfer, and search'
                       of instructions with multi-opcodes */
                    case 0xE3: EX(z, aSP, IY); break;

                    /* this is for the '8-bit arithmetic group' of
                       instructions with multi-opcodes */
                    case 0x86: ADD(z, A, IYd); break;
                    case 0x8E: ADC(z, IYd); break;
                    case 0x96: SUB(z, IYd); break;
                    case 0x9E: SBC(z, IYd); break;
                    case 0xA6: AND(z, IYd); break;
                    case 0xB6: OR(z, IYd); break;
                    case 0xAE: XOR(z, IYd); break;
                    case 0xBE: CP(z, IYd); break;
                    case 0x34: INC(z, IYd); break;
                    case 0x35: DEC(z, IYd); break;
                    case 0x24: INC(z, IYh); break; /* undocumented */
                    case 0x25: DEC(z, IYh); break; /* undocumented */
                    case 0x2C: INC(z, IYl); break; /* undocumented */
                    case 0x2D: DEC(z, IYl); break; /* undocumented */
                    case 0x84: ADD(z, A, IYh); break; /* undocumented */
                    case 0x85: ADD(z, A, IYl); break; /* undocumented */
                    case 0x8C: ADC(z, IYh); break; /* undocumented */
                    case 0x8D: ADC(z, IYl); break; /* undocumented */
                    case 0x94: SUB(z, IYh); break; /* undocumented */
                    case 0x95: SUB(z, IYl); break; /* undocumented */
                    case 0x9C: SBC(z, IYh); break; /* undocumented */
                    case 0x9D: SBC(z, IYl); break; /* undocumented */
                    case 0xA4: AND(z, IYh); break; /* undocumented */
                    case 0xA5: AND(z, IYl); break; /* undocumented */
                    case 0xAC: XOR(z, IYh); break; /* undocumented */
                    case 0xAD: XOR(z, IYl); break; /* undocumented */
                    case 0xB4: OR(z, IYh); break; /* undocumented */
                    case 0xB5: OR(z, IYl); break; /* undocumented */
                    case 0xBC: CP(z, IYh); break; /* undocumented */
                    case 0xBD: CP(z, IYl); break; /* undocumented */

                    /* this is for the '16-bit arithmetic group' of
                       instructions with multi-opcodes */
                    case 0x09: ADD(z, IY, BC); break;
                    case 0x19: ADD(z, IY, DE); break;
                    case 0x29: ADD(z, IY, IY); break;
                    case 0x39: ADD(z, IY, SP); break;
                    case 0x23: INC(z, IY); break;
                    case 0x2B: DEC(z, IY); break;

                    /* this is for the 'jump group' of instructions with
                       multi-opcodes */
                    case 0xE9: JP(z, IY); break;

                    /* this is for instructions following the FDCB opcode */
                    case 0xCB:
                    {
                        incrementProgramCounter(z);
                        cbOpcode = readFromMemory(z, readProgramCounter(z));
                        incrementProgramCounter(z);
                        switch (cbOpcode)
                        {
                            /* this is for the 'rotate and shift group' of instructions
                               with multi-opcodes */
                            case 0x06: RLC(z, IYd); break;
                            case 0x16: RL(z, IYd); break;
                            case 0x0E: RRC(z, IYd); break;
                            case 0x1E: RR(z, IYd); break;
                            case 0x26: SLA(z, IYd); break;
                            case 0x2E: SRA(z, IYd); break;
                            case 0x3E: SRL(z, IYd); break;
                            case 0x36: SLL(z, IXd); break; /* undocumented */
                                
                            /* this is for the 'bit set, reset, and test group' of
                               instructions with multi-opcodes */
                            case 0x46: BIT(z, 0, IYd); break;
                            case 0x4E: BIT(z, 1, IYd); break;
                            case 0x56: BIT(z, 2, IYd); break;
                            case 0x5E: BIT(z, 3, IYd); break;
                            case 0x66: BIT(z, 4, IYd); break;
                            case 0x6E: BIT(z, 5, IYd); break;
                            case 0x76: BIT(z, 6, IYd); break;
                            case 0x7E: BIT(z, 7, IYd); break;
                            case 0xC6: SET(z, 0, IYd); break;
                            case 0xCE: SET(z, 1, IYd); break;
                            case 0xD6: SET(z, 2, IYd); break;
                            case 0xDE: SET(z, 3, IYd); break;
                            case 0xE6: SET(z, 4, IYd); break;
                            case 0xEE: SET(z, 5, IYd); break;
                            case 0xF6: SET(z, 6, IYd); break;
                            case 0xFE: SET(z, 7, IYd); break;
                            case 0x86: RES(z, 0, IYd); break;
                            case 0x8E: RES(z, 1, IYd); break;
                            case 0x96: RES(z, 2, IYd); break;
                            case 0x9E: RES(z, 3, IYd); break;
                            case 0xA6: RES(z, 4, IYd); break;
                            case 0xAE: RES(z, 5, IYd); break;
                            case 0xB6: RES(z, 6, IYd); break;
                            case 0xBE: RES(z, 7, IYd); break;
                        }
                    } break;
                }
            } break;

        }
    }

    if (z->interruptPending)
        z->interruptCounter += z->cycles;

    return z->cycles;
}

/* this function hides the details, meaning the Z80 internals can just write to memory addresses
   with this function and the implementation within will do the rest */
static void writeToMemory(Z80 z, emuint address, emubyte data)
{
    console_memWrite(z->ms, address, 0xFF & data);
}

/* this function hides the details, meaning the Z80 internals can just read from memory addresses
 with this function and the implementation within will do the rest */
static emubyte readFromMemory(Z80 z, emuint address)
{
    return 0xFF & console_memRead(z->ms, address);
}

/* this function hides the details, meaning the Z80 internals can just write to I/O addresses
   with this function and the implementation within will do the rest */
static void writeToIO(Z80 z, emuint address, emubyte data)
{
    console_ioWrite(z->ms, address, 0xFF & data);
}

/* this function hides the details, meaning the Z80 internals can just read from I/O addresses
   with this function and the implementation within will do the rest */
static emubyte readFromIO(Z80 z, emuint address)
{
    return 0xFF & console_ioRead(z->ms, address);
}

/* this function sets or resets the S flag */
static void setSFlag(Z80 z, emubool value)
{
    if (value)
        z->regF = 0xFF & (z->regF | 0x80);
    else
        z->regF = z->regF & 0x7F;
}

/* this function sets or resets the Z flag */
static void setZFlag(Z80 z, emubool value)
{
    if (value)
        z->regF = 0xFF & (z->regF | 0x40);
    else
        z->regF = z->regF & 0xBF;
}

/* this function sets or resets the H flag */
static void setHFlag(Z80 z, emubool value)
{
    if (value)
        z->regF = 0xFF & (z->regF | 0x10);
    else
        z->regF = z->regF & 0xEF;
}

/* this function sets or resets the P/V flag */
static void setPVFlag(Z80 z, emubool value)
{
    if (value)
        z->regF = 0xFF & (z->regF | 0x04);
    else
        z->regF = z->regF & 0xFB;
}

/* this function sets or resets the N flag */
static void setNFlag(Z80 z, emubool value)
{
    if (value)
        z->regF = 0xFF & (z->regF | 0x02);
    else
        z->regF = z->regF & 0xFD;
}

/* this function sets or resets the C flag */
static void setCFlag(Z80 z, emubool value)
{
    if (value)
        z->regF = 0xFF & (z->regF | 0x01);
    else
        z->regF = z->regF & 0xFE;
}

/* this function writes a new value to the program counter */
static void writeProgramCounter(Z80 z, emuint address)
{
    z->regPC = 0xFFFF & address;
}

/* this function reads the program counter */
static emuint readProgramCounter(Z80 z)
{
    return 0xFFFF & z->regPC;
}

/* this function increments the program counter by one */
static void incrementProgramCounter(Z80 z)
{
    emuint tempPC = readProgramCounter(z);
    ++tempPC;
    z->regPC = 0xFFFF & tempPC;
}

/* this function decrements the program counter by one */
static void decrementProgramCounter(Z80 z)
{
    emuint tempPC = readProgramCounter(z);
    --tempPC;
    z->regPC = 0xFFFF & tempPC;
}

/* this increments the low seven bits of the refresh register */
static void incrementRefreshRegister(Z80 z)
{
    emubyte temp = 0x7F & z->regR;
    ++temp;
    if (temp > 127)
        temp = 0;
    z->regR = (z->regR & 0x80) | (temp & 0x7F);
}

/* this function writes a new value to the stack pointer */
static void writeStackPointer(Z80 z, emuint address)
{
    z->regSP = 0xFFFF & address;
}

/* this function reads the stack pointer */
static emuint readStackPointer(Z80 z)
{
    return 0xFFFF & z->regSP;
}

/* this function increments the stack pointer by one */
static void incrementStackPointer(Z80 z)
{
    z->regSP = 0xFFFF & (z->regSP + 1);
}

/* this function decrements the stack pointer by one */
static void decrementStackPointer(Z80 z)
{
    z->regSP = 0xFFFF & (z->regSP - 1);
}

/* this function calculates the parity of the result passed in and
sets the P/V flag accordingly */
static void calculatePVFlag(Z80 z, emubyte temp)
{
    emubyte parityCheck = 0xFF & temp;
    emubyte parityCount = 0;
    emubyte i;
    for (i = 0; i < 8; ++i) {
        parityCount = parityCount + (parityCheck & 0x01);
        parityCheck = parityCheck >> 1;
    }
    if (parityCount % 2 == 0)
        setPVFlag(z, true);
    else
        setPVFlag(z, false);
}

/* this function checks if result passed in is zero and sets the Z flag accordingly */
static void calculateZFlag(Z80 z, emubyte temp)
{
    if ((temp & 0xFF) == 0)
        setZFlag(z, true);
    else
        setZFlag(z, false);
}

/* this function checks if result passed in is negative and sets S flag accordingly */
static void calculateSFlag(Z80 z, emubyte temp)
{
    if ((temp & 0x80) == 128)
        setSFlag(z, true);
    else
        setSFlag(z, false);
}

/* this function checks if a borrow has occurred from bit 4 (during subtraction) or
   a carry from bit 3 (during addition) and sets the H flag accordingly */
static void calculateHFlag(Z80 z, emubyte operand1, emubyte operand2, emuint result)
{
    if ((((operand1 ^ operand2) ^ result) & 0x10) == 16)
        setHFlag(z, true);
    else
        setHFlag(z, false);
}

/* this function checks if a borrow has occurred from bit 8 (during subtraction) or
   a carry from bit 7 (during addition) and sets the C flag accordingly */
static void calculateCFlag(Z80 z, emubyte operand, emuint result)
{
    if ((((z->regA ^ operand) ^ result) & 0x100) == 256)
        setCFlag(z, true);
    else
        setCFlag(z, false);
}

/* this functions checks for overflow and sets the P/V flag if so, else resets it */
static void calculatePVOverflow(Z80 z, emubyte operand, emuint result, calcType type)
{
    if (((z->regA ^ operand) & 0x80) == type) {
        if (((z->regA ^ result) & 0x80) == 128)
            setPVFlag(z, true);
        else
            setPVFlag(z, false);
    } else {
        setPVFlag(z, false);
    }
}

/* this function checks if the 16-bit result passed in is zero and sets the Z flag accordingly */
static void calculateZFlag16(Z80 z, emuint temp)
{
    if ((temp & 0xFFFF) == 0)
        setZFlag(z, true);
    else
        setZFlag(z, false);
}

/* this function checks if the 16-bit result passed in is negative and sets S flag accordingly */
static void calculateSFlag16(Z80 z, emuint temp)
{
    if ((temp & 0x8000) == 32768)
        setSFlag(z, true);
    else
        setSFlag(z, false);
}

/* this function checks if a borrow has occurred from bit 12 (during subtraction) or
   a carry from bit 11 (during addition) and sets the H flag accordingly */
static void calculateHFlag16(Z80 z, emuint tempHL, emuint operand, emuint result)
{
    if ((((tempHL ^ operand) ^ result) & 0x1000) == 4096)
        setHFlag(z, true);
    else
        setHFlag(z, false);
}

/* this function checks if a borrow has occurred from bit 16 (during subtraction) or
   a carry from bit 15 (during addition) and sets the C flag accordingly */
static void calculateCFlag16(Z80 z, emuint tempHL, emuint operand, emuint result)
{
    if ((((tempHL ^ operand) ^ result) & 0x10000) == 65536)
        setCFlag(z, true);
    else
        setCFlag(z, false);
}

/* this functions checks for 16-bit overflow and sets the P/V flag if so, else resets it */
static void calculatePVOverflow16(Z80 z, emuint tempHL, emuint operand, emuint result, calcType type)
{
    if (((tempHL ^ operand) & 0x8000) == type) {
        if (((tempHL ^ result) & 0x8000) == 32768)
            setPVFlag(z, true);
        else
            setPVFlag(z, false);
    } else {
        setPVFlag(z, false);
    }
}


/* this function emulates the NOP instruction */
static void NOP(Z80 z)
{
    z->cycles = 4;
}

/* this function emulates the DAA instruction - it was written using Sean Young's
   reverse engineered information, which is largely undocumented by Zilog */
static void DAA(Z80 z)
{
    /* import relevant flags */
    emubool cFlag = false, nFlag = false, hFlag = false;
    if ((z->regF & 0x01) == 1)
        cFlag = true;
    if ((z->regF & 0x02) == 2)
        nFlag = true;
    if ((z->regF & 0x10) == 16)
        hFlag = true;

    /* import high and low nibbles from accumulator */
    emubyte highNibble = 0x0F & (z->regA >> 4);
    emubyte lowNibble = 0x0F & z->regA;

    /* test flag and accumulator and make necessary adjustment */
    emubyte modification = 0;
    if (cFlag) {
        if (lowNibble >= 10 && lowNibble <= 15) {
            modification = 0x66;
        }
        else if (lowNibble >= 0 && lowNibble <= 9) {
            if (hFlag)
                modification = 0x66;
            else
                modification = 0x60;
        }
    }
    else {
        if (lowNibble >= 10 && lowNibble <= 15) {
            if (highNibble >= 0 && highNibble <= 8)
                modification = 0x06;
            else if (highNibble >= 9 && highNibble <= 15)
                modification = 0x66;
        }
        else if (lowNibble >= 0 && lowNibble <= 9) {
            if (highNibble >= 0 && highNibble <= 9) {
                if (hFlag)
                    modification = 0x06;
                else
                    modification = 0;
            }
            else if (highNibble >= 10 && highNibble <= 15) {
                if (hFlag)
                    modification = 0x66;
                else
                    modification = 0x60;
            }
        }
    }

    /* calculate what carry flag will be */
    emubool newCFlag = false;
    if (cFlag) {
        newCFlag = true;
    }
    else {
        if (lowNibble >= 0 && lowNibble <= 9) {
            if (highNibble >= 0 && highNibble <= 9)
                newCFlag = false;
            else if (highNibble >= 10 && highNibble <= 15)
                newCFlag = true;
        }
        else if (lowNibble >= 10 && lowNibble <= 15) {
            if (highNibble >= 0 && highNibble <= 8)
                newCFlag = false;
            else if (highNibble >= 9 && highNibble <= 15)
                newCFlag = true;
        }
    }

    /* calculate what half carry flag will be */
    emubool newHFlag = false;
    if (nFlag) {
        if (hFlag) {
            if (lowNibble >= 0 && lowNibble <= 5)
                newHFlag = true;
            else if (lowNibble >= 6 && lowNibble <= 15)
                newHFlag = false;
        } else {
            newHFlag = false;
        }
    }
    else {
        if (lowNibble >= 0 && lowNibble <= 9)
            newHFlag = false;
        else if (lowNibble >= 10 && lowNibble <= 15)
            newHFlag = true;
    }

    /* perform calculation */
    if (nFlag)
        z->regA = 0xFF & (z->regA - modification);
    else
        z->regA = 0xFF & (z->regA + modification);

    /* set C and H flags accordingly */
    setCFlag(z, newCFlag);
    setHFlag(z, newHFlag);

    /* calculate S flag */
    calculateSFlag(z, z->regA);

    /* copy bits 5 and 3 to their respective places in reg F */
    z->regF = (0xD7 & z->regF) | (0x28 & z->regA);

    /* set Z flag accordingly */
    calculateZFlag(z, z->regA);

    /* calculate parity of result and set P/V flag accordingly */
    calculatePVFlag(z, z->regA);

    /* set number of cycles */
    z->cycles = 4;
}

/* this function emulates the EI instruction */
static void EI(Z80 z)
{
    /* set both interrupt flip-flop registers to 1 */
    z->iff1 = 1;
    z->iff2 = 1;

    /* prevent interrupts from being accepted until end of next instruction */
    z->followingInstruction = true;

    /* set number of cycles */
    z->cycles = 4;
}

/* this function emulates the DI instruction */
static void DI(Z80 z)
{
    /* set both interrupt flip-flop registers to 0 */
    z->iff1 = 0;
    z->iff2 = 0;

    /* set number of cycles */
    z->cycles = 4;
}

/* this function emulates the HALT instruction */
static void HALT(Z80 z)
{
    /* halt the CPU */
    z->halt = 1;

    /* set number of cycles */
    z->cycles = 4;
}

/* this function emulates instructions IM0, IM1 and IM2 */
static void IM(Z80 z, emubyte mode)
{
    /* set interrupt mode */
    z->intMode = mode;

    /* set number of cycles */
    z->cycles = 8;
}

/* this function emulates all of the XOR instructions */
static void XOR(Z80 z, cpuVal value)
{
    /* obtain relevant value and set cycle count */
    emubyte temp = 0, result = 0;
    signed_emubyte d = 0;
    switch (value) {
        case A: temp = z->regA; z->cycles = 4; break;
        case B: temp = z->regB; z->cycles = 4; break;
        case C: temp = z->regC; z->cycles = 4; break;
        case D: temp = z->regD; z->cycles = 4; break;
        case E: temp = z->regE; z->cycles = 4; break;
        case H: temp = z->regH; z->cycles = 4; break;
        case L: temp = z->regL; z->cycles = 4; break;
        case IXh: temp = 0xFF & (z->regIX >> 8); z->cycles = 4; break;
        case IXl: temp = 0xFF & z->regIX; z->cycles = 4; break;
        case IYh: temp = 0xFF & (z->regIY >> 8); z->cycles = 4; break;
        case IYl: temp = 0xFF & z->regIY; z->cycles = 4; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 7; break;
        case n: temp = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                z->cycles = 7; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 19; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 19; break;
        default: break;
    }

    /* do logical XOR between values */
    result = z->regA ^ temp;

    /* calculate S, Z and P/V flags */
    calculateSFlag(z, result);
    calculateZFlag(z, result);
    calculatePVFlag(z, result);

    /* reset H, N and C flags */
    setHFlag(z, false);
    setNFlag(z, false);
    setCFlag(z, false);

    /* store result back to accumulator */
    z->regA = 0xFF & result;
}

/* this function emulates all of the BIT instructions */
static void BIT(Z80 z, emubyte bit, cpuVal value)
{
    /* get relevant value and set cycle count */
    emubyte temp = 0;
    signed_emubyte d = 0;
    emubyte bitToTest = 1 << bit;
    switch (value) {
        case A: temp = z->regA; z->cycles = 8; break;
        case B: temp = z->regB; z->cycles = 8; break;
        case C: temp = z->regC; z->cycles = 8; break;
        case D: temp = z->regD; z->cycles = 8; break;
        case E: temp = z->regE; z->cycles = 8; break;
        case H: temp = z->regH; z->cycles = 8; break;
        case L: temp = z->regL; z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 12; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 20; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 20; break;
        default: break;
    }
    
    /* check relevant bit and set Z flag if unset, else reset it */
    temp &= bitToTest;
    calculateZFlag(z, temp);
    if (temp == 0)
        setPVFlag(z, true); /* this is undocumented */
    else if (bit == 7)
        setSFlag(z, true); /* this is undocumented */

    /* deal with other flags */
    setHFlag(z, true);
    setNFlag(z, false);
}

/* this function emulates all of the RST instructions */
static void RST(Z80 z, emubyte vector)
{
    /* push PC value onto stack */
    emubyte low = readProgramCounter(z) & 0xFF;
    emubyte high = (readProgramCounter(z) >> 8) & 0xFF;
    decrementStackPointer(z);
    writeToMemory(z, readStackPointer(z), high);
    decrementStackPointer(z);
    writeToMemory(z, readStackPointer(z), low);
    
    /* write new address to PC */
    writeProgramCounter(z, vector);
    
    /* set number of cycles */
    z->cycles = 11;
}

/* this function emulates the EXX instruction */
static void EXX(Z80 z)
{
    /* define temporary variables */
    emubyte tempHigh, tempLow;
    
    /* use temporary variables to swap registers and shadow registers around */
    tempHigh = z->regB;
    tempLow = z->regC;
    z->regB = z->regBShadow;
    z->regC = z->regCShadow;
    z->regBShadow = tempHigh;
    z->regCShadow = tempLow;
    tempHigh = z->regD;
    tempLow = z->regE;
    z->regD = z->regDShadow;
    z->regE = z->regEShadow;
    z->regDShadow = tempHigh;
    z->regEShadow = tempLow;
    tempHigh = z->regH;
    tempLow = z->regL;
    z->regH = z->regHShadow;
    z->regL = z->regLShadow;
    z->regHShadow = tempHigh;
    z->regLShadow = tempLow;
    
    /* set number of cycles */
    z->cycles = 4;
}

/* this function emulates all of the SET instructions */
static void SET(Z80 z, emubyte bit, cpuVal value)
{
    /* get relevant value and set cycle count */
    emubyte temp = 0;
    signed_emubyte d = 0;
    emubyte bitToSet = 1 << bit;
    switch (value) {
        case A: temp = z->regA; z->cycles = 8; break;
        case B: temp = z->regB; z->cycles = 8; break;
        case C: temp = z->regC; z->cycles = 8; break;
        case D: temp = z->regD; z->cycles = 8; break;
        case E: temp = z->regE; z->cycles = 8; break;
        case H: temp = z->regH; z->cycles = 8; break;
        case L: temp = z->regL; z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 15; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        default: break;
    }

    /* set relevant bit of value */
    temp |= bitToSet;

    /* store value back to correct location */
    switch (value) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        default: break;
    }
}

/* this function emulates all of the RES instructions */
static void RES(Z80 z, emubyte bit, cpuVal value)
{
    /* get relevant value and set cycle count */
    emubyte temp = 0;
    signed_emubyte d = 0;
    emubyte bitToReset = 1 << bit;
    switch (value) {
        case A: temp = z->regA; z->cycles = 8; break;
        case B: temp = z->regB; z->cycles = 8; break;
        case C: temp = z->regC; z->cycles = 8; break;
        case D: temp = z->regD; z->cycles = 8; break;
        case E: temp = z->regE; z->cycles = 8; break;
        case H: temp = z->regH; z->cycles = 8; break;
        case L: temp = z->regL; z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 15; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        default: break;
    }

    /* reset relevant bit of value */
    bitToReset = ~bitToReset;
    temp &= bitToReset;

    /* store value back to correct location */
    switch (value) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        default: break;
    }
}

/* this function emulates all of the 8-bit LD instructions */
static void LD_8Bit(Z80 z, cpuVal destination, cpuVal source)
{
    /* define variables */
    emubyte temp = 0, low = 0, high = 0;
    signed_emubyte d = 0;

    /* gather 'd' now if needed, as it is easier */
    if (destination == IXd || destination == IYd ||
        source == IXd || source == IYd) {
        d = readFromMemory(z, readProgramCounter(z));
        incrementProgramCounter(z);
    }

    /* retrieve value */
    switch (source) {
        case A: temp = z->regA; break;
        case B: temp = z->regB; break;
        case C: temp = z->regC; break;
        case D: temp = z->regD; break;
        case E: temp = z->regE; break;
        case H: temp = z->regH; break;
        case L: temp = z->regL; break;
        case I: temp = z->regI; break;
        case R: temp = z->regR; break;
        case n: temp = readFromMemory(z, readProgramCounter(z));
                       incrementProgramCounter(z); break;
        case IXh: temp = 0xFF & (z->regIX >> 8); break;
        case IXl: temp = 0xFF & z->regIX; break;
        case IYh: temp = 0xFF & (z->regIY >> 8); break;
        case IYl: temp = 0xFF & z->regIY; break;
        case aBC: temp = readFromMemory(z, 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF)));
                 break;
        case aDE: temp = readFromMemory(z, 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF)));
                 break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                 break;
        case IXd: temp = readFromMemory(z, 0xFFFF & (z->regIX + d)); break;
        case IYd: temp = readFromMemory(z, 0xFFFF & (z->regIY + d)); break;
        case a_nn: low = readFromMemory(z, readProgramCounter(z));
                   incrementProgramCounter(z);
                   high = readFromMemory(z, readProgramCounter(z));
                   incrementProgramCounter(z);
                   temp = readFromMemory(z, 0xFFFF & ((high << 8) | (low & 0xFF)));
                   break;
        default: break;
    }

    /* test for case of reg I or R, and deal with flags if so */
    if (source == I || source == R) {
        calculateSFlag(z, temp);
        calculateZFlag(z, temp);
        setHFlag(z, false);
        if (z->iff2 == 1)
            setPVFlag(z, true);
        else
            setPVFlag(z, false);
        setNFlag(z, false);
        if (console_checkInterrupt(z->ms) || z->nmi == 1)
            setPVFlag(z, false);
    }

    /* store value */
    switch (destination) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case I: z->regI = temp; break;
        case R: z->regR = temp; break;
        case IXh: z->regIX = 0xFFFF & ((temp << 8) | (z->regIX & 0xFF)); break;
        case IXl: z->regIX = 0xFFFF & ((z->regIX & 0xFF00) | (temp & 0xFF)); break;
        case IYh: z->regIY = 0xFFFF & ((temp << 8) | (z->regIY & 0xFF)); break;
        case IYl: z->regIY = 0xFFFF & ((z->regIY & 0xFF00) | (temp & 0xFF)); break;
        case aBC: writeToMemory(z, 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF)), temp);
                  break;
        case aDE: writeToMemory(z, 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF)), temp);
                  break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        case a_nn: low = readFromMemory(z, readProgramCounter(z));
                   incrementProgramCounter(z);
                   high = readFromMemory(z, readProgramCounter(z));
                   incrementProgramCounter(z);
                   writeToMemory(z, 0xFFFF & ((high << 8) | (low & 0xFF)), temp);
                   break;
        default: break;
    }
    
    /* set cycle count accordingly */
    z->cycles = 4; /* the majority of LD 8-bit instructions take this many cycles */
    if (destination == IXd || destination == IYd ||
        source == IXd || source == IYd) {
        z->cycles = 19;
    }
    else if (destination == a_nn || source == a_nn)
        z->cycles = 13;
    else if (destination == aHL && source == n)
        z->cycles = 10;    
    else if (destination == R || destination == I ||
             source == R || source == I) {
        z->cycles = 9;
    }
    else if (destination == aBC || destination == aDE ||
             destination == aHL || destination == n ||
             source == aBC || source == aDE ||
             source == aHL || source == n) {
        z->cycles = 7;
    }
}

/* this function emulates all of the 16-bit LD instructions */
static void LD_16Bit(Z80 z, cpuVal destination, cpuVal source, emubyte longer)
{
    /* define variables */
    emubyte low = 0, high = 0;
    emuint temp = 0, address = 0;

    /* retrieve value */
    switch (source) {
        case BC: temp = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF)); break;
        case DE: temp = 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF)); break;
        case HL: temp = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)); break;
        case IX: temp = z->regIX; break;
        case IY: temp = z->regIY; break;
        case SP: temp = z->regSP; break;
        case nn: low = readFromMemory(z, readProgramCounter(z));
                 incrementProgramCounter(z);
                 high = readFromMemory(z, readProgramCounter(z));
                 incrementProgramCounter(z);
                 temp = 0xFFFF & ((high << 8) | (low & 0xFF)); break;
        case a_nn: low = readFromMemory(z, readProgramCounter(z));
                   incrementProgramCounter(z);
                   high = readFromMemory(z, readProgramCounter(z));
                   incrementProgramCounter(z);
                   address = 0xFFFF & ((high << 8) | (low & 0xFF));
                   temp = readFromMemory(z, address);
                   address = 0xFFFF & (address + 1);
                   temp = 0xFFFF & ((readFromMemory(z, address) << 8) | (temp & 0xFF));
                   break;
        default: break;
    }

    /* store value */
    switch (destination) {
        case BC: z->regB = 0xFF & (temp >> 8);
                 z->regC = 0xFF & temp; break;
        case DE: z->regD = 0xFF & (temp >> 8);
                 z->regE = 0xFF & temp; break;
        case HL: z->regH = 0xFF & (temp >> 8);
                 z->regL = 0xFF & temp; break;
        case IX: z->regIX = temp; break;
        case IY: z->regIY = temp; break;
        case SP: z->regSP = temp; break;
        case a_nn: low = readFromMemory(z, readProgramCounter(z));
                   incrementProgramCounter(z);
                   high = readFromMemory(z, readProgramCounter(z));
                   incrementProgramCounter(z);
                   address = 0xFFFF & ((high << 8) | (low & 0xFF));
                   writeToMemory(z, address, temp & 0xFF);
                   address = 0xFFFF & (address + 1);
                   writeToMemory(z, address, 0xFF & (temp >> 8));
                   break;
        default: break;
    }

    /* set cycle count accordingly */
    switch (destination) {
        case a_nn: switch (source) {
                       case HL: if (longer == 0)
                                    z->cycles = 16;
                                else
                                    z->cycles = 20;
                                break;
                       case BC:
                       case DE:
                       case IX:
                       case IY:
                       case SP: z->cycles = 20; break;
                       default: break;
                   } break;
        case BC: switch (source) {
                     case a_nn: z->cycles = 20; break;
                     case nn: z->cycles = 10; break;
                     default: break;
                 } break;
        case DE: switch (source) {
                     case a_nn: z->cycles = 20; break;
                     case nn: z->cycles = 10; break;
                     default: break;
                 } break;
        case HL: switch (source) {
                     case a_nn: if (longer == 0)
                                    z->cycles = 16;
                                else
                                    z->cycles = 20;
                                break;
                     case nn: z->cycles = 10; break;
                     default: break;
                 } break;
        case IX: switch (source) {
                     case a_nn: z->cycles = 20; break;
                     case nn: z->cycles = 14; break;
                     default: break;
                 } break;
        case IY: switch (source) {
                     case a_nn: z->cycles = 20; break;
                     case nn: z->cycles = 14; break;
                     default: break;
                 } break;
        case SP: switch (source) {
                     case a_nn: z->cycles = 20; break;
                     case HL: z->cycles = 6; break;
                     case IX:
                     case IY:
                     case nn: z->cycles = 10; break;
                     default: break;
                 } break;
        default: break;
    }
}

/* this function emulates all of the OR instructions */
static void OR(Z80 z, cpuVal value)
{
    /* define variables */
    emubyte temp = 0;
    signed_emubyte d = 0;

    /* get correct value and set cycle count */
    switch (value) {
        case A: temp = z->regA; z->cycles = 4; break;
        case B: temp = z->regB; z->cycles = 4; break;
        case C: temp = z->regC; z->cycles = 4; break;
        case D: temp = z->regD; z->cycles = 4; break;
        case E: temp = z->regE; z->cycles = 4; break;
        case H: temp = z->regH; z->cycles = 4; break;
        case L: temp = z->regL; z->cycles = 4; break;
        case IXh: temp = 0xFF & (z->regIX >> 8); z->cycles = 4; break;
        case IXl: temp = 0xFF & z->regIX; z->cycles = 4; break;
        case IYh: temp = 0xFF & (z->regIY >> 8); z->cycles = 4; break;
        case IYl: temp = 0xFF & z->regIY; z->cycles = 4; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 7; break;
        case n: temp = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                z->cycles = 7; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 19; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 19; break;
        default: break;
    }

    /* perform OR between accumulator and value */
    z->regA |= temp;

    /* deal with flags */
    calculateSFlag(z, z->regA);
    calculateZFlag(z, z->regA);
    setHFlag(z, false);
    calculatePVFlag(z, z->regA);
    setNFlag(z, false);
    setCFlag(z, false);
}

/* this function emulates all of the SUB instructions */
static void SUB(Z80 z, cpuVal value)
{
    /* define variables, including one to hold result (must be larger than
       one byte, to allow for checking of borrow from bit 8) */
    emubyte temp = 0;
    emuint result = 0;
    signed_emubyte d = 0;
    
    /* retrieve correct value and set cycle count */
    switch (value) {
        case A: temp = z->regA; z->cycles = 4; break;
        case B: temp = z->regB; z->cycles = 4; break;
        case C: temp = z->regC; z->cycles = 4; break;
        case D: temp = z->regD; z->cycles = 4; break;
        case E: temp = z->regE; z->cycles = 4; break;
        case H: temp = z->regH; z->cycles = 4; break;
        case L: temp = z->regL; z->cycles = 4; break;
        case IXh: temp = 0xFF & (z->regIX >> 8); z->cycles = 4; break;
        case IXl: temp = 0xFF & z->regIX; z->cycles = 4; break;
        case IYh: temp = 0xFF & (z->regIY >> 8); z->cycles = 4; break;
        case IYl: temp = 0xFF & z->regIY; z->cycles = 4; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 7; break;
        case n: temp = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                z->cycles = 7; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 19; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 19; break;
        default: break;
    }
    
    /* do the calculation */
    result = z->regA - temp;
    
    /* deal with flags */
    calculateSFlag(z, result);
    calculateZFlag(z, result);
    calculateHFlag(z, z->regA, temp, result);
    calculatePVOverflow(z, temp, result, subtraction);
    setNFlag(z, true);
    calculateCFlag(z, temp, result);
    
    /* store result in accumulator */
    z->regA = 0xFF & result;
}

/* this function emulates all of the SBC instructions (including the 16-bit ones) */
static void SBC(Z80 z, cpuVal value)
{
    /* define variables, including one to hold result (must be larger than
       two bytes, to allow for checking of borrow from bit 16 for 16-bit operations) */
    emuint tempHL = 0;
    emuint temp = 0;
    emuint result = 0;
    signed_emubyte d = 0;
    
    /* retrieve correct value and set cycle count */
    switch (value) {
        case A: temp = z->regA; z->cycles = 4; break;
        case B: temp = z->regB; z->cycles = 4; break;
        case C: temp = z->regC; z->cycles = 4; break;
        case D: temp = z->regD; z->cycles = 4; break;
        case E: temp = z->regE; z->cycles = 4; break;
        case H: temp = z->regH; z->cycles = 4; break;
        case L: temp = z->regL; z->cycles = 4; break;
        case IXh: temp = 0xFF & (z->regIX >> 8); z->cycles = 4; break;
        case IXl: temp = 0xFF & z->regIX; z->cycles = 4; break;
        case IYh: temp = 0xFF & (z->regIY >> 8); z->cycles = 4; break;
        case IYl: temp = 0xFF & z->regIY; z->cycles = 4; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 7; break;
        case n: temp = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                z->cycles = 7; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 19; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 19; break;
        case BC: tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 temp = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
                 z->cycles = 15; break;
        case DE: tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 temp = 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF));
                 z->cycles = 15; break;
        case HL: tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 temp = tempHL; z->cycles = 15; break;
        case SP: tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 temp = readStackPointer(z); z->cycles = 15; break;
        default: break;
    }
    
    /* do the calculation (utilising the value of the carry flag) */
    switch (value) {
        case BC: case DE: case HL: case SP: result = tempHL - temp - (z->regF & 0x01); break;
        default: result = z->regA - temp - (z->regF & 0x01); break;
    }
    
    /* deal with flags */
    switch (value) {
        case BC: case DE: case HL: case SP: calculateSFlag16(z, result);
                                            calculateZFlag16(z, result);
                                            calculateHFlag16(z, tempHL, temp, result);
                                            calculatePVOverflow16(z, tempHL, temp, result, subtraction16);
                                            setNFlag(z, true);
                                            calculateCFlag16(z, tempHL, temp, result);
                                            break;
        default: calculateSFlag(z, result);
                 calculateZFlag(z, result);
                 calculateHFlag(z, z->regA, temp, result);
                 calculatePVOverflow(z, temp, result, subtraction);
                 setNFlag(z, true);
                 calculateCFlag(z, temp, result);
                 break;
    }
    
    /* store result accordingly */
    switch (value) {
        case BC: case DE: case HL: case SP: z->regH = 0xFF & (result >> 8);
                                            z->regL = 0xFF & result;
                                            break;
        default: z->regA = 0xFF & result; break;
    }
}

/* this function emulates all of the PUSH instructions */
static void PUSH(Z80 z, cpuVal value)
{
    /* decrement stack pointer */
    decrementStackPointer(z);
    
    /* write higher byte to stack */
    switch (value) {
        case BC: writeToMemory(z, readStackPointer(z), z->regB); break;
        case DE: writeToMemory(z, readStackPointer(z), z->regD); break;
        case HL: writeToMemory(z, readStackPointer(z), z->regH); break;
        case AF: writeToMemory(z, readStackPointer(z), z->regA); break;
        case IX: writeToMemory(z, readStackPointer(z), 0xFF & (z->regIX >> 8)); break;
        case IY: writeToMemory(z, readStackPointer(z), 0xFF & (z->regIY >> 8)); break;
        default: break;
    }
    
    /* decrement stack pointer again */
    decrementStackPointer(z);
    
    /* write lower byte to stack and set cycle count */
    switch (value) {
        case BC: writeToMemory(z, readStackPointer(z), z->regC); z->cycles = 11; break;
        case DE: writeToMemory(z, readStackPointer(z), z->regE); z->cycles = 11; break;
        case HL: writeToMemory(z, readStackPointer(z), z->regL); z->cycles = 11; break;
        case AF: writeToMemory(z, readStackPointer(z), z->regF); z->cycles = 11; break;
        case IX: writeToMemory(z, readStackPointer(z), 0xFF & z->regIX); z->cycles = 15; break;
        case IY: writeToMemory(z, readStackPointer(z), 0xFF & z->regIY); z->cycles = 15; break;
        default: break;
    }
}

/* this function emulates all of the POP instructions */
static void POP(Z80 z, cpuVal value)
{
    /* read lower byte from stack */
    switch (value) {
        case BC: z->regC = readFromMemory(z, readStackPointer(z)); break;
        case DE: z->regE = readFromMemory(z, readStackPointer(z)); break;
        case HL: z->regL = readFromMemory(z, readStackPointer(z)); break;
        case AF: z->regF = readFromMemory(z, readStackPointer(z)); break;
        case IX: z->regIX = readFromMemory(z, readStackPointer(z)); break;
        case IY: z->regIY = readFromMemory(z, readStackPointer(z)); break;
        default: break;
    }
    
    /* increment stack pointer */
    incrementStackPointer(z);
    
    /* read higher byte from stack and set cycle count */
    switch (value) {
        case BC: z->regB = readFromMemory(z, readStackPointer(z)); z->cycles = 10; break;
        case DE: z->regD = readFromMemory(z, readStackPointer(z)); z->cycles = 10; break;
        case HL: z->regH = readFromMemory(z, readStackPointer(z)); z->cycles = 10; break;
        case AF: z->regA = readFromMemory(z, readStackPointer(z)); z->cycles = 10; break;
        case IX: z->regIX = 0xFFFF & ((readFromMemory(z, readStackPointer(z)) << 8) | (z->regIX & 0xFF));
                 z->cycles = 14; break;
        case IY: z->regIY = 0xFFFF & ((readFromMemory(z, readStackPointer(z)) << 8) | (z->regIY & 0xFF));
                 z->cycles = 14; break;
        default: break;
    }
    
    /* increment stack pointer again */
    incrementStackPointer(z);
}

/* this function emulates all of the conditional RET instructions */
static void RET_CONDITIONAL(Z80 z, cpuVal value)
{
    /* define variables to hold correct bitmask and expected result */
    emubyte bitmask = 0, expected_result = 0;
    
    /* check value and set variables accordingly */
    switch (value) {
        case NZ: bitmask = 0x40; expected_result = 0; break; /* test for no zero flag */
        case Z: bitmask = 0x40; expected_result = 64; break; /* test for zero flag */
        case NC: bitmask = 0x01; expected_result = 0; break; /* test for no carry flag */
        case C: bitmask = 0x01; expected_result = 1; break; /* test for carry flag */
        case PO: bitmask = 0x04; expected_result = 0; break; /* test for odd parity flag */
        case PE: bitmask = 0x04; expected_result = 4; break; /* test for even parity flag */
        case P: bitmask = 0x80; expected_result = 0; break; /* test for no negative flag */
        case M: bitmask = 0x80; expected_result = 128; break; /* test for negative flag */
        default: break;
    }
    
    /* run test and act accordingly */
    if ((z->regF & bitmask) == expected_result) {
        writeProgramCounter(z, readFromMemory(z, readStackPointer(z)));
        incrementStackPointer(z);
        writeProgramCounter(z, 0xFFFF & ((readFromMemory(z, readStackPointer(z)) << 8) | (0xFF & readProgramCounter(z))));
        incrementStackPointer(z);
        z->cycles = 11;
    } else {
        z->cycles = 5;
    }
}

/* this function emulates the RET instruction */
static void RET(Z80 z)
{
    /* read low then high byte from stack and store both in turn to the program counter */
    writeProgramCounter(z, readFromMemory(z, readStackPointer(z)));
    incrementStackPointer(z);
    writeProgramCounter(z, 0xFFFF & ((readFromMemory(z, readStackPointer(z)) << 8) | (0xFF & readProgramCounter(z))));
    incrementStackPointer(z);
    
    /* set cycle count */
    z->cycles = 10;
}

/* this function emulates all of the ADC instructions (including the 16-bit ones) */
static void ADC(Z80 z, cpuVal value)
{
    /* define variables, including one to hold result (must be larger than
       two bytes, to allow for checking of borrow from bit 16 (for 16-bit operations) */
    emuint tempHL = 0;
    emuint temp = 0;
    emuint result = 0;
    signed_emubyte d = 0;
    
    /* retrieve correct value and set cycle count */
    switch (value) {
        case A: temp = z->regA; z->cycles = 4; break;
        case B: temp = z->regB; z->cycles = 4; break;
        case C: temp = z->regC; z->cycles = 4; break;
        case D: temp = z->regD; z->cycles = 4; break;
        case E: temp = z->regE; z->cycles = 4; break;
        case H: temp = z->regH; z->cycles = 4; break;
        case L: temp = z->regL; z->cycles = 4; break;
        case IXh: temp = 0xFF & (z->regIX >> 8); z->cycles = 4; break;
        case IXl: temp = 0xFF & z->regIX; z->cycles = 4; break;
        case IYh: temp = 0xFF & (z->regIY >> 8); z->cycles = 4; break;
        case IYl: temp = 0xFF & z->regIY; z->cycles = 4; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 7; break;
        case n: temp = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                z->cycles = 7; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 19; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 19; break;
        case BC: tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 temp = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
                 z->cycles = 15; break;
        case DE: tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 temp = 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF));
                 z->cycles = 15; break;
        case HL: tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 temp = tempHL; z->cycles = 15; break;
        case SP: tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 temp = readStackPointer(z); z->cycles = 15; break;
        default: break;
    }
    
    /* do the calculation (utilising the value of the carry flag) */
    switch (value) {
        case BC: case DE: case HL: case SP: result = tempHL + temp + (z->regF & 0x01); break;
        default: result = z->regA + temp + (z->regF & 0x01); break;
    }
    
    /* deal with flags */
    switch (value) {
        case BC: case DE: case HL: case SP: calculateSFlag16(z, result);
                                            calculateZFlag16(z, result);
                                            calculateHFlag16(z, tempHL, temp, result);
                                            calculatePVOverflow16(z, tempHL, temp, result, addition16);
                                            setNFlag(z, false);
                                            calculateCFlag16(z, tempHL, temp, result);
                                            break;
        default: calculateSFlag(z, result);
                 calculateZFlag(z, result);
                 calculateHFlag(z, z->regA, temp, result);
                 calculatePVOverflow(z, temp, result, addition);
                 setNFlag(z, false);
                 calculateCFlag(z, temp, result);
                 break;
    }
    
    /* store result accordingly */
    switch (value) {
        case BC: case DE: case HL: case SP: z->regH = 0xFF & (result >> 8);
                                            z->regL = 0xFF & result;
                                            break;
        default: z->regA = 0xFF & result; break;
    }
}

/* this function emulates all of the ADD instructions (including the 16-bit ones) */
static void ADD(Z80 z, cpuVal valueone, cpuVal valuetwo)
{
    /* define variables, including one to hold result (variables must be larger than
       one byte, to allow for 16-bit addition) */
    emuint temp = 0;
    emuint temp16source = 0;
    emuint result = 0;
    signed_emubyte d = 0;
    
    /* retrieve correct value and set cycle count */
    switch (valueone) {
        case A: switch (valuetwo) {
                    case A: temp = z->regA; z->cycles = 4; break;
                    case B: temp = z->regB; z->cycles = 4; break;
                    case C: temp = z->regC; z->cycles = 4; break;
                    case D: temp = z->regD; z->cycles = 4; break;
                    case E: temp = z->regE; z->cycles = 4; break;
                    case H: temp = z->regH; z->cycles = 4; break;
                    case L: temp = z->regL; z->cycles = 4; break;
                    case IXh: temp = 0xFF & (z->regIX >> 8); z->cycles = 4; break;
                    case IXl: temp = 0xFF & z->regIX; z->cycles = 4; break;
                    case IYh: temp = 0xFF & (z->regIY >> 8); z->cycles = 4; break;
                    case IYl: temp = 0xFF & z->regIY; z->cycles = 4; break;
                    case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                              z->cycles = 7; break;
                    case n: temp = readFromMemory(z, readProgramCounter(z));
                            incrementProgramCounter(z);
                            z->cycles = 7; break;
                    case IXd: d = readFromMemory(z, readProgramCounter(z));
                              incrementProgramCounter(z);
                              temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                              z->cycles = 19; break;
                    case IYd: d = readFromMemory(z, readProgramCounter(z));
                              incrementProgramCounter(z);
                              temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                              z->cycles = 19; break;
                    default: break;
                } break;
        case HL: temp16source = 0xFFFF & ((z->regH << 8) | (0xFF & z->regL));
                 switch (valuetwo) {
                     case BC: temp = 0xFFFF & ((z->regB << 8) | (0xFF & z->regC));
                              z->cycles = 11; break;
                     case DE: temp = 0xFFFF & ((z->regD << 8) | (0xFF & z->regE));
                              z->cycles = 11; break;
                     case HL: temp = 0xFFFF & ((z->regH << 8) | (0xFF & z->regL));
                              z->cycles = 11; break;
                     case SP: temp = readStackPointer(z);
                              z->cycles = 11; break;
                     default: break;
                 } break;
        case IX: temp16source = z->regIX;
                 switch (valuetwo) {
                     case BC: temp = 0xFFFF & ((z->regB << 8) | (0xFF & z->regC));
                              z->cycles = 15; break;
                     case DE: temp = 0xFFFF & ((z->regD << 8) | (0xFF & z->regE));
                              z->cycles = 15; break;
                     case IX: temp = z->regIX;
                              z->cycles = 15; break;
                     case SP: temp = readStackPointer(z);
                              z->cycles = 15; break;
                     default: break;
                 } break;
        case IY: temp16source = z->regIY;
                 switch (valuetwo) {
                     case BC: temp = 0xFFFF & ((z->regB << 8) | (0xFF & z->regC));
                              z->cycles = 15; break;
                     case DE: temp = 0xFFFF & ((z->regD << 8) | (0xFF & z->regE));
                              z->cycles = 15; break;
                     case IY: temp = z->regIY;
                              z->cycles = 15; break;
                     case SP: temp = readStackPointer(z);
                              z->cycles = 15; break;
                     default: break;
                 } break;
        default: break;
    }
    
    /* do the calculation */
    switch (valueone) {
        case HL: case IX: case IY: result = temp16source + temp; break;
        default: result = z->regA + temp; break;
    }
    
    /* deal with flags */
    switch (valueone) {
        case HL: case IX: case IY: calculateHFlag16(z, temp16source, temp, result);
                                   setNFlag(z, false);
                                   calculateCFlag16(z, temp16source, temp, result);
                                   break;
        default: calculateSFlag(z, result);
                 calculateZFlag(z, result);
                 calculateHFlag(z, z->regA, temp, result);
                 calculatePVOverflow(z, temp, result, addition);
                 setNFlag(z, false);
                 calculateCFlag(z, temp, result);
                 break;
    }

    /* store result in relevant location */
    switch (valueone) {
        case HL: z->regH = 0xFF & (result >> 8);
                 z->regL = 0xFF & result;
                 break;
        case IX: z->regIX = 0xFFFF & result;
                 break;
        case IY: z->regIY = 0xFFFF & result;
                 break;
        default: z->regA = 0xFF & result;
                 break;
    }
}

/* this function emulates all of the AND instructions */
static void AND(Z80 z, cpuVal value)
{
    /* define variables */
    emubyte temp = 0;
    emubyte result = 0;
    signed_emubyte d = 0;
    
    /* retrieve correct value */
    switch (value) {
        case A: temp = z->regA; z->cycles = 4; break;
        case B: temp = z->regB; z->cycles = 4; break;
        case C: temp = z->regC; z->cycles = 4; break;
        case D: temp = z->regD; z->cycles = 4; break;
        case E: temp = z->regE; z->cycles = 4; break;
        case H: temp = z->regH; z->cycles = 4; break;
        case L: temp = z->regL; z->cycles = 4; break;
        case IXh: temp = 0xFF & (z->regIX >> 8); z->cycles = 4; break;
        case IXl: temp = 0xFF & z->regIX; z->cycles = 4; break;
        case IYh: temp = 0xFF & (z->regIY >> 8); z->cycles = 4; break;
        case IYl: temp = 0xFF & z->regIY; z->cycles = 4; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 7; break;
        case n: temp = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                z->cycles = 7; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 19; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 19; break;
        default: break;
    }
    
    /* perform bitwise AND calculation */
    result = z->regA & temp;
    
    /* deal with flags */
    calculateSFlag(z, result);
    calculateZFlag(z, result);
    setHFlag(z, true);
    calculatePVFlag(z, result);
    setNFlag(z, false);
    setCFlag(z, false);
    
    /* store result back to accumulator */
    z->regA = result;
}

/* this function emulates all of the conditional CALL instructions */
static void CALL_CONDITIONAL(Z80 z, cpuVal value)
{
    /* define variables to hold correct bitmask and expected result,
       as well as the new address */
    emubyte bitmask = 0, expected_result = 0;
    emuint newAddress = 0;
    
    /* check value and set variables accordingly */
    switch (value) {
        case NZ: bitmask = 0x40; expected_result = 0; break; /* test for no zero flag */
        case Z: bitmask = 0x40; expected_result = 64; break; /* test for zero flag */
        case NC: bitmask = 0x01; expected_result = 0; break; /* test for no carry flag */
        case C: bitmask = 0x01; expected_result = 1; break; /* test for carry flag */
        case PO: bitmask = 0x04; expected_result = 0; break; /* test for odd parity flag */
        case PE: bitmask = 0x04; expected_result = 4; break; /* test for even parity flag */
        case P: bitmask = 0x80; expected_result = 0; break; /* test for no negative flag */
        case M: bitmask = 0x80; expected_result = 128; break; /* test for negative flag */
        default: break;
    }
    
    /* get new address from memory */
    newAddress = readFromMemory(z, readProgramCounter(z));
    incrementProgramCounter(z);
    newAddress = 0xFFFF & ((readFromMemory(z, readProgramCounter(z)) << 8) | (0xFF & newAddress));
    incrementProgramCounter(z);
    
    /* run test and act accordingly */
    if ((z->regF & bitmask) == expected_result) {
        decrementStackPointer(z);
        writeToMemory(z, readStackPointer(z), 0xFF & (readProgramCounter(z) >> 8));
        decrementStackPointer(z);
        writeToMemory(z, readStackPointer(z), 0xFF & readProgramCounter(z));
        writeProgramCounter(z, newAddress);
        z->cycles = 17;
    } else {
        z->cycles = 10;
    }
}

/* this emulates the CALL nn instruction */
static void CALL(Z80 z)
{
    /* define variables to hold correct bitmask and expected result,
       as well as the new address */
    emuint newAddress = 0;
    
    /* get new address from memory */
    newAddress = readFromMemory(z, readProgramCounter(z));
    incrementProgramCounter(z);
    newAddress = 0xFFFF & ((readFromMemory(z, readProgramCounter(z)) << 8) | (0xFF & newAddress));
    incrementProgramCounter(z);
    
    /* push old program counter value to stack */
    decrementStackPointer(z);
    writeToMemory(z, readStackPointer(z), 0xFF & (readProgramCounter(z) >> 8));
    decrementStackPointer(z);
    writeToMemory(z, readStackPointer(z), 0xFF & readProgramCounter(z));

    /* store new address in program counter */
    writeProgramCounter(z, newAddress);
    
    /* set cycle count */
    z->cycles = 17;
}

/* this emulates the CCF instruction */
static void CCF(Z80 z)
{
    /* define variables */
    emubyte tempH = 0, tempC = 0;
    
    /* copy previous carry flag to the H flag */
    tempH = 0x1F & (z->regF << 4);
    z->regF = (0xEF & z->regF) | tempH;
    
    /* reset the N flag */
    setNFlag(z, false);
    
    /* invert the C flag */
    tempC = 0x01 & z->regF;
    tempC = ~tempC & 0x01;
    z->regF = (0xFE & z->regF) | tempC;
    
    /* set cycle count */
    z->cycles = 4;
}

/* this function emulates all of the CP instructions */
static void CP(Z80 z, cpuVal value)
{
    /* define variables (the result must be larger than a byte to check for
       various flag conditions */
    emubyte temp = 0;
    emuint result = 0;
    signed_emubyte d = 0;

    /* retrieve correct value, and set cycle count */
    switch (value) {
        case A: temp = z->regA; z->cycles = 4; break;
        case B: temp = z->regB; z->cycles = 4; break;
        case C: temp = z->regC; z->cycles = 4; break;
        case D: temp = z->regD; z->cycles = 4; break;
        case E: temp = z->regE; z->cycles = 4; break;
        case H: temp = z->regH; z->cycles = 4; break;
        case L: temp = z->regL; z->cycles = 4; break;
        case IXh: temp = 0xFF & (z->regIX >> 8); z->cycles = 4; break;
        case IXl: temp = 0xFF & z->regIX; z->cycles = 4; break;
        case IYh: temp = 0xFF & (z->regIY >> 8); z->cycles = 4; break;
        case IYl: temp = 0xFF & z->regIY; z->cycles = 4; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 7; break;
        case n: temp = readFromMemory(z, readProgramCounter(z));
                incrementProgramCounter(z);
                z->cycles = 7; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 19; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 19; break;
        default: break;
    }

    /* perform calculation */
    result = z->regA - temp;

    /* deal with flags */
    calculateSFlag(z, result);
    calculateZFlag(z, result);
    calculateHFlag(z, z->regA, temp, result);
    calculatePVOverflow(z, temp, result, subtraction);
    setNFlag(z, true);
    calculateCFlag(z, temp, result);
}

/* this function emulates the CPD instruction */
static void CPD(Z80 z)
{
    /* define variables */
    emuint tempBC = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte calculation = 0xFF & (z->regA - readFromMemory(z, tempHL));
    
    /* calculate S, Z and H flags */
    calculateSFlag(z, calculation);
    calculateZFlag(z, calculation);
    calculateHFlag(z, z->regA, readFromMemory(z, tempHL), calculation);
    
    /* decrement BC then check if it isn't zero - set P/V flag if so, else reset it */
    tempBC = 0xFFFF & (tempBC - 1);
    if (!(tempBC == 0))
        setPVFlag(z, true);
    else
        setPVFlag(z, false);
    
    /* set N flag */
    setNFlag(z, true);
    
    /* decrement HL */
    tempHL = 0xFFFF & (tempHL - 1);
    
    /* store BC and HL back into consituent 8-bit registers */
    z->regB = 0xFF & (tempBC >> 8);
    z->regC = 0xFF & tempBC;
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* set cycle count */
    z->cycles = 16;
}

/* this function emulates the CPDR instruction */
static void CPDR(Z80 z)
{
    /* define variables */
    emuint tempBC = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte calculation = 0xFF & (z->regA - readFromMemory(z, tempHL));
    
    /* calculate S, Z and H flags */
    calculateSFlag(z, calculation);
    calculateZFlag(z, calculation);
    calculateHFlag(z, z->regA, readFromMemory(z, tempHL), calculation);
    
    /* decrement BC then check if it isn't zero - set P/V flag if so, else reset it */
    tempBC = 0xFFFF & (tempBC - 1);
    if (!(tempBC == 0))
        setPVFlag(z, true);
    else
        setPVFlag(z, false);
    
    /* set N flag */
    setNFlag(z, true);
    
    /* decrement HL */
    tempHL = 0xFFFF & (tempHL - 1);
    
    /* store BC and HL back into consituent 8-bit registers */
    z->regB = 0xFF & (tempBC >> 8);
    z->regC = 0xFF & tempBC;
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* set cycle count depending on outcome, and repeat instruction if necessary */
    if ((calculation == 0) || (tempBC == 0)) {
        z->cycles = 16;
    } else {
        z->cycles = 21;
        decrementProgramCounter(z);
        decrementProgramCounter(z);
    }
}

/* this function emulates the CPI instruction */
static void CPI(Z80 z)
{
    /* define variables */
    emuint tempBC = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte calculation = 0xFF & (z->regA - readFromMemory(z, tempHL));
    
    /* calculate S, Z and H flags */
    calculateSFlag(z, calculation);
    calculateZFlag(z, calculation);
    calculateHFlag(z, z->regA, readFromMemory(z, tempHL), calculation);
    
    /* decrement BC then check if it isn't zero - set P/V flag if so, else reset it */
    tempBC = 0xFFFF & (tempBC - 1);
    if (!(tempBC == 0))
        setPVFlag(z, true);
    else
        setPVFlag(z, false);
    
    /* set N flag */
    setNFlag(z, true);
    
    /* increment HL */
    tempHL = 0xFFFF & (tempHL + 1);
    
    /* store BC and HL back into consituent 8-bit registers */
    z->regB = 0xFF & (tempBC >> 8);
    z->regC = 0xFF & tempBC;
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* set cycle count */
    z->cycles = 16;
}

/* this function emulates the CPIR instruction */
static void CPIR(Z80 z)
{
    /* define variables */
    emuint tempBC = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte calculation = 0xFF & (z->regA - readFromMemory(z, tempHL));
    
    /* calculate S, Z and H flags */
    calculateSFlag(z, calculation);
    calculateZFlag(z, calculation);
    calculateHFlag(z, z->regA, readFromMemory(z, tempHL), calculation);
    
    /* decrement BC then check if it isn't zero - set P/V flag if so, else reset it */
    tempBC = 0xFFFF & (tempBC - 1);
    if (!(tempBC == 0))
        setPVFlag(z, true);
    else
        setPVFlag(z, false);
    
    /* set N flag */
    setNFlag(z, true);
    
    /* increment HL */
    tempHL = 0xFFFF & (tempHL + 1);
    
    /* store BC and HL back into consituent 8-bit registers */
    z->regB = 0xFF & (tempBC >> 8);
    z->regC = 0xFF & tempBC;
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* set cycle count depending on outcome, and repeat instruction if necessary */
    if ((calculation == 0) || (tempBC == 0)) {
        z->cycles = 16;
    } else {
        z->cycles = 21;
        decrementProgramCounter(z);
        decrementProgramCounter(z);
    }
}

/* this function emulates the CPL instruction */
static void CPL(Z80 z)
{
    /* invert value in accumulator */
    z->regA = 0xFF & ~z->regA;
    
    /* set H and N flags */
    setHFlag(z, true);
    setNFlag(z, true);
    
    /* set cycle count */
    z->cycles = 4;
}

/* this function emulates the NEG instruction */
static void NEG(Z80 z)
{
    /* subtract accumulator from zero */
    emubyte result = 0xFF & (0 - z->regA);
    
    /* calculate S and Z flags */
    calculateSFlag(z, result);
    calculateZFlag(z, result);
    
    /* check for borrow from bit 4 and set H flag if so, else reset it */
    if ((((0 ^ z->regA) ^ result) & 0x10) == 16)
        setHFlag(z, true);
    else
        setHFlag(z, false);
    
    /* check if accumulator was 0x80 before calculation and set P/V flag
       if so, else reset it */
    if ((z->regA & 0xFF) == 0x80)
        setPVFlag(z, true);
    else
        setPVFlag(z, false);
    
    /* set N flag */
    setNFlag(z, true);
    
    /* set C flag if accumulator was not equal to 0 before instruction,
       reset otherwise */
    if ((z->regA & 0xFF) != 0)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    
    /* store result back into accumulator */
    z->regA = result;
    
    /* set cycle count */
    z->cycles = 8;
}

/* this function emulates the LDD instruction */
static void LDD(Z80 z)
{
    /* define temporary variables to store 16-bit register values */
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emuint tempDE = 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF));
    emuint tempBC = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    
    /* write from memory location (tempHL) to memory location (tempDE) */
    writeToMemory(z, tempDE, readFromMemory(z, tempHL));
    
    /* decrement all three register pairs */
    tempHL = 0xFFFF & (tempHL - 1);
    tempDE = 0xFFFF & (tempDE - 1);
    tempBC = 0xFFFF & (tempBC - 1);
    
    /* store register values back to their constituent registers */
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    z->regD = 0xFF & (tempDE >> 8);
    z->regE = 0xFF & tempDE;
    z->regB = 0xFF & (tempBC >> 8);
    z->regC = 0xFF & tempBC;
    
    /* reset H and N flags */
    setHFlag(z, false);
    setNFlag(z, false);
    
    /* reset P/V flag if BC is 0, else set it */
    if (tempBC == 0)
        setPVFlag(z, false);
    else
        setPVFlag(z, true);
    
    /* set cycle count */
    z->cycles = 16;
}

/* this function emulates the LDI instruction */
static void LDI(Z80 z)
{
    /* define temporary variables to store 16-bit register values */
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emuint tempDE = 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF));
    emuint tempBC = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    
    /* write from memory location (tempHL) to memory location (tempDE) */
    writeToMemory(z, tempDE, readFromMemory(z, tempHL));
    
    /* increment HL and DE, and decrement BC */
    tempHL = 0xFFFF & (tempHL + 1);
    tempDE = 0xFFFF & (tempDE + 1);
    tempBC = 0xFFFF & (tempBC - 1);
    
    /* store register values back to their constituent registers */
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    z->regD = 0xFF & (tempDE >> 8);
    z->regE = 0xFF & tempDE;
    z->regB = 0xFF & (tempBC >> 8);
    z->regC = 0xFF & tempBC;
    
    /* reset H and N flags */
    setHFlag(z, false);
    setNFlag(z, false);
    
    /* reset P/V flag if BC is 0, else set it */
    if (tempBC == 0)
        setPVFlag(z, false);
    else
        setPVFlag(z, true);
    
    /* set cycle count */
    z->cycles = 16;

}

/* this function emulates the LDDR instruction */
static void LDDR(Z80 z)
{
    /* define temporary variables to store 16-bit register values */
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emuint tempDE = 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF));
    emuint tempBC = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    
    /* write from memory location (tempHL) to memory location (tempDE) */
    writeToMemory(z, tempDE, readFromMemory(z, tempHL));
    
    /* decrement all three register pairs */
    tempHL = 0xFFFF & (tempHL - 1);
    tempDE = 0xFFFF & (tempDE - 1);
    tempBC = 0xFFFF & (tempBC - 1);
    
    /* store register values back to their constituent registers */
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    z->regD = 0xFF & (tempDE >> 8);
    z->regE = 0xFF & tempDE;
    z->regB = 0xFF & (tempBC >> 8);
    z->regC = 0xFF & tempBC;
    
    /* reset H, N and P/V flags */
    setHFlag(z, false);
    setNFlag(z, false);
    setPVFlag(z, false);
    
    /* check if BC is now zero and set cycle count accordingly,
       repeating instruction if necessary */
    if (tempBC == 0) {
        z->cycles = 16;
    } else {
        decrementProgramCounter(z);
        decrementProgramCounter(z);
        z->cycles = 21;
    }
}

/* this function emulates the LDIR instruction */
static void LDIR(Z80 z)
{
    /* define temporary variables to store 16-bit register values */
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emuint tempDE = 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF));
    emuint tempBC = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    
    /* write from memory location (tempHL) to memory location (tempDE) */
    writeToMemory(z, tempDE, readFromMemory(z, tempHL));
    
    /* increment HL and DE, and decrement BC */
    tempHL = 0xFFFF & (tempHL + 1);
    tempDE = 0xFFFF & (tempDE + 1);
    tempBC = 0xFFFF & (tempBC - 1);
    
    /* store register values back to their constituent registers */
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    z->regD = 0xFF & (tempDE >> 8);
    z->regE = 0xFF & tempDE;
    z->regB = 0xFF & (tempBC >> 8);
    z->regC = 0xFF & tempBC;
    
    /* reset H, N and P/V flags */
    setHFlag(z, false);
    setNFlag(z, false);
    setPVFlag(z, false);
    
    /* check if BC is now zero and set cycle count accordingly,
       repeating instruction if necessary */
    if (tempBC == 0) {
        z->cycles = 16;
    } else {
        decrementProgramCounter(z);
        decrementProgramCounter(z);
        z->cycles = 21;
    }
}

/* this function emulates the DJNZ,e instruction */
static void DJNZ(Z80 z)
{
    /* define variables */
    signed_emubyte adjustment = 0;
    emuint tempPC = 0;
    
    /* decrement reg B */
    z->regB = 0xFF & (z->regB - 1);
    
    /* read in adjustment value */
    adjustment = readFromMemory(z, readProgramCounter(z));
    incrementProgramCounter(z);
    
    /* create temporary program counter value */
    tempPC = 0xFFFF & (readProgramCounter(z) + adjustment);
    
    /* check reg B value and act accordingly */
    if ((z->regB & 0xFF) != 0) {
        writeProgramCounter(z, tempPC);
        z->cycles = 13;
    } else {
        z->cycles = 8;
    }
}

/* this function emulates all of the EX instructions */
static void EX(Z80 z, cpuVal valueone, cpuVal valuetwo)
{
    /* define variables */
    emubyte high = 0, low = 0;
    
    /* perform the relevant swap */
    switch (valueone) {
        case AF: high = z->regA;
                 low = z->regF;
                 z->regA = z->regAShadow;
                 z->regF = z->regFShadow;
                 z->regAShadow = high;
                 z->regFShadow = low;
                 z->cycles = 4; break;
        case DE: high = z->regD;
                 low = z->regE;
                 z->regD = z->regH;
                 z->regE = z->regL;
                 z->regH = high;
                 z->regL = low;
                 z->cycles = 4; break;
        case aSP: low = readFromMemory(z, readStackPointer(z));
                  high = readFromMemory(z, readStackPointer(z) + 1);
                  switch (valuetwo) {
                      case HL: writeToMemory(z, readStackPointer(z), z->regL);
                               writeToMemory(z, readStackPointer(z) + 1, z->regH);
                               z->regL = low;
                               z->regH = high;
                               z->cycles = 19; break;
                      case IX: writeToMemory(z, readStackPointer(z), z->regIX & 0xFF);
                               writeToMemory(z, readStackPointer(z) + 1, (z->regIX >> 8) & 0xFF);
                               z->regIX = 0xFFFF & ((high << 8) | (low & 0xFF));
                               z->cycles = 23; break;
                      case IY: writeToMemory(z, readStackPointer(z), z->regIY & 0xFF);
                               writeToMemory(z, readStackPointer(z) + 1, (z->regIY >> 8) & 0xFF);
                               z->regIY = 0xFFFF & ((high << 8) | (low & 0xFF));
                               z->cycles = 23; break;
                      default: break;
                  } break;
        default: break;
    }
}

/* this function emulates all of the DEC instructions */
static void DEC(Z80 z, cpuVal value)
{
    /* define variables */
    signed_emubyte d = 0;
    emuint temp = 0;
    emuint result = 0;
    
    /* retrieve correct value and set cycle count */
    switch (value) {
        case A: temp = z->regA;
                z->cycles = 4; break;
        case B: temp = z->regB;
                z->cycles = 4; break;
        case C: temp = z->regC;
                z->cycles = 4; break;
        case D: temp = z->regD;
                z->cycles = 4; break;
        case E: temp = z->regE;
                z->cycles = 4; break;
        case H: temp = z->regH;
                z->cycles = 4; break;
        case L: temp = z->regL;
                z->cycles = 4; break;
        case IXh: temp = 0xFF & (z->regIX >> 8);
                  z->cycles = 4; break;
        case IXl: temp = 0xFF & z->regIX;
                  z->cycles = 4; break;
        case IYh: temp = 0xFF & (z->regIY >> 8);
                  z->cycles = 4; break;
        case IYl: temp = 0xFF & z->regIY;
                  z->cycles = 4; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 11; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        case BC: temp = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
                 z->cycles = 6; break;
        case DE: temp = 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF));
                 z->cycles = 6; break;
        case HL: temp = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 z->cycles = 6; break;
        case SP: temp = readStackPointer(z);
                 z->cycles = 6; break;
        case IX: temp = z->regIX;
                 z->cycles = 10; break;
        case IY: temp = z->regIY;
                 z->cycles = 10; break;
        default: break;
    }
    
    /* perform calculation */
    switch (value) {
        case BC: case DE: case HL:
        case SP: case IX: case IY: result = 0xFFFF & (temp - 1); break;
        default: result = 0xFF & (temp - 1);
    }
    
    /* deal with flags */
    switch (value) {
        case BC: case DE: case HL:
        case SP: case IX: case IY: break;
        default: calculateSFlag(z, result);
                 calculateZFlag(z, result);
                 calculateHFlag(z, temp, 0x01, result);
                 if ((temp & 0xFF) == 0x80)
                     setPVFlag(z, true);
                 else
                     setPVFlag(z, false);
                 setNFlag(z, true);
                 break;
    }
    
    /* store result back to correct location */
    switch (value) {
        case A: z->regA = result; break;
        case B: z->regB = result; break;
        case C: z->regC = result; break;
        case D: z->regD = result; break;
        case E: z->regE = result; break;
        case H: z->regH = result; break;
        case L: z->regL = result; break;
        case IXh: z->regIX = 0xFFFF & ((result << 8) | (z->regIX & 0xFF)); break;
        case IXl: z->regIX = 0xFFFF & ((z->regIX & 0xFF00) | (result)); break;
        case IYh: z->regIY = 0xFFFF & ((result << 8) | (z->regIY & 0xFF)); break;
        case IYl: z->regIY = 0xFFFF & ((z->regIY & 0xFF00) | (result)); break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), result); break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), result); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), result); break;
        case BC: z->regB = 0xFF & (result >> 8);
                 z->regC = 0xFF & result; break;
        case DE: z->regD = 0xFF & (result >> 8);
                 z->regE = 0xFF & result; break;
        case HL: z->regH = 0xFF & (result >> 8);
                 z->regL = 0xFF & result; break;
        case SP: writeStackPointer(z, result); break;
        case IX: z->regIX = result; break;
        case IY: z->regIY = result; break;
        default: break;
    }
}

/* this function emulates all of the INC instructions */
static void INC(Z80 z, cpuVal value)
{
    /* define variables */
    signed_emubyte d = 0;
    emuint temp = 0;
    emuint result = 0;
    
    /* retrieve correct value and set cycle count */
    switch (value) {
        case A: temp = z->regA;
                z->cycles = 4; break;
        case B: temp = z->regB;
                z->cycles = 4; break;
        case C: temp = z->regC;
                z->cycles = 4; break;
        case D: temp = z->regD;
                z->cycles = 4; break;
        case E: temp = z->regE;
                z->cycles = 4; break;
        case H: temp = z->regH;
                z->cycles = 4; break;
        case L: temp = z->regL;
                z->cycles = 4; break;
        case IXh: temp = 0xFF & (z->regIX >> 8);
                  z->cycles = 4; break;
        case IXl: temp = 0xFF & z->regIX;
                  z->cycles = 4; break;
        case IYh: temp = 0xFF & (z->regIY >> 8);
                  z->cycles = 4; break;
        case IYl: temp = 0xFF & z->regIY;
                  z->cycles = 4; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 11; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        case BC: temp = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
                 z->cycles = 6; break;
        case DE: temp = 0xFFFF & ((z->regD << 8) | (z->regE & 0xFF));
                 z->cycles = 6; break;
        case HL: temp = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 z->cycles = 6; break;
        case SP: temp = readStackPointer(z);
                 z->cycles = 6; break;
        case IX: temp = z->regIX;
                 z->cycles = 10; break;
        case IY: temp = z->regIY;
                 z->cycles = 10; break;
        default: break;
    }
    
    /* perform calculation */
    switch (value) {
        case BC: case DE: case HL:
        case SP: case IX: case IY: result = 0xFFFF & (temp + 1); break;
        default: result = 0xFF & (temp + 1);
    }
    
    /* deal with flags */
    switch (value) {
        case BC: case DE: case HL:
        case SP: case IX: case IY: break;
        default: calculateSFlag(z, result);
                 calculateZFlag(z, result);
                 calculateHFlag(z, temp, 0x01, result);
                 if ((temp & 0xFF) == 0x7F)
                     setPVFlag(z, true);
                 else
                     setPVFlag(z, false);
                 setNFlag(z, false);
                 break;
    }
    
    /* store result back to correct location */
    switch (value) {
        case A: z->regA = result; break;
        case B: z->regB = result; break;
        case C: z->regC = result; break;
        case D: z->regD = result; break;
        case E: z->regE = result; break;
        case H: z->regH = result; break;
        case L: z->regL = result; break;
        case IXh: z->regIX = 0xFFFF & ((result << 8) | (z->regIX & 0xFF)); break;
        case IXl: z->regIX = 0xFFFF & ((z->regIX & 0xFF00) | (result)); break;
        case IYh: z->regIY = 0xFFFF & ((result << 8) | (z->regIY & 0xFF)); break;
        case IYl: z->regIY = 0xFFFF & ((z->regIY & 0xFF00) | (result)); break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), result); break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), result); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), result); break;
        case BC: z->regB = 0xFF & (result >> 8);
                 z->regC = 0xFF & result; break;
        case DE: z->regD = 0xFF & (result >> 8);
                 z->regE = 0xFF & result; break;
        case HL: z->regH = 0xFF & (result >> 8);
                 z->regL = 0xFF & result; break;
        case SP: writeStackPointer(z, result); break;
        case IX: z->regIX = result; break;
        case IY: z->regIY = result; break;
        default: break;
    }
}

/* this function emulates all of the IN instructions */
static void IN(Z80 z, cpuVal destination, cpuVal source)
{
    /* define variables */
    emuint address = 0;
    emubyte temp = 0;
    
    /* formulate address and obtain byte from it */
    switch (source) {
        case aC: address = z->regC;
                 address = 0xFFFF & ((z->regB << 8) | (address & 0xFF));
                 temp = readFromIO(z, address); break;
        case a_n: address = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  address = 0xFFFF & ((z->regA << 8) | (address & 0xFF));
                  temp = readFromIO(z, address);
                  break;
        default: break;
    }
    
    /* deal with flags and set cycle count */
    switch (source) {
        case a_n: z->cycles = 11; break;
        default: z->cycles = 12;
                 calculateSFlag(z, temp);
                 calculateZFlag(z, temp);
                 setHFlag(z, false);
                 calculatePVFlag(z, temp);
                 setNFlag(z, false);
                 break;
    }
    
    /* store byte in correct location */
    switch (destination) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        default: break;
    }
}

/* this function emulates all of the OUT instructions */
static void OUT(Z80 z, cpuVal destination, cpuVal source)
{
    /* define variables */
    emuint address = 0;
    
    /* formulate address and obtain byte from it */
    switch (destination) {
        case aC: address = z->regC;
                 address = 0xFFFF & ((z->regB << 8) | (address & 0xFF));
                 break;
        case a_n: address = readFromMemory(z, readProgramCounter(z));
                  incrementProgramCounter(z);
                  address = 0xFFFF & ((z->regA << 8) | (address & 0xFF));
                  break;
        default: break;
    }
    
    /* deal with flags and set cycle count */
    switch (destination) {
        case a_n: z->cycles = 11; break;
        default: z->cycles = 12; break;
    }
    
    /* store byte in correct location */
    switch (source) {
        case A: writeToIO(z, address, z->regA); break;
        case B: writeToIO(z, address, z->regB); break;
        case C: writeToIO(z, address, z->regC); break;
        case D: writeToIO(z, address, z->regD); break;
        case E: writeToIO(z, address, z->regE); break;
        case H: writeToIO(z, address, z->regH); break;
        case L: writeToIO(z, address, z->regL); break;
        default: break;
    }
}

/* this function emulates the IND instruction */
static void IND(Z80 z)
{
    /* define variables */
    emuint address = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte temp = readFromIO(z, address);
    
    /* write byte to (HL) */
    writeToMemory(z, tempHL, temp);
    
    /* decrement B register and HL */
    z->regB = 0xFF & (z->regB - 1);
    tempHL = 0xFFFF & (tempHL - 1);

    /* store HL back into constituent registers */
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* deal with flags */
    calculateZFlag(z, z->regB);
    setNFlag(z, true);
    
    /* set cycle count */
    z->cycles = 16;
}

/* this function emulates the INDR instruction */
static void INDR(Z80 z)
{
    /* define variables */
    emuint address = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte temp = readFromIO(z, address);
    
    /* write byte to (HL) */
    writeToMemory(z, tempHL, temp);
    
    /* decrement B register and HL */
    z->regB = 0xFF & (z->regB - 1);
    tempHL = 0xFFFF & (tempHL - 1);

    /* store HL back into constituent registers */
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* deal with flags */
    setZFlag(z, true);
    setNFlag(z, true);
    
    /* check value of reg B and act accordingly, repeating
       instruction if necessary */
    if ((z->regB & 0xFF) == 0) {
        z->cycles = 16;
    } else {
        decrementProgramCounter(z);
        decrementProgramCounter(z);
        z->cycles = 21;
    }
}

/* this function emulates the INI instruction */
static void INI(Z80 z)
{
    /* define variables */
    emuint address = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte temp = readFromIO(z, address);
    
    /* write byte to (HL) */
    writeToMemory(z, tempHL, temp);
    
    /* decrement B register and increment HL */
    z->regB = 0xFF & (z->regB - 1);
    tempHL = 0xFFFF & (tempHL + 1);

    /* store HL back into constituent registers */
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* deal with flags */
    calculateZFlag(z, z->regB);
    setNFlag(z, true);
    
    /* set cycle count */
    z->cycles = 16;
}

/* this function emulates the INIR instruction */
static void INIR(Z80 z)
{
    /* define variables */
    emuint address = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte temp = readFromIO(z, address);
    
    /* write byte to (HL) */
    writeToMemory(z, tempHL, temp);
    
    /* decrement B register and increment HL */
    z->regB = 0xFF & (z->regB - 1);
    tempHL = 0xFFFF & (tempHL + 1);

    /* store HL back into constituent registers */
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* deal with flags */
    setZFlag(z, true);
    setNFlag(z, true);
    
    /* check value of reg B and act accordingly, repeating
       instruction if necessary */
    if ((z->regB & 0xFF) == 0) {
        z->cycles = 16;
    } else {
        decrementProgramCounter(z);
        decrementProgramCounter(z);
        z->cycles = 21;
    }
}

/* this function emulates the OUTD instruction */
static void OUTD(Z80 z)
{
    /* define variables */
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte temp = readFromMemory(z, tempHL);
    emuint address = 0;
    
    /* decrement reg B */
    z->regB = 0xFF & (z->regB - 1);
    
    /* formulate address and write byte to it */
    address = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    writeToIO(z, address, temp);
    
    /* decrement HL then write it back to its constituent registers */
    tempHL = 0xFFFF & (tempHL - 1);
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* deal with flags */
    calculateZFlag(z, z->regB);
    setNFlag(z, true);
    
    /* set cycle count */
    z->cycles = 16;
}

/* this function emulates the OTDR instruction */
static void OTDR(Z80 z)
{
    /* define variables */
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte temp = readFromMemory(z, tempHL);
    emuint address = 0;
    
    /* decrement reg B */
    z->regB = 0xFF & (z->regB - 1);
    
    /* formulate address and write byte to it */
    address = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    writeToIO(z, address, temp);
    
    /* decrement HL then write it back to its constituent registers */
    tempHL = 0xFFFF & (tempHL - 1);
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* deal with flags */
    setZFlag(z, true);
    setNFlag(z, true);
    
    /* check value of reg B and act accordingly, repeating
       instruction if necessary */
    if ((z->regB & 0xFF) == 0) {
        z->cycles = 16;
    } else {
        decrementProgramCounter(z);
        decrementProgramCounter(z);
        z->cycles = 21;
    }
}

/* this function emulates the OUTI instruction */
static void OUTI(Z80 z)
{
    /* define variables */
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte temp = readFromMemory(z, tempHL);
    emuint address = 0;
    
    /* decrement reg B */
    z->regB = 0xFF & (z->regB - 1);
    
    /* formulate address and write byte to it */
    address = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    writeToIO(z, address, temp);
    
    /* increment HL then write it back to its constituent registers */
    tempHL = 0xFFFF & (tempHL + 1);
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* deal with flags */
    calculateZFlag(z, z->regB);
    setNFlag(z, true);
    
    /* set cycle count */
    z->cycles = 16;
}

/* this function emulates the OTIR instruction */
static void OTIR(Z80 z)
{
    /* define variables */
    emuint tempHL = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
    emubyte temp = readFromMemory(z, tempHL);
    emuint address = 0;
    
    /* decrement reg B */
    z->regB = 0xFF & (z->regB - 1);
    
    /* formulate address and write byte to it */
    address = 0xFFFF & ((z->regB << 8) | (z->regC & 0xFF));
    writeToIO(z, address, temp);
    
    /* decrement HL then write it back to its constituent registers */
    tempHL = 0xFFFF & (tempHL + 1);
    z->regH = 0xFF & (tempHL >> 8);
    z->regL = 0xFF & tempHL;
    
    /* deal with flags */
    setZFlag(z, true);
    setNFlag(z, true);
    
    /* check value of reg B and act accordingly, repeating
       instruction if necessary */
    if ((z->regB & 0xFF) == 0) {
        z->cycles = 16;
    } else {
        decrementProgramCounter(z);
        decrementProgramCounter(z);
        z->cycles = 21;
    }
}

/* this function emulates all of the conditional JP instructions */
static void JP_CONDITIONAL(Z80 z, cpuVal value)
{
    /* define variables to hold correct bitmask and expected result,
       as well as the new address */
    emubyte bitmask = 0, expected_result = 0;
    emuint newAddress = 0;
    
    /* check value and set variables accordingly */
    switch (value) {
        case NZ: bitmask = 0x40; expected_result = 0; break; /* test for no zero flag */
        case Z: bitmask = 0x40; expected_result = 64; break; /* test for zero flag */
        case NC: bitmask = 0x01; expected_result = 0; break; /* test for no carry flag */
        case C: bitmask = 0x01; expected_result = 1; break; /* test for carry flag */
        case PO: bitmask = 0x04; expected_result = 0; break; /* test for odd parity flag */
        case PE: bitmask = 0x04; expected_result = 4; break; /* test for even parity flag */
        case P: bitmask = 0x80; expected_result = 0; break; /* test for no negative flag */
        case M: bitmask = 0x80; expected_result = 128; break; /* test for negative flag */
        default: break;
    }
    
    /* read in new program counter address */
    newAddress = readFromMemory(z, readProgramCounter(z));
    incrementProgramCounter(z);
    newAddress = 0xFFFF & ((readFromMemory(z, readProgramCounter(z)) << 8) | (newAddress & 0xFF));
    incrementProgramCounter(z);
    
    /* run conditional test and act accordingly */
    if ((z->regF & bitmask) == expected_result)
        writeProgramCounter(z, newAddress);
    
    /* set cycle count */
    z->cycles = 10;
}

/* this function emulates all of the non-conditional JP instructions */
static void JP(Z80 z, cpuVal value)
{
    /* define variables */
    emuint newAddress = 0;
    
    /* formulate address correctly and set cycle count */
    switch (value) {
        case HL: newAddress = 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF));
                 z->cycles = 4; break;
        case IX: newAddress = z->regIX;
                 z->cycles = 8; break;
        case IY: newAddress = z->regIY;
                 z->cycles = 8; break;
        case nn: newAddress = readFromMemory(z, readProgramCounter(z));
                 incrementProgramCounter(z);
                 newAddress = 0xFFFF & ((readFromMemory(z, readProgramCounter(z)) << 8)
                                       | (newAddress & 0xFF));
                 z->cycles = 10; break;
        default: break;
    }
    
    /* store new address to program counter */
    writeProgramCounter(z, newAddress);
}

/* this function emulates all the conditional JR e instructions */
static void JR_CONDITIONAL(Z80 z, cpuVal value)
{
    /* define variables to hold correct bitmask and expected result,
       as well as the new address and adjustment value */
    emubyte bitmask = 0, expected_result = 0;
    signed_emubyte adjustment = 0;
    emuint tempPC = 0;
    
    /* check value and set variables accordingly */
    switch (value) {
        case NZ: bitmask = 0x40; expected_result = 0; break; /* test for no zero flag */
        case Z: bitmask = 0x40; expected_result = 64; break; /* test for zero flag */
        case NC: bitmask = 0x01; expected_result = 0; break; /* test for no carry flag */
        case C: bitmask = 0x01; expected_result = 1; break; /* test for carry flag */
        default: break;
    }
    
    /* read in adjustment value and create new program counter address */
    adjustment = readFromMemory(z, readProgramCounter(z));
    incrementProgramCounter(z);
    tempPC = 0xFFFF & (readProgramCounter(z) + adjustment);
    
    /* run conditional test and branch accordingly */
    if ((z->regF & bitmask) == expected_result) {
        writeProgramCounter(z, tempPC);
        z->cycles = 12;
    } else {
        z->cycles = 7;
    }
}

/* this function emulates the JR e instruction */
static void JR(Z80 z)
{
    /* define variables */
    signed_emubyte adjustment = 0;
    
    /* read in adjustment value and update program counter */
    adjustment = readFromMemory(z, readProgramCounter(z));
    incrementProgramCounter(z);
    writeProgramCounter(z, 0xFFFF & (readProgramCounter(z) + adjustment));
    
    /* set cycle count */
    z->cycles = 12;
}

/* this function emulates the RETI instruction */
static void RETI(Z80 z)
{
    /* define variables */
    emubyte high = 0, low = 0;
    
    /* assemble new program counter address from the stack */
    low = readFromMemory(z, readStackPointer(z));
    incrementStackPointer(z);
    high = readFromMemory(z, readStackPointer(z));
    incrementStackPointer(z);
    
    /* store new address in program counter and set cycle count */
    writeProgramCounter(z, 0xFFFF & ((high << 8) | (low & 0xFF)));
    z->iff1 = z->iff2;
    z->cycles = 14;
}

/* this function emulates the RETN instruction */
static void RETN(Z80 z)
{
    /* define variables */
    emubyte high = 0, low = 0;
    
    /* assemble new program counter address from the stack */
    low = readFromMemory(z, readStackPointer(z));
    incrementStackPointer(z);
    high = readFromMemory(z, readStackPointer(z));
    incrementStackPointer(z);
    
    /* store new address in program counter, set cycle count
       and set reg iff1 to reg iff2 */
    writeProgramCounter(z, 0xFFFF & ((high << 8) | (low & 0xFF)));
    z->iff1 = z->iff2;
    z->cycles = 14;
}

/* this function emulates all of the RL instructions */
static void RL(Z80 z, cpuVal value)
{
    /* define variables */
    emubyte temp = 0, tempCarry = 0;
    signed_emubyte d = 0;
    
    /* determine which value to retrieve, and set cycle count */
    switch (value) {
        case A: temp = z->regA;
                z->cycles = 8; break;
        case B: temp = z->regB;
                z->cycles = 8; break;
        case C: temp = z->regC;
                z->cycles = 8; break;
        case D: temp = z->regD;
                z->cycles = 8; break;
        case E: temp = z->regE;
                z->cycles = 8; break;
        case H: temp = z->regH;
                z->cycles = 8; break;
        case L: temp = z->regL;
                z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 15; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        default: break;
    }
    
    /* store previous carry flag */
    tempCarry = 0x01 & z->regF;
    
    /* store bit 7 of retrieved value into carry flag */
    if ((temp & 0x80) == 128)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    
    /* rotate value left 1 bit and merge in previous carry bit */
    temp = temp << 1;
    temp = (temp & 0xFE) | tempCarry;
    
    /* deal with flags */
    calculateSFlag(z, temp);
    calculateZFlag(z, temp);
    setHFlag(z, false);
    calculatePVFlag(z, temp);
    setNFlag(z, false);
    
    /* store value back to relevant location */
    switch (value) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        default: break;
    }
}

/* this function emulates the RLA instruction */
static void RLA(Z80 z)
{
    /* define variables */
    emubyte tempCarry = 0;
    emuint tempA = 0;
    
    /* retrieve reg A */
    tempA = z->regA;
    
    /* store previous carry flag */
    tempCarry = 0x01 & z->regF;
    
    /* rotate temp A left 1 bit, and copy previous bit 7 to carry flag, as
       well as previous carry to bit 0 */
    tempA = tempA << 1;
    if ((tempA & 0x100) == 256)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    tempA = (tempA & 0xFE) | tempCarry;
    
    /* deal with flags */
    setHFlag(z, false);
    setNFlag(z, false);
    
    /* store value back to reg A */
    z->regA = 0xFF & tempA;
    
    /* set cycle count */
    z->cycles = 4;
}

/* this function emulates all of the RLC instructions */
static void RLC(Z80 z, cpuVal value)
{
    /* define variables */
    emubyte temp = 0, tempBit = 0;
    signed_emubyte d = 0;
    
    /* determine which value to retrieve, and set cycle count */
    switch (value) {
        case A: temp = z->regA;
                z->cycles = 8; break;
        case B: temp = z->regB;
                z->cycles = 8; break;
        case C: temp = z->regC;
                z->cycles = 8; break;
        case D: temp = z->regD;
                z->cycles = 8; break;
        case E: temp = z->regE;
                z->cycles = 8; break;
        case H: temp = z->regH;
                z->cycles = 8; break;
        case L: temp = z->regL;
                z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 15; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        default: break;
    }
    
    /* store bit 7 of retrieved value into carry flag */
    if ((temp & 0x80) == 128)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    
    /* copy bit 7 to tempBit */
    tempBit = 0x80 & temp;
    tempBit = tempBit >> 7;
    
    /* rotate value left 1 bit and merge in previous bit 7 */
    temp = temp << 1;
    temp = (temp & 0xFE) | tempBit;
    
    /* deal with flags */
    calculateSFlag(z, temp);
    calculateZFlag(z, temp);
    setHFlag(z, false);
    calculatePVFlag(z, temp);
    setNFlag(z, false);
    
    /* store value back to relevant location */
    switch (value) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        default: break;
    }
}

/* this function emulates the RLCA instruction */
static void RLCA(Z80 z)
{
    /* define variables */
    emuint tempA = 0;
    emubyte tempBit = 0;
    
    /* retrieve reg A */
    tempA = z->regA;
    
    /* rotate left 1 bit, and copy previous bit 7 to carry flag and bit 0 */
    tempA = tempA << 1;
    if ((tempA & 0x100) == 256)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    tempBit = 0x01 & (tempA >> 8);
    tempA = (tempA & 0xFE) | tempBit;
    
    /* deal with flags */
    setHFlag(z, false);
    setNFlag(z, false);
    
    /* store value back to reg A */
    z->regA = 0xFF & tempA;
    
    /* set cycle count */
    z->cycles = 4;
}

/* this function emulates the RLD instruction */
static void RLD(Z80 z)
{
    /* define variables */
    emuint tempMem = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
    emubyte tempA = z->regA;
    emuint tempHigh = 0;
    
    /* move (HL) left 4 bits */
    tempMem = tempMem << 4;
    
    /* merge in lower 4 bits of accumulator */
    tempMem = tempMem | (0x0F & tempA);
    
    /* copy previous higher order nibble of (HL) to temp A and adjust */
    tempHigh = 0xF00 & tempMem;
    tempHigh = tempHigh >> 8;
    
    /* merge this higher order nibble to make it the new lower order nibble of reg A */
    tempA = (tempA & 0xF0) | tempHigh;
    
    /* deal with flags */
    calculateSFlag(z, tempA);
    calculateZFlag(z, tempA);
    setHFlag(z, false);
    calculatePVFlag(z, tempA);
    setNFlag(z, false);
    
    /* store values back to reg A and (HL) */
    z->regA = 0xFF & tempA;
    writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), 0xFF & tempMem);
    
    /* set cycle count */
    z->cycles = 18;
}

/* this function emulates the RR instructions */
static void RR(Z80 z, cpuVal value)
{
    /* define variables */
    emubyte temp = 0, tempCarry = 0;
    signed_emubyte d = 0;
    
    /* determine which value to retrieve, and set cycle count */
    switch (value) {
        case A: temp = z->regA;
                z->cycles = 8; break;
        case B: temp = z->regB;
                z->cycles = 8; break;
        case C: temp = z->regC;
                z->cycles = 8; break;
        case D: temp = z->regD;
                z->cycles = 8; break;
        case E: temp = z->regE;
                z->cycles = 8; break;
        case H: temp = z->regH;
                z->cycles = 8; break;
        case L: temp = z->regL;
                z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 15; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        default: break;
    }
    
    /* store previous carry flag and adjust for merging */
    tempCarry = 0x01 & z->regF;
    tempCarry = tempCarry << 7;
    
    /* store bit 0 of retrieved value into carry flag */
    if ((temp & 0x01) == 1)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    
    /* rotate value right 1 bit and merge in previous carry */
    temp = temp >> 1;
    temp = tempCarry | (temp & 0x7F);
    
    /* deal with flags */
    calculateSFlag(z, temp);
    calculateZFlag(z, temp);
    setHFlag(z, false);
    calculatePVFlag(z, temp);
    setNFlag(z, false);
    
    /* store value back to relevant location */
    switch (value) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        default: break;
    }
}

/* this function emulates the RRA instruction */
static void RRA(Z80 z)
{
    /* retrieve reg A and carry flag, and prepare carry flag for merging */
    emubyte tempA = z->regA, tempCarry = 0x01 & z->regF;
    tempCarry = tempCarry << 7;

    /* copy bit 0 to cary flag before rotation */
    if ((tempA & 0x01) == 1)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    
    /* rotate 1 bit to the right, and merge previous carry value */
    tempA = tempA >> 1;
    tempA = tempCarry | (tempA & 0x7F);
    
    /* deal with flags */
    setHFlag(z, false);
    setNFlag(z, false);
    
    /* store value back into reg A */
    z->regA = 0xFF & tempA;

    /* set cycle count */
    z->cycles = 4;
}

/* this function emulates all of the RRC instructions */
static void RRC(Z80 z, cpuVal value)
{
    /* define variables */
    emubyte temp = 0, tempBit = 0;
    signed_emubyte d = 0;
    
    /* determine which value to retrieve, and set cycle count */
    switch (value) {
        case A: temp = z->regA;
                z->cycles = 8; break;
        case B: temp = z->regB;
                z->cycles = 8; break;
        case C: temp = z->regC;
                z->cycles = 8; break;
        case D: temp = z->regD;
                z->cycles = 8; break;
        case E: temp = z->regE;
                z->cycles = 8; break;
        case H: temp = z->regH;
                z->cycles = 8; break;
        case L: temp = z->regL;
                z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 15; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        default: break;
    }
    
    /* copy bit 0 to carry flag and temporary variable, adjusting temporary variable
       for merging */
    tempBit = 0x01 & temp;
    if (tempBit == 1)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    tempBit = tempBit << 7;
    
    /* rotate value right 1 bit and merge in previous bit 0 into bit 7 */
    temp = temp >> 1;
    temp = tempBit | (temp & 0x7F);
    
    /* deal with flags */
    calculateSFlag(z, temp);
    calculateZFlag(z, temp);
    setHFlag(z, false);
    calculatePVFlag(z, temp);
    setNFlag(z, false);
    
    /* store value back to relevant location */
    switch (value) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        default: break;
    }
}

/* this function emulates the RRCA instruction */
static void RRCA(Z80 z)
{
    /* retrieve reg A and copy its bit 0 to a temporary variable */
    emubyte tempA = z->regA, tempBit = z->regA & 0x01;
    
    /* copy bit 0 to carry flag and adjust it in preparation for replacing bit 7 */
    if (tempBit == 1)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    tempBit = tempBit << 7;
    
    /* rotate temp A right 1 bit, and copy previous bit 0 to bit 7 */
    tempA = tempA >> 1;
    tempA = tempBit | (0x7F & tempA);
    
    /* deal with flags */
    setHFlag(z, false);
    setNFlag(z, false);
    
    /* store value back into reg A */
    z->regA = 0xFF & tempA;
    
    /* set cycle count */
    z->cycles = 4;
}

/* this function emulates the RRD instruction */
static void RRD(Z80 z)
{
    /* retrieve reg A and (HL) */
    emubyte tempA = z->regA;
    emubyte tempMem = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
    
    /* copy lower four bits of accumulator to temporary variable and adjust them */
    emubyte tempNibble = 0x0F & tempA;
    tempNibble = tempNibble << 4;
    
    /* merge lower nibble of (HL) with accumulator */
    tempA = (tempA & 0xF0) | (tempMem & 0x0F);
    
    /* shift (HL) right 4 bits and merge in temporary variable from earlier */
    tempMem = tempMem >> 4;
    tempMem = (tempNibble & 0xF0) | tempMem;
    
    /* deal with flags */
    calculateSFlag(z, tempA);
    calculateZFlag(z, tempA);
    setHFlag(z, false);
    calculatePVFlag(z, tempA);
    setNFlag(z, false);
    
    /* store values back to accumulator and (HL) */
    z->regA = tempA;
    writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), tempMem);
    
    /* set cycle count */
    z->cycles = 18;
}

/* this function emulates all of the SLA instructions */
static void SLA(Z80 z, cpuVal value)
{
    /* define variables */
    emubyte temp = 0;
    signed_emubyte d = 0;
    
    /* determine which value to retrieve, and set cycle count */
    switch (value) {
        case A: temp = z->regA;
                z->cycles = 8; break;
        case B: temp = z->regB;
                z->cycles = 8; break;
        case C: temp = z->regC;
                z->cycles = 8; break;
        case D: temp = z->regD;
                z->cycles = 8; break;
        case E: temp = z->regE;
                z->cycles = 8; break;
        case H: temp = z->regH;
                z->cycles = 8; break;
        case L: temp = z->regL;
                z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 15; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        default: break;
    }
    
    /* copy bit 7 to carry flag */
    if ((temp & 0x80) == 128)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    
    /* move value left 1 bit */
    temp = temp << 1;
    
    /* deal with flags */
    calculateSFlag(z, temp);
    calculateZFlag(z, temp);
    setHFlag(z, false);
    calculatePVFlag(z, temp);
    setNFlag(z, false);
    
    /* store value back to relevant location */
    switch (value) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        default: break;
    }
}

/* this function emulates all of the SLL instructions */
static void SLL(Z80 z, cpuVal value)
{
    /* define variables */
    emubyte temp = 0;
    signed_emubyte d = 0;
    
    /* determine which value to retrieve, and set cycle count */
    switch (value) {
        case A: temp = z->regA;
                z->cycles = 8; break;
        case B: temp = z->regB;
                z->cycles = 8; break;
        case C: temp = z->regC;
                z->cycles = 8; break;
        case D: temp = z->regD;
                z->cycles = 8; break;
        case E: temp = z->regE;
                z->cycles = 8; break;
        case H: temp = z->regH;
                z->cycles = 8; break;
        case L: temp = z->regL;
                z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 15; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        default: break;
    }
    
    /* copy bit 7 to carry flag */
    if ((temp & 0x80) == 128)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    
    /* move value left 1 bit */
    temp = temp << 1;
    
    /* set bit 0 of value */
    temp = temp | 0x01;
    
    /* deal with flags */
    calculateSFlag(z, temp);
    setZFlag(z, false);
    setHFlag(z, false);
    calculatePVFlag(z, temp);
    setNFlag(z, false);
    
    /* store value back to relevant location */
    switch (value) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        default: break;
    }
}

/* this function emulates all of the SRA instructions */
static void SRA(Z80 z, cpuVal value)
{
    /* define variables */
    emubyte temp = 0, tempBit = 0;
    signed_emubyte d = 0;
    
    /* determine which value to retrieve, and set cycle count */
    switch (value) {
        case A: temp = z->regA;
                z->cycles = 8; break;
        case B: temp = z->regB;
                z->cycles = 8; break;
        case C: temp = z->regC;
                z->cycles = 8; break;
        case D: temp = z->regD;
                z->cycles = 8; break;
        case E: temp = z->regE;
                z->cycles = 8; break;
        case H: temp = z->regH;
                z->cycles = 8; break;
        case L: temp = z->regL;
                z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 15; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        default: break;
    }
    
    /* copy bit 0 to carry flag, and bit 7 to tempBit variable */
    if ((temp & 0x01) == 1)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    tempBit = 0x80 & temp;
    
    /* shift value right 1 bit, and merge in previous value of bit 7 */
    temp = temp >> 1;
    temp = tempBit | (temp & 0x7F);
    
    /* deal with flags */
    calculateSFlag(z, temp);
    calculateZFlag(z, temp);
    setHFlag(z, false);
    calculatePVFlag(z, temp);
    setNFlag(z, false);
    
    /* store value back to relevant location */
    switch (value) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        default: break;
    }
}

/* this function emulates all of the SRL instructions */
static void SRL(Z80 z, cpuVal value)
{
    /* define variables */
    emubyte temp = 0;
    signed_emubyte d = 0;
    
    /* determine which value to retrieve, and set cycle count */
    switch (value) {
        case A: temp = z->regA;
                z->cycles = 8; break;
        case B: temp = z->regB;
                z->cycles = 8; break;
        case C: temp = z->regC;
                z->cycles = 8; break;
        case D: temp = z->regD;
                z->cycles = 8; break;
        case E: temp = z->regE;
                z->cycles = 8; break;
        case H: temp = z->regH;
                z->cycles = 8; break;
        case L: temp = z->regL;
                z->cycles = 8; break;
        case aHL: temp = readFromMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)));
                  z->cycles = 15; break;
        case IXd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIX + d));
                  z->cycles = 23; break;
        case IYd: d = readFromMemory(z, readProgramCounter(z) - 2);
                  temp = readFromMemory(z, 0xFFFF & (z->regIY + d));
                  z->cycles = 23; break;
        default: break;
    }
    
    /* copy bit 0 to carry flag */
    if ((temp & 0x01) == 1)
        setCFlag(z, true);
    else
        setCFlag(z, false);
    
    /* shift value right 1 bit */
    temp = temp >> 1;
    
    /* deal with flags */
    setSFlag(z, false);
    calculateZFlag(z, temp);
    setHFlag(z, false);
    calculatePVFlag(z, temp);
    setNFlag(z, false);
    
    /* store value back to relevant location */
    switch (value) {
        case A: z->regA = temp; break;
        case B: z->regB = temp; break;
        case C: z->regC = temp; break;
        case D: z->regD = temp; break;
        case E: z->regE = temp; break;
        case H: z->regH = temp; break;
        case L: z->regL = temp; break;
        case aHL: writeToMemory(z, 0xFFFF & ((z->regH << 8) | (z->regL & 0xFF)), temp);
                  break;
        case IXd: writeToMemory(z, 0xFFFF & (z->regIX + d), temp); break;
        case IYd: writeToMemory(z, 0xFFFF & (z->regIY + d), temp); break;
        default: break;
    }
}

/* this function emulates the SCF instruction */
static void SCF(Z80 z)
{
    /* deal with flags */
    setHFlag(z, false);
    setNFlag(z, false);
    setCFlag(z, true);
    
    /* set cycle count */
    z->cycles = 4;
}

/* this function returns an emubool pointer to the state of the Z80 object */
emubyte *Z80_saveState(Z80 z)
{
    /* define variables */
    emuint z80Size = 34 + 7; /* add extra 7 bytes for header and size */
    emuint marker = 0;

    /* allocate the memory */
    emubyte *z80State = malloc(sizeof(emubyte) * z80Size);
    if (z80State == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "z80.c", "Couldn't allocate memory for Z80 state buffer...\n");
        return NULL;
    }

    /* fill first 7 bytes with interrupt stuff and state size */
    z80State[marker++] = z80Size & 0xFF;
    z80State[marker++] = (z80Size >> 8) & 0xFF;
    z80State[marker++] = (z80Size >> 16) & 0xFF;
    z80State[marker++] = (z80Size >> 24) & 0xFF;
    z80State[marker++] = z->interruptPending;
    z80State[marker++] = z->interruptCounter & 0xFF;
    z80State[marker++] = (z->interruptCounter >> 8) & 0xFF;

    /* copy interrupt related values to state buffer */
    z80State[marker++] = z->nmi;
    z80State[marker++] = z->iff1;
    z80State[marker++] = z->iff2;
    z80State[marker++] = z->halt;
    z80State[marker++] = z->intMode;
    if (z->followingInstruction)
        z80State[marker++] = 1;
    else
        z80State[marker++] = 0;

    /* copy main 8 bit registers to state buffer */
    z80State[marker++] = z->regA;
    z80State[marker++] = z->regB;
    z80State[marker++] = z->regC;
    z80State[marker++] = z->regD;
    z80State[marker++] = z->regE;
    z80State[marker++] = z->regH;
    z80State[marker++] = z->regL;
    z80State[marker++] = z->regF;

    /* copy shadow 8 bit registers to state buffer */
    z80State[marker++] = z->regAShadow;
    z80State[marker++] = z->regBShadow;
    z80State[marker++] = z->regCShadow;
    z80State[marker++] = z->regDShadow;
    z80State[marker++] = z->regEShadow;
    z80State[marker++] = z->regHShadow;
    z80State[marker++] = z->regLShadow;
    z80State[marker++] = z->regFShadow;

    /* copy miscellaneous registers to state buffer */
    z80State[marker++] = z->regIX & 0xFF;
    z80State[marker++] = (z->regIX >> 8) & 0xFF;
    z80State[marker++] = z->regIY & 0xFF;
    z80State[marker++] = (z->regIY >> 8) & 0xFF;
    z80State[marker++] = z->regSP & 0xFF;
    z80State[marker++] = (z->regSP >> 8) & 0xFF;
    z80State[marker++] = z->regI;
    z80State[marker++] = z->regR;
    z80State[marker++] = z->regPC & 0xFF;
    z80State[marker++] = (z->regPC >> 8) & 0xFF;

    /* copy miscellaneous attributes to state buffer */
    if (z->ddStub)
        z80State[marker++] = 1;
    else
        z80State[marker++] = 0;
    if (z->fdStub)
        z80State[marker++] = 1;
    else
        z80State[marker++] = 0;

    /* return Z80 state */
    return z80State;
}

/* this function returns the number of bytes required by a Z80 object */
emuint Z80_getMemoryUsage(void)
{
    return sizeof(struct Z80) * sizeof(emubyte);
}