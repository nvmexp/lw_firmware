/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//--------------------------------------------------------------------
// For loop
//
#define For(LBL, LOOP_REG, START, STOP, STEP)           \
    {                                                   \
        .reg.pred %pred;                                \
        mov.u32 LOOP_REG, START;                        \
        bra _START_FOR_##LBL;                           \
    _CONTINUE_FOR_##LBL:                                \
        add.u32 LOOP_REG, LOOP_REG, STEP;               \
    _START_FOR_##LBL:                                   \
        setp.ge.u32 %pred, LOOP_REG, STOP;              \
        @%pred bra _END_FOR_##LBL;

#define EndFor(LBL)                                     \
        bra _CONTINUE_FOR_##LBL;                        \
    _END_FOR_##LBL:                                     \
    }

//--------------------------------------------------------------------
// If
//
#define If(LBL, CMP, ARG1, ARG2)                        \
    {                                                   \
        .reg.pred %pred;                                \
        setp.CMP.u32 %pred, ARG1, ARG2;                 \
        @!%pred bra _ENDIF_##LBL;

#define Else(LBL)                                       \
        bra _ENDELSEIF_##LBL;                           \
    _ENDIF_##LBL:

#define EndElseIf(LBL)                                  \
    _ENDELSEIF_##LBL:                                   \
    }

#define EndIf(LBL)                                      \
    _ENDIF_##LBL:                                       \
    }

//--------------------------------------------------------------------
// While
//
#define While(LBL, CMP, ARG1, ARG2)                     \
    {                                                   \
    .reg.pred %pred;                                    \
    _CONTINUEWHILE_##LBL:                               \
        setp.CMP.u32 %pred, ARG1, ARG2;                 \
        @!%pred bra _ENDWHILE_##LBL;

#define EndWhile(LBL)                                   \
        bra _CONTINUEWHILE_##LBL;                       \
    _ENDWHILE_##LBL:                                    \
    }

//--------------------------------------------------------------------
// UpdateRng()
//
#define UpdateRng(RNG)                                  \
    mad.lo.u32 RNG, 0x19660d, RNG, 0x3c6ef35f;

//--------------------------------------------------------------------
// Rand()
//
#define Rand(DST, RNG, RANGE)                           \
    UpdateRng(RNG)                                      \
    brev.b32 DST, RNG;                                  \
    rem.u32 DST, DST, RANGE;

//--------------------------------------------------------------------
// Rand32()
//
#define Rand32(DST, RNG)                                \
    UpdateRng(RNG)                                      \
    mov.u32 DST, RNG;                                   \
    UpdateRng(RNG)                                      \
    prmt.b32 DST, DST, RNG, 0x7632;

#define Toggle1Bit(DST,RNG)                             \
    {                                                   \
        .reg.u32 %R0;                                   \
        .reg.u32 %MASK;                                 \
                                                        \
        Rand(%R0,RNG,32)                                \
        mov.u32 %MASK, 1;                               \
        shl.b32 %MASK, %MASK, %R0;                      \
        xor.b32 DST, DST, %MASK;                        \
    }

#define Toggle2Bits(LBL,DST,RNG)                        \
    {                                                   \
        .reg.u32 %R<2>;                                 \
        .reg.u32 %MASK;                                 \
                                                        \
        Rand(%R0,RNG,32)                                \
        mov.u32 %MASK, 1;                               \
        shl.b32 %MASK, %MASK, %R0;                      \
        xor.b32 DST, DST, %MASK;                        \
                                                        \
        mov.u32 %R1, %R0;                               \
        While(LBL,eq,%R1,%R0)                           \
            Rand(%R1,RNG,32)                            \
        EndWhile(LBL)                                   \
        mov.u32 %MASK, 1;                               \
        shl.b32 %MASK, %MASK, %R1;                      \
        xor.b32 DST, DST, %MASK;                        \
    }

