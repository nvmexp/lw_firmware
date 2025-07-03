/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_FETCH_H
#define PLM_WPR_FETCH_H

/*!
 * @file        fetch.h
 * @brief       Instruction-fetch helper.
 */

//
// Value returned by fetchAddress() (assuming target code returns), split into
// high (20-bit) and low (12-bit) parts for easier assembly use. Note that the
// combined value of 0x00008067 is equivalent to the RISC-V machine code for
// "ret" pseudo-instruction.
//
#define FETCH_RET_CODE_HI    0x00008
#define FETCH_RET_CODE_LO    0x067
#define FETCH_RET_CODE_SHIFT 12U

// Combined value for use by C code.
#define FETCH_RET_CODE ((FETCH_RET_CODE_HI << FETCH_RET_CODE_SHIFT) |      \
    (FETCH_RET_CODE_LO & ((1U << FETCH_RET_CODE_SHIFT) - 1U)))


// Below portions to be excluded when compiling assembly.
#ifndef AS_ILWOKED

// Standard includes.
#include <stdint.h>


/*!
 * @brief Attempts to jump to the specified target address.
 *
 * @param[in] addr  The target address to jump to.
 *
 * @return
 *    FETCH_RET_CODE.
 *
 * @note Will not return unless the code at the target location also returns.
 */
uint32_t fetchAddress(uintptr_t addr);

#endif // AS_ILWOKED

#endif // PLM_WPR_FETCH_H
