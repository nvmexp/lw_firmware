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
 * @file: acr_rv_falct234.c
 */

#include "rv_acr.h"

#include <liblwriscv/io.h>
#include <liblwriscv/io_pri.h>
#include <liblwriscv/io_csb.h>
#include <liblwriscv/shutdown.h>

#include "dev_falcon_v4.h"
#include "dev_fbif_v4.h"
#include "dev_graphics_nobundle.h"
#include "dev_master.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"
#include "dev_fb.h"
#include "dev_gsp.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_gpc.h"

/*!
 * Rounds up "a" w.r.t "b"
 */
#define DIV_ROUND_UP(a, b) ( ((a) / (b)) + ( ((a) % (b) == 0U) ? 0U :1U ) )

/*!
 * @brief Check if falcon is halted or not
 *        Read FALCON_CPUCTL registers to know falcon state.
 *
 * @param[in] pFlcnCfg Falcon configuration.
 *
 * @return TRUE If falcon is in halted state.
 * @return FALSE If falcon is not in halted state.
 */
LwBool
acrlibIsFalconHalted_T234
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    LwU32 data = 0;
    data = acrlibFlcnRegRead_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL);
    return FLD_TEST_DRF(_PFALCON_FALCON, _CPUCTL, _HALTED, _TRUE, data);
}

/*!
 * @brief Programs the bootvector i.e. PC location from where falcon will start exelwtion.
 *        Write FALCON_BOOTVEC register with the address.
 *
 * @param[in] pFlcnCfg Falcon Configuration.
 * @param[in] bootvec  Falcon Start PC address in IMEM.
 */
void
acrlibSetupBootvec_T234
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 bootvec
)
{
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, 
          REG_LABEL_FLCN_BOOTVEC, bootvec);
}

/*!
 * @brief Enable ICD(Inter-circuit Debug) registers.
 *        Falcon ICD registers are used for debugging.
 * @param[in] pFlcnCfg Falcon configuration.
 */
void
acrlibEnableICD
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    // Enable all of ICD
    acrlibFlcnRegLabelWrite_HAL(pFlcnCfg, REG_LABEL_FLCN_DBGCTL, 0x1ff); 
    return;
}

/*!
 * @brief Get current time in NS
 * @param[out] pTime TIMESTAMP struct to hold the current time
 */
void
acrlibGetLwrrentTimeNs_T234
(
    PACR_TIMESTAMP pTime
)
{
    LwU32 hi;
    LwU32 lo;

    // Read loop specified by 'dev_timer.ref'.
    do
    {
        hi = localRead(LW_PRGNLCL_FALCON_PTIMER1);
        lo = localRead(LW_PRGNLCL_FALCON_PTIMER0);
    }
    while (hi != localRead(LW_PRGNLCL_FALCON_PTIMER1));

    // Stick the values into the timestamp.
    pTime->parts.lo = lo;
    pTime->parts.hi = hi;
}

/*!
 * @brief Measure the time in nanoseconds elapsed since 'pTime'.
 *
 * Since the time is measured in nanoseconds and returned as an LwU32, this
 * function will only measure time-spans less than 4.29 seconds (2^32-1
 * nanoseconds).
 *
 * @param[in]   pTime   Timestamp to measure from.
 *
 * @return Number of nanoseconds elapsed since 'pTime'.
 *
 */
LwU32
acrlibGetElapsedTimeNs_T234
(
    const PACR_TIMESTAMP pTime
)
{
    //
    // Since time-spans are less than 4.29 secs, only the lower 32-bits of the
    // timestamps need to be compared. Subtracting these values will yield
    // the correct result. We also only need to read the PTIMER0 and avoid the
    // read loop ilwolved with reading both PTIMER0 and PTIMER1.
    //
    return (localRead(LW_PRGNLCL_FALCON_PTIMER0) > pTime->parts.lo ?
        localRead(LW_PRGNLCL_FALCON_PTIMER0) - pTime->parts.lo : 0U);
}

/*!
 * @brief Find the IMEM block from the end to load BL code
 *        Read PFALCON_FALCON_HWCFG to know imem size of the falcon in units of FLCN_IMEM_BLK_SIZE_IN_BYTES.\n
 *        Rounds up size of codeSize of BL to size of a IMEM block. i.e. FLCN_IMEM_BLK_SIZE_IN_BYTES. Subtract that from total IMEM size and return.\n
 *        This subtraction will give farthest IMEM block that can accomodate BL code in falcon's IMEM.
 *
 * @param[in] pFlcnCfg Falcon configuration.
 * @param[in] codeSizeInBytes size of ucode to be placed in IMEM.
 *
 * @return    last IMEM block where BL ucode can be copied to the end of IMEM.
 */
LwU32
acrlibFindFarthestImemBl_T234
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    LwU32              codeSizeInBytes
)
{
    LwU32 hwcfg    = acrlibFlcnRegLabelRead_HAL(pFlcnCfg, REG_LABEL_FLCN_HWCFG);
    LwU32 imemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG, _IMEM_SIZE, hwcfg);
    LwU32 codeBlks = DIV_ROUND_UP(codeSizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES);

    //CHECK_WRAP_AND_HALT(imemSize < codeBlks);

    return (imemSize - codeBlks);
}
 
/*!
 * @brief Check if ucode fits into falcon IMEM/DMEM.
 *        Read PFALCON_FALCON_HWCFG to read DMEM/IMEM size in units of 256 bytes.
 *
 * @param[in] pFlcnCfg falcon configuration.
 * @param[in] ucodeSize ucode size to be loaded into falcon IMEM/DMEM.
 * @param[in] bIsDstImem If ucode to be loaded is falcon's IMEM.
 *
 * @return LW_TRUE If Ucode fits into falcon IMEM/DMEM.
 * @return LW_FALSE If Ucode doesn't fit into falcon IMEM/DMEM.
 */
LwBool
acrlibCheckIfUcodeFitsFalcon_T234
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    LwU32               ucodeSize,
    LwBool              bIsDstImem
)
{
    LwU32          tmp = 0;
    LwU32          imemBlks = 0;
    LwU32          dmemBlks = 0;

    tmp = acrlibFlcnRegRead_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_HWCFG);

    imemBlks = DRF_VAL(_PFALCON_FALCON, _HWCFG, _IMEM_SIZE, tmp);
    dmemBlks = DRF_VAL(_PFALCON_FALCON, _HWCFG, _DMEM_SIZE, tmp);

    if (bIsDstImem)
    {
        if (imemBlks < (ucodeSize / FLCN_IMEM_BLK_SIZE_IN_BYTES))
        {
            return LW_FALSE;
        }
    }
    else
    {
        if (dmemBlks < (ucodeSize / FLCN_DMEM_BLK_SIZE_IN_BYTES))
        {
            return LW_FALSE;
        }
    }

    return LW_TRUE;
}

/*!
 * @brief Initialize ACR Register map
 * @param[out] acrRegMap Pointer to ACR regMap to be initialized.
 */
static void
acrlibInitRegMapping_T234(PACR_REG_MAPPING acrRegMap)
{
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_SCTL_PRIV_LEVEL_MASK)       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL_PRIV_LEVEL_MASK,           LW_CPWR_FALCON_SCTL_PRIV_LEVEL_MASK};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_SCTL)                       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_SCTL,                           LW_CPWR_FALCON_SCTL};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_CPUCTL)                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL,                         LW_CPWR_FALCON_CPUCTL};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_DMACTL)                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMACTL,                         LW_CPWR_FALCON_DMACTL};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_DMATRFBASE)                 = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFBASE,                     LW_CPWR_FALCON_DMATRFBASE};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_DMATRFCMD)                  = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFCMD,                      LW_CPWR_FALCON_DMATRFCMD};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_DMATRFMOFFS)                = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFMOFFS,                    LW_CPWR_FALCON_DMATRFMOFFS};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_DMATRFFBOFFS)               = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMATRFFBOFFS,                   LW_CPWR_FALCON_DMATRFFBOFFS};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_IMEM_PRIV_LEVEL_MASK)       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IMEM_PRIV_LEVEL_MASK,           LW_CPWR_FALCON_IMEM_PRIV_LEVEL_MASK};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_DMEM_PRIV_LEVEL_MASK)       = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DMEM_PRIV_LEVEL_MASK,           LW_CPWR_FALCON_DMEM_PRIV_LEVEL_MASK};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_CPUCTL_PRIV_LEVEL_MASK)     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_CPUCTL_PRIV_LEVEL_MASK,         LW_CPWR_FALCON_CPUCTL_PRIV_LEVEL_MASK};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_EXE_PRIV_LEVEL_MASK)        = (ACR_REG_MAPPING){LW_PFALCON_FALCON_EXE_PRIV_LEVEL_MASK,            LW_CPWR_FALCON_EXE_PRIV_LEVEL_MASK};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_IRQTMR_PRIV_LEVEL_MASK)     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_IRQTMR_PRIV_LEVEL_MASK,         LW_CPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_MTHDCTX_PRIV_LEVEL_MASK)    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_MTHDCTX_PRIV_LEVEL_MASK,        LW_CPWR_FALCON_MTHDCTX_PRIV_LEVEL_MASK};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_HWCFG)                      = (ACR_REG_MAPPING){LW_PFALCON_FALCON_HWCFG,                          LW_CPWR_FALCON_HWCFG};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_BOOTVEC)                    = (ACR_REG_MAPPING){LW_PFALCON_FALCON_BOOTVEC,                        LW_CPWR_FALCON_BOOTVEC};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_DBGCTL)                     = (ACR_REG_MAPPING){LW_PFALCON_FALCON_DBGCTL,                         LW_CPWR_FALCON_DBGCTL};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FLCN_SCPCTL)                     = (ACR_REG_MAPPING){0xFFFFFFFF,                                       LW_CPWR_PMU_SCP_CTL_STAT};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FBIF_REGIONCFG_PRIV_LEVEL_MASK)  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG_PRIV_LEVEL_MASK,        LW_CPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FBIF_REGIONCFG)                  = (ACR_REG_MAPPING){LW_PFALCON_FBIF_REGIONCFG,                        LW_CPWR_FBIF_REGIONCFG};
    REG_MAP_INDEX(acrRegMap, REG_LABEL_FBIF_CTL)                        = (ACR_REG_MAPPING){LW_PFALCON_FBIF_CTL,                              LW_CPWR_FBIF_REGIONCFG};
}

/*!
 * @brief Find reg mapping with reg label using acrRegMap
 *        This function halts falcon in case REG LABEL is outside defined range.
 *
 * @param[in]  pFlcnCfg falcon configuration.
 * @param[in]  acrLabel Register Label corresponding to which register mapping is to be found.
 * @param[out] pMap     Register mapping found corresponding to input register Label.
 * @param[in]  pTgt     Bus type target access for the register.
 *
 * @return     ACR_OK   If RegMapping corresponding to input register Label is found.
 */
ACR_STATUS
acrlibFindRegMapping_T234
(
    PACR_FLCN_CONFIG    pFlcnCfg, 
    ACR_FLCN_REG_LABEL  acrLabel,
    PACR_REG_MAPPING    pMap,
    PFLCN_REG_TGT       pTgt
)
{
    ACR_REG_MAPPING REG_MAP_INDEX(acrRegMap, REG_LABEL_END + 1);
    if ((acrLabel <= REG_LABEL_START) ||
        (acrLabel >= REG_LABEL_END) ||
        (pMap == LW_NULL) || (pTgt == LW_NULL) ||
        (acrLabel == REG_LABEL_FLCN_END) ||
        (acrLabel == REG_LABEL_FBIF_END))
    {
        riscvShutdown(); //earlier was return ACR_ERROR_REG_MAPPING;
    }
    //Above halt needs to be revisited to propagate error to caller and handle it well
    //Doing this to satisfy coverity for now.
    else
    {
        if (pFlcnCfg->bIsBoOwner)
        {
            *pTgt = CSB_FLCN;
        }
        else if (acrLabel < REG_LABEL_FLCN_END)
        {
            *pTgt = BAR0_FLCN;
        }
        else
        {
                *pTgt = BAR0_FBIF;
        }

    	acrlibInitRegMapping_T234(acrRegMap);

    	pMap->bar0Off   = REG_MAP_INDEX(acrRegMap, acrLabel).bar0Off;
    	pMap->pwrCsbOff = REG_MAP_INDEX(acrRegMap, acrLabel).pwrCsbOff;
    }

    return ACR_OK;
}

/*!
 * @brief function to write falcon registers using register label.\n
 *        The register mapping is first found by calling acrlibFindRegMapping_HAL.\n
 *        The register mapping obtained is then used to write using acrlibFlcnRegWrite_HAL.\n
 *
 * @param[in]  pFlcnCfg  Falcon config.
 * @param[in]  reglabel  Label of the register to be written.
 * @param[in]  data      Data to be written.
 */
void
acrlibFlcnRegLabelWrite_T234
(
    PACR_FLCN_CONFIG   pFlcnCfg, 
    ACR_FLCN_REG_LABEL reglabel,
    LwU32              data
)
{
    ACR_REG_MAPPING regMapping;
    FLCN_REG_TGT    tgt = BAR0_FBIF; //default

    //always returns ACR_OK, so no need to check
    (void)acrlibFindRegMapping_T234(pFlcnCfg, reglabel, &regMapping, &tgt);

    if (tgt == CSB_FLCN)
    {
        acrlibFlcnRegWrite_T234(pFlcnCfg, tgt, regMapping.pwrCsbOff, data);
    }
    else
    {
        acrlibFlcnRegWrite_T234(pFlcnCfg, tgt, regMapping.bar0Off, data);
    }
}
 
/*!
 * @brief function to read falcon registers using register label.\n
 *        The register mapping is first found by calling acrlibFindRegMapping_HAL.\n
 *        The register mapping obtained is then used to read using acrlibFlcnRegRead_HAL.\n
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  reglabel  Label of the register to be read.
 *
 * @return  value read from the register.
 */
LwU32
acrlibFlcnRegLabelRead_T234
(
    PACR_FLCN_CONFIG   pFlcnCfg, 
    ACR_FLCN_REG_LABEL reglabel
)
{
    ACR_REG_MAPPING regMapping;
    FLCN_REG_TGT    tgt = BAR0_FBIF; //default

    //always returns ACR_OK, so no need to check
    (void)acrlibFindRegMapping_T234(pFlcnCfg, reglabel, &regMapping, &tgt);
    if (tgt == CSB_FLCN)
    {
        return acrlibFlcnRegRead_T234(pFlcnCfg, tgt, regMapping.pwrCsbOff);
    }
    else
    {
        return acrlibFlcnRegRead_T234(pFlcnCfg, tgt, regMapping.bar0Off);
    }
}

/*!
 * @brief function to read falcon registers. Check target access bus type and then use register/write function with appropriate args.
 *
 * @param[in]  pFlcnCfg  Falcon config
 * @param[in]  tgt       Register target access bus type.
 * @param[in]  regOff    Register offset
 *
 * @return     value read from the register.
 */
LwU32
acrlibFlcnRegRead_T234
(
    PACR_FLCN_CONFIG pFlcnCfg, 
    FLCN_REG_TGT     tgt, 
    LwU32            regOff
)
{
    LwU32 regAddr;

    if ((pFlcnCfg != LW_NULL) && (tgt == BAR0_FLCN))
    {
        regAddr = (pFlcnCfg->registerBase) + regOff;
        CHECK_WRAP_AND_HALT(regAddr < regOff);
        return priRead(regAddr); 
    }
    else if (tgt == CSB_FLCN)
    {
        return csbRead(regOff); 
    }
    else if ((pFlcnCfg != LW_NULL) && (tgt == BAR0_FBIF))
    {
        regAddr = (pFlcnCfg->fbifBase) + regOff;
        CHECK_WRAP_AND_HALT(regAddr < regOff);
        return priRead(regAddr);
    }
    else
    {
        acrWriteStatusToFalconMailbox(ACR_ERROR_ILWALID_ARGUMENT);
        riscvShutdown();    // This halt needs to be revisited to propagate error to caller
    }
    return 0;
}
 
/*!
 * @brief function to write falcon registers. Check target access bus type and then use register/write function with appropriate args.
 *
 * @param[in]  pFlcnCfg  Falcon config.
 * @param[in]  tgt       Register target access bus type.
 * @param[in]  regOff    Register offset.
 * @param[in]  data      Data to be written to register.
 *
 */
void
acrlibFlcnRegWrite_T234
(
    PACR_FLCN_CONFIG pFlcnCfg, 
    FLCN_REG_TGT     tgt, 
    LwU32            regOff, 
    LwU32            data
)
{
    LwU32 regAddr;

    if ((pFlcnCfg != LW_NULL) && (tgt == BAR0_FLCN))
    {
        regAddr = (pFlcnCfg->registerBase) + regOff;
        CHECK_WRAP_AND_HALT(regAddr < regOff);
        priWrite(regAddr, data);
    }
    else if (tgt == CSB_FLCN)
    {
        csbWrite(regOff, data); 
    }
#ifndef ACR_SAFE_BUILD
    else if ((pFlcnCfg != LW_NULL) && (tgt == BAR0_FBIF))
    {
        regAddr = (pFlcnCfg->fbifBase) + regOff;
        CHECK_WRAP_AND_HALT(regAddr < regOff);
        priWrite(regAddr, data);
    }
#endif
    else 
    {
        acrWriteStatusToFalconMailbox(ACR_ERROR_ILWALID_ARGUMENT);
        riscvShutdown();    // This halt needs to be revisited to propagate error to caller
    }
}

/*!
 * @brief Given falcon ID and instance ID, get the falcon
 *        specifications such as registerBase, pmcEnableMask, imemPLM, dmemPLM  etc
 *
 * @param[in]  falconId         Falcon ID
 * @param[in]  falconInstance   Instance (either 0 or 1)
 * @param[out] pFlcnCfg         falcon configuration
 *
 * @return ACR_OK if falconId and instance are valid
 * @return ACR_ERROR_FLCN_ID_NOT_FOUND If falconId is not found.
 *
 */ 
ACR_STATUS
acrlibGetFalconConfig_T234
(
    LwU32                falconId,
    LwU32                falconInstance,
    PACR_FLCN_CONFIG     pFlcnCfg
)
{
    ACR_STATUS  status  = ACR_OK;

    pFlcnCfg->falconId            = falconId;
    pFlcnCfg->bIsBoOwner          = LW_FALSE;
    pFlcnCfg->imemPLM             = ACR_PLMASK_READ_L0_WRITE_L2;
    pFlcnCfg->dmemPLM             = ACR_PLMASK_READ_L0_WRITE_L2;
    pFlcnCfg->bStackCfgSupported  = LW_FALSE;
    pFlcnCfg->pmcEnableMask       = 0;
    pFlcnCfg->regSelwreResetAddr  = 0;
    switch (falconId)
    {
        case LSF_FALCON_ID_PMU_RISCV:
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0);
            pFlcnCfg->registerBase        = LW_PPWR_FALCON_IRQSSET;
            pFlcnCfg->fbifBase            = LW_PPWR_FBIF_TRANSCFG(0U);
            pFlcnCfg->bFbifPresent        = LW_TRUE;
            pFlcnCfg->ctxDma              = RM_PMU_DMAIDX_UCODE;
            pFlcnCfg->bOpenCarve          = LW_TRUE;
#if (ACR_LSF_LWRRENT_BOOTSTRAP_OWNER == LSF_BOOTSTRAP_OWNER_PMU)
            pFlcnCfg->range0Addr          = LW_CPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr          = LW_CPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->bIsBoOwner          = LW_TRUE; // PMU is the Bootstrap Owner
#else
            pFlcnCfg->range0Addr          = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr          = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->pmcEnableMask       = DRF_DEF(_PMC, _ENABLE, _PWR, _ENABLED);
#endif
            pFlcnCfg->bStackCfgSupported = LW_TRUE;
            pFlcnCfg->targetMaskIndex    = LW_PPRIV_SYS_PRI_MASTER_fecs2pwr_pri;
            break;
#ifndef ACR_ONLY_BO
        case LSF_FALCON_ID_FECS:
            // ENGINE GR shouldnt be reset
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_FECS_CFGA(0);
            pFlcnCfg->pmcEnableMask       = 0;
            pFlcnCfg->registerBase        = LW_PGRAPH_PRI_FECS_FALCON_IRQSSET;
            pFlcnCfg->fbifBase            = 0;
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_FALSE;
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->regCfgAddr          = LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG;
            pFlcnCfg->regCfgMaskAddr      = LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK;
            pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_FECS;
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_SYS_PRI_MASTER_fecs2fecs_pri0;
            break;
        case LSF_FALCON_ID_GPCCS:
            // ENGINE GR shouldnt be reset
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_GPCCS_CFGA(0);
            pFlcnCfg->pmcEnableMask       = 0;
            pFlcnCfg->registerBase        = LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQSSET;
            pFlcnCfg->fbifBase            = 0;
            pFlcnCfg->range0Addr          = 0;
            pFlcnCfg->range1Addr          = 0;
            pFlcnCfg->bOpenCarve          = LW_FALSE;
            pFlcnCfg->bFbifPresent        = LW_FALSE;
            // RegCfgAddr and RegCfgMaskAddr are needed only for falcons with no FBIF
            pFlcnCfg->regCfgAddr          = LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG;
            pFlcnCfg->regCfgMaskAddr      = LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK;
            pFlcnCfg->ctxDma              = LSF_BOOTSTRAP_CTX_DMA_FECS;
            pFlcnCfg->targetMaskIndex     = LW_PPRIV_GPC_PRI_MASTER_gpc_pri_hub2gpccs_pri;
            break;
#endif
        case LSF_FALCON_ID_GSP_RISCV:
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_GSP_CFGA(0);
            pFlcnCfg->registerBase        = LW_PGSP_FALCON_IRQSSET;
            break;
        default:
            status = ACR_ERROR_FLCN_ID_NOT_FOUND;
            break;
    }

    return status;
}

/*!
 * @brief Poll for IMEM and DMEM scrubbing to complete.
 *        Read LW_PFALCON_FALCON_DMACTL to check if IMEM/DMEM scrubbing is complete.
 *
 * @param[in] pFlcnCfg Falcon Config
 *
 * @return    ACR_OK   If poll for scrubbing is successful.
 * @return    ACR_ERROR_TIMEOUT If poll for scrubbing is not successful for "ACR_DEFAULT_TIMEOUT_NS". 
 */
ACR_STATUS
acrlibPollForScrubbing_T234
(
    PACR_FLCN_CONFIG  pFlcnCfg
)
{
    ACR_STATUS    status          = ACR_OK;
    LwU32         data            = 0;
    ACR_TIMESTAMP startTimeNs;
    LwS32         timeoutLeftNs;

    // Poll for SRESET to complete
    acrlibGetLwrrentTimeNs_HAL(&startTimeNs);
    data = acrlibFlcnRegRead_HAL(pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMACTL);
    while((FLD_TEST_DRF(_PFALCON_FALCON, _DMACTL, _DMEM_SCRUBBING, _PENDING, data)) ||
         (FLD_TEST_DRF(_PFALCON_FALCON, _DMACTL, _IMEM_SCRUBBING, _PENDING, data)))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(ACR_DEFAULT_TIMEOUT_NS, 
                                                    startTimeNs, &timeoutLeftNs));
        data = acrlibFlcnRegRead_HAL(pFlcnCfg, BAR0_FLCN,
                                                    LW_PFALCON_FALCON_DMACTL);
    }

    return status;
}
 
/*!
 * @brief Resets a given falcon.
 *        Do Falcon reset by writing to FALCON_CPUCTL_SRESET field.\n
 *        Poll for Falcon reset to complete.\n
 * @param[in] pFlcnCfg            Falcon configuration.
 * @param[in] bForceFlcnOnlyReset TRUE - falcon reset. FALSE - engine reset using LW_PMC_ENABLE
 *
 * @return ACR_OK If falcons resets successfully.
 * @return ACR_ERROR_TIMEOUT If encountered timeout polling for completion of steps ilwolved in this function.
 */
ACR_STATUS
acrlibResetFalcon_T234
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    LwBool              bForceFlcnOnlyReset
)
{
    ACR_STATUS       status          = ACR_OK;
    LwU32            sftR            = 0;
    LwU32            data            = 0;
    ACR_TIMESTAMP    startTimeNs;
    LwS32            timeoutLeftNs;

    if (((pFlcnCfg->pmcEnableMask == 0U) &&
        (pFlcnCfg->regSelwreResetAddr == 0U)) ||
        (bForceFlcnOnlyReset))
    {
        // Do falcon reset
        sftR = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _SRESET, _TRUE, 0);
        acrlibFlcnRegWrite_HAL(pFlcnCfg, BAR0_FLCN,
                                            LW_PFALCON_FALCON_CPUCTL, sftR);

        // Poll for SRESET to complete
        acrlibGetLwrrentTimeNs_HAL(&startTimeNs);
        data = acrlibFlcnRegRead_HAL(pFlcnCfg, BAR0_FLCN,
                                                    LW_PFALCON_FALCON_CPUCTL);
        while(FLD_TEST_DRF(_PFALCON_FALCON, _CPUCTL, _SRESET, _TRUE, data))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(ACR_DEFAULT_TIMEOUT_NS, 
                                            startTimeNs, &timeoutLeftNs));
            data = acrlibFlcnRegRead_HAL(pFlcnCfg, BAR0_FLCN,
                                                    LW_PFALCON_FALCON_CPUCTL);
        }
    }

    return status;
}
