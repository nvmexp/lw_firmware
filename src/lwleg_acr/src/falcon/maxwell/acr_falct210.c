
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
 * @file: acr_falct210.c
 */

//
// Includes
//
#include "acr.h"
#include "acr_objacrlib.h"
#include "dev_falcon_v4.h"
#ifdef ACR_SUB_WPR
#include "dev_fb.h"
#endif
#include "dev_fbif_v4.h"
#include "dev_graphics_nobundle.h"
#include "dev_master.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_pri.h"
#include "dev_pri_ringstation_sys.h"
#include "dev_pri_ringstation_gpc.h"

/*! Rounds up "a" w.r.t "b"
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
acrlibIsFalconHalted_GM200
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    LwU32 data = 0;
    data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL);
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
acrlibSetupBootvec_GM200
(
    PACR_FLCN_CONFIG pFlcnCfg,
    LwU32 bootvec
)
{
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg,
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
    acrlibFlcnRegLabelWrite_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_DBGCTL, 0x1ff);
    return;
}

/*!
 * @brief Get current time in NS
 * @param[out] pTime TIMESTAMP struct to hold the current time
 */
void
acrlibGetLwrrentTimeNs_GM200
(
    PACR_TIMESTAMP pTime
)
{
    LwU32 hi;
    LwU32 lo;

    // Read loop specified by 'dev_timer.ref'.
    do
    {
        hi = ACR_REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER1);
        lo = ACR_REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0);
    }
    while (hi != ACR_REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER1));

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
acrlibGetElapsedTimeNs_GM200
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
    return (ACR_REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0) > pTime->parts.lo ?
        ACR_REG_RD32(CSB, LW_CMSDEC_FALCON_PTIMER0) - pTime->parts.lo : 0U);
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
acrlibFindFarthestImemBl_GM200
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    LwU32              codeSizeInBytes
)
{
    LwU32 hwcfg    = acrlibFlcnRegLabelRead_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_HWCFG);
    LwU32 imemSize = DRF_VAL(_PFALCON_FALCON, _HWCFG, _IMEM_SIZE, hwcfg);
    LwU32 codeBlks = DIV_ROUND_UP(codeSizeInBytes, FLCN_IMEM_BLK_SIZE_IN_BYTES);

    CHECK_WRAP_AND_HALT(imemSize < codeBlks);

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
acrlibCheckIfUcodeFitsFalcon_GM200
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    LwU32               ucodeSize,
    LwBool              bIsDstImem
)
{

    LwU32          tmp = 0;
    LwU32          imemBlks = 0;
    LwU32          dmemBlks = 0;

    tmp = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_HWCFG);

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

#ifndef ACR_SAFE_BUILD
/*!
 * @brief Find the total number of DMEM blocks.\n
 *        Read PFALCON_FALCON_HWCFG to read DMEM size in units of 256 bytes.
 *
 * @param[in] pFlcnCfg falcon configuration.
 * @return Total DMEM blocks. (size of a DMEM block is 256 bytes)
 */
LwU32
acrlibFindTotalDmemBlocks_GM200
(
    PACR_FLCN_CONFIG    pFlcnCfg
)
{
    LwU32 hwcfg    = acrlibFlcnRegLabelRead_HAL(pAcrlib, pFlcnCfg, REG_LABEL_FLCN_HWCFG);
    return DRF_VAL(_PFALCON_FALCON, _HWCFG, _DMEM_SIZE, hwcfg);
}
#endif

/*!
 * @brief Initialize ACR Register map
 * @param[out] acrRegMap Pointer to ACR regMap to be initialized.
 */
static void
acrlibInitRegMapping_GM200(PACR_REG_MAPPING acrRegMap)
{
#ifdef ACR_FALCON_PMU
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

#else
    // Control is not expected to come here. If you hit this assert, please check build profile config
    ct_assert(ACR_FALCON_PMU || ACR_FALCON_SEC2);
#endif
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
acrlibFindRegMapping_GM200
(
    PACR_FLCN_CONFIG    pFlcnCfg,
    ACR_FLCN_REG_LABEL  acrLabel,
    PACR_REG_MAPPING    pMap,
    PFLCN_REG_TGT       pTgt
)
{
    ACR_REG_MAPPING REG_MAP_INDEX(acrRegMap, REG_LABEL_END + 1);
    if ((acrLabel <= REG_LABEL_START) || (acrLabel >= REG_LABEL_END) || (pMap == LW_NULL) || (pTgt == LW_NULL) ||
        (acrLabel == REG_LABEL_FLCN_END) || (acrLabel == REG_LABEL_FBIF_END)) {
        lwuc_halt(); //earlier was return ACR_ERROR_REG_MAPPING;
    }
        //Above lwuc_halt needs to be revisited to propagate error to caller and handle it well
        //Doing this to satisfy coverity for now.
    else
    {
        if (pFlcnCfg->bIsBoOwner) {
            *pTgt = CSB_FLCN;
        } else if (acrLabel < REG_LABEL_FLCN_END) {
            *pTgt = BAR0_FLCN;
        } else {
#ifndef ACR_SAFE_BUILD
            *pTgt = BAR0_FBIF;
#endif
        }

        acrlibInitRegMapping_GM200(acrRegMap);

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
acrlibFlcnRegLabelWrite_GM200
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    ACR_FLCN_REG_LABEL reglabel,
    LwU32              data
)
{
    ACR_REG_MAPPING regMapping;
    FLCN_REG_TGT    tgt = BAR0_FBIF; //default
    if ( pFlcnCfg->falconId == LSF_FALCON_ID_PMU)
    {
        tgt = CSB_FLCN;
    }
    //always returns ACR_OK, so no need to check
    (void)acrlibFindRegMapping_HAL(pAcrlib, pFlcnCfg, reglabel, &regMapping, &tgt);

    if (tgt == CSB_FLCN)
    {
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, tgt, regMapping.pwrCsbOff, data);
    }
    else
    {
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, tgt, regMapping.bar0Off, data);
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
acrlibFlcnRegLabelRead_GM200
(
    PACR_FLCN_CONFIG   pFlcnCfg,
    ACR_FLCN_REG_LABEL reglabel
)
{
    ACR_REG_MAPPING regMapping;
    FLCN_REG_TGT    tgt = BAR0_FBIF; //default

    //always returns ACR_OK, so no need to check
    (void)acrlibFindRegMapping_HAL(pAcrlib, pFlcnCfg, reglabel, &regMapping, &tgt);
    if (tgt == CSB_FLCN)
    {
        return acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, tgt, regMapping.pwrCsbOff);
    }
    else
    {
        return acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, tgt, regMapping.bar0Off);
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
acrlibFlcnRegRead_GM200
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
        return ACR_REG_RD32(BAR0, regAddr);
    }
    else if (tgt == CSB_FLCN)
    {
        return ACR_REG_RD32_STALL(CSB, regOff);
    }
#ifndef ACR_SAFE_BUILD
    else if ((pFlcnCfg != LW_NULL) && (tgt == BAR0_FBIF))
    {
        regAddr = (pFlcnCfg->fbifBase) + regOff;
        CHECK_WRAP_AND_HALT(regAddr < regOff);
        return ACR_REG_RD32(BAR0, regAddr);
    }
#endif
    else
    {
        lwuc_halt();    // This halt needs to be revisited to propagate error to caller
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
acrlibFlcnRegWrite_GM200
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
        ACR_REG_WR32(BAR0, regAddr, data);
    }
    else if (tgt == CSB_FLCN)
    {
        ACR_REG_WR32_STALL(CSB, regOff, data);
    }
#ifndef ACR_SAFE_BUILD
    else if ((pFlcnCfg != LW_NULL) && (tgt == BAR0_FBIF))
    {
        regAddr = (pFlcnCfg->fbifBase) + regOff;
        CHECK_WRAP_AND_HALT(regAddr < regOff);
        ACR_REG_WR32(BAR0, regAddr, data);
    }
#endif
    else
    {
        lwuc_halt();
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
acrlibGetFalconConfig_GM200
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
    pFlcnCfg->subWprRangeAddrBase = ILWALID_REG_ADDR;
    pFlcnCfg->bStackCfgSupported  = LW_FALSE;
    pFlcnCfg->pmcEnableMask       = 0;
    pFlcnCfg->regSelwreResetAddr  = 0;
    switch (falconId)
    {
        case LSF_FALCON_ID_PMU:
#ifdef ACR_SUB_WPR
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_PMU_CFGA(0);
#endif
            pFlcnCfg->registerBase      = LW_PPWR_FALCON_IRQSSET;
            pFlcnCfg->fbifBase          = LW_PPWR_FBIF_TRANSCFG(0U);
            pFlcnCfg->bFbifPresent      = LW_TRUE;
            pFlcnCfg->ctxDma            = RM_PMU_DMAIDX_UCODE;
#ifdef LW_CPWR_FALCON_DMEM_PRIV_RANGE0
            pFlcnCfg->bOpenCarve        = LW_TRUE;
#if (ACR_LSF_LWRRENT_BOOTSTRAP_OWNER == LSF_BOOTSTRAP_OWNER_PMU)
            pFlcnCfg->range0Addr        = LW_CPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr        = LW_CPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->bIsBoOwner        = LW_TRUE; // PMU is the Bootstrap Owner
#else
            pFlcnCfg->range0Addr        = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->range1Addr        = LW_PPWR_FALCON_DMEM_PRIV_RANGE0;
            pFlcnCfg->pmcEnableMask     = DRF_DEF(_PMC, _ENABLE, _PWR, _ENABLED);
#endif
#else
            pFlcnCfg->bOpenCarve        = LW_FALSE;
            pFlcnCfg->range0Addr        = 0;
            pFlcnCfg->range1Addr        = 0;
#endif
            pFlcnCfg->bStackCfgSupported = LW_TRUE;
            pFlcnCfg->targetMaskIndex    = LW_PPRIV_SYS_PRI_MASTER_fecs2pwr_pri;
            break;

#ifndef ACR_ONLY_BO

        case LSF_FALCON_ID_FECS:
            // ENGINE GR shouldnt be reset
#ifdef ACR_SUB_WPR
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_FECS_CFGA(0);
#endif
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
#ifdef ACR_SUB_WPR
            pFlcnCfg->subWprRangeAddrBase = LW_PFB_PRI_MMU_FALCON_GPCCS_CFGA(0);
#endif
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
acrlibPollForScrubbing_GM200
(
    PACR_FLCN_CONFIG  pFlcnCfg
)
{
    ACR_STATUS    status          = ACR_OK;
    LwU32         data            = 0;
    ACR_TIMESTAMP startTimeNs;
    LwS32         timeoutLeftNs;


    // Poll for SRESET to complete
    acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
    data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMACTL);
    while((FLD_TEST_DRF(_PFALCON_FALCON, _DMACTL, _DMEM_SCRUBBING, _PENDING, data)) ||
         (FLD_TEST_DRF(_PFALCON_FALCON, _DMACTL, _IMEM_SCRUBBING, _PENDING, data)))
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_DEFAULT_TIMEOUT_NS,
                                                            startTimeNs, &timeoutLeftNs));
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_DMACTL);
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
acrlibResetFalcon_GM200
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

    if (((pFlcnCfg->pmcEnableMask == 0U) && (pFlcnCfg->regSelwreResetAddr == 0U)) ||
         (bForceFlcnOnlyReset))
    {
        //
        // Either the Falcon doesn't have a full-blown engine reset, or we have
        // been asked to do a soft reset
        //
#ifndef ACR_SAFE_BUILD
        if (pFlcnCfg->bFbifPresent)
        {
            // TODO: Revisit this once FECS halt bug has been fixed

            // Wait for STALLREQ
            sftR = FLD_SET_DRF(_PFALCON, _FALCON_ENGCTL, _SET_STALLREQ, _TRUE, 0);
            acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_ENGCTL, sftR);

            // Poll for FHSTATE.EXT_HALTED to complete
            acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
            data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_FHSTATE);
            while(FLD_TEST_DRF_NUM(_PFALCON, _FALCON_FHSTATE, _EXT_HALTED, 0, data))
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_DEFAULT_TIMEOUT_NS,
                                                           startTimeNs, &timeoutLeftNs));
                data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_FHSTATE);
            }

            // Do SFTRESET
            sftR = FLD_SET_DRF(_PFALCON, _FALCON_SFTRESET, _EXT, _TRUE, 0);
            acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET, sftR);

            // Poll for SFTRESET to complete
            acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
            data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET);
            while(FLD_TEST_DRF(_PFALCON, _FALCON_SFTRESET, _EXT, _TRUE, data))
            {
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_DEFAULT_TIMEOUT_NS,
                                                           startTimeNs, &timeoutLeftNs));
                data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_SFTRESET);
            }
        }
#endif
        // Do falcon reset
        sftR = FLD_SET_DRF(_PFALCON, _FALCON_CPUCTL, _SRESET, _TRUE, 0);
        acrlibFlcnRegWrite_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL, sftR);

        // Poll for SRESET to complete
        acrlibGetLwrrentTimeNs_HAL(pAcrlib, &startTimeNs);
        data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL);
        while(FLD_TEST_DRF(_PFALCON_FALCON, _CPUCTL, _SRESET, _TRUE, data))
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibCheckTimeout_HAL(pAcrlib, ACR_DEFAULT_TIMEOUT_NS,
                                            startTimeNs, &timeoutLeftNs));
            data = acrlibFlcnRegRead_HAL(pAcrlib, pFlcnCfg, BAR0_FLCN, LW_PFALCON_FALCON_CPUCTL);
        }
    }
#ifndef ACR_SAFE_BUILD
    else
    {
        //
        // Falcon has an engine reset. Check if it has a PMC_ENABLE bit, or
        // secure reset, or both
        //

        // First reset using secure reset
        if (pFlcnCfg->regSelwreResetAddr != 0U)
        {
            acrlibSelwreResetFalcon_HAL(pAcrlib, pFlcnCfg);
        }

        // Then reset using PMC_ENABLE. SEC2, for example, uses both resets.
        if (pFlcnCfg->pmcEnableMask != 0U)
        {
            // Disable first
            data = ACR_REG_RD32(BAR0, LW_PMC_ENABLE);
            data &= ~pFlcnCfg->pmcEnableMask;
            ACR_REG_WR32(BAR0, LW_PMC_ENABLE, data);
            //function updates value at address pointer, so no need to return
            (void)ACR_REG_RD32(BAR0, LW_PMC_ENABLE);

            // Now Enable
            data = ACR_REG_RD32(BAR0, LW_PMC_ENABLE);
            data |= pFlcnCfg->pmcEnableMask;
            ACR_REG_WR32(BAR0, LW_PMC_ENABLE, data);
            //function updates value at address pointer, so no need to return
            (void)ACR_REG_RD32(BAR0, LW_PMC_ENABLE);
        }
    }
#endif
    return status;
}

#ifndef UPROC_RISCV
// Since the Peregrine RISCV core generates immediate exceptions on read errors, the below is not required on RISCV.

/*!
 * @brief Wrapper function for blocking falcon read instruction to halt in case of Priv Error(CSB interface).
 *        use falc_ldxb_i assembly instruction to read from register address.\n
 *        read LW_CPWR_FALCON_CSBERRSTAT to know if an PRIV error oclwred and halt falcon in case this is true.\n
 *        Exception to this is PTIMER0 & PTIMER1 ehich can return 0xbadfxxxx.\n
 *
 * @param[in] addr address of the register to be read.
 * @return    value read from the register.
 */
LwU32
falc_ldxb_i_with_halt
(
    LwU32 addr
)
{
    LwU32   val           = 0;
    LwU32   csbErrStatVal = 0;

    acr_falc_ldxb_i (addr, 0, &val);

#ifdef ACR_FALCON_PMU
     acr_falc_ldxb_i (LW_CPWR_FALCON_CSBERRSTAT, 0, &csbErrStatVal);
#else
    ct_assert(0);
#endif

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal) ||
        ((val & CSB_INTERFACE_MASK_VALUE) == CSB_INTERFACE_BAD_VALUE))
    {
        //
        // If we get here, we thought we read a bad value. However, there are
        // certain cases where the values may lead us to think we read something
        // bad, but in fact, they were just valid values that the register could
        // report. For example, the host ptimer PTIMER1 and PTIMER0 registers
        // can return 0xbadfxxxx value that we interpret to be "bad".
        //

#ifdef ACR_FALCON_PMU
        if (addr == LW_CPWR_FALCON_PTIMER1)
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
#ifdef ACR_FALCON_PMU
        else if (addr == LW_CPWR_FALCON_PTIMER0)
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
            LwBool  bChanging = LW_FALSE;
            LwU32   val1      = 0;
            LwU32   i;

            for (i = 0; i < 3U; i++)
            {
                acr_falc_ldx_i (addr, 0, &val1);
                if (val1 != val)
                {
                    bChanging = LW_TRUE;
                    break;
                }
            }

            if (bChanging)
            {
                return val;
            }
            falc_halt();
        }
#if defined(ACR_FALCON_PMU) || defined(ACR_FALCON_SEC2)
        else
        {
            return val;
        }
#endif
    }

    return val;
}

/*!
 * @brief Wrapper function for blocking falcon write instruction to halt in case of Priv Error.
 *        use falc_stxb_i assembly instruction to write to register address.\n
 *        read LW_CPWR_FALCON_CSBERRSTAT to know if an PRIV error oclwred and halt falcon in case this is true.\n
 *
 * @param[in] addr address of the register to be written.
 * @param[in] data value to be written to the register.
 */
void
falc_stxb_i_with_halt
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 csbErrStatVal = 0;

    acr_falc_stxb_i (addr, 0, data);

#ifdef ACR_FALCON_PMU
    acr_falc_ldxb_i (LW_CPWR_FALCON_CSBERRSTAT, 0, &csbErrStatVal);
#else
    ct_assert(0);
#endif

    if (FLD_TEST_DRF(_PFALCON, _FALCON_CSBERRSTAT, _VALID, _TRUE, csbErrStatVal))
    {
        falc_halt();
    }
}
#endif // UPROC_RISCV

#ifndef UPROC_RISCV
#ifdef DMEM_APERT_ENABLED
/*!
 * @brief Wrapper function to read register value through External Interface when DMEM Apertue is enabled
 *        and halt in case of Priv Error.
 *        It also reads LW_CPWR_FALCON_EXTERRSTAT to make sure there is no failure. It halts the falcon in case there is some failure.\n
 *
 * @param[in] addr address of the register to be read.
 * @return    data present in the register.
 */
LwU32
acrBar0RegReadDmemApert
(
    LwU32 addr
)
{
    LwU32 value = 0;
    LwU32 extErrStatVal = 0;

    const volatile LwU32 *pAddr = (const volatile LwU32 *)(addr | DMEM_APERTURE_OFFSET);
    value = *pAddr;

    // Doing CSB read to get the Status, since BAR0 read has higher chances of failure.
    extErrStatVal = falc_ldxb_i_with_halt(LW_CPWR_FALCON_EXTERRSTAT);

    if (!FLD_TEST_DRF(_CPWR, _FALCON_EXTERRSTAT, _STAT, _ACK_POS, extErrStatVal) ||
        ((value & ACR_EXT_INTERFACE_MASK_VALUE) == ACR_EXT_INTERFACE_BAD_VALUE))
    {
        lwuc_halt();
    }

    return value;
}

/*!
 * @brief Wrapper Funtion to write register value through External Interface when DMEM Apertue is enabled
 *        and halt in case of Priv Error.
 *        It also reads LW_CPWR_FALCON_EXTERRSTAT to make sure there is no failure. It halts the falcon in case there is some failure.\n
 *
 * @param[in] addr address of the register to be written.
 * @param[in] data value to be written to register.
 */
void
acrBar0RegWriteDmemApert
(
    LwU32 addr,
    LwU32 data
)
{
    LwU32 extErrStatVal = 0;

    volatile LwU32 *pAddr = (volatile LwU32 *)(addr | DMEM_APERTURE_OFFSET);
    *pAddr = data;

    // Doing CSB read to get the Status, since BAR0 read has higher chances of failure.
    extErrStatVal = falc_ldxb_i_with_halt(LW_CPWR_FALCON_EXTERRSTAT);

    if (!FLD_TEST_DRF(_CPWR, _FALCON_EXTERRSTAT, _STAT, _ACK_POS, extErrStatVal))
    {
        lwuc_halt();
    }
}
#else // DMEM_APERT_ENABLED
/*!
 * @brief Wait for BAR0 to become idle
 */
void
_acrBar0WaitIdle(void)
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
                lwuc_halt();
        }
    }
    while (!bDone);

}

LwU32
_acrBar0RegRead
(
    LwU32 addr
)
{
    LwU32  val;

    ACR_REG_WR32(CSB, LW_CSEC_BAR0_TMOUT, (1<<17));
    _acrBar0WaitIdle();
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_ADDR, addr);

    ACR_REG_WR32_STALL(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _READ) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF  ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE));

    (void)ACR_REG_RD32(CSB, LW_CSEC_BAR0_CSR);

    _acrBar0WaitIdle();
    val = ACR_REG_RD32(CSB, LW_CSEC_BAR0_DATA);
    return val;
}

void
_acrBar0RegWrite
(
    LwU32 addr,
    LwU32 data
)
{
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_TMOUT, (1<<17));
    _acrBar0WaitIdle();
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_ADDR, addr);
    ACR_REG_WR32(CSB, LW_CSEC_BAR0_DATA, data);

    ACR_REG_WR32(CSB, LW_CSEC_BAR0_CSR,
        DRF_DEF(_CSEC, _BAR0_CSR, _CMD ,        _WRITE) |
        DRF_NUM(_CSEC, _BAR0_CSR, _BYTE_ENABLE, 0xF   ) |
        DRF_DEF(_CSEC, _BAR0_CSR, _TRIG,        _TRUE ));

    (void)ACR_REG_RD32(CSB, LW_CSEC_BAR0_CSR);
    _acrBar0WaitIdle();

}
#endif // DMEM_APERT_ENABLED
#endif // UPROC_RISCV
