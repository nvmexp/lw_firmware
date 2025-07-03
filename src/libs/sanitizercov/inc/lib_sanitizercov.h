/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_SANITIZERCOV_H
#define LIB_SANITIZERCOV_H

/*!
 * @file lib_sanitizercov.h
 *
 * @brief Common library containing implementations of SanitizerCoverage
 *        (GCC coverage-guided fuzzing instrumentation) callback functions.
 *
 * This library stores run-time data gathered by SanitizerCoverage in a
 * designated DMEM buffer, sanitizerCovInternalState.buf. Data can be retrieved
 * from this buffer via sanitizerCovCopyData().
 *
 * The format of the data is specified at:
 * https://confluence.lwpu.com/display/GPUSysSwSec/SanitizerCoverage+DMEM+buffer+format
 *
 * Coverage collection can be enabled/disabled on-the-fly via
 * sanitizerCovSetEnabled(bool).
 *
 * This library is not FI (Fault Injection) aware. So, usage of this library
 * in use cases like establishment of chain of trust is not advisable.
 *
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "lib_sanitizercov_common.h"
#ifdef LIB_SANITIZER_COV_UNIT_TEST
#include "lib_sanitizercov_test.h"
#endif
/* ------------------------ Defines ----------------------------------------- */
#ifndef SANITIZER_COV_BUF_NUM_ELEMENTS
// default number of elements supported in buffer
// (can be overridden via Makefile variable of the same name or CFLAGs)
#define SANITIZER_COV_BUF_NUM_ELEMENTS  1022
#endif  // (SANITIZER_COV_BUF_NUM_ELEMENTS)

/* ------------------------ Types Definitions ------------------------------- */
typedef struct {
    LwU32 start;          // start of valid data in buf (idx where to read)
    volatile LwU32 end;   // one past end of valid data in buf (idx where to
                          // write, note: could have multiple writers due to
                          // task switching)
    LwU32 missed;         // number of callbacks missed due to full buffer
    LwBool bEnabled;      // whether enabled or not

    LwU64 buf[SANITIZER_COV_BUF_NUM_ELEMENTS];
} SANITIZER_COV_INTERNAL_STATE;

typedef struct {
    LwU32 used;       // number of elements used in buffer
    LwU32 missed;     // number of callbacks missed due to full buffer
    LwBool bEnabled;  // whether enabled or not
} SANITIZER_COV_STATUS;

/* ------------------------ External Definitions ---------------------------- */
/*!
 * Initializes SanitizerCoverage run-time data gathering
 * (note: initializes to a disabled state).
 */
extern void sanitizerCovInit(void);

/*!
 * Retrieves the status of SanitizerCoverage run-time data gathering,
 * such as whether it is enabled or not.
 *
 * @param[out]     pStatus->used      number of elements used in buffer
 * @param[out]     pStatus->missed    number of callbacks missed due to
 *                                    a full buffer
 * @param[out]     pStatus->bEnabled  whether run-time data gathering is
 *                                    enabled (LW_TRUE) or not (LW_FALSE)
 *
 * @return         FLCN_OK                    on success
 * @return         FLCN_ERR_ILWALID_ARGUMENT  on invalid argument/pointer
 */
extern FLCN_STATUS sanitizerCovGetStatus(SANITIZER_COV_STATUS *pStatus);

/*!
 * Adjusts the status of SanitizerCoverage run-time data gathering,
 * such as whether it is enabled or not.
 *
 * @param[in]      pStatus->used      set to 0 to forcibly empty the buffer
 *                                    (all other values ignored)
 * @param[in]      pStatus->missed    set to 0 to reset the counter of callbacks
 *                                    missed (all other values ignored)
 * @param[in]      pStatus->bEnabled  whether to enable (LW_TRUE) or disable
 *                                    (LW_FALSE) run-time data gathering
 *
 * @return         FLCN_OK                    on success
 * @return         FLCN_ERR_ILWALID_ARGUMENT  on invalid argument/pointer
 */
extern FLCN_STATUS sanitizerCovSetStatus(SANITIZER_COV_STATUS *pStatus);

/*!
 * Copies SanitizerCoverage data from the internal designated DMEM buffer
 * out to the specified buffer of QWORDs.
 *
 * @param[in,out]  pDest         destination buffer to copy data to
 * @param[in,out]  pNumElements  number of elements to copy
 *                               (and actually copied)
 * @param[out]     pDone         whether all data was copied (LW_TRUE) or
 *                               if there is more data available to be copied
 *
 * @return         FLCN_OK                    on success
 * @return         FLCN_ERR_ILWALID_ARGUMENT  on invalid argument/pointer
 * @return         FLCN_ERR_ILWALID_STATE     on invalid internal state
 */
extern FLCN_STATUS sanitizerCovCopyData(LwU64 *pDest, LwU32 *pNumElements, LwBool *pDone);

/*!
 * Copies SanitizerCoverage data from the internal designated DMEM buffer
 * out to the specified buffer of BYTEs (representing QWORDs in little-endian).
 * 
 * This function is identitical to sanitizerCovCopyData except it takes
 * pDest as a buffer of LwU8 instead of LwU64 (for type colwenience).
 * 
 * pDest must have capacity (in bytes) of 8 times the number of requested
 * elements.
 *
 * @param[in,out]  pDest         destination buffer to copy data to
 * @param[in,out]  pNumElements  number of elements to copy
 *                               (and actually copied)
 * @param[out]     pDone         whether all data was copied (LW_TRUE) or
 *                               if there is more data available to be copied
 *
 * @return         FLCN_OK                    on success
 * @return         FLCN_ERR_ILWALID_ARGUMENT  on invalid argument/pointer
 * @return         FLCN_ERR_ILWALID_STATE     on invalid internal state
 */
extern FLCN_STATUS sanitizerCovCopyDataAsBytes(LwU8 *pDest, LwU32 *pNumElements, LwBool *pDone);

/* ------------------------ Prototypes -------------------------------- */
void sanitizerCovInit(void)
    GCC_ATTRIB_SECTION("imem_resident", "sanitizerCovInit");
FLCN_STATUS sanitizerCovGetStatus(SANITIZER_COV_STATUS *pStatus)
    GCC_ATTRIB_SECTION("imem_resident", "sanitizerCovGetStatus");
FLCN_STATUS sanitizerCovSetStatus(SANITIZER_COV_STATUS *pStatus)
    GCC_ATTRIB_SECTION("imem_resident", "sanitizerCovSetStatus");
FLCN_STATUS sanitizerCovCopyData(LwU64 *pDest, LwU32 *pNumElements, LwBool *pDone)
    GCC_ATTRIB_SECTION("imem_resident", "sanitizerCovCopyData");
FLCN_STATUS sanitizerCovCopyDataAsBytes(LwU8 *pDest, LwU32 *pNumElements, LwBool *pDone)
    GCC_ATTRIB_SECTION("imem_resident", "sanitizerCovCopyDataAsBytes");

void __sanitizer_cov_trace_pc(void)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_pc");

void __sanitizer_cov_trace_cmp1(LwU8 Arg1, LwU8 Arg2)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_cmp1");
void __sanitizer_cov_trace_cmp2(LwU16 Arg1, LwU16 Arg2)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_cmp2");
void __sanitizer_cov_trace_cmp4(LwU32 Arg1, LwU32 Arg2)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_cmp4");
void __sanitizer_cov_trace_cmp8(LwU64 Arg1, LwU64 Arg2)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_cmp8");

void __sanitizer_cov_trace_const_cmp1(LwU8 Arg1, LwU8 Arg2)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_const_cmp1");
void __sanitizer_cov_trace_const_cmp2(LwU16 Arg1, LwU16 Arg2)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_const_cmp2");
void __sanitizer_cov_trace_const_cmp4(LwU32 Arg1, LwU32 Arg2)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_const_cmp4");
void __sanitizer_cov_trace_const_cmp8(LwU64 Arg1, LwU64 Arg2)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_const_cmp8");

void __sanitizer_cov_trace_cmpf(LwF32 Arg1, LwF32 Arg2)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_cmpf");
void __sanitizer_cov_trace_cmpd(LwF64 Arg1, LwF64 Arg2)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_cmpd");

void __sanitizer_cov_trace_switch(LwU64 Val, LwU64* Cases)
    GCC_ATTRIB_SECTION("imem_resident", "__sanitizer_cov_trace_switch");

#endif // LIB_SANITIZERCOV_H
