/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOOT_H
#define BOOT_H

#include "lwuproc.h"
#include "rmsoecmdif.h"
#include "dev_soe_csb.h"

// REV definitions
// HS REV increments each time HS is re-signed.
#define LWL_REV_UCODE_HS   1

// Signature related defines
// FALCON_ID below comes from <branch>/drivers/resman/arch/lwalloc/common/inc/rmlsfm.h
#define SCP_SIG_SIZE_IN_BYTES      16
#define LS_VERIF_KEY_INDEX_DEBUG   0
#define LS_VERIF_KEY_INDEX_PROD    43
#define LS_FALCON_ID               14

#define SCP_REG_SIZE_IN_DWORDS       0x00000004
#define SCP_ADDR_ALIGNMENT_IN_BYTES 0x00000010


// Memory type defines
#define MEMTYPE_DMEM               0
#define MEMTYPE_IMEM               1
#define IS_IMEM(a) (a == MEMTYPE_IMEM)

// Falcon helper macros
#define LWL_FALCON_PC_TRIM(pc)   (pc  & 0x0FFFFFFF)

// Destructive operation on n32
// Implementation copied from chips_a/sdk/lwpu/inc/lwmisc.h
#define HIGHESTBITIDX_32(n32)   \
{                               \
    LwU32 count = 0;            \
    while (n32 >>= 1)           \
    {                           \
        count++;                \
    }                           \
    n32 = count;                \
}

/*!
 * @brief Write status into MAILBOX1
 * This function is shared by NS and HS Size code, and hence should always be inline.
 */
static inline void
soeWriteStatusToFalconMailbox(SOE_STATUS status)
{
    REG_WR32_STALL(CSB, SOE_STATUS_SCRATCH_REGISTER, status);
}
#endif // BOOT_H
