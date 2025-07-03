/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_register_access_tu10x.c
 */
// Includes
//
#include "acr.h"
#include "acrdrfmacros.h"
//#include "acr_objacr.h"
#include "dev_se_seb.h"
#include "dev_se_seb_addendum.h"
#include "dev_bus.h"
#include "dev_pwr_pri.h"
#include "dev_falcon_v4.h"
#include "dev_gsp.h"
#include "dev_fb.h"
#include "dev_master.h"
#include "dev_fbfalcon_pri.h"
#include "dev_sec_pri.h"
#include "dev_pwr_csb.h"
#include "dev_graphics_nobundle.h"
#include "dev_fbif_v4.h"
#include "sec2/sec2ifcmn.h"
#include "dpu/dpuifcmn.h"

#ifdef ACR_RISCV_LS
#include "dev_riscv_pri.h"
#endif

#ifdef LWDEC_PRESENT
#include "dev_lwdec_pri.h"
#endif

#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
#include "dev_sec_csb.h"
#endif

#ifdef ACR_BUILD_ONLY
#include "config/g_acrlib_private.h"
#include "config/g_acr_private.h"
#else
#include "g_acrlib_private.h"
#endif


static void _acrlibInitRegMapping_TU10X(ACR_REG_MAPPING acrRegMap[REG_LABEL_END])   ATTR_OVLY(OVL_NAME);

//
// This is added to fix SEC2 and PMU preTuring and Ampere_and_later ACRLIB builds
// TODO: Get this define added in Ampere_and_later manuals
// HW bug tracking priv latency improvement - 2117032
// Current value of this timeout is taken from http://lwbugs/2198976/285
//
#ifndef LW_CSEC_BAR0_TMOUT_CYCLES_PROD
#define LW_CSEC_BAR0_TMOUT_CYCLES_PROD    (0x01312d00)
#endif //LW_CSEC_BAR0_TMOUT_CYCLES_PROD

#ifdef DMEM_APERT_ENABLED
/*!
 * Function to read register value through External Interface when DMEM Apertue is enabled
 * and halt in case of Priv Error.
 */
LwU32
acrlibBar0RegReadDmemApert_TU10X
(
    LwU32 addr
)
{
    LwU32 value = 0;
    LwU32 extErrStatVal = 0;

    value = (*((const volatile LwU32 *)((addr) | DMEM_APERTURE_OFFSET )));

    // Doing CSB read to get the Status, since BAR0 read has higher chances of failure.
    extErrStatVal = falc_ldxb_i_with_halt(LW_CPWR_FALCON_EXTERRSTAT);

    if (!FLD_TEST_DRF(_CPWR, _FALCON_EXTERRSTAT, _STAT, _ACK_POS, extErrStatVal) ||
        ((value & EXT_INTERFACE_MASK_VALUE) == EXT_INTERFACE_BAD_VALUE))
    {
        FAIL_ACR_WITH_ERROR(ACR_ERROR_FLCN_REG_ACCESS);
    }

    return value;
}

/*!
 * Funtion to write register value through External Interface when DMEM Apertue is enabled
 * and halt in case of Priv Error.
 */
void
acrlibBar0RegWriteDmemApert_TU10X
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 extErrStatVal = 0;

    *((volatile LwU32 *) ((LwU32) (addr) | DMEM_APERTURE_OFFSET )) = (data);

    // Doing CSB read to get the Status, since BAR0 read has higher chances of failure.
    extErrStatVal = falc_ldxb_i_with_halt(LW_CPWR_FALCON_EXTERRSTAT);

    if (!FLD_TEST_DRF(_CPWR, _FALCON_EXTERRSTAT, _STAT, _ACK_POS, extErrStatVal))
    {
        FAIL_ACR_WITH_ERROR(ACR_ERROR_FLCN_REG_ACCESS);
    }
}
#else
/*!
 * @brief Wait for BAR0 to become idle
 */
void
acrlibBar0WaitIdle_TU10X(void)
{
    LwBool bDone = LW_FALSE;

    // As this is an infinite loop, timeout should be considered on using bewlo loop elsewhere.
    do
    {
        switch (DRF_VAL(_CSEC, _BAR0_CSR, _STATUS,
                    ACR_REG_RD32(CSB, LW_CSEC_BAR0_CSR)))
        {
            case LW_CSEC_BAR0_CSR_STATUS_IDLE:
                bDone = LW_TRUE;
                break;

            case LW_CSEC_BAR0_CSR_STATUS_BUSY:
                break;

            // unexpected error conditions
            case LW_CSEC_BAR0_CSR_STATUS_ERR:
            case LW_CSEC_BAR0_CSR_STATUS_TMOUT:
            case LW_CSEC_BAR0_CSR_STATUS_DIS:
            default:
                FAIL_ACR_WITH_ERROR(ACR_ERROR_FLCN_REG_ACCESS);
        }
    }
    while (!bDone);

}

LwU32
acrlibBar0RegRead_TU10X
(
    LwU32 addr
)
{
    LwU32  val;

    acrlibBar0WaitIdle_HAL(pAcrlib);
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_ADDR, addr);

    ACR_REG_WR32_STALL(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE));

    (void)ACR_REG_RD32(CSB, LW_CSEC_BAR0_CSR);

    acrlibBar0WaitIdle_HAL(pAcrlib);
    val = ACR_REG_RD32(CSB, LW_CSEC_BAR0_DATA);
    return val;
}

void
acrlibBar0RegWrite_TU10X
(
    LwU32 addr,
    LwU32 data
)
{
    acrlibBar0WaitIdle_HAL(pAcrlib);
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_ADDR, addr);
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_DATA, data);

    ACR_REG_WR32(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _WRITE) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE ));

    (void)ACR_REG_RD32(CSB, LW_CSEC_BAR0_CSR);
    acrlibBar0WaitIdle_HAL(pAcrlib);

}
#endif

/*!
 * @brief Find reg mapping using reg label
 *
 */
ACR_STATUS
acrlibFindRegMapping_TU10X
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    ACR_FLCN_REG_LABEL  acrLabel,
    PACR_REG_MAPPING    pMap,
    PFLCN_REG_TGT       pTgt
)
{
    ACR_REG_MAPPING acrRegMap[REG_LABEL_END];
    if ((acrLabel >= REG_LABEL_END) || (!pMap) || (!pTgt) ||
        (acrLabel == REG_LABEL_FLCN_END) ||
#ifdef ACR_RISCV_LS
        (acrLabel == REG_LABEL_RISCV_END) ||
#endif
        (acrLabel == REG_LABEL_FBIF_END))
        return ACR_ERROR_REG_MAPPING;

    if (pFlcnCfg->bIsBoOwner)
    {
        *pTgt = CSB_FLCN;
    }
    else if (acrLabel < REG_LABEL_FLCN_END)
    {
        *pTgt = BAR0_FLCN;
    }
#ifdef ACR_RISCV_LS
    else if (acrLabel < REG_LABEL_RISCV_END)
    {
        if (pFlcnCfg->uprocType != ACR_TARGET_ENGINE_CORE_RISCV &&
            pFlcnCfg->uprocType != ACR_TARGET_ENGINE_CORE_RISCV_EB)
        {
            return ACR_ERROR_REG_MAPPING;
        }

        *pTgt = BAR0_RISCV;
    }
#endif
    else
    {
        *pTgt = BAR0_FBIF;
    }

    _acrlibInitRegMapping_TU10X(acrRegMap);

    pMap->bar0Off   = acrRegMap[acrLabel].bar0Off;
    pMap->pwrCsbOff = acrRegMap[acrLabel].pwrCsbOff;

    return ACR_OK;
}

/*!
 * @brief Acr util function to write falcon registers using register label
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
void
acrlibFlcnRegLabelWrite_TU10X
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    ACR_FLCN_REG_LABEL reglabel,
    LwU32              data
)
{
    ACR_REG_MAPPING regMapping = {0};
    FLCN_REG_TGT     tgt        = ILWALID_TGT;

    acrlibFindRegMapping_HAL(pAcrlib, pFlcnCfg, reglabel, &regMapping, &tgt);

    switch(tgt)
    {
        case CSB_FLCN:
            return acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, tgt, regMapping.pwrCsbOff, data);
        case BAR0_FLCN:
        case BAR0_FBIF:
#ifdef ACR_RISCV_LS
        case BAR0_RISCV:
#endif
            return acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, tgt, regMapping.bar0Off, data);
        default:
            // Find better way to propogate error
            FAIL_ACR_WITH_ERROR(ACR_ERROR_REG_MAPPING);
    }
}

/*!
 * @brief Acr util function to read falcon registers
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
LwU32
acrlibFlcnRegRead_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg,
    FLCN_REG_TGT     tgt,
    LwU32            regOff
)
{
    if (pFlcnCfg && (tgt == BAR0_FLCN))
    {
        return ACR_REG_RD32(BAR0, ((pFlcnCfg->registerBase) + regOff));
    }
    else if (tgt == CSB_FLCN)
    {
        return ACR_REG_RD32_STALL(CSB, regOff);
    }
    else if (pFlcnCfg && (tgt == BAR0_FBIF))
    {
        return ACR_REG_RD32(BAR0, ((pFlcnCfg->fbifBase) + regOff));
    }
    else if (pFlcnCfg && (tgt == BAR0_RISCV))
    {
        return ACR_REG_RD32(BAR0, ((pFlcnCfg->riscvRegisterBase) + regOff));
    }
    else
    {
        FAIL_ACR_WITH_ERROR(ACR_ERROR_ILWALID_ARGUMENT);    // This halt needs to be revisited to propagate error to caller
    }
    return 0;
}

/*!
 * @brief Acr util function to write falcon registers
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
void
acrlibFlcnRegWrite_TU10X
(
    PACR_FLCN_CONFIG pFlcnCfg,
    FLCN_REG_TGT     tgt,
    LwU32            regOff,
    LwU32            data
)
{
    if (pFlcnCfg && (tgt == BAR0_FLCN))
    {
        ACR_REG_WR32(BAR0, ((pFlcnCfg->registerBase) + regOff), data);
    }
    else if (tgt == CSB_FLCN)
    {
        ACR_REG_WR32_STALL(CSB, regOff, data);
    }
    else if (pFlcnCfg && (tgt == BAR0_FBIF))
    {
        ACR_REG_WR32(BAR0, ((pFlcnCfg->fbifBase) + regOff), data);
    }
    else if (pFlcnCfg && (tgt == BAR0_RISCV))
    {
        ACR_REG_WR32(BAR0, ((pFlcnCfg->riscvRegisterBase) + regOff), data);
    }
    else
    {
        FAIL_ACR_WITH_ERROR(ACR_ERROR_ILWALID_ARGUMENT);
    }
}


/*!
 * @brief Acr util function to read falcon registers using register label
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  pIsFbif   TRUE if register is offset from FBIF base
 * @param[in]  regOff    Register offset
 */
LwU32
acrlibFlcnRegLabelRead_TU10X
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    ACR_FLCN_REG_LABEL reglabel
)
{
    ACR_REG_MAPPING regMapping = {0};
    FLCN_REG_TGT    tgt        = ILWALID_TGT;

    acrlibFindRegMapping_HAL(pAcrlib, pFlcnCfg, reglabel, &regMapping, &tgt);
    switch(tgt)
    {
        case CSB_FLCN:
            return acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, tgt, regMapping.pwrCsbOff);
        case BAR0_FLCN:
        case BAR0_FBIF:
#ifdef ACR_RISCV_LS
        case BAR0_RISCV:
#endif
            return acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, tgt, regMapping.bar0Off);
        default:
            // Find better way to propogate error
            FAIL_ACR_WITH_ERROR(ACR_ERROR_REG_MAPPING);
    }

    // Control will not reach here since falcon halts in default case.
    return 0xffffffff;
}

/*!
 * Wrapper function for blocking falcon read instruction to halt in case of Priv Error.
 */
LwU32
falc_ldxb_i_with_halt
(
    LwU32 addr
)
{
    LwU32   val           = 0;
    LwU32   csbErrStatVal = 0;

    val = falc_ldxb_i ((LwU32*)(addr), 0);

#if defined(ACR_UNLOAD)
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CPWR_FALCON_CSBERRSTAT), 0);
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);
#else
    ct_assert(0);
#endif

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        // If CSBERRSTAT is true, halt no matter what register was read.
        FAIL_ACR_WITH_ERROR(ACR_ERROR_FLCN_REG_ACCESS);
    }
    else if ((val & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE)
    {

#ifdef ACR_BUILD_ONLY
        if (acrIgnoreShaResultRegForBadValueCheck_HAL(pAcr, addr))
        {
            return val;
        }
#endif

        //
        // If we get here, we thought we read a bad value. However, there are
        // certain cases where the values may lead us to think we read something
        // bad, but in fact, they were just valid values that the register could
        // report. For example, the host ptimer PTIMER1 and PTIMER0 registers
        // can return 0xbadfxxxx value that we interpret to be "bad".
        //

#ifndef BSI_LOCK // Exclude due to BSI space constraints

#if defined(ACR_UNLOAD)
        if (addr == LW_CPWR_FALCON_PTIMER1)
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
        if (addr == LW_CSEC_FALCON_PTIMER1)
#else
        ct_assert(0);
#endif
        {
            //
            // PTIMER1 can report a constant 0xbadfxxxx for a long time, so
            // just make an exception for it.
            //
            return val;
        }
#if defined(ACR_UNLOAD)
        else if (addr == LW_CPWR_FALCON_PTIMER0)
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
        else if (addr == LW_CSEC_FALCON_PTIMER0)
#else
        ct_assert(0);
#endif
        {
            //
            // PTIMER0 can report a 0xbadfxxxx value for up to 2^16 ns, but it
            // should keep changing its value every nanosecond. We just check
            // to make sure we can detect its value changing by reading it a
            // few times.
            //
            LwU32   val1      = 0;
            LwU32   i         = 0;

            for (i = 0; i < 3; i++)
            {
                val1 = falc_ldx_i ((LwU32 *)(addr), 0);
                if (val1 != val)
                {
                    // Timer0 is progressing, so return latest value, i.e. val1.
                    return val1;
                }
            }
        }
#endif // #ifndef BSI_LOCK

        //
        // While reading SE_TRNG, the random number generator can return a badfxxxx value
        // which is actually a random number provided by TRNG unit. Since the output of this
        // SE_TRNG registers comes in FALCON_DIC_D0, we need to skip PEH for this FALCON_DIC_D0
        // This in turn reduces our PEH coverage for all SE registers since all SE registers are read from
        // FALCON_DIC_D0 register.
        // Skipping 0xBADFxxxx check here on FALCON_DIC_D0 is fine, this check is being done in the
        // upper level parent function acrSelwreBusGetData_HAL and explicitly discounting only the
        // SE TRNG registers from 0xBADFxxxx check.
        // More details in Bug 200326572
        // Also, the ifdef on LW_CPWR_FALCON_DIC_D0 and LW_CSEC_FALCON_DIC_D0 is required since
        // these registers are not defined for GM20X (this file is also built for GM20X).
        // We should make this function a HAL and to fork separate and appropriate implementation for GM20X/GP10X/GV100+
        //
#if defined(ACR_UNLOAD)
#ifdef LW_CPWR_FALCON_DIC_D0
        if (addr == LW_CPWR_FALCON_DIC_D0)
        {
            return val;
        }
#endif
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
#ifdef LW_CSEC_FALCON_DIC_D0
        if (addr == LW_CSEC_FALCON_DIC_D0)
        {
            return val;
        }
#endif
#else
        ct_assert(0);
#endif

        FAIL_ACR_WITH_ERROR(ACR_ERROR_FLCN_REG_ACCESS);
    }

    return val;
}

/*!
 * Wrapper function for blocking falcon write instruction to halt in case of Priv Error.
 */
void
falc_stxb_i_with_halt
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 csbErrStatVal = 0;

    falc_stxb_i ((LwU32*)(addr), 0, data);

#if defined(ACR_UNLOAD)
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CPWR_FALCON_CSBERRSTAT), 0);
#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
    csbErrStatVal = falc_ldxb_i ((LwU32*)(LW_CSEC_FALCON_CSBERRSTAT), 0);
#else
    ct_assert(0);
#endif

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        FAIL_ACR_WITH_ERROR(ACR_ERROR_FLCN_REG_ACCESS);
    }
}

/*!
 * @brief: initializes ACR Register map
 *
 * @param[out] acrRegMap    Table to place register mappings into.
 */
static void
_acrlibInitRegMapping_TU10X(ACR_REG_MAPPING acrRegMap[REG_LABEL_END])
{
#ifdef ACR_UNLOAD
    acrRegMap[REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL_PRIV_LEVEL_MASK,           LW_CPWR_FALCON_SCTL_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_SCTL]                       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL,                           LW_CPWR_FALCON_SCTL};
    acrRegMap[REG_LABEL_FLCN_CPUCTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL,                         LW_CPWR_FALCON_CPUCTL};
    acrRegMap[REG_LABEL_FLCN_DMACTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMACTL,                         LW_CPWR_FALCON_DMACTL};
    acrRegMap[REG_LABEL_FLCN_DMATRFBASE]                 = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFBASE,                     LW_CPWR_FALCON_DMATRFBASE};
    acrRegMap[REG_LABEL_FLCN_DMATRFCMD]                  = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFCMD,                      LW_CPWR_FALCON_DMATRFCMD};
    acrRegMap[REG_LABEL_FLCN_DMATRFMOFFS]                = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFMOFFS,                    LW_CPWR_FALCON_DMATRFMOFFS};
    acrRegMap[REG_LABEL_FLCN_DMATRFFBOFFS]               = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFFBOFFS,                   LW_CPWR_FALCON_DMATRFFBOFFS};
    acrRegMap[REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IMEM_PRIV_LEVEL_MASK,           LW_CPWR_FALCON_IMEM_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMEM_PRIV_LEVEL_MASK,           LW_CPWR_FALCON_DMEM_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK]     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL_PRIV_LEVEL_MASK,         LW_CPWR_FALCON_CPUCTL_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK]        = (ACR_REG_MAPPING){LW_PFALCON_FALCON_EXE_PRIV_LEVEL_MASK,            LW_CPWR_FALCON_EXE_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK]     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IRQTMR_PRIV_LEVEL_MASK,         LW_CPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK]    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_MTHDCTX_PRIV_LEVEL_MASK,        LW_CPWR_FALCON_MTHDCTX_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_HWCFG]                      = (ACR_REG_MAPPING){LW_PFALCON_FALCON_HWCFG,                          LW_CPWR_FALCON_HWCFG};
    acrRegMap[REG_LABEL_FLCN_BOOTVEC]                    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_BOOTVEC,                        LW_CPWR_FALCON_BOOTVEC};
    acrRegMap[REG_LABEL_FLCN_DBGCTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DBGCTL,                         LW_CPWR_FALCON_DBGCTL};
    acrRegMap[REG_LABEL_FLCN_SCPCTL]                     = (ACR_REG_MAPPING){0xFFFFFFFF,                                       LW_CPWR_PMU_SCP_CTL_STAT};
    acrRegMap[REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK]  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK,        LW_CPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FBIF_REGIONCFG]                  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG,                        LW_CPWR_FBIF_REGIONCFG};
    acrRegMap[REG_LABEL_FBIF_CTL]                        = (ACR_REG_MAPPING){LW_PFALCON_FBIF_CTL,                              LW_CPWR_FBIF_REGIONCFG};

#elif defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
   // Initialize all mappings
    acrRegMap[REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL_PRIV_LEVEL_MASK,           LW_CSEC_FALCON_SCTL_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_SCTL]                       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL,                           LW_CSEC_FALCON_SCTL};
    acrRegMap[REG_LABEL_FLCN_CPUCTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL,                         LW_CSEC_FALCON_CPUCTL};
    acrRegMap[REG_LABEL_FLCN_DMACTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMACTL,                         LW_CSEC_FALCON_DMACTL};
    acrRegMap[REG_LABEL_FLCN_DMATRFBASE]                 = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFBASE,                     LW_CSEC_FALCON_DMATRFBASE};
    acrRegMap[REG_LABEL_FLCN_DMATRFCMD]                  = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFCMD,                      LW_CSEC_FALCON_DMATRFCMD};
    acrRegMap[REG_LABEL_FLCN_DMATRFMOFFS]                = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFMOFFS,                    LW_CSEC_FALCON_DMATRFMOFFS};
    acrRegMap[REG_LABEL_FLCN_DMATRFFBOFFS]               = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFFBOFFS,                   LW_CSEC_FALCON_DMATRFFBOFFS};
    acrRegMap[REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IMEM_PRIV_LEVEL_MASK,           LW_CSEC_FALCON_IMEM_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK]       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMEM_PRIV_LEVEL_MASK,           LW_CSEC_FALCON_DMEM_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK]     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL_PRIV_LEVEL_MASK,         LW_CSEC_FALCON_CPUCTL_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK]        = (ACR_REG_MAPPING){LW_PFALCON_FALCON_EXE_PRIV_LEVEL_MASK,            LW_CSEC_FALCON_EXE_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK]     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IRQTMR_PRIV_LEVEL_MASK,         LW_CSEC_FALCON_IRQTMR_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK]    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_MTHDCTX_PRIV_LEVEL_MASK,        LW_CSEC_FALCON_MTHDCTX_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_HWCFG]                      = (ACR_REG_MAPPING){LW_PFALCON_FALCON_HWCFG,                          LW_CSEC_FALCON_HWCFG};
    acrRegMap[REG_LABEL_FLCN_BOOTVEC]                    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_BOOTVEC,                        LW_CSEC_FALCON_BOOTVEC};
    acrRegMap[REG_LABEL_FLCN_DBGCTL]                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DBGCTL,                         LW_CSEC_FALCON_DBGCTL};
    acrRegMap[REG_LABEL_FLCN_SCPCTL]                     = (ACR_REG_MAPPING){0xFFFFFFFF,                                       LW_CSEC_SCP_CTL_STAT};
#ifdef NEW_WPR_BLOBS
    acrRegMap[REG_LABEL_FLCN_BOOTVEC_PRIV_LEVEL_MASK]    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_BOOTVEC_PRIV_LEVEL_MASK,        LW_CSEC_FALCON_BOOTVEC_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FLCN_DMA_PRIV_LEVEL_MASK]        = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMA_PRIV_LEVEL_MASK,            LW_CSEC_FALCON_DMA_PRIV_LEVEL_MASK};
#endif
    acrRegMap[REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK]  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK,        LW_CSEC_FBIF_REGIONCFG_PRIV_LEVEL_MASK};
    acrRegMap[REG_LABEL_FBIF_REGIONCFG]                  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG,                        LW_CSEC_FBIF_REGIONCFG};
    acrRegMap[REG_LABEL_FBIF_CTL]                        = (ACR_REG_MAPPING){LW_PFALCON_FBIF_CTL,                              LW_CSEC_FBIF_REGIONCFG};
#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(0);
#endif

#ifdef ACR_RISCV_LS
    acrRegMap[REG_LABEL_RISCV_BOOT_VECTOR_LO]            = (ACR_REG_MAPPING){LW_PRISCV_RISCV_BOOT_VECTOR_LO,                   LW_CSEC_RISCV_BOOT_VECTOR_LO};
    acrRegMap[REG_LABEL_RISCV_BOOT_VECTOR_HI]            = (ACR_REG_MAPPING){LW_PRISCV_RISCV_BOOT_VECTOR_HI,                   LW_CSEC_RISCV_BOOT_VECTOR_HI};
    acrRegMap[REG_LABEL_RISCV_CPUCTL]                    = (ACR_REG_MAPPING){LW_PRISCV_RISCV_CPUCTL,                           LW_CSEC_RISCV_CPUCTL};
#endif
}


// ACR_UNLOAD runs on PMU on Turing and accesses BAR0 through DMEM aperture so skipped
#if defined(AHESASC) || defined(ASB) || defined(BSI_LOCK) || defined(ACR_UNLOAD_ON_SEC2)
/*!
 * @brief: Set timeout value for BAR0 transactions
 */
void acrSetBar0Timeout_TU10X(void)
{
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_TMOUT,
        DRF_DEF(_CSEC, _BAR0_TMOUT, _CYCLES, _PROD));
}
#endif

#if defined(ACR_UNLOAD) && defined(DMEM_APERT_ENABLED)
/*!
 * @brief: Enable DMEM apertures.
 *         Timeout time = cycles / PWRCLK
 *         cycles       = (1 + TIME_OUT) * 256 * (2 ^ TIME_UNIT) / PWRCLK
 *         Assuming 540MHz PWRCLK,
 *         10055us = (1+166) * 256 * (2^7) / 540MHz
 *         Source: HW manuals and also refer PMU RTOS code
 *         This timeout value needs to be >10ms on Turing as specified in bug 2198976
 */
void acrEnableDmemAperture_TU10X(void)
{
    ACR_REG_WR32(CSB, LW_CPWR_FALCON_DMEMAPERT,
        DRF_NUM(_CPWR_FALCON, _DMEMAPERT, _ENABLE,    1) |
        DRF_NUM(_CPWR_FALCON, _DMEMAPERT, _TIME_UNIT, 7) |
        DRF_NUM(_CPWR_FALCON, _DMEMAPERT, _TIME_OUT,  166));
}
#endif


/*
* @brief: Return Falcon instance count on the current chip
*/
ACR_STATUS
acrlibGetFalconInstanceCount_TU10X
(
    LwU32  falconId,
    LwU32 *pInstanceCount
)
{
    if (pInstanceCount == NULL)
    {
         return ACR_ERROR_ILWALID_ARGUMENT;
    }

    *pInstanceCount = LSF_FALCON_INSTANCE_COUNT_DEFAULT_1;

    return ACR_OK;
}

