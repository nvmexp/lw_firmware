/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_SCP_COMMON_H
#define LIBLWRISCV_SCP_COMMON_H

/*!
 * @file        scp_common.h
 * @brief       Common SCP types and constants.
 *
 * @details     Contains all symbols not otherwise belonging to any specific
 *              subcomponent of the SCP driver.
 *
 * @note        Client applications should include scp.h instead of this file.
 */

//
// The minimum required alignment for input and output buffers passed to
// SCP interfaces, in bytes. Note that this alignment only applies to buffer
// arguments and not to other interface parameters.
//
#define SCP_BUFFER_ALIGNMENT 16


// A collection of enumerated values for referring to specific SCP GPRs.
typedef enum SCP_REGISTER_INDEX
{
    // Long-form versions to match naming scheme.

    SCP_REGISTER_INDEX_0 = 0,
    SCP_REGISTER_INDEX_1,
    SCP_REGISTER_INDEX_2,
    SCP_REGISTER_INDEX_3,
    SCP_REGISTER_INDEX_4,
    SCP_REGISTER_INDEX_5,
    SCP_REGISTER_INDEX_6,
    SCP_REGISTER_INDEX_7,
    SCP_REGISTER_INDEX_COUNT,

    // Short-form versions for brevity.

    SCP_R0 = 0,
    SCP_R1,
    SCP_R2,
    SCP_R3,
    SCP_R4,
    SCP_R5,
    SCP_R6,
    SCP_R7,
    SCP_RCOUNT,

} SCP_REGISTER_INDEX;

#endif // LIBLWRISCV_SCP_COMMON_H
