/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpudev.h"
#include "gv100gpu.h"
#include "core/include/platform.h"
#include "gpu/include/protmanager.h"
#include "volta/gv100/dev_h2j_unlock.h"
#include "js_gpu.h"
#include "maxwell/gm200/dev_fb.h"
#include "gpu/hbm/hbm_spec_devid.h"

GV100GpuSubdevice::GV100GpuSubdevice
(
    const char *       name,
    Gpu::LwDeviceId    deviceId,
    const PciDevInfo * pPciDevInfo
) :
    VoltaGpuSubdevice(name, deviceId, pPciDevInfo)
{
}

//------------------------------------------------------------------------------
/* virtual */
void GV100GpuSubdevice::SetupBugs()
{
    VoltaGpuSubdevice::SetupBugs();

    if (static_cast<GpuSubdevice*>(this)->IsInitialized())
    {
        // Setup bugs that require the system to be properly initialized
        // (i.e. LwRm can be accessed, all GpuSubdevice data setup, registers
        // can be accessed, etc.)

        SetHasBug(1804258); //Compiling a compute shader causes compute init in GL which is broken
        SetHasBug(1842362); //Injecting HDRPARITYERR can cause hw hangs
        SetHasBug(1923924); // VGA bundle timeout issue
        SetHasBug(2114920); // Vulkan timestamps are intermittently incorrect
    }
    else
    {
        // Setup bugs that do not require the system to be properly initialized

        // Allow dramclk to be programmed at devinit with "-dramclk_war_mhz"
        // Also need special GetClock/SetClock methods for dramclk until the RM
        // gets its FB falcon microcode working
        SetHasBug(200167665);

        SetHasBug(200296583); //Possible bad register read: addr: 0x61d98c

        // AMAP WAR for dual-rank HBM
        SetHasBug(1748287);
    }
}

//--------------------------------------------------------------------
/* virtual */
bool GV100GpuSubdevice::GetHBMTypeInfo(string *pVendor, string *pRevision) const
{
    using HbmManufacturerId = HbmDeviceIdData::Manufacturer;

    if (GpuSubdevice::GetHBMTypeInfo(pVendor, pRevision))
    {
        return true;
    }

    // If we weren't succesful getting HBM info from HBM fuses, try getting the info from GPU fuses
    Printf(Tee::PriLow, "Unable to get HBM info from HBM fuses, trying GPU fuses instead\n");

    MASSERT(pVendor);
    MASSERT(pRevision);

    const RegHal& regs = TestDevice::Regs();

    // HBM Manufacturer Id for GV100 is stored in spare bits 28:31
    vector<UINT32> hbmManufacturerIdBits =
    {
        regs.Read32(MODS_FUSE_SPARE_BIT_31),
        regs.Read32(MODS_FUSE_SPARE_BIT_30),
        regs.Read32(MODS_FUSE_SPARE_BIT_29),
        regs.Read32(MODS_FUSE_SPARE_BIT_28)
    };

    UINT32 hbmManufacturerId = 0;
    for (UINT32 reg: hbmManufacturerIdBits)
    {
        hbmManufacturerId = (hbmManufacturerId << 1) | reg;
    }

    // HBM Model Part Number for GV100 is stored in spare bits 20:26
    vector<UINT32> hbmModelPartNumberBits =
    {
        regs.Read32(MODS_FUSE_SPARE_BIT_26),
        regs.Read32(MODS_FUSE_SPARE_BIT_25),
        regs.Read32(MODS_FUSE_SPARE_BIT_24),
        regs.Read32(MODS_FUSE_SPARE_BIT_23),
        regs.Read32(MODS_FUSE_SPARE_BIT_22),
        regs.Read32(MODS_FUSE_SPARE_BIT_21),
        regs.Read32(MODS_FUSE_SPARE_BIT_20)
    };

    UINT32 hbmModelPartNumber = 0;
    for (UINT32 reg: hbmModelPartNumberBits)
    {
        hbmModelPartNumber = (hbmModelPartNumber << 1) | reg;
    }

    const UINT32 hbmStackHeight = regs.Read32(MODS_FUSE_SPARE_BIT_27) ? 8 : 4;

    *pRevision = HbmDeviceIdData::ColwertModelPartNumberToRevisionStr(
        static_cast<HbmManufacturerId>(hbmManufacturerId),
        hbmModelPartNumber,
        hbmStackHeight);

    *pVendor = HbmDeviceIdData::ColwertManufacturerIdToVendorName(
        static_cast<HbmManufacturerId>(hbmManufacturerId));

    return true;
}

/* virtual */ RC GV100GpuSubdevice::GetHbmLinkRepairRegister
(
    const LaneError& laneError
    ,UINT32* pOutRegAddr
) const
{
    RC rc;

    const bool withDwordSwizzle = false;
    CHECK_RC(GpuSubdevice::GetHbmLinkRepairRegister(laneError, pOutRegAddr, withDwordSwizzle));

    return rc;
}

namespace
{
    constexpr UINT32 hbmPllRefFreqkHz = 27000;
}

RC GV100GpuSubdevice::CallwlateDivsForTargetFreq
(
    UINT32 targetkHz,
    UINT32* pPDiv,
    UINT32* pNDiv,
    UINT32* pMDiv
)
{
    RC rc;

    /*
     * Target Freq = (Reference Freq  * N) / (M * P)
     * HBM PLL Reference Freq = 27MHz
     *
     * 600 < Vco < 1200 mhz (+/- 10% margin)
     * Vco = Refclk * N/M
     * 1 <= M <= 2 because of update rate
     * 13.5 < (update rate = refclk/m) < 27
     *
     * N >= 40 (comes from HBM PLL refman)
     * MED team has requested a max NDIV of 99
     * Therefore, 40 <= N <= 99
     *
     * If N is outside of this range, glitching can occur.
     *
     * Algorithm: Iterate through PDIV values in powers of 2. Because there
     *            is a fixed range of values for N, we get the following table:
     *
     *
     *            MDIV | PDIV | Target freq MHz (N=40) | Target Freq MHz (N=99)
     *            -------------------------------------------------------------
     *            1    | 1    | 1080                   | 2673
     *            2    | 1    | 540                    | 1336.5
     *            2    | 2    | 270                    | 668.25
     *            2    | 4    | 135                    | 334.125
     *            2    | 8    | 67.5                   | 167.0625
     *
     *            Pick the closest MDIV/PDIV to the target freq and callwlate NDIV
     *            from the above equation
     */
    constexpr UINT32 ndivMin = 40;
    constexpr UINT32 ndivMax = 99;

    if (targetkHz > 1336500)
    {
        *pPDiv = 1;
        *pMDiv = 1;
    }
    else if (targetkHz > 668250)
    {
        *pPDiv = 1;
        *pMDiv = 2;
    }
    else if (targetkHz > 334125)
    {
        *pPDiv = 2;
        *pMDiv = 2;
    }
    else if (targetkHz > 167062)
    {
        *pPDiv = 4;
        *pMDiv = 2;
    }
    else
    {
        *pPDiv = 8;
        *pMDiv = 2;
    }

    *pNDiv = (targetkHz * (*pPDiv) * (*pMDiv) + (hbmPllRefFreqkHz / 2)) /
             hbmPllRefFreqkHz;

    if (*pNDiv < ndivMin)
    {
        Printf(Tee::PriWarn,
               "Dramclk NDIV %d is below the HW supported NDIV min of %u. "
               "Rounding up to %u.\n",
               *pNDiv, ndivMin, ndivMin);
        *pNDiv = ndivMin;
    }
    else if (*pNDiv > ndivMax)
    {
        Printf(Tee::PriWarn,
               "Dramclk NDIV %d is above the HW supported NDIV max of %u. "
               "Rounding down to %u.\n",
               *pNDiv, ndivMax, ndivMax);
        *pNDiv = ndivMax;
    }

    return rc;
}

RC GV100GpuSubdevice::GetHBMSiteMasterFbpaNumber
(
    const UINT32 siteID
    ,UINT32* const pFbpaNum
) const
{
    MASSERT(pFbpaNum);

    /*
       Can read the following registers to determine the master FBPA on Volta:

       Site A (site ID 0)
       LW_PPRIV_FBP_FBP0_MC_PRIV_FS_CONFIG4 -> Bit 1 is for PA0; Bit 3 is for PA1
       LW_PPRIV_FBP_FBP1_MC_PRIV_FS_CONFIG4 -> Bit 1 is for PA2; Bit 3 is for PA3
       (NOTE only 1 bit i.e. one PA can be programmed to  1’b1 per site)
       Site B (site ID 1)
       LW_PPRIV_FBP_FBP2_MC_PRIV_FS_CONFIG4 -> Bit 1 is for PA4; Bit 3 is for PA5
       LW_PPRIV_FBP_FBP3_MC_PRIV_FS_CONFIG4 -> Bit 1 is for PA6; Bit 3 is for PA7
       Site C (site ID 2)
       LW_PPRIV_FBP_FBP4_MC_PRIV_FS_CONFIG4 -> Bit 1 is for PA8; Bit 3 is for PA9
       LW_PPRIV_FBP_FBP5_MC_PRIV_FS_CONFIG4 -> Bit 1 is for PA10; Bit 3 is for PA11
       Site D (site ID 3)
       LW_PPRIV_FBP_FBP6_MC_PRIV_FS_CONFIG4 -> Bit 1 is for PA12; Bit 3 is for PA13
       LW_PPRIV_FBP_FBP7_MC_PRIV_FS_CONFIG4 -> Bit 1 is for PA14; Bit 3 is for PA15
    */
    const RegHal& regs = TestDevice::Regs();
    const UINT32 firstFBPNum = siteID * 2;
    const UINT32 secondFBPNum = firstFBPNum + 1;
    const UINT32 firstFbpMcConfig
        = regs.Read32(MODS_PPRIV_FBP_FBPx_MC_PRIV_FS_CONFIG, {firstFBPNum, 4});
    const UINT32 secondFbpMcConfig
        = regs.Read32(MODS_PPRIV_FBP_FBPx_MC_PRIV_FS_CONFIG, {secondFBPNum, 4});

    switch (firstFbpMcConfig & 0xA)
    {
    case 2:
        *pFbpaNum = siteID * 4;
        break;
    case 8:
        *pFbpaNum = siteID * 4 + 1;
        break;
    default:
        switch (secondFbpMcConfig & 0xA)
        {
        case 2:
            *pFbpaNum = siteID * 4 + 2;
            break;
        case 8:
            *pFbpaNum = siteID * 4 + 3;
            break;
        default:
            Printf(Tee::PriLow,
                   "Unknown site %u master FBPA, potentially floorswept\n", siteID);
            return RC::SOFTWARE_ERROR;
        }
    }

    return OK;
}

RC GV100GpuSubdevice::GetHBMSiteFbps
(
    const UINT32 siteID,
    UINT32* const pFbpNum1,
    UINT32* const pFbpNum2
) const
{
    MASSERT(pFbpNum1);
    MASSERT(pFbpNum2);

    if (siteID >= (GetMaxFbpCount()/2))
    {
        Printf(Tee::PriError, "Invalid HBM site ID %u\n", siteID);
        return RC::ILWALID_ARGUMENT;
    }

    *pFbpNum1 = siteID * 2;
    *pFbpNum2 = *pFbpNum1 + 1;
    return RC::OK;
}

RC GV100GpuSubdevice::DramclkDevInitWARkHz(UINT32 freqkHz)
{
    RC rc;
    RegHal& regs = TestDevice::Regs();

    UINT32 pdiv, ndiv, mdiv;
    CHECK_RC(CallwlateDivsForTargetFreq(freqkHz, &pdiv, &ndiv, &mdiv));

    const UINT32 regVal =
        regs.SetField(MODS_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF_MDIV, mdiv) |
        regs.SetField(MODS_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF_NDIV, ndiv) |
        regs.SetField(MODS_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF_PLDIV, pdiv);

    Printf(Tee::PriWarn,
           "Using the GV100 dramclk devinit WAR with COEFF register = 0x%08x\n",
           regVal);

    // VBIOS sets dramclk during devinit by setting
    // LW_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF to regVal
    regs.Write32(MODS_PBUS_SW_SCRATCH, 13, regVal);

    return rc;
}

RC GV100GpuSubdevice::GetClock
(
    Gpu::ClkDomain clkToGet,
    UINT64 * pActualPllHz,
    UINT64 * pTargetPllHz,
    UINT64 * pSlowedHz,
    Gpu::ClkSrc * pSrc
)
{
    RegHal& regs = TestDevice::Regs();

    // Do not remove this function or hasbug check until *after* GV100 is in
    // production - even if the SetHasBug has been removed. MED team may want
    // to do experiments with FB falcon disabled.
    if (clkToGet != Gpu::ClkM || !HasBug(200167665))
    {
        return VoltaGpuSubdevice::GetClock(clkToGet, pActualPllHz, pTargetPllHz, pSlowedHz, pSrc);
    }

    UINT32 hbmPllNum;
    if (GetFirstValidHBMPllNumber(&hbmPllNum) != OK)
    {
        // None of the HBM sites appear to be enabled, something is really wrong...
        return RC::HW_STATUS_ERROR;
    }

    UINT64 pllFreqHz = 0;
    UINT32 pllCoeff = 0;

    pllCoeff = regs.Read32(MODS_PFB_FBPA_0_FBIO_HBMPLL_COEFF, hbmPllNum);
    if (!pllCoeff)
    {
        Printf(Tee::PriError, "HBM PLL coeff register is 0\n");
        return RC::HW_STATUS_ERROR;
    }
    const UINT32 mdiv = regs.GetField(pllCoeff,
        MODS_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF_MDIV);
    const UINT32 ndiv = regs.GetField(pllCoeff,
        MODS_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF_NDIV);
    const UINT32 pdiv = regs.GetField(pllCoeff,
        MODS_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF_PLDIV);

    MASSERT((mdiv * pdiv) != 0);

    pllFreqHz = 1000 * hbmPllRefFreqkHz * ndiv / (mdiv * pdiv);

    Printf(Tee::PriDebug,
           "Callwlated MClk freq: %llu, coeff: 0x%x\n", pllFreqHz, pllCoeff);

    if (pActualPllHz)
    {
        *pActualPllHz = pllFreqHz;
    }

    if (pTargetPllHz)
    {
        *pTargetPllHz = pllFreqHz;
    }

    if (pSlowedHz)
    {
        *pSlowedHz = pllFreqHz;
    }

    if (pSrc)
    {
        *pSrc = Gpu::ClkSrcHBMPLL; // Assume that GV100 has HBM
    }

    return OK;
}

RC GV100GpuSubdevice::SetClock
(
    Gpu::ClkDomain clkDomain,
    UINT64 targetHz,
    UINT32 pstateNum,
    UINT32 flags
)
{
    // Do not remove this function or hasbug check until *after* GV100 is in
    // production - even if the SetHasBug has been removed. MED team may want
    // to do experiments with FB falcon disabled.
    if (clkDomain != Gpu::ClkM || !HasBug(200167665))
    {
        return VoltaGpuSubdevice::SetClock(clkDomain, targetHz, pstateNum, flags);
    }

    RC rc;
    RegHal& regs = TestDevice::Regs();

    regs.Write32(MODS_PTRIM_FBPA_BCAST_HBMPLL_CFG0_BYPASSPLL, 1);
    Utility::SleepUS(2);

    UINT32 pdiv, ndiv, mdiv;
    CHECK_RC(CallwlateDivsForTargetFreq(static_cast<UINT32>(targetHz) / 1000,
                                        &pdiv, &ndiv, &mdiv));

    UINT32 hbmPllNum;
    if (GetFirstValidHBMPllNumber(&hbmPllNum) != OK)
    {
        // None of the HBM sites appear to be enabled, something is really wrong...
        return RC::CANNOT_SET_MEMORY_CLOCK;
    }

    UINT32 pllCoeff = regs.Read32(MODS_PFB_FBPA_0_FBIO_HBMPLL_COEFF, hbmPllNum);
    if (!pllCoeff)
    {
        Printf(Tee::PriError, "HBM PLL coeff register is 0\n");
        return RC::HW_STATUS_ERROR;
    }
    regs.SetField(&pllCoeff, MODS_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF_NDIV,
                    ndiv);
    regs.SetField(&pllCoeff, MODS_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF_PLDIV,
                    pdiv);
    regs.SetField(&pllCoeff, MODS_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF_MDIV,
                    mdiv);
    regs.Write32(MODS_PFB_FBPA_MC_2_FBIO_HBMPLL_COEFF, pllCoeff);

    Utility::SleepUS(2);

    regs.Write32(MODS_PTRIM_FBPA_BCAST_HBMPLL_CFG0_BYPASSPLL, 0);

    return OK;

}

RC GV100GpuSubdevice::GetFirstValidHBMPllNumber(UINT32 *pOutHbmPllNum)
{
    // Find the first master FBPA. This will be used to read the HBMPLL
    // coefficient register to determine dramclk.
    UINT32 fbpMask = GetFsImpl()->FbpMask();
    INT32 fbpIndex;
    while ((fbpIndex = Utility::BitScanForward(fbpMask)) != -1)
    {
        fbpMask ^= 1 << fbpIndex;

        UINT32 fsConfig = TestDevice::Regs().Read32(m_Mc2MasterRegs[fbpIndex], 4);
        if (fsConfig & 2)
        {
            *pOutHbmPllNum = fbpIndex * m_FbpsPerSite;
            return OK;
        }
        else if (fsConfig & 8)
        {
            *pOutHbmPllNum = fbpIndex * m_FbpsPerSite + 1;
            return OK;
        }
    }

    return RC::SOFTWARE_ERROR;
}

RC GV100GpuSubdevice::GetAndClearL2EccCounts
(
    UINT32 ltc,
    UINT32 slice,
    L2EccCounts * pL2EccCounts
)
{
    MASSERT(pL2EccCounts);

    if ((ltc >= GetFB()->GetLtcCount()) || (slice >= GetFB()->GetMaxSlicesPerLtc()))
    {
        MASSERT(!"Invalid LTC or SLICE Slice");
        Printf(Tee::PriError, "LTC %u or Slice %u out of range\n",
               ltc, slice);
        return RC::BAD_PARAMETER;
    }

    // Read error count
    RegHal &regs = TestDevice::Regs();
    UINT32 errCountReg = RegRd32(
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_DSTG_ECC_REPORT) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE));

    // Reset error count to 0
    RegWr32(
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_DSTG_ECC_REPORT) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE),
        0);

    pL2EccCounts->correctedTotal    =
        regs.GetField(errCountReg, MODS_PLTCG_LTC0_LTS0_DSTG_ECC_REPORT_SEC_COUNT);
    pL2EccCounts->correctedUnique   = pL2EccCounts->correctedTotal;
    pL2EccCounts->uncorrectedTotal  =
        regs.GetField(errCountReg, MODS_PLTCG_LTC0_LTS0_DSTG_ECC_REPORT_DED_COUNT);
    pL2EccCounts->uncorrectedUnique = pL2EccCounts->uncorrectedTotal;
    return RC::OK;
}

RC GV100GpuSubdevice::SetL2EccCounts
(
    UINT32 ltc,
    UINT32 slice,
    const L2EccCounts & l2EccCounts
)
{
    if ((ltc >= GetFB()->GetLtcCount()) || (slice >= GetFB()->GetMaxSlicesPerLtc()))
    {
        MASSERT(!"Invalid LTC or SLICE Slice");
        Printf(Tee::PriError, "LTC %u or Slice %u out of range\n",
               ltc, slice);
        return RC::BAD_PARAMETER;
    }

    UINT32 errCountReg = 0;
    RegHal &regs = TestDevice::Regs();
    regs.SetField(&errCountReg,
                  MODS_PLTCG_LTC0_LTS0_DSTG_ECC_REPORT_SEC_COUNT,
                  l2EccCounts.uncorrectedTotal);
    regs.SetField(&errCountReg,
                  MODS_PLTCG_LTC0_LTS0_DSTG_ECC_REPORT_DED_COUNT,
                  l2EccCounts.uncorrectedTotal);
    RegWr32(
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_DSTG_ECC_REPORT) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE),
        errCountReg);
    return RC::OK;
}

RC GV100GpuSubdevice::GetAndClearL2EccInterrupts
(
    UINT32 ltc,
    UINT32 slice,
    bool *pbCorrInterrupt,
    bool *pbUncorrInterrupt
)
{
    MASSERT(pbCorrInterrupt);
    MASSERT(pbUncorrInterrupt);

    if ((ltc >= GetFB()->GetLtcCount()) || (slice >= GetFB()->GetMaxSlicesPerLtc()))
    {
        MASSERT(!"Invalid LTC or SLICE Slice");
        Printf(Tee::PriError, "LTC %u or Slice %u out of range\n",
               ltc, slice);
        return RC::BAD_PARAMETER;
    }

    RegHal &regs = TestDevice::Regs();
    const UINT32 regAddr =
        regs.LookupAddress(MODS_PLTCG_LTC0_LTS0_INTR) +
        ltc * regs.LookupAddress(MODS_LTC_PRI_STRIDE) +
        slice * regs.LookupAddress(MODS_LTS_PRI_STRIDE);
    UINT32 interruptReg = RegRd32(regAddr);

    *pbCorrInterrupt   = regs.TestField(interruptReg,
                                        MODS_PLTCG_LTC0_LTS0_INTR_ECC_SEC_ERROR_PENDING);
    *pbUncorrInterrupt = regs.TestField(interruptReg,
                                        MODS_PLTCG_LTC0_LTS0_INTR_ECC_DED_ERROR_PENDING);

    // Reset ECC interrupts
    interruptReg = 0;
    regs.SetField(&interruptReg, MODS_PLTCG_LTC0_LTS0_INTR_ECC_SEC_ERROR_RESET);
    regs.SetField(&interruptReg, MODS_PLTCG_LTC0_LTS0_INTR_ECC_DED_ERROR_RESET);
    RegWr32(regAddr, interruptReg);

    return RC::OK;
}

double GV100GpuSubdevice::GetArchEfficiency(Instr type)
{
    // Values from https://wiki.lwpu.com/gpuhwvolta/index.php/Volta_Decoder_Ring
    switch (type)
    {
        case Instr::FFMA:
            return 128.0;
        case Instr::DFMA:
            return 64.0;
        case Instr::HFMA2:
            return 256.0;
        case Instr::HMMA_884_F16_F16:
            return 1024.0;
        case Instr::HMMA_884_F32_F16:
            return 1024.0;
        case Instr::IDP4A_S32_S8:
            return 512.0;
        default:
            return 0.0;
    }
}

UINT32 GV100GpuSubdevice::GetNumHbmSites() const
{
    return 4;
}

RC GV100GpuSubdevice::CheckHbmIeee1500RegAccess(const HbmSite& hbmSite) const
{
    // NOTE(stewarts): Volta has been in the field for a long time and we know
    // we have permissions when appropriate. Can fill this out later if needed.
    return RC::OK;
}

CREATE_GPU_SUBDEVICE_FUNCTION(GV100GpuSubdevice);
