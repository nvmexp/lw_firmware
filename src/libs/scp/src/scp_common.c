/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   scp_common.c
 * @brief  Common SCP functionality
 *
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "scp_internals_common.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc scpRegistersClearAll
 */
void scpRegistersClearAll(void)
{
    lwuc_scp_xor(SCP_R0, SCP_R0);
    lwuc_scp_xor(SCP_R1, SCP_R1);
    lwuc_scp_xor(SCP_R2, SCP_R2);
    lwuc_scp_xor(SCP_R3, SCP_R3);
    lwuc_scp_xor(SCP_R4, SCP_R4);
    lwuc_scp_xor(SCP_R5, SCP_R5);
    lwuc_scp_xor(SCP_R6, SCP_R6);
    lwuc_scp_xor(SCP_R7, SCP_R7);
}

#ifdef UPROC_RISCV
// MMINTS-TODO: somehow make this function generic or move it into a separate file!

/*!
 * @copydoc scpDmaPollIsIdle
 */
LwBool
scpDmaPollIsIdle(void *pArgs)
{
    (void)pArgs;

    LwU32 rdVal = bar0Read(RV_SCP_REG(_SCPDMAPOLL));
    return (DRF_VAL_RISCV(DMAPOLL, _DMA_ACTIVE, rdVal) == DRF_DEF_RISCV(DMAPOLL, _DMA_ACTIVE, _IDLE));
}
#endif // UPROC_RISCV
