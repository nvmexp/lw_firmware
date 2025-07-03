/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SCP_INTERNALS_COMMON_H
#define SCP_INTERNALS_COMMON_H

#include "cpu-intrinsics.h"

#if defined(UPROC_RISCV)
    #include "riscv_scp_internals.h"
#elif defined(UPROC_FALCON)
    #include "scp_internals.h"
#else // defined(UPROC_RISCV)
    #error Unsupported CPU architecture!
#endif // defined(UPROC_RISCV)

/*!
 * @defgroup    ScpIntrinsics
 *
 * Provide intrinsics for interacting with the secure co-processor.
 *
 * @note    Instructions here are common to both Falcon and RISC-V.
 *
 * @note    These are not all instructions common to both architectures. This
 *          list has been created on-demand so far.
 *
 * @{
 */
#define lwuc_scp_xor    INSN_NAME(scp_xor)
/*!@}*/

#endif // SCP_INTERNALS_COMMON_H
