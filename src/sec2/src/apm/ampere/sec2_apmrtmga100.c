/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_apmrtmga100.c
 * @brief  The hooks to measure dynamic state of the GPU
 */

/* ------------------------- System Includes -------------------------------- */
#include "sec2sw.h"
#include "dev_fb.h"
#include "dev_pri_ringstation_sys.h"

/* ------------------------- Application Includes --------------------------- */
#include "sec2_objapm.h"
#include "lib_intfcshahw.h"
#include "apm_rts.h"
#include "sw_rts_msr_usage.h"

/**
 * @brief Captures and measures Vpr state and stores the measurement in RTS.
 * 
 * @param apmRtsOffset[in] The offset of RTS in FB
 *
 * @return FLCN_STATUS FLCN_OK if successful, relevant error otherwise.
 */
FLCN_STATUS
apmCaptureVprMmuState_GA100
(
    LwU64  apmRtsOffset
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       newMeasurement[APM_HASH_SIZE_WORDS] = { 0 };
    LwU32       idx                                 =   0;

    newMeasurement[idx++] = BAR0_REG_RD32(LW_PFB_PRI_MMU_VPR_CYA_LO);
    newMeasurement[idx++] = BAR0_REG_RD32(LW_PFB_PRI_MMU_VPR_CYA_HI);
    newMeasurement[idx++] = 0;
    newMeasurement[idx++] = 0;
    newMeasurement[idx++] = 0;
    newMeasurement[idx++] = 0;
    newMeasurement[idx++] = 0;
    newMeasurement[idx++] = 0;

    CHECK_FLCN_STATUS(apmMsrExtend(apmRtsOffset, APM_RTS_MSR_INDEX_VPR_MMU_STATE, (LwU8 *) newMeasurement, LW_TRUE));

ErrorExit:
    return flcnStatus;
}

/**
 * @brief Captures and measures decode traps and stores the measurement in RTS.
 *
 * @param apmRtsOffset[in] The offset of RTS in FB
 * 
 * @return FLCN_STATUS FLCN_OK if successfull, relevant error otherwise.
 */
FLCN_STATUS
apmCaptureDecodeTrapState_GA100
(
    LwU64  apmRtsOffset
)
{
    FLCN_STATUS  flcnStatus                          = FLCN_OK;
    LwU32        newMeasurement[APM_HASH_SIZE_WORDS] = { 0 };
    LwU32        idx                                 =   0;
    LwU32        decodeTrapCount                     = LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH__SIZE_1;

    //
    // Every decode trap consists of six registers
    // Loop over every trap and measure all six registers at once
    //
    for (idx = 0; idx < decodeTrapCount; idx++)
    {
        LwU32 idy = 0;
        memset(newMeasurement, sizeof(newMeasurement), 0x00);

        newMeasurement[idy++] = BAR0_REG_RD32(LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH(idx));
        newMeasurement[idy++] = BAR0_REG_RD32(LW_PPRIV_SYS_PRI_DECODE_TRAP_MATCH_CFG(idx));
        newMeasurement[idy++] = BAR0_REG_RD32(LW_PPRIV_SYS_PRI_DECODE_TRAP_MASK(idx));
        newMeasurement[idy++] = BAR0_REG_RD32(LW_PPRIV_SYS_PRI_DECODE_TRAP_DATA1(idx));
        newMeasurement[idy++] = BAR0_REG_RD32(LW_PPRIV_SYS_PRI_DECODE_TRAP_DATA2(idx));
        newMeasurement[idy++] = BAR0_REG_RD32(LW_PPRIV_SYS_PRI_DECODE_TRAP_ACTION(idx));
        newMeasurement[idy++] = 0;
        newMeasurement[idy++] = 0;

        if (idx == 0)
        {
            CHECK_FLCN_STATUS(apmMsrExtend(apmRtsOffset, APM_RTS_MSR_INDEX_DECODE_TRAPS, (LwU8 *) newMeasurement, LW_TRUE));
        }
        else
        {
            CHECK_FLCN_STATUS(apmMsrExtend(apmRtsOffset, APM_RTS_MSR_INDEX_DECODE_TRAPS, (LwU8 *) newMeasurement, LW_FALSE));
        }
    }

ErrorExit:
    return flcnStatus;
}