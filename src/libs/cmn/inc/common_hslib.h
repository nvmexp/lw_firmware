/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef COMMON_HSLIB_H
#define COMMON_HSLIB_H


/*!
 * @file common_hslib.h
 * This file holds inline definitions of common functions which can be used
 * in HS libs
 */

/* ------------------------- System includes -------------------------------- */
#include "flcnretval.h"

/* ------------------------- Application includes --------------------------- */
/* ------------------------- Types definitions ------------------------------ */
/* ------------------------- External definitions --------------------------- */
/* ------------------------- Static variables ------------------------------- */
/* ------------------------- Defines ---------------------------------------- */

#if SEC2_CSB_ACCESS
#include "dev_sec_csb_addendum.h"
#include "dev_sec_csb.h"
#define VALIDATE_THAT_CALLER_IS_HS()    _checkIfLibCallerIsHs()
#define REG_HS_FLAG                     LW_CSEC_SCRATCH0
#define REG_FLCN_MAILBOX                LW_CSEC_FALCON_MAILBOX0
#define SCRATCH0_FLD_SET_DRF(c,v) FLD_SET_DRF(_CSEC, _SCRATCH0, _HS_LIB_CALLER, c, v)
#define SCRATCH0_FLD_TEST_DRF(c,v) FLD_TEST_DRF(_CSEC, _SCRATCH0, _HS_LIB_CALLER, c, v)

#elif GSPLITE_CSB_ACCESS
#include "dev_gsp_csb_addendum.h"
#include "dev_gsp_csb.h"
#define VALIDATE_THAT_CALLER_IS_HS()    _checkIfLibCallerIsHs()
#define REG_HS_FLAG                     LW_CGSP_SCRATCH0
#define REG_FLCN_MAILBOX                LW_CGSP_FALCON_MAILBOX0
#define SCRATCH0_FLD_SET_DRF(c,v) FLD_SET_DRF(_CGSP, _SCRATCH0, _HS_LIB_CALLER, c, v)
#define SCRATCH0_FLD_TEST_DRF(c,v) FLD_TEST_DRF(_CGSP, _SCRATCH0, _HS_LIB_CALLER, c, v)

#else 
//
// These defines are empty for non SEC2/GSP case
// Eventually this has to be implemented in all ucodes which use HS_libs.
//
#define VALIDATE_THAT_CALLER_IS_HS()
#define preHsLibEntryPointJump()
#define postHsLibEntryPointJump()
#endif

/* ------------------------- Function Prototypes ---------------------------- */
#if SEC2_CSB_ACCESS || GSPLITE_CSB_ACCESS
static inline void _checkIfLibCallerIsHs(void)
    GCC_ATTRIB_ALWAYSINLINE();

static inline void preHsLibEntryPointJump(void)
    GCC_ATTRIB_ALWAYSINLINE();

static inline void postHsLibEntryPointJump(void)
    GCC_ATTRIB_ALWAYSINLINE();

/*!
 * @brief  Ensure that the caller of HS lib entry function is HS overlay, halt otherwise
 */
static inline void
_checkIfLibCallerIsHs(void)
{
    LwU32 reg = REG_RD32(CSB, REG_HS_FLAG);

    //
    // SCRATCH0 register can be written only by HS, read by all levels.
    // Only HS caller can and needs to set HS_LIB_CALLER_HS before calling
    // lib HS entry function.
    // Halt if caller is not HS
    //
    if(!SCRATCH0_FLD_TEST_DRF(_HS, reg))
    {
        REG_WR32(CSB, REG_FLCN_MAILBOX, FLCN_ERR_HS_CHK_HS_LIB_ENTRY_CALLED_BY_NON_HS);
        lwuc_halt();
    }

    // Clear HS flag register for reuse
    REG_WR32(CSB, REG_HS_FLAG, SCRATCH0_FLD_SET_DRF(_NOT_HS, reg));

    // Error Handling
    reg = REG_RD32(CSB, REG_HS_FLAG);
    if (!SCRATCH0_FLD_TEST_DRF(_NOT_HS, reg))
    {
        REG_WR32(CSB, REG_FLCN_MAILBOX, FLCN_ERR_HS_REGISTER_READ_WRITE_ERROR);
        lwuc_halt();
    }
}

/*!
 * @brief  This function needs to be called before HS lib entryFn is called.
 *         It sets flag to indicate that caller is HS.
 */
static inline void
preHsLibEntryPointJump(void)
{
    LwU32 reg = REG_RD32(CSB, REG_HS_FLAG);

    //
    // Set HS flag to indicate lib HS entryFn that caller is HS
    // Only HS can write SCRATCH0 register
    //
    REG_WR32(CSB, REG_HS_FLAG, SCRATCH0_FLD_SET_DRF(_HS, reg));

    // Error Handling
    reg = REG_RD32(CSB, REG_HS_FLAG);
    if (!SCRATCH0_FLD_TEST_DRF(_HS, reg))
    {
        REG_WR32(CSB, REG_FLCN_MAILBOX, FLCN_ERR_HS_REGISTER_READ_WRITE_ERROR);
        lwuc_halt();
    }
}

/*!
 * @brief  This function needs to be called immediately after HS lib entryFn.
 *         It checks that HS flag is cleared.
 */
static inline void
postHsLibEntryPointJump(void)
{
    //
    // Lib HS entryFn should clear the HS flag
    // If it is still set, it means the called HS lib is missing check for HS caller
    //
    if (SCRATCH0_FLD_TEST_DRF(_HS, REG_RD32(CSB, REG_HS_FLAG)))
    {
        lwuc_halt();
    }
}

#endif // SEC2_CSB_ACCESS, GSPLITE_CSB_ACCESS

#endif // COMMON_HSLIB_H
