/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  lib_sanitizercov.c
 */

/* ------------------------- System Includes -------------------------------- */
#ifndef LIB_SANITIZER_COV_UNIT_TEST
#include "lwuproc.h"
#endif  // LIB_SANITIZER_COV_UNIT_TEST

#include <string.h>

/* ------------------------ Application Includes ---------------------------- */
#ifndef LIB_SANITIZER_COV_UNIT_TEST
#include "flcntypes.h"
#include "flcnretval.h"
#endif  // LIB_SANITIZER_COV_UNIT_TEST

#include "lib_sanitizercov.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define _RET_IP_  ((unsigned long) __builtin_return_address(0))

// return early we are not enabled or we do not have enough available buffer space
#define CHECK_ENABLED_AND_RESERVE_OR_RETURN(nelts, idxout) do { \
    if (!sanitizerCovInternalState.bEnabled) { \
        return; \
    } \
    *(idxout) = sanitizerCovInternalState.end; \
    if (*(idxout) + (nelts) > SANITIZER_COV_BUF_NUM_ELEMENTS) { \
        sanitizerCovInternalState.missed++; \
        return; \
    } \
    sanitizerCovInternalState.end = *(idxout) + (nelts); \
} while(0)

// encodes the first QWORD of a DMEM buffer entry
// (including magic, callback type, size of parameters, and function return address),
// see DMEM buffer format details at
// https://confluence.lwpu.com/display/GPUSysSwSec/SanitizerCoverage+DMEM+buffer+format
#define ENCODE_FIRST(typ, psz, pc) \
    ((((LwU64) (SANITIZER_COV_MAGIC)) << 54) | (((LwU64) (typ)) << 50) | (((LwU64) (psz)) << 48) | (((LwU64) (pc)) & 0xffffffffffff))

// encodes parameters of different sizes (in bits) for *_cmp callbacks
#define ENCODE_CMP_PARAMS(psznbits, arg1, arg2) \
    ((((LwU64) (arg2)) << (psznbits)) | ((LwU64) (arg1)))

// stores a QWORD into our buffer
#define STORE(idxvar, value) do { \
    sanitizerCovInternalState.buf[idxvar++] = (LwU64) (value); \
} while(0)

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static SANITIZER_COV_INTERNAL_STATE sanitizerCovInternalState
    GCC_ATTRIB_SECTION("dmem_resident", "sanitizerCovInternalState");

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
void sanitizerCovInit(void)
{
    sanitizerCovInternalState.start = 0;
    sanitizerCovInternalState.end = 0;
    sanitizerCovInternalState.missed = 0;
    sanitizerCovInternalState.bEnabled = LW_FALSE;
}

FLCN_STATUS sanitizerCovGetStatus
(
    SANITIZER_COV_STATUS *pStatus
)
{
    if (pStatus == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pStatus->used = sanitizerCovInternalState.end;
    pStatus->missed = sanitizerCovInternalState.missed;
    pStatus->bEnabled = sanitizerCovInternalState.bEnabled;
    return FLCN_OK;
}

FLCN_STATUS sanitizerCovSetStatus
(
    SANITIZER_COV_STATUS *pStatus
)
{
    if (pStatus == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (pStatus->used == 0) {
        sanitizerCovInternalState.start = 0;
        sanitizerCovInternalState.end = 0;
    }
    if (pStatus->missed == 0) {
        sanitizerCovInternalState.missed = 0;
    }
    sanitizerCovInternalState.bEnabled = pStatus->bEnabled;
    return FLCN_OK;
}

FLCN_STATUS sanitizerCovCopyData
(
    LwU64 *pDest,
    LwU32 *pNumElements,
    LwBool *pDone
)
{
    return sanitizerCovCopyDataAsBytes((LwU8 *) pDest, pNumElements, pDone);
}

FLCN_STATUS sanitizerCovCopyDataAsBytes
(
    LwU8 *pDest,
    LwU32 *pNumElements,
    LwBool *pDone
)
{
    LwU32       start = sanitizerCovInternalState.start;
    const LwU32 end   = sanitizerCovInternalState.end;
    LwU32       nread;

    if (pDest == NULL || pNumElements == NULL)
    {
        *pNumElements = 0;
        *pDone = LW_FALSE;
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (end > SANITIZER_COV_BUF_NUM_ELEMENTS)
    {
        *pNumElements = 0;
        *pDone = LW_FALSE;
        return FLCN_ERR_ILWALID_STATE;
    }

    nread = *pNumElements;
    if (nread > (end - start))
    {
        nread = end - start;
    }

    if (nread > end)
    {
        *pNumElements = 0;
        *pDone = LW_FALSE;
        return FLCN_ERR_ILWALID_STATE;
    }

    memcpy(pDest, &sanitizerCovInternalState.buf[start], nread * sizeof(LwU64));
    *pNumElements = nread;

    start += nread;
    if (start == end)
    {
        sanitizerCovInternalState.start = 0;
        sanitizerCovInternalState.end = 0;
        *pDone = LW_TRUE;
    }
    else
    {
        sanitizerCovInternalState.start = start;
        *pDone = LW_FALSE;
    }

    return FLCN_OK;
}


void __sanitizer_cov_trace_pc(void)
{
    LwU32 idx;
    CHECK_ENABLED_AND_RESERVE_OR_RETURN(1, &idx);
    sanitizerCovInternalState.buf[idx] = ENCODE_FIRST(SANITIZER_COV_TYP_TRACE_PC, 0, _RET_IP_);
}

// called before a comparison instruction, Arg1 and Arg2 are arguments of the comparison
void __sanitizer_cov_trace_cmp1(LwU8 Arg1, LwU8 Arg2)
{
    LwU32 idx;
    CHECK_ENABLED_AND_RESERVE_OR_RETURN(2, &idx);
    sanitizerCovInternalState.buf[idx]     = ENCODE_FIRST(SANITIZER_COV_TYP_TRACE_CMP, 0, _RET_IP_);
    sanitizerCovInternalState.buf[idx + 1] = ENCODE_CMP_PARAMS(8, Arg1, Arg2);
}

void __sanitizer_cov_trace_cmp2(LwU16 Arg1, LwU16 Arg2)
{
    LwU32 idx;
    CHECK_ENABLED_AND_RESERVE_OR_RETURN(2, &idx);
    sanitizerCovInternalState.buf[idx]     = ENCODE_FIRST(SANITIZER_COV_TYP_TRACE_CMP, 1, _RET_IP_);
    sanitizerCovInternalState.buf[idx + 1] = ENCODE_CMP_PARAMS(16, Arg1, Arg2);
}

void __sanitizer_cov_trace_cmp4(LwU32 Arg1, LwU32 Arg2)
{
    LwU32 idx;
    CHECK_ENABLED_AND_RESERVE_OR_RETURN(2, &idx);
    sanitizerCovInternalState.buf[idx]     = ENCODE_FIRST(SANITIZER_COV_TYP_TRACE_CMP, 2, _RET_IP_);
    sanitizerCovInternalState.buf[idx + 1] = ENCODE_CMP_PARAMS(32, Arg1, Arg2);
}

void __sanitizer_cov_trace_cmp8(LwU64 Arg1, LwU64 Arg2)
{
    LwU32 idx;
    CHECK_ENABLED_AND_RESERVE_OR_RETURN(3, &idx);
    sanitizerCovInternalState.buf[idx]     = ENCODE_FIRST(SANITIZER_COV_TYP_TRACE_CMP, 3, _RET_IP_);
    sanitizerCovInternalState.buf[idx + 1] = (LwU64) Arg1;
    sanitizerCovInternalState.buf[idx + 2] = (LwU64) Arg2;
}

void __sanitizer_cov_trace_cmpf(LwF32 Arg1, LwF32 Arg2)
{
    // do nothing
}

void __sanitizer_cov_trace_cmpd(LwF64 Arg1, LwF64 Arg2)
{
    // do nothing
}

// called before a comparison instruction if exactly one of the arguments is constant,
// Arg1 and Arg2 are arguments of the comparison (Arg1 is a compile-time constant),
// these callbacks are emitted by -fsanitize-coverage=trace-cmp since 2017-08-11
void __sanitizer_cov_trace_const_cmp1(LwU8 Arg1, LwU8 Arg2)
{
    LwU32 idx;
    CHECK_ENABLED_AND_RESERVE_OR_RETURN(2, &idx);
    sanitizerCovInternalState.buf[idx]     = ENCODE_FIRST(SANITIZER_COV_TYP_TRACE_CONST_CMP, 0, _RET_IP_);
    sanitizerCovInternalState.buf[idx + 1] = ENCODE_CMP_PARAMS(8, Arg1, Arg2);
}

void __sanitizer_cov_trace_const_cmp2(LwU16 Arg1, LwU16 Arg2)
{
    LwU32 idx;
    CHECK_ENABLED_AND_RESERVE_OR_RETURN(2, &idx);
    sanitizerCovInternalState.buf[idx]     = ENCODE_FIRST(SANITIZER_COV_TYP_TRACE_CONST_CMP, 1, _RET_IP_);
    sanitizerCovInternalState.buf[idx + 1] = ENCODE_CMP_PARAMS(16, Arg1, Arg2);
}

void __sanitizer_cov_trace_const_cmp4(LwU32 Arg1, LwU32 Arg2)
{
    LwU32 idx;
    CHECK_ENABLED_AND_RESERVE_OR_RETURN(2, &idx);
    sanitizerCovInternalState.buf[idx]     = ENCODE_FIRST(SANITIZER_COV_TYP_TRACE_CONST_CMP, 2, _RET_IP_);
    sanitizerCovInternalState.buf[idx + 1] = ENCODE_CMP_PARAMS(32, Arg1, Arg2);
}

void __sanitizer_cov_trace_const_cmp8(LwU64 Arg1, LwU64 Arg2)
{
    LwU32 idx;
    CHECK_ENABLED_AND_RESERVE_OR_RETURN(3, &idx);
    sanitizerCovInternalState.buf[idx]     = ENCODE_FIRST(SANITIZER_COV_TYP_TRACE_CONST_CMP, 3, _RET_IP_);
    sanitizerCovInternalState.buf[idx + 1] = (LwU64) Arg1;
    sanitizerCovInternalState.buf[idx + 2] = (LwU64) Arg2;
}

// Called before a switch statement.
// Val is the switch operand.
// Cases[0] is the number of case constants.
// Cases[1] is the size of Val in bits.
// Cases[2:] are the case constants.
void __sanitizer_cov_trace_switch(LwU64 Val, LwU64 *Cases)
{
    LwU32 idx;
    LwU32 c;
    CHECK_ENABLED_AND_RESERVE_OR_RETURN(2 + Cases[0] + 2, &idx);
    sanitizerCovInternalState.buf[idx++] = ENCODE_FIRST(SANITIZER_COV_TYP_TRACE_SWITCH, 3, _RET_IP_);
    sanitizerCovInternalState.buf[idx++] = Val;
    for (c = 0; c < Cases[0] + 2; c++)
    {
        sanitizerCovInternalState.buf[idx++] = Cases[c];
    }
}
