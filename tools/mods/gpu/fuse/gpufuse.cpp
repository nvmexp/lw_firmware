/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/version.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpudev.h"
#include "gpufuse.h"
#include "gm10x_fuse.h"
#include "gm20x_fuse.h"
#include "gp10x_fuse.h"
#include "gv100_fuse.h"
#include "core/include/fuseparser.h"
#include "core/tests/testconf.h"
#include "fusesrc.h"
#include "core/include/utility.h"
#include "gpu/perf/perfsub.h"
#include "core/include/crypto.h"
#include "core/include/fileholder.h"
#include "gpu/utility/scopereg.h"
#include "core/include/platform.h"
#include "mods_reg_hal.h"
#include "gpu/include/floorsweepimpl.h"
#include <algorithm>

//-----------------------------------------------------------------------------
GpuFuse::GpuFuse(Device *pSubdev) :
    OldFuse(pSubdev),
    m_AvailableRecIdx(0)
{
    // see bug 684219
    m_ReblowAttempts = 0;
    m_MarginReadEnabled = false;
    m_WaitForIdleEnabled = true;
    m_FuseReadDirty = true;
}
//-----------------------------------------------------------------------------
// static function: Fuse Factory
GpuFuse* GpuFuse::CreateFuseObj(GpuSubdevice* pSubdev)
{
    GpuFuse *pGpuFuse = NULL;

    switch (pSubdev->DeviceId())
    {
#if LWCFG(GLOBAL_ARCH_MAXWELL)
        case Gpu::GM108:
        case Gpu::GM107:
            pGpuFuse = new GM10xFuse(pSubdev);
            break;
        case Gpu::GM200:
        case Gpu::GM204:
        case Gpu::GM206:
            pGpuFuse = new GM20xFuse(pSubdev);
            break;
#endif
#if LWCFG(GLOBAL_ARCH_PASCAL)
        case Gpu::GP100:
        case Gpu::GP102:
        case Gpu::GP104:
        case Gpu::GP106:
        case Gpu::GP107:
        case Gpu::GP108:
            pGpuFuse = new GP10xFuse(pSubdev);
            break;
#endif
#if LWCFG(GLOBAL_ARCH_VOLTA)
        case Gpu::GV100:
            pGpuFuse = new GV100Fuse(pSubdev);
            break;
#endif
        default:
            pGpuFuse = new GV100Fuse(pSubdev);
            break;
    }

    return pGpuFuse;
}
//-----------------------------------------------------------------------------
RC GpuFuse::CacheFuseReg()
{
    if (AreSimFusesUsed())
    {
        m_FuseReg.clear();
        m_FuseReadDirty = true;
        return GetSimulatedFuses(&m_FuseReg);
    }

    // Is a fuse re-read needed?
    if (!m_FuseReadDirty)
        return OK;

    Tasker::MutexHolder mh(GetMutex());

    if (!m_FuseReadDirty)
        return OK;

    RC rc;
    m_FuseReg.clear();
    TestConfiguration LwrrTestConfig;
    CHECK_RC(LwrrTestConfig.InitFromJs());
    FLOAT64 TimeoutMs = LwrrTestConfig.TimeoutMs();

    for (UINT32 i=0; i < m_FuseSpec.NumOfFuseReg; i++)
    {
        UINT32 FuseReg;
        CHECK_RC(GetFuseWord(i, TimeoutMs, &FuseReg));
        m_FuseReg.push_back(FuseReg);

        // lower priority
        if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
            Printf(Tee::PriDebug, "Fuse %02d : = 0x%08x\n", i, FuseReg);
    }
    CHECK_RC(FindConsistentRead(&m_FuseReg, TimeoutMs));

    m_FuseReadDirty = false;
    return OK;
}
//-----------------------------------------------------------------------------
RC GpuFuse::GetCachedFuseReg(vector<UINT32> *pCachedReg)
{
    MASSERT(pCachedReg);
    RC rc;
    CHECK_RC(CacheFuseReg());
    *pCachedReg = m_FuseReg;
    return OK;
}
//-----------------------------------------------------------------------------
RC GpuFuse::GetFuseWord(UINT32 fuseRow, FLOAT64 TimeoutMs, UINT32 *pRetData)
{
    MASSERT(pRetData);
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    RegHal &gpuRegs = pSubdev->Regs();
    ScopedRegister fuseCtrl(pSubdev, gpuRegs.LookupAddress(MODS_FUSE_FUSECTRL));

    bool FuseWasVisible = IsFuseVisible();
    // set to visible
    SetFuseVisibility(true);

    // set the Row of the fuse
    UINT32 fuseAddr = gpuRegs.Read32(MODS_FUSE_FUSEADDR);
    gpuRegs.SetField(&fuseAddr, MODS_FUSE_FUSEADDR_VLDFLD, fuseRow);
    gpuRegs.Write32(MODS_FUSE_FUSEADDR, fuseAddr);

    // set fuse control to 'read'
    UINT32 fuseCtrlRead = fuseCtrl;
    gpuRegs.SetField(&fuseCtrlRead, MODS_FUSE_FUSECTRL_CMD_READ);
    gpuRegs.Write32(MODS_FUSE_FUSECTRL, fuseCtrlRead);

    // wait for state to become idle
    RC rc = POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs);

    *pRetData = gpuRegs.Read32(MODS_FUSE_FUSERDATA);

    SetFuseVisibility(FuseWasVisible);
    return rc;
}
//-----------------------------------------------------------------------------
RC GpuFuse::LatchFuseToOptReg(FLOAT64 TimeoutMs)
{
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    RegHal &gpuRegs = pSubdev->Regs();
    UINT32 pri2Intfc = 0;
    gpuRegs.SetField(&pri2Intfc, MODS_FUSE_PRIV2INTFC_SKIP_RECORDS_DATA, 1);
    gpuRegs.Write32(MODS_FUSE_PRIV2INTFC, pri2Intfc);
    gpuRegs.SetField(&pri2Intfc, MODS_FUSE_PRIV2INTFC_START_DATA, 1);
    gpuRegs.Write32(MODS_FUSE_PRIV2INTFC, pri2Intfc);
    Platform::Delay(1);
    gpuRegs.Write32(MODS_FUSE_PRIV2INTFC, 0);   // reset _START and _RAMREPAIR
    Platform::Delay(4);
    return POLLWRAP_HW(PollFuseSenseDone, pSubdev, TimeoutMs);
}
//-----------------------------------------------------------------------------
bool GpuFuse::CanOnlyUseOptFuses()
{
    return !Platform::HasClientSideResman() &&
        static_cast<GpuSubdevice*>(GetDevice())->HasBug(1722075);
}
//-----------------------------------------------------------------------------
UINT32 GpuFuse::ExtractFuseVal(const FuseUtil::FuseDef* pDef, Tolerance Strictness)
{
    if (pDef->Type == FuseUtil::TYPE_PSEUDO)
    {
        Printf(Tee::PriError, "%s is a pseudo-fuse! Cannot extract value!\n",
            pDef->Name.c_str());
        return 0;
    }

    if (pDef->Type == "fuseless")
    {
        return ExtractFuselessFuseVal(pDef, Strictness);
    }
    MASSERT(pDef);
    UINT32 FuseVal  = 0;
    list<FuseUtil::FuseLoc>::const_reverse_iterator FuseIter =
        pDef->FuseNum.rbegin();

    for (; FuseIter != pDef->FuseNum.rend(); FuseIter++)
    {
        GetPartialFuseVal(FuseIter->Number,
                          FuseIter->Lsb,
                          FuseIter->Msb,
                         &FuseVal);
    }

    // Get the Redundant fuse value
    UINT32 RdFuseVal = 0;
    FuseIter = pDef->RedundantFuse.rbegin();
    for (; FuseIter != pDef->RedundantFuse.rend(); FuseIter++)
    {
        GetPartialFuseVal(FuseIter->Number,
                          FuseIter->Lsb,
                          FuseIter->Msb,
                         &RdFuseVal);
    }

    if (Strictness & OldFuse::REDUNDANT_MATCH)
        return (FuseVal | RdFuseVal);
    else
        return FuseVal;
    return 0;
}
//-----------------------------------------------------------------------------
UINT32 GpuFuse::ExtractFuselessFuseVal(const FuseUtil::FuseDef* pDef, Tolerance Strictness)
{
    if (0 == m_Records.size())
    {
        if (OK != ReadInRecords())
        {
            Printf(Tee::PriNormal, "Cannot read in fuse records\n");
            return 0;
        }
    }

    // todo: lower the print priority here to Debug or delete them
    Printf(Tee::PriDebug, "Extract %s\n", pDef->Name.c_str());
    UINT32 FuseVal = 0;
    // need to reconstruct the fuse from subfuses. Go from MSB subfuses to LSB
    list<FuseUtil::FuseLoc>::const_reverse_iterator SubFuseIter;
    SubFuseIter = pDef->FuseNum.rbegin();
    for (; SubFuseIter != pDef->FuseNum.rend(); SubFuseIter++)
    {
        Printf(Tee::PriDebug, " ChainId;0x%x, Offset:0x%x, "
                               "DataShift:0x%x, Msb:0x%x, Lsb:0x%x||",
               SubFuseIter->ChainId, SubFuseIter->ChainOffset,
               SubFuseIter->DataShift, SubFuseIter->Msb, SubFuseIter->Lsb);

        // just like DRF_VAL:
        UINT32 NumBits = SubFuseIter->Msb - SubFuseIter->Lsb + 1;
        UINT32 Mask    = 0xFFFFFFFF >> (32 - NumBits);
        // get the subfuse value - if it's set, it'd be in the Jtag Chain
        UINT32 SubFuse = GetJtagChainData(SubFuseIter->ChainId,
                                          SubFuseIter->Msb,
                                          SubFuseIter->Lsb);
        FuseVal        = (FuseVal << NumBits) | SubFuse;
        Printf(Tee::PriDebug,
               "NumBits %d, Mask:%d, SubFuse:0x%0x, FuseVal:0x%x",
               NumBits, Mask, SubFuse, FuseVal);

        Printf(Tee::PriDebug, "\n");
    }

    return FuseVal;
}
//-----------------------------------------------------------------------------
UINT32 GpuFuse::ExtractOptFuseVal(const FuseUtil::FuseDef* pDef)
{
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    bool FuseVisible = IsFuseVisible();
    SetFuseVisibility(true);
    UINT32 Offset = pDef->PrimaryOffset.Offset;
    UINT32 FuseVal = pSubdev->RegRd32(Offset);
    SetFuseVisibility(FuseVisible);
    return FuseVal;
}
//-----------------------------------------------------------------------------
//  Get a portion of the fuse value from one register
// With '2D' fuses, a 'fuse' can be a collection of bits at different
// fuse rows. For example the OPT_DAC_CRT_MAP fuse on GT218 is on
// bit 31:31 of row 12, and bit 6:0 of row 14. In FuseXml parsing,
// the FuseLoc is stored by 'push_back' with LEAST SIGNIFICANT BITS FIRST
// In ExtractFuseVal, this function can be called multiple times in order to
// construct the value of a Fuse.
// This function will act on pFuseVal; it will be updated for each interaction
void GpuFuse::GetPartialFuseVal(UINT32 RowNum,
                                  UINT32 Lsb,
                                  UINT32 Msb,
                                  UINT32 *pFuseVal)
{
    if (RowNum >= m_FuseReg.size())
    {
        Printf(Tee::PriNormal, "XML error... fuse number too large\n");
        MASSERT(!"XML ERROR\n");
        return;
    }
    UINT32 RowVal = m_FuseReg[RowNum];
    // this is kind of like doing a DRF_VAL()
    UINT32 NumBits = Msb - Lsb + 1;
    MASSERT(NumBits <= 32);
    UINT32 Mask    = static_cast<UINT32>((1ULL<<NumBits) - 1);
    UINT32 Partial = (RowVal >> Lsb) & Mask;

    *pFuseVal = (*pFuseVal << NumBits) | Partial;
}
//-----------------------------------------------------------------------------
RC GpuFuse::SetPartialFuseVal(const list<FuseUtil::FuseLoc> &FuseLocList,
                                UINT32 DesiredValue,
                                vector<UINT32> *pRegsToBlow)
{
    list<FuseUtil::FuseLoc>::const_iterator FuseLocIter = FuseLocList.begin();
    // don't do anything - if there are no fuses (eg redundant fuses)
    if (FuseLocIter == FuseLocList.end())
        return OK;

    // Example: OPT_DAC_CRT_MAP row 12, bit 31:31, row 14, bit 6:0
    // MSB is row 12 31:31, then row 14 6:0
    // Construct the LSB of desired Value first, then right shift the desired
    // value by (MSB-LSB+1) each iteraction until we marked all the bits
    // in the given fuse
    for (; FuseLocIter != FuseLocList.end(); ++FuseLocIter)
    {
        UINT32 NumBits = FuseLocIter->Msb - FuseLocIter->Lsb + 1;
        MASSERT(NumBits <= 32);
        UINT32 Mask    = static_cast<UINT32>((1ULL<<NumBits) - 1);
        UINT32 Row     = FuseLocIter->Number;

        UINT32 ValInRow = (DesiredValue & Mask);
        (*pRegsToBlow)[Row] |= ValInRow << FuseLocIter->Lsb;
        if (ValInRow)
        {
            Printf(GetPrintPriority(), "Row,Val = (%d, 0x%x)",
                   Row, (*pRegsToBlow)[Row]);
        }
        DesiredValue >>= NumBits;
    }
    return OK;
}

RC GpuFuse::ParseXmlInt(const FuseUtil::FuseDefMap **ppFuseDefMap,
                        const FuseUtil::SkuList    **ppSkuList,
                        const FuseUtil::MiscInfo   **ppMiscInfo,
                        const FuseDataSet          **ppFuseDataSet)
{
    RC rc;
    string fuseFileName = GetFuseFilename();
    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fuseFileName));
    CHECK_RC(pParser->ParseFile(fuseFileName,
                                ppFuseDefMap,
                                ppSkuList,
                                ppMiscInfo,
                                ppFuseDataSet));
    return rc;
}
//-----------------------------------------------------------------------------
bool GpuFuse::IsFuseSupported(const FuseUtil::FuseInfo &Spec, Tolerance Strictness)
{
    if (!OldFuse::IsFuseSupported(Spec, Strictness))
        return false;

    CheckPriority FuseLevel = (OldFuse::CheckPriority)Spec.pFuseDef->Visibility;
    if (Strictness & OldFuse::LWSTOMER_MATCH)
        return (OldFuse::LEVEL_LWSTOMER <= FuseLevel);
    return (GetCheckLevel() <= FuseLevel);
}
//-----------------------------------------------------------------------------
RC GpuFuse::BlowArray(const vector<UINT32>  &RegsToBlow,
                      string                Method,
                      UINT32                DurationNs,
                      UINT32                LwVddMv)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        return OK;

    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    RC rc;
    vector<UINT32>FuseReg;
    Utility::CleanupOnReturn<GpuFuse, RC> WrPrivSecSwFuse(this, &GpuFuse::RestorePrivSelwritySwFuse);
    WrPrivSecSwFuse.Cancel();

    // Bug 1366704 WAR
    // Attempting to blow HW fuses when priv security is enabled in SW fuses
    // will fail because the LW_FUSE_XXX registers to control HW fusing
    // are protected.
    // So disable priv security in SW fuses before proceeding
    if (IsSwFusingEnabled() && IsFusePrivSelwrityEnabled())
    {
        CHECK_RC(CachePrivSelwritySwFuse());
        WrPrivSecSwFuse.Activate();
        CHECK_RC(ClearPrivSelwritySwFuse());
    }

    CHECK_RC(GetCachedFuseReg(&FuseReg));
    if (FuseReg.size() != RegsToBlow.size())
    {
        Printf(Tee::PriNormal, "Input fuse array size mismatch.\n");
        return RC::BAD_PARAMETER;
    }

    // OR the current value and desired values together
    for (UINT32 i = 0 ; i < FuseReg.size(); i++)
    {
        FuseReg[i] |= RegsToBlow[i];
    }

    // make instance of Fuse source [init func to make sure ctor doesn't fail]
    FuseSource FuseSrc;
    CHECK_RC(FuseSrc.Initialize(pSubdev, Method));

    // set up vdd, fuse blow time
    UINT32 OldLwVdd;
    Perf *pPerf = pSubdev->GetPerf();
    if (pPerf)
    {
        CHECK_RC(pPerf->GetCoreVoltageMv(&OldLwVdd));
    }

    if (DurationNs)
    {
        SetFuseBlowTime(DurationNs);
    }

    // blow
    bool Reblow = false;
    UINT32 AttemptsLeft = GetReblowAttempts();
    do
    {
        rc = BlowFusesInt(FuseReg, LwVddMv, &FuseSrc);
        Reblow = ((OK != rc) && (AttemptsLeft > 0));
        AttemptsLeft--;
    } while (Reblow);

    // Cleanup:
    // restore volrate settings
    if (pPerf)
    {
        Printf(Tee::PriNormal, "Restore lwvdd to %d mV\n", OldLwVdd);
        pPerf->SetCoreVoltageMv(OldLwVdd);
    }

    // Note : SLT does not want to restore fuse register (LW_FUSE_FUSETIME2)!
    return rc;
}
//-----------------------------------------------------------------------------
RC GpuFuse::BlowFusesInt(const vector<UINT32> &RegsToBlow,
                         UINT32                LwVddMv,
                         FuseSource*           pFuseSrc)
{
    MASSERT(pFuseSrc);
    RC rc;
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    MASSERT(pSubdev);
    // mark dirty regardless of whether this funciton succeeds. Even if it
    // fails mid way through, some bits might have been blown
    MarkFuseReadDirty();

    // set the required LW VDD
    if (LwVddMv > 0 && pSubdev->GetPerf())
    {
        CHECK_RC(pSubdev->GetPerf()->SetCoreVoltageMv(LwVddMv));
        Printf(GetPrintPriority(), "Set lwvdd to %d mV\n", LwVddMv);
    }

    // get the original fuse values:
    vector<UINT32> OrignalValues;   // read the fuse
    vector<UINT32> NewBitsArray;    // new bits to be blown

    Printf(GetPrintPriority(), "New Bits   | Expected Final Value \n");
    Printf(GetPrintPriority(), "----------   ----------\n");

    TestConfiguration LwrrTestConfig;
    CHECK_RC(LwrrTestConfig.InitFromJs());
    FLOAT64 TimeoutMs = LwrrTestConfig.TimeoutMs();

    for (UINT32 i = 0; i < m_FuseSpec.NumOfFuseReg; i++)
    {
        UINT32 FuseReg;
        CHECK_RC(GetFuseWord(i, TimeoutMs, &FuseReg));
        OrignalValues.push_back(FuseReg);
        UINT32 NewBits = RegsToBlow[i] & ~FuseReg;
        NewBitsArray.push_back(NewBits);

        Printf(GetPrintPriority(), "0x%08x : 0x%08x\n", NewBits, RegsToBlow[i]);
    }

    bool FuseIsVisible = IsFuseVisible();

    rc = BlowFuseRows(NewBitsArray, RegsToBlow, pFuseSrc, TimeoutMs);

    SetFuseVisibility(FuseIsVisible);
    pFuseSrc->Toggle(FuseSource::TO_LOW);
    CHECK_RC(rc);
    Tasker::Sleep(m_FuseSpec.SleepTime[FuseSpec::SLEEP_AFTER_GPIO_LOW]);

    CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));
    Printf(Tee::PriNormal, "Fuse blow completed.\n");

    MarkFuseReadDirty();
    // verify whether we have the written the right data:
    vector<UINT32> ReRead;
    for (UINT32 i = 0; i < m_FuseSpec.NumOfFuseReg; i++)
    {
        UINT32 FuseReg;
        CHECK_RC(GetFuseWord(i, TimeoutMs, &FuseReg));
        ReRead.push_back(FuseReg);
    }
    CHECK_RC(FindConsistentRead(&ReRead, TimeoutMs));
    bool FailFuseVerif = false;
    for (UINT32 i = 0; i < m_FuseSpec.NumOfFuseReg; i++)
    {
        Printf(Tee::PriNormal, "Final Fuse value at row %d: 0x%x\n", i, ReRead[i]);
        if (ReRead[i] != RegsToBlow[i])
        {
            Printf(Tee::PriNormal,"  Expected data 0x%08x != 0x%08x at row%d\n",
                   RegsToBlow[i], ReRead[i], i);
            FailFuseVerif = true;
        }
    }
    if (FailFuseVerif)
    {
        CHECK_RC(RC::VECTORDATA_VALUE_MISMATCH);
    }

    // Make fuse visible after fuse blow: details in bug 569056
    CHECK_RC(LatchFuseToOptReg(TimeoutMs));
    return rc;
}
//-----------------------------------------------------------------------------
RC GpuFuse::BlowFuseRows
(
    const vector<UINT32> &NewBitsArray,
    const vector<UINT32> &RegsToBlow,
    FuseSource*           pFuseSrc,
    FLOAT64               TimeoutMs
)
{
    RC rc;
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    RegHal &gpuRegs = pSubdev->Regs();
    // make fuse visible
    ScopedRegister fuseCtrl(pSubdev, gpuRegs.LookupAddress(MODS_FUSE_FUSECTRL));

    UINT32 fuseCtrlSenseCtl = fuseCtrl;
    gpuRegs.SetField(&fuseCtrlSenseCtl, MODS_FUSE_FUSECTRL_CMD_SENSE_CTRL);
    gpuRegs.Write32(MODS_FUSE_FUSECTRL, fuseCtrlSenseCtl);

    CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));

    SetFuseVisibility(true);
    bool FuseSrcHigh = false;

    for (UINT32 i = 0; i < m_FuseSpec.NumOfFuseReg; i++)
    {
        if (Utility::CountBits(NewBitsArray[i]) == 0)
        {
            // don't bother attempting to blow this row if there's no new bits
            continue;
        }

        // wait until state is idle
        CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));

        // set the Row of the fuse
        UINT32 fuseAddr = gpuRegs.Read32(MODS_FUSE_FUSEADDR);
        gpuRegs.SetField(&fuseAddr, MODS_FUSE_FUSEADDR_VLDFLD, i);
        gpuRegs.Write32(MODS_FUSE_FUSEADDR, fuseAddr);

        // write the new fuse bits in
        gpuRegs.Write32(MODS_FUSE_FUSEWDATA, NewBitsArray[i]);

        if (!FuseSrcHigh)
        {
            // Flick FUSE_SRC to high
            CHECK_RC(pFuseSrc->Toggle(FuseSource::TO_HIGH));
            Tasker::Sleep(m_FuseSpec.SleepTime[FuseSpec::SLEEP_AFTER_GPIO_HIGH]);
            FuseSrcHigh = true;
        }

        // issue the Write command
        UINT32 fuseCtrlWrite = fuseCtrl;
        gpuRegs.SetField(&fuseCtrlWrite, MODS_FUSE_FUSECTRL_CMD_WRITE);
        gpuRegs.Write32(MODS_FUSE_FUSECTRL, fuseCtrlWrite);
        CHECK_RC(POLLWRAP_HW(PollFusesIdleWrapper, this, TimeoutMs));
    }
    return rc;
}

//-----------------------------------------------------------------------------
UINT32 GpuFuse::SetFuseBlowTime(UINT32 NewTimeNs)
{
    if (NewTimeNs == 0)
    {
        return 0;
    }
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    RegHal &gpuRegs = pSubdev->Regs();
    UINT32 fuseTime = gpuRegs.Read32(MODS_FUSE_FUSETIME_PGM2);

    Printf(GetPrintPriority(), "LW_FUSE_FUSETIME_PGM2 was 0x%x\n", fuseTime);

    UINT32 Ticks = NewTimeNs / m_FuseSpec.FuseClockPeriodNs;

    gpuRegs.SetField(&fuseTime, MODS_FUSE_FUSETIME_PGM2_TWIDTH_PGM, Ticks);
    gpuRegs.Write32(MODS_FUSE_FUSETIME_PGM2, fuseTime);

    Printf(GetPrintPriority(), "LW_FUSE_FUSETIME_PGM2 set to 0x%x\n", fuseTime);

    return Ticks;
}
// bug 622250: we sometimes read back 0xFFFFFFFF/0xFFFF0000/0x0000FFFF
// after we do a fuse write. This is to ensure that we have 3 consistent
// reads first. Find the most common one. If all 3 are different, fail!
RC GpuFuse::FindConsistentRead(vector<UINT32> *pFuseRegs, FLOAT64 TimeoutMs)
{
    RC rc;
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    if (!pSubdev->HasBug(622250))
    {
        return OK;
    }

    vector<UINT32> ReReads[2];
    for (UINT32 j = 0; j < 2; j++)
    {
        // each loop
        ReReads[j].resize(m_FuseReg.size());
        for (UINT32 i = 0; i < m_FuseReg.size(); i++)
        {
            UINT32 FuseReg;
            CHECK_RC(GetFuseWord(i, TimeoutMs, &FuseReg));
            ReReads[j][i]= FuseReg;
        }
        Tasker::Sleep(5);
    }

    // find the most frequent one.
    for (UINT32 i = 0; i < m_FuseReg.size(); i++)
    {
        if (((*pFuseRegs)[i] == ReReads[0][i]) &&
            ((*pFuseRegs)[i] == ReReads[1][i]))
            continue;
        // there is a mismatch pick the most common one and store it
        Printf(Tee::PriNormal, "re-read got different results: 0x%x, 0x%x, 0x%x\n",
               (*pFuseRegs)[i],
               ReReads[0][i],
               ReReads[1][i]);

        if (ReReads[0][i] == ReReads[1][i])
        {
            (*pFuseRegs)[i] = ReReads[0][i];
        }
        else if (((*pFuseRegs)[i] == ReReads[0][i]) ||
                 ((*pFuseRegs)[i] == ReReads[1][i]))
        {
            // we have something that agrees with original
        }
        else
        {
            Printf(Tee::PriNormal, "all 3 reads are unique... bad chip!\n");
            return RC::CANNOT_MEET_FS_REQUIREMENTS;
        }
    }
    return OK;
}
//-----------------------------------------------------------------------------
// This ensures that OPT fuses agrees with RawFuses
bool GpuFuse::DoesOptAndRawFuseMatch(const FuseUtil::FuseDef* pDef,
                                     UINT32 RawFuseVal,
                                     UINT32 OptFuseVal)
{
    bool RetVal = false;
    if (pDef->Type == "dis")
    {
        // for disable fuse types: 0 in RawFuse = 1 in Opt
        // create a mask first - it can be multi bits!
        UINT32 Mask = (1 << pDef->NumBits) - 1;
        UINT32 IlwertedRawFuseVal = (~RawFuseVal) & Mask;
        RetVal = (OptFuseVal == IlwertedRawFuseVal);
    }
    else if (pDef->Type == "en/dis")
    {
        // If the undo bit (bit 1) is set, we expect the OPT value to be 0
        bool validUndo = (RawFuseVal & (1 << pDef->NumBits)) != 0 && OptFuseVal == 0;

        RetVal = RawFuseVal == OptFuseVal || validUndo;
    }
    else
    {
        RetVal = (RawFuseVal == OptFuseVal);
    }
    return RetVal;
}
//-----------------------------------------------------------------------------
bool GpuFuse::IsFuseVisible()
{
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    return pSubdev->Regs().Test32(MODS_PTOP_DEBUG_1_FUSES_VISIBLE_ENABLED);
}
//-----------------------------------------------------------------------------
void GpuFuse::SetFuseVisibility(bool IsVisible)
{
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    RegHal &gpuRegs = pSubdev->Regs();
    UINT32 debug1 = gpuRegs.Read32(MODS_PTOP_DEBUG_1);
    if (IsVisible)
    {
        gpuRegs.SetField(&debug1, MODS_PTOP_DEBUG_1_FUSES_VISIBLE_ENABLED);
    }
    else
    {
        gpuRegs.SetField(&debug1, MODS_PTOP_DEBUG_1_FUSES_VISIBLE_DISABLED);

    }
    gpuRegs.Write32(MODS_PTOP_DEBUG_1, debug1);
}
//-----------------------------------------------------------------------------
RC GpuFuse::DumpFuseRecToRegister(vector<UINT32> *pFuseReg)
{
    RC rc;
    MASSERT(pFuseReg);
    Printf(GetPrintPriority(), "Dump Fuse Records to Regiser\n");
    UINT32 LwrrOffset = m_FuseSpec.FuselessStart;
    for (UINT32 i = 0;
         (i < m_Records.size()) || (LwrrOffset < m_FuseSpec.FuselessEnd) ; i++)
    {
        if (m_Records[i].ChainId == 0)
        {
            LwrrOffset += m_Records[i].TotalSize;
            continue;
        }
        // write ChainId
        CHECK_RC(WriteFuseSnippet(pFuseReg,
                                  m_Records[i].ChainId,
                                  CHAINID_SIZE,
                                  &LwrrOffset));
        // write type
        CHECK_RC(WriteFuseSnippet(pFuseReg,
                                  m_Records[i].Type,
                                  GetRecordTypeSize(),
                                  &LwrrOffset));
        // write Offset
        CHECK_RC(WriteFuseSnippet(pFuseReg,
                                  m_Records[i].Offset,
                                  m_Records[i].OffsetSize,
                                  &LwrrOffset));
        // write Data
        CHECK_RC(WriteFuseSnippet(pFuseReg,
                                  m_Records[i].Data,
                                  m_Records[i].DataSize,
                                  &LwrrOffset));
        Printf(Tee::PriLow,
               "Rec %d|ChainId:0x%x|Type:0x%x|Offset:%d|Data:0x%x\n",
               i,
               m_Records[i].ChainId,
               m_Records[i].Type,
               GetBitOffsetInChain(m_Records[i].Offset),
               m_Records[i].DataSize);
    }
    return OK;
}
//-----------------------------------------------------------------------------
void GpuFuse::PrintRecords()
{
    Printf(GetPrintPriority(), "Fuse Records:\n"
                           "Rec  Data    Offset   type  Chain\n"
                           "---  -----   ------   ---   ------\n");
    for (UINT32 i = 0; i < m_Records.size(); i++)
    {
        if ((m_Records[i].ChainId == 0) &&
            (m_Records[i].Data == 0) &&
            (m_Records[i].Type == 0) &&
            (m_Records[i].Offset == 0))
        {
            // don't print records that is completely empty
            continue;
        }

        Printf(GetPrintPriority(), "%03d  0x%03x   0x%04x    %d    0x%02x\n",
               i, m_Records[i].Data,
               GetBitOffsetInChain(m_Records[i].Offset),
               m_Records[i].Type,
               m_Records[i].ChainId);
    }
}
//-----------------------------------------------------------------------------
// Assumption with WriteFuseSnippet: We assume that GenerateFuseRecords was
// created fuse records such that there will never be a transition from 1->0
// So we ALWAYS just OR the actual value into the
RC GpuFuse::WriteFuseSnippet(vector<UINT32> *pFuseRegs,
                               UINT32 ValToOR,
                               UINT32 NumBits,
                               UINT32 *pLwrrOffset)
{
    UINT32 Lsb = *pLwrrOffset % 32;
    UINT32 Msb = Lsb + NumBits - 1;
    UINT32 Row = *pLwrrOffset / 32;

    if (Row >= pFuseRegs->size())
    {
        Printf(Tee::PriNormal, "fuse write out of bounds\n");
        Printf(Tee::PriNormal, "Row = %d, LwrrOffset = %d pFuseRegs->size=%d\n",
               Row, *pLwrrOffset, (UINT32)pFuseRegs->size());
        return RC::SOFTWARE_ERROR;
    }

    // OR the lower bits first
    (*pFuseRegs)[Row] |= (ValToOR << Lsb);
    Printf(Tee::PriLow, "  row %d = 0x%x", Row, (*pFuseRegs)[Row]);
    if (Msb >= 32)
    {
        // set the upper bits
        UINT32 ShiftForUpperBits = NumBits - (32 - Lsb);
        Row++;
        (*pFuseRegs)[Row] |= (ValToOR >> ShiftForUpperBits);
        Printf(Tee::PriLow, " |  row %d = 0x%x", Row, (*pFuseRegs)[Row]);
    }
    *pLwrrOffset += NumBits;
    Printf(Tee::PriLow, " | offset = %d\n", *pLwrrOffset);
    return OK;
}
//-----------------------------------------------------------------------------
// The new 'fuseless' fuse allows 'instructions' to be blown into the fuses.
// The instructions (aka Fuse records) can encode one or more logical fuse. The
// fuse space consist of the traditional fuses and then fuse instructions:
//     |-----------------------------| <== Bit 0
//     |  traditional fuses          |
//     |  eg. SPARE_BITS, COMPUTE_EN |
//     |                             |
//     |_____________________________|
//     |  Fuse instructions          | <== START_BIT
//     | (eg. TPC_DISABLE  )         |
//     |   RECORD 1  |  RECORD 2     |
//     |-----------------------------|
//     |Record 3 (rec can be 32 bits)|
//     |-----------------------------|
//     |                             | <== END_BIT
RC GpuFuse::ReadInRecords()
{
    StickyRC rc;
    m_Records.clear();
    m_AvailableRecIdx = 0;

    vector<UINT32> FuseRegs;
    CHECK_RC(GetCachedFuseReg(&FuseRegs));
    UINT32 LwrrOffset = m_FuseSpec.FuselessStart;
    while (LwrrOffset < m_FuseSpec.FuselessEnd)
    {
        FuseRecord NewRec = {0};
        // get ChainID
        NewRec.ChainId = GetFuseSnippet(FuseRegs, CHAINID_SIZE, &LwrrOffset);
        if (NewRec.ChainId == 0)
        {
            // NULL instruction (empty location)
            // add 11 bits to the location of next instruction. All instruction
            // needs to be 16 bits aligned.

            // When there are leading NULL records make sure we increment fuse
            // by 32 bits each time
            LwrrOffset += REPLACE_RECORD_SIZE - CHAINID_SIZE;

            // use replace records only for now
            NewRec.TotalSize = REPLACE_RECORD_SIZE;
            // Here's why we don't increment m_AvailableRecIdx:
            // This has to do with the way HW scans the fuse records -> from top
            // to bottom, and if there are overlapping window, the bottom wins.
            // Imagine this:
            //
            // 3  Record 3
            // 4   NULL   ---> bubble (bad bad bad) should never happen
            // 5   Record 4
            // 6   Record 5
            // 7   NULL
            // 8   NULL
            // 9   NULL
            // It is unsafe to put anything in slot (4) because if any future
            // record that has an overlapping window with slot 4, the new settings
            // in slot 4 is not going to take any effect! So skip 4.
            m_Records.push_back(NewRec);
            continue;
        }
        // get instruction type
        // Assumption: there are no 'bubble instrucitons' - case where we have
        // empty 32 bits of fuse cells between two records (can't think of a
        // reason SLT / ATE would ever do this)
        NewRec.Type = GetFuseSnippet(FuseRegs,
                                     GetRecordTypeSize(),
                                     &LwrrOffset);
        if (NewRec.Type == GetReplaceRecordBitPattern())
        {
            NewRec.OffsetSize = GetRecordAddressSize();

            // The offset we program into a fuse record may have to be cooked
            // depending on the chip family
            NewRec.Offset = GetFuseSnippet(FuseRegs,
                                           GetRecordAddressSize(),
                                           &LwrrOffset);
            NewRec.DataSize = GetRecordDataSize();
            NewRec.TotalSize = REPLACE_RECORD_SIZE;
            NewRec.Data = GetFuseSnippet(FuseRegs,
                                         GetRecordDataSize(),
                                         &LwrrOffset);
            m_Records.push_back(NewRec);
            m_AvailableRecIdx = static_cast<UINT32>(m_Records.size());
        }
        else // this is not a replace record, handle it by chip family
        {
            CHECK_RC(ProcessNonRRFuseRecord(NewRec.Type,
                                            NewRec.ChainId,
                                            FuseRegs,
                                            &LwrrOffset));
        }
    }

    rc = RereadJtagChain();
    return rc;
}
//-----------------------------------------------------------------------------
// This is kind of like DRF_VAL for multiple rows of fuses
UINT32 GpuFuse::GetFuseSnippet(const vector<UINT32> &FuseRegs,
                                 UINT32 NumBits,
                                 UINT32 *pLwrrOffset)
{
    MASSERT((NumBits > 0) && pLwrrOffset);

    UINT32 RetVal = 0;
    UINT32 Lsb = *pLwrrOffset % 32;
    UINT32 Msb = Lsb + NumBits - 1;
    UINT32 Row = *pLwrrOffset / 32;

    if ((Row >= FuseRegs.size()) ||
        (Msb >= 64))
    {
        Printf(Tee::PriNormal, "fuse read out of bounds\n");
        return 0;
    }

    UINT32 Mask     = 0;
    if (Msb < 32)
    {
        // most cases
        Mask = 0xFFFFFFFF >> (31 - (Msb - Lsb));
        RetVal = (FuseRegs[Row] >> Lsb) & Mask;
    }
    else
    {
        // when Snippet spans across two words - OR them together
        UINT32 LowerBits = FuseRegs[Row] >> Lsb;
        UINT32 NumLowerBits = 31 - Lsb + 1;
        UINT32 UpperMsb  = NumBits - NumLowerBits - 1;
        Mask = 0xFFFFFFFF >> (31 - UpperMsb);
        UINT32 UpperBits = FuseRegs[Row + 1] & Mask;
        RetVal = LowerBits | (UpperBits << (NumBits - UpperMsb + 1));
    }

    *pLwrrOffset += NumBits;
    return RetVal;
}
//------------------------------------------------------------------------------
// Common method to handle ALL types of fuse records EXCEPT replace records
// (Fermi/Kepler style fuse records)
//------------------------------------------------------------------------------
/* virtual */ RC GpuFuse::ProcessNonRRFuseRecord
(
    UINT32 RecordType,
    UINT32 ChainId,
    const vector<UINT32> &FuseRegs,
    UINT32 *pLwrrOffset
)
{
    RC rc;
    switch (RecordType)
    {
        case REPLACE_REC: // should never hit this condition
                Printf(Tee::PriNormal,
                       "Unable to process replace records\n");
                rc = RC::BAD_PARAMETER;
                break;
        case BITWISE_OR_REC:
        case BLOCK_REC:
        case SHORT_REC:
                Printf(Tee::PriNormal,
                       "type %d unsupported\n", RecordType);
                rc = RC::UNSUPPORTED_FUNCTION;
                break;
        default:
                Printf(Tee::PriNormal,
                       "unknown record type %d\n", RecordType);
                rc = RC::SOFTWARE_ERROR;
                break;
    }
    return rc;
}
//-----------------------------------------------------------------------------
// This takes one fuse that we need to burn and update the fuse records. Each
// fuse can have one or more subfuses.
// Note: multiple fuses can share one fuse record (different location of data
// bits in the data field:
// Example:  format of Replace Record
// |------------------------------------------------------------------------|
// | Data (11 bits)     | Offset (13 bits)  |Type (3 bits)| ChainId (5 bits)|
// | 11110000101 (0x785)|0000000100110(0x26)|  111        |  0x1            |
// |------------------------------------------------------------------------|
// The example above is one fuse record, but can represent 2 sub fuses. See the
// data field:
// 1) bit 4-7 of OPT_TPC_DISABLE (second subfuse)
// 2) bit 0-3 of OPT_ZLWLL_DISABLE (first subfuse)
RC GpuFuse::UpdateRecord(const FuseUtil::FuseDef &Def, UINT32 Value)
{
    if (Def.Type != "fuseless")
        return OK;

    RC rc;
    list<FuseUtil::FuseLoc>::const_iterator SubFuseIter = Def.FuseNum.begin();
    Printf(GetPrintPriority(), "----- update %s -----:user needs 0x%x\n",
           Def.Name.c_str(), Value);

    // What is a subfuse?? (got to love how HW folks break down fuse into subfuses)
    // A feature fuse (eg. OPT_TPC_DISABLE) can be composed of multiple subfuses.
    // On GF100, OPT_TPC_DISABLE, can be broken down into 4 subfuses, each of them 4 bits long
    // The location of the subfuse in the JTAG chain is described in the XML. The
    // XML also have a 'suggested' way of telling SLT/MODS how to put the subfuse
    // into a fuse record.
    // The Offset field describe where the LSB of Data in the record relative to
    // a JTAG chain (specified by ChainId)
    // Each fuse record has a 11 bit data window. The subfuse may or may not be
    // in the entire record window; so a subfuse can potentially spam across
    // multiple records (YUCK!)
    //
    // For each subfuse in each fuse
    // 1) See if there already is an existing record on the subfuse
    // 2) Modify the record if necessary
    // 3) Create new record if the original records are insufficient
    //
    // So how do we put subfuses together to become a 'feature fuse'?
    // The subfuses are specified in least signficant bits first format in the XML
    // So for OPT_TPC_DISABLE Value of 0x1234, the first subfuse value would be 4,
    // second would be 3, then 2 and 1.
    // User specifies 'Value', so for each subfuse, we shift down a portion of the
    // subfuse we need to update
    //
    for (; SubFuseIter != Def.FuseNum.end(); SubFuseIter++)
    {
        const UINT32 NumBitsInSubFuse = SubFuseIter->Msb - SubFuseIter->Lsb + 1;
        const UINT32 RecordDataSize = GetRecordDataSize();

        if (NumBitsInSubFuse > RecordDataSize)
        {
            //split this subfuseo into sub-records each of size RecordDataSize
            UINT32 NumSubRecords = (NumBitsInSubFuse + RecordDataSize - 1)/RecordDataSize;

            for (UINT32 i = 0; i < NumSubRecords; i++)
            {
                FuseUtil::FuseLoc SubRecord = *SubFuseIter;
                SubRecord.Lsb = SubFuseIter->Lsb + i * RecordDataSize;
                SubRecord.Msb = min(SubRecord.Lsb + RecordDataSize - 1, SubFuseIter->Msb);

                SubRecord.ChainOffset = (SubRecord.Lsb / RecordDataSize) * RecordDataSize;
                SubRecord.DataShift = SubRecord.Lsb - SubRecord.ChainOffset;

                const UINT32 NumBits = SubRecord.Msb - SubRecord.Lsb + 1;
                const UINT32 NewDataField = Value & ((1<<NumBits) - 1);
                CHECK_RC(UpdateSubFuseRecord(SubRecord, NewDataField));
                Value >>= NumBits;
            }
        }
        else
        {
            const UINT32 NewDataField = Value & ((1<<NumBitsInSubFuse) - 1);
            CHECK_RC(UpdateSubFuseRecord(*SubFuseIter, NewDataField));
            Value >>= NumBitsInSubFuse;
        }

    } // end for each subfuse
    return OK;
}
//-----------------------------------------------------------------------------
RC GpuFuse::UpdateSubFuseRecord(FuseUtil::FuseLoc SubFuse, UINT32 NewDataField)
{
    RC rc;

    UINT32 NumBitsInSubFuse = SubFuse.Msb - SubFuse.Lsb + 1;
    // SubFuseMask: this is the bits that each loop works on. Should be 0
    // by the end of this loop
    UINT32 SubFuseMask  = (1<<NumBitsInSubFuse) - 1;
    UINT32 ChainMsb  = SubFuse.Msb;
    UINT32 ChainLsb  = SubFuse.Lsb;
    UINT32 OldChainData = GetJtagChainData(SubFuse.ChainId, ChainMsb, ChainLsb);

    Printf(GetPrintPriority(),
            "Chain=%d|Offset=%d|DataShift=%d|NewDataField=0x%x|Msb=%d|Lsb=%d\n",
            SubFuse.ChainId,
            SubFuse.ChainOffset,
            SubFuse.DataShift,
            NewDataField,
            ChainMsb,
            ChainLsb);

    if (OldChainData == NewDataField)
    {
        // sub fuse was already blown correctly
        return OK;
    }
    // Scan from the back of the records - JTAG scans the record forward,
    // so in case of duplicates (overwrites), the last record is the one
    // that the hardware sees.
    // 1) For each record, find  the overlap window of the JTAG chain in
    // the record
    // 2) based on overlap, determine whether there is a 0 -> 1 transition
    //    0 -> 1 transition means a new record needs to be added
    for (INT32 i = static_cast<INT32>(m_Records.size())-1; i >= 0; i--)
    {
        if (0 == SubFuseMask)
        {
            // no more bits to worry about - done
            break;
        }

        // only Replace record is supported
        if (m_Records[i].Type != GetReplaceRecordBitPattern())
        {
            continue;
        }

        // Note:
        // In Fermi/Kepler, the offset is directly programmed into raw fuses
        // In Maxwell, the raw fuse record contains a block ID to a 16-bit
        // block in the JTAG chain
        UINT32 Offset = GetBitOffsetInChain(m_Records[i].Offset);

        // record window
        UINT32 RecWindowMsb = Offset + GetRecordDataSize() - 1;
        UINT32 RecWindowLsb = Offset;

        // make sure the record exists and fits inside the JTAG window:
        if ((m_Records[i].ChainId != SubFuse.ChainId) ||
                (RecWindowLsb > ChainMsb) ||
                (RecWindowMsb < ChainLsb))
        {
            continue;
        }

        UINT32 OldRecData = GetJtagChainData(m_Records[i].ChainId,
                                             RecWindowMsb,
                                             RecWindowLsb);
        Printf(GetPrintPriority(),
            "  OldData@record[%d] = 0x%x. SubfuseMask=0x%x Rec(Msb,Lsb)=%d,%d\n",
            i, OldRecData, SubFuseMask, RecWindowMsb, RecWindowLsb);

        UINT32 CaseNum           = 0;
        UINT32 NumOverlapBits    = 0;
        UINT32 NumNonOverlapBits = 0;
        UINT32 NewRecData        = OldRecData;
        UINT32 OverlapMask       = 0;
        UINT32 ShiftedSubfuseMask;
        if ((RecWindowLsb <= ChainMsb) && (RecWindowLsb >= ChainLsb))
        {
            // Example
            // Suppose NewData field = 0xC3
            //            case 1 Overlap like this
            //                     ChainMsb              ChainLsb
            //         NewDataField:  |----------------------|
            //                        |24|23|22|       |18|17|
            //                                 |<---NumNonOverlapBits
            //                                             -->
            //                             RecWinLsb
            // |32|31|30              |24|23|22|
            // |-------------------------------|   Record offset = 22
            //
            // NumOverlapBits = 24 - 22 + 1 = 3
            // NumNonOverlapBits = 22 - 17 = 5
            // OverlapMask = 0x7
            NumOverlapBits    = ChainMsb - RecWindowLsb + 1;
            NumNonOverlapBits = RecWindowLsb - ChainLsb;
            // suppose SubFuseMask was b'1010 1111
            // ShiftedSubfuseMask becomse b'101 (chain# 24-22)
            ShiftedSubfuseMask = SubFuseMask >> NumNonOverlapBits;
            OverlapMask = (1<<NumOverlapBits) - 1;
            // Clear the overlap bits - make sure we clear only the bits
            // that hasn't been worked on (indicated by ShiftedSubfuseMask)
            NewRecData &= ~(OverlapMask & ShiftedSubfuseMask);
            NewRecData |= (NewDataField >> NumNonOverlapBits) & ShiftedSubfuseMask;
            // clear fuse 24 to 22 on the subfuse mask
            // SubFuseMask = b'0000 1111
            SubFuseMask &= ~(OverlapMask << NumNonOverlapBits);
            CaseNum = 1;
        }
        else if ((RecWindowMsb >= ChainLsb) && (RecWindowMsb <= ChainMsb))
        {
            // suppose NewDataField = 0x00E
            //
            //            case 2 Overlap like this
            //
            //           ChainMsb                  ChainLsb
            //NewDataField |--------------------------|
            //             |20|19|18|17|16|15|14|13|12|      (9 bits total in this subfuse)
            //                                        | <--NumNonOverlapBits --->
            //                         RecWindowMsb                        RecWindowLsb
            //                               |14|13|12|                 |06|05|04|
            //      record offset = 4        |-----------------------------------|   record is always 11 bits
            //
            // NumOverlapBits = 14 - 12 + 1 = 3
            // NumNonOverlapBits = 12 - 4 = 8
            // OverlapMask = 0x7
            NumOverlapBits = RecWindowMsb - ChainLsb + 1;
            NumNonOverlapBits = ChainLsb - RecWindowLsb;
            OverlapMask = (1<<NumOverlapBits) - 1;
            // suppose SubFuseMask was b' 1 1100 1011
            // ShiftedSubfuseMask is   b'011
            ShiftedSubfuseMask = SubFuseMask & OverlapMask;

            // Suppose record was originally b'111 1110 1010
            //                now it becomes b'100 1110 1010
            NewRecData &= ~((OverlapMask & ShiftedSubfuseMask) << NumNonOverlapBits);
            // NewRecData = NewRecData | ((0xE & 0x3)<<8
            //                             = b'110 1110 1010
            NewRecData |= (NewDataField & ShiftedSubfuseMask) << NumNonOverlapBits;
            // clear fuse 12-14 on the SubfuseMask -> b' 1 1100 1000
            SubFuseMask &= ~ShiftedSubfuseMask;
            CaseNum = 2;
        }
        else
        {
            //                case3:   subfuse is entirely in the record
            // suppose NewDataField = 0x3A
            //
            //            case 3 Overlap like this
            //                         ChainMsb                  ChainLsb
            //    NewDataField             |-----------------------|
            //                             |18|17|16|15|14|13|12|11|      (8 bits total in this subfuse)
            //
            //                      RecWindowMsb                    RecWindowLsb
            //                          |19|18|                 |11|10|09|
            //   record offset = 4      |--------------------------------|   record is always 11 bits
            //
            //
            // ShiftUp 11 - 9 = 2
            UINT32 ShiftUp = ChainLsb - RecWindowLsb;
            NewRecData &= ~(SubFuseMask << ShiftUp);
            NewRecData |= (NewDataField & SubFuseMask) << ShiftUp;
            // should be no more subfuse bits to set since it is already in the record
            SubFuseMask = 0;
            CaseNum = 3;
        }
        Printf(GetPrintPriority(), "  **case %d **, SubFuseMask = 0x%x\n",
        CaseNum, SubFuseMask);

        // determine if we can use the existing record by just OR the
        // data field of current record
        UINT32 OneToZeroMask = OldRecData & (~NewRecData);
        if (OneToZeroMask)
        {
            // we have a 1->0 transition in one of the bits, so need
            // to overwrite this record and create a new one. Also,
            // make sure we don't lose the other fuse bits that are not
            // part of the current subfuse - store this in OldData
            // clear the bits for the new field
            Printf(GetPrintPriority(), "  discard record #%d :"
                                        "offset:%d, ChaindId %d\n",
                                        i, Offset, m_Records[i].ChainId);

            Printf(GetPrintPriority(), "   overwrite old record required\n");
            CHECK_RC(AppendRecord(SubFuse.ChainId,
                                  Offset,
                                  NewRecData));
        }
        else if (OldRecData != NewRecData)
        {
            // Update the existing fuse record (no change in data offset)
            Printf(GetPrintPriority(), "  old record %d (old, new) = (0x%x, 0x%x)\n",
                                    i, OldRecData, NewRecData);
            m_Records[i].Data = NewRecData;
        }

        // reread the entire JTAG chain from m_Records
        CHECK_RC(RereadJtagChain());
    }   // end search all records

    // SubFuseMask remains non zero -> so more fuses to blow!
    // Just create a new record using default location (in XML). The default
    // location will guarentee that the subfuse will fit in the new records. ---> that was an out right lie
    if ((NewDataField & SubFuseMask) != 0)
    {
        UINT32 DataField;
        // overflow will happens when:
        if ((SubFuse.ChainOffset+GetRecordDataSize()-1) < SubFuse.Msb)
        {
            // example: GK107 OPT_TPC_DISABLE
            //          <key>blockIndex</key>
            //          <str>49</str>            -> record only supports 49-59
            //          <key>blockOffset</key>
            //          <str>10</str>
            //          <key>chainId</key>
            //          <str>1</str>
            //          <key>lsbIndex</key>
            //          <str>59</str>
            //          <key>msbIndex</key>
            //          <str>60</str>            -> bit 60 overflows and needs its own record
            //          <key>name</key>
            //          <str>opt_tpc_disable_0</str>
            UINT32 NewLsb = SubFuse.ChainOffset + GetRecordDataSize();
            DataField = GetJtagChainData(SubFuse.ChainId,
                                         NewLsb + GetRecordDataSize() - 1,
                                         NewLsb);
            UINT32 NumShifts       = NewLsb - SubFuse.Lsb;
            UINT32 OverflowBits    = DataField;
            OverflowBits &= ~(SubFuseMask >> NumShifts);
            OverflowBits |= (NewDataField >> NumShifts);

            Printf(GetPrintPriority(),
                "   brand new record required for overflow OldData=0x%x, New=0x%x\n",
                DataField, OverflowBits);
            CHECK_RC(AppendRecord(SubFuse.ChainId,
                                    NewLsb,
                                    OverflowBits));
            CHECK_RC(RereadJtagChain());

            // clear the bits that have been updated
            UINT32 NumOverflow = SubFuse.Msb - SubFuse.Lsb + 1 - NumShifts;
            UINT32 BitsToClear = (1 << NumOverflow) - 1;
            SubFuseMask &= ~(BitsToClear << NumShifts);
            NewDataField &= ~(BitsToClear << NumShifts);
        }

        // handle normal cases - if still required
        if ((NewDataField & SubFuseMask) != 0)
        {
            DataField = GetJtagChainData(SubFuse.ChainId,
                                         SubFuse.ChainOffset + GetRecordDataSize() - 1,
                                         SubFuse.ChainOffset);
            Printf(GetPrintPriority(),
                "   brand new record required. SubFuseMask=0x%x, OldData=0x%x\n",
                SubFuseMask, DataField);

            UINT32 BitsToClear = (1 << NumBitsInSubFuse) - 1;
            DataField &= ~(BitsToClear << SubFuse.DataShift);
            DataField |= NewDataField << SubFuse.DataShift;

            CHECK_RC(AppendRecord(SubFuse.ChainId,
                                  SubFuse.ChainOffset,
                                  DataField));
            CHECK_RC(RereadJtagChain());
        }
    }
    return OK;
}
//-----------------------------------------------------------------------------
// Append fuse records based on the chain offset, data, and chain ID
// defined in fuse XML
RC GpuFuse::AppendRecord(UINT32 ChainId, UINT32 Offset, UINT32 Data)
{
    if (m_AvailableRecIdx == m_Records.size())
    {
        Printf(Tee::PriNormal, "No more space to burn fuse records\n");
        Printf(Tee::PriNormal, "m_AvailableRecIdx = %d m_Records.size=%d\n",
               m_AvailableRecIdx, (UINT32)m_Records.size());
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    // What is a record address ?
    // A record address tells the fuse engine how to locate the data payload
    // in a fuse record within the JTAG chain.
    //
    // Record addressing differs across chip families:
    // Fermi/Kepler:
    //     Fuse records can be addressed using any offset in the JTAG chain
    // Maxwell:
    //     Fuse records are always addressed as 16 bit blocks in the JTAG chain
    UINT32 RecordAddress = GetRecordAddress(Offset);
    UINT32 OffsetInChain = GetBitOffsetInChain(RecordAddress);

    m_Records[m_AvailableRecIdx].ChainId    = ChainId;
    m_Records[m_AvailableRecIdx].Type       = GetReplaceRecordBitPattern();
    m_Records[m_AvailableRecIdx].Offset     = RecordAddress;
    m_Records[m_AvailableRecIdx].OffsetSize = GetRecordAddressSize();

    if (Offset != OffsetInChain)
    {
        // In Maxwell if the actual offset is not aligned to the 16-bit block
        // we need to shift the data relative to a 16-bit boundary
        m_Records[m_AvailableRecIdx].Data   = Data << (Offset - OffsetInChain);
    }
    else
    {
        // in Fermi/Kepler we write to the offset in chain directly
        // (no concept of 16-bit block addressing in fuse records)
        m_Records[m_AvailableRecIdx].Data   = Data;
    }

    // Quick check to see if we are writing more than the data payload
    // to a fuse record
    UINT32 RecordDataMask = ((1 << GetRecordDataSize()) - 1);
    if (m_Records[m_AvailableRecIdx].Data > RecordDataMask)
    {
        Printf(Tee::PriNormal,
               "Error: Detected overflow in data in fuse record\n");
        Printf(Tee::PriNormal,
               "m_AvailableRecIdx = %d m_Records.Data =0x%08x\n",
               m_AvailableRecIdx, m_Records[m_AvailableRecIdx].Data);
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    m_Records[m_AvailableRecIdx].DataSize   = GetRecordDataSize();

    Printf(GetPrintPriority(), "  add new record(%d) offset,data = %d,0x%x\n",
           m_AvailableRecIdx, Offset, Data);

    m_AvailableRecIdx++;
    return OK;
}
//-----------------------------------------------------------------------------
UINT32 GpuFuse::GetJtagChainData(UINT32 ChainId, UINT32 Msb, UINT32 Lsb)
{
    UINT32 RetVal = 0;
    if (m_JtagChain.find(ChainId) != m_JtagChain.end())
    {
        for (UINT32 i = Lsb; i <= Msb; i++)
        {
            if (m_JtagChain[ChainId].find(i) != m_JtagChain[ChainId].end())
            {
                if (m_JtagChain[ChainId][i])
                {
                    RetVal |= (1 << (i - Lsb));
                }
            }
        }
    }
    return RetVal;
}
//-----------------------------------------------------------------------------
// This creates a virtual Jtag chain for our own records
RC GpuFuse::UpdateJtagChain(const FuseRecord &Record)
{
    // only support replace reocrds -> just skip over if it's empty
    if ((Record.Type != GetReplaceRecordBitPattern()) ||
        (Record.ChainId == ILWALID_CHAIN_ID))
    {
        return OK;
    }

    UINT32 ChainId = Record.ChainId;
    UINT32 DataToShift = Record.Data;

    // using the offset and data size, mark each bit on the jtag chain
    for (UINT32 j = 0; j < GetRecordDataSize(); j++)
    {
        // each record has an offset from the base of the chain
        UINT32 BitOffsetInChain = j + GetBitOffsetInChain(Record.Offset);
        if (DataToShift & (1<<j))
        {
            m_JtagChain[ChainId][BitOffsetInChain] = true;
        }
        else
        {
            m_JtagChain[ChainId][BitOffsetInChain] = false;
        }
    }
    return OK;
}
//-----------------------------------------------------------------------------
RC GpuFuse::RereadJtagChain()
{
    RC rc;
    // update our virtual JTAG chain
    m_JtagChain.clear();
    for (UINT32 i = 0; i < m_Records.size(); i++)
    {
        CHECK_RC(UpdateJtagChain(m_Records[i]));
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC GpuFuse::SetSwSku(const string SkuName)
{
    RC rc;
    const FuseUtil::SkuConfig *pSku = GetSkuSpecFromName(Utility::ToUpperCase(SkuName));
    if (NULL == pSku)
    {
        Printf(Tee::PriNormal, "Sku %s is known\n", SkuName.c_str());
        return RC::BAD_PARAMETER;
    }

    list<FuseUtil::FuseInfo>::const_iterator FuseIter;
    FuseIter = pSku->FuseList.begin();
    for (; FuseIter != pSku->FuseList.end(); FuseIter++)
    {
        // when blowing a SKU, XML cannot define a readonly (ATE) value
        // or a 'not' value
        if (FuseIter->Attribute & FuseUtil::FLAG_READONLY)
            continue;

        OverrideInfo *pOverride = GetOverrideFromName(FuseIter->pFuseDef->Name);
        UINT32 Value = 0;
        const FuseUtil::FuseDef *pDef = FuseIter->pFuseDef;
        if (pOverride != nullptr)
        {
            if (pOverride->Flag & USE_SKU_FLOORSWEEP_DEF)
            {
                Value = GetSwFuseValToSet(*FuseIter);
            }
            else
            {
                Value = pOverride->Value;
            }
        }
        else if (FuseIter->Attribute & FuseUtil::FLAG_DONTCARE)
        {
            // Tracking fuses don't need to be set if they're not part of the override
            continue;
        }
        else
        {
            Value = GetSwFuseValToSet(*FuseIter);
        }
        CHECK_RC(SetSwFuse(pDef, Value));
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC GpuFuse::CheckSwSku(const string SkuName)
{
    RC rc;
    const FuseUtil::SkuConfig *pSku = GetSkuSpecFromName(Utility::ToUpperCase(SkuName));
    if (NULL == pSku)
    {
        Printf(Tee::PriNormal, "Sku %s is not in database\n", SkuName.c_str());
        return RC::BAD_PARAMETER;
    }

    SetFuseVisibility(true);

    list<FuseUtil::FuseInfo>::const_iterator FuseIter;
    FuseIter = pSku->FuseList.begin();
    for (; FuseIter != pSku->FuseList.end(); FuseIter++)
    {
        if (!IsFuseSupported(*FuseIter, OldFuse::ALL_MATCH))
            continue;

        if (FuseIter->Attribute & FuseUtil::FLAG_DONTCARE)
            continue;

        const FuseUtil::FuseDef *pDef = FuseIter->pFuseDef;
        UINT32 RegOffset = pDef->PrimaryOffset.Offset;

        if (RegOffset == FuseUtil::UNKNOWN_OFFSET)
            continue;

        GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
        UINT32 OptFuseVal = pSubdev->RegRd32(RegOffset);

        bool ThisFuseIsRight = false;
        if (FuseIter->Attribute & FuseUtil::FLAG_MATCHVAL)
        {
            for (UINT32 i = 0; i < FuseIter->Vals.size(); i++)
            {
                if (OptFuseVal == FuseIter->Vals[i])
                {
                    ThisFuseIsRight = true;
                    break;
                }
            }
        }
        else if (FuseIter->Attribute & FuseUtil::FLAG_BITCOUNT)
        {
            UINT32 NumBits = (UINT32)Utility::CountBits(OptFuseVal);
            ThisFuseIsRight = (NumBits == FuseIter->Value);
        }
        else if (FuseIter->Attribute & FuseUtil::FLAG_UNDOFUSE)
        {
            if (pDef->Type == "dis")
            {
                // Fuse XML fuse value definition refers to RAW fuse values
                // Opt fuse values values are actually flipped.. yuck
                UINT32 Mask = (1 << pDef->NumBits) - 1;
                OptFuseVal = (~OptFuseVal) & Mask;
            }
            ThisFuseIsRight = (OptFuseVal == FuseIter->Value);
        }
        else if (FuseIter->Attribute & FuseUtil::FLAG_ATE_NOT)
        {
            ThisFuseIsRight = std::find(FuseIter->Vals.begin(), FuseIter->Vals.end(), OptFuseVal) ==
                              FuseIter->Vals.end();
        }
        else if (FuseIter->Attribute & FuseUtil::FLAG_ATE_RANGE)
        {
            for (UINT32 i = 0; i < FuseIter->ValRanges.size(); i++)
            {
                if ((OptFuseVal >= FuseIter->ValRanges[i].Min) &&
                    (OptFuseVal <= FuseIter->ValRanges[i].Max))
                {
                    ThisFuseIsRight = true;
                    break;
                }
            }
        }

        if (!ThisFuseIsRight)
        {
            Printf(Tee::PriNormal, "Fuse %s has val 0x%x. INCORRECT\n",
                   FuseIter->pFuseDef->Name.c_str(),
                   OptFuseVal);
            rc = RC::INCORRECT_FEATURE_SET;
        }
    }

    return rc;
}
//-----------------------------------------------------------------------------
//! Trigger a SW reset
//  This should only be called during fusing when the GPU is inactive
//      (i.e. only right after SW Fusing)
RC GpuFuse::SwReset()
{
    GpuSubdevice *pSubdev = (GpuSubdevice*)GetDevice();
    return pSubdev->GetFsImpl()->SwReset(false);
}
//-----------------------------------------------------------------------------
RC GpuFuse::SetSwFuseByName(string FuseName, UINT32 Value)
{
    RC rc;
    const FuseUtil::FuseDef *pDef = GetFuseDefFromName(FuseName);
    if (NULL == pDef)
    {
        Printf(Tee::PriNormal, "unknown fuse name, cannot set SW fuse\n");
        return RC::BAD_PARAMETER;
    }
    CHECK_RC(SetSwFuse(pDef , Value));
    return rc;
}
//-----------------------------------------------------------------------------
// This is for getting the SW fuse value based on SKU Spec for one fuse
UINT32 GpuFuse::GetSwFuseValToSet(const FuseUtil::FuseInfo &Info)
{
    UINT32 fuseVal = 0;
    OverrideInfo *pOverride = GetOverrideFromName(Info.pFuseDef->Name);

    if (pOverride != nullptr && (pOverride->Flag & USE_SKU_FLOORSWEEP_DEF))
    {
        fuseVal = pOverride->Value;
    }

    if (Info.Attribute & FuseUtil::FLAG_BITCOUNT)
    {
        for (UINT32 bits = static_cast<UINT32>(Utility::CountBits(fuseVal));
              bits < Info.Value; bits++)
        {
            UINT32 firstZeroMask = (fuseVal + 1) & (~fuseVal);
            fuseVal |= firstZeroMask;
        }
    }
    else if (Info.Attribute & FuseUtil::FLAG_ATE_RANGE)
    {
        // OPT fuse registers should always match the raw fuse settings by ATE.
        fuseVal = ExtractFuseVal(Info.pFuseDef, OldFuse::ALL_MATCH);
    }
    else
    {
        // TODO: Bug 962278 - doesn't handle multiple ValMatchHex values (GK106)
        // remove above comment when bug 962278 is fixed
        // for ValMatchHex, ValMatchBin
        fuseVal |= Info.Value;
    }
    return fuseVal;
}
//-----------------------------------------------------------------------------
string GpuFuse::GetFuseFilename()
{
    GpuSubdevice *pSubdev  = (GpuSubdevice*)GetDevice();
    Gpu::LwDeviceId ChipId = pSubdev->DeviceId();
    string FileName        = Gpu::DeviceIdToString(ChipId);

    // Prefer JSON fuse files when available
    string jsonFileName = Utility::ToLowerCase(FileName) + "_f.json";
    if (Xp::DoesFileExist(Utility::DefaultFindFile(jsonFileName, true)) ||
        Xp::DoesFileExist(Utility::DefaultFindFile(jsonFileName + "e", true)))
    {
        return jsonFileName;
    }

    return Utility::ToLowerCase(FileName) + "_f.xml";
}
//------------------------------------------------------------------------------
// Fuse API to encrypt a fuse binary
//------------------------------------------------------------------------------
RC GpuFuse::EncryptFuseFile(string fileName, string keyFileName, bool isEncryptionKey)
{
    RC rc;
    vector<UINT08> fileContents;
    vector<UINT08> keyContents;
    long filesize = 0, keysize = 0;

    FileHolder file;
    CHECK_RC(file.Open(fileName, "rb"));
    CHECK_RC(Utility::FileSize(file.GetFile(), &filesize));
    fileContents.resize(filesize);

    // Copy the file into a buffer and verify the operation
    size_t length = fread((void *)&fileContents[0], 1, filesize,
                            file.GetFile());
    if (length != (size_t)filesize || length == 0)
    {
        Printf(Tee::PriHigh, "Failed to read file %s.\n", fileName.c_str());
        return RC::FILE_READ_ERROR;
    }

    // Encryption requirement - buffer must be aligned to 16 bytes
    // padding with spaces only if its not a fuse key
    if(filesize % Crypto::ByteAlignment)
    {
        if(!isEncryptionKey)
        {
            filesize = ALIGN_UP(static_cast<UINT32>(filesize),
                                Crypto::ByteAlignment);
            fileContents.resize(filesize, ' ');
        }
        else
        {
            if(filesize > static_cast<long>(FUSE_KEY_LENGTH))
            {
                filesize = FUSE_KEY_LENGTH;
                Printf(Tee::PriDebug, "Warning: Keys are going to be truncated.\n");
            }
            if(filesize != static_cast<long>(FUSE_KEY_LENGTH))
            {
                Printf(Tee::PriHigh, "Error: Keys are of not right length.\n");
                return RC::SOFTWARE_ERROR;
            }
            fileContents.resize(filesize);
        }
    }

    FileHolder key;
    CHECK_RC(key.Open(keyFileName, "rb"));
    CHECK_RC(Utility::FileSize(key.GetFile(), &keysize));
    keyContents.resize(keysize);

    // Copy the key into a buffer and verify the operation
    length = fread((void *)&keyContents[0], 1, keysize, key.GetFile());
    if (length != (size_t)keysize || length == 0)
    {
        Printf(Tee::PriHigh, "Failed to read key %s.\n", keyFileName.c_str());
        return RC::FILE_READ_ERROR;
    }
    if(keysize > static_cast<long>(FUSE_KEY_LENGTH))
    {
        keysize = FUSE_KEY_LENGTH;
        Printf(Tee::PriDebug, "Warning: Keys are going to be truncated.\n");
    }
    if(keysize != static_cast<long>(FUSE_KEY_LENGTH))
    {
        Printf(Tee::PriHigh, "Error: Keys are of not right length.\n");
        return RC::SOFTWARE_ERROR;
    }
    keyContents.resize(keysize);

    // Encrypt fileContents with keyContents
    vector<UINT08> cipherText(filesize);
    Crypto::XTSAESKey xtsAesKey;
    memset(&xtsAesKey, 0 , sizeof(xtsAesKey));
    memcpy(&xtsAesKey.keys[0].dwords, keyContents.data(), keysize);

    Printf(Tee::PriDebug, "Encrypting file %s with key %s.\n",
        fileName.c_str(),
        reinterpret_cast<char *>(&(xtsAesKey.keys[0].dwords[0].dw)));
    CHECK_RC(Crypto::XtsAesEncSect(
                xtsAesKey,
                0,
                filesize,
                reinterpret_cast<const UINT08 *>(&fileContents[0]),
                reinterpret_cast<UINT08 *>(&cipherText[0])));

    // We dont want null characters in encrypted key, because we treat them as
    // strings. Fail this input key, and try another.
    if (isEncryptionKey)
    {
        if (find(cipherText.begin(), cipherText.end(), 0) != cipherText.end())
        {
            Printf(Tee::PriHigh, "Error: Not a valid encryption(L1/L2) Key.\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    // Write the cipher text to an encrypted ".bin" file
    FileHolder writefile;
    CHECK_RC(writefile.Open(fileName + ".bin", "wb"));

    length = fwrite((void *)&cipherText[0], 1, filesize, writefile.GetFile());
    if (length != (size_t)filesize || length == 0)
    {
        Printf(Tee::PriHigh, "Failed to write file %s.bin\n", fileName.c_str());
        return RC::FILE_WRITE_ERROR;
    }
    return OK;
}

//-----------------------------------------------------------------------------
// KFUSE
//-----------------------------------------------------------------------------
RC GpuFuse::GetKFuseWord(UINT32 Row, UINT32 *pFuseVal, FLOAT64 TimeoutMs)
{
    MASSERT(pFuseVal);
    GpuSubdevice* pSubdev = (GpuSubdevice*)GetDevice();
    RegHal &gpuRegs = pSubdev->Regs();
    ScopedRegister fuseCtrl(pSubdev, gpuRegs.LookupAddress(MODS_KFUSE_FUSECTRL));

    Printf(Tee::PriDebug, "GetKFuseWord(Row=%d)\n", Row);
    Printf(Tee::PriDebug, "WrRow 0x%x, 0x%x\n",
           gpuRegs.LookupAddress(MODS_KFUSE_FUSEADDR), Row);
    gpuRegs.Write32(MODS_KFUSE_FUSEADDR, Row);

    // set fuse control to 'read'
    UINT32 fuseCtrlRead = fuseCtrl;
    gpuRegs.SetField(&fuseCtrlRead, MODS_KFUSE_FUSECTRL_CMD_READ);

    Printf(Tee::PriLow, "Wr 0x%x, 0x%x\n",
        gpuRegs.LookupAddress(MODS_KFUSE_FUSECTRL), fuseCtrlRead);
    gpuRegs.Write32(MODS_KFUSE_FUSECTRL, fuseCtrlRead);

    // make sure data is strobed
    RC rc = POLLWRAP_HW(PollKFusesIdle, pSubdev, TimeoutMs);

    *pFuseVal = gpuRegs.Read32(MODS_KFUSE_FUSERDATA);

    Printf(Tee::PriLow, "Wr 0x%x, 0x%x\n",
           gpuRegs.LookupAddress(MODS_KFUSE_FUSECTRL), (UINT32)fuseCtrl);
    return rc;
}

// Procedure described in bug
RC GpuFuse::BlowKFuses(const vector<UINT32> &RegsToBlow,
                         FuseSource*           pFuseSrc)
{
    RC rc;

    UINT32 NumBitsToBlow = 0;
    const UINT32 NumKFuseWords = 192;
    for (UINT32 i = 0; i < NumKFuseWords; i++)
    {
        NumBitsToBlow += Utility::CountBits(RegsToBlow[i]);
    }

    // check if we have anything to blow first - if there's no new bits to blow
    if (NumBitsToBlow == 0)
    {
        Printf(GetPrintPriority(), "Nothing to blow.\n");
        return OK;
    }

    TestConfiguration LwrrTestConfig;
    CHECK_RC(LwrrTestConfig.InitFromJs());
    FLOAT64 TimeoutMs     = LwrrTestConfig.TimeoutMs();
    GpuSubdevice* pSubdev = (GpuSubdevice*)GetDevice();
    RegHal &gpuRegs = pSubdev->Regs();

    // save kfuse control register
    ScopedRegister fuseCtrl(pSubdev, gpuRegs.LookupAddress(MODS_KFUSE_FUSECTRL));
    UINT32 fuseCtrlWrite = fuseCtrl;
    gpuRegs.SetField(&fuseCtrlWrite, MODS_KFUSE_FUSECTRL_CMD_WRITE);
    Printf(GetPrintPriority(), "Buring KFuse - toggle Fuse Src High\n");
    // Flick FUSE_SRC to high
    CHECK_RC(pFuseSrc->Toggle(FuseSource::TO_HIGH));
    Tasker::Sleep(m_FuseSpec.SleepTime[FuseSpec::SLEEP_AFTER_GPIO_HIGH]);

    // Cleanup
    Utility::CleanupOnReturnArg<FuseSource, RC, FuseSource::Level>
                ToggleToLow(pFuseSrc, &FuseSource::Toggle, FuseSource::TO_LOW);

    for (UINT32 i = 0; i < NumKFuseWords; i++)
    {
        const UINT32 PAUSE_MS_BETWEEN_ROWS = 1;
        // set the Row of the fuse
        gpuRegs.Write32(MODS_KFUSE_FUSEADDR, i);
        Printf(Tee::PriDebug, "Wr 0x%x, 0x%x\n",
            gpuRegs.LookupAddress(MODS_KFUSE_FUSEADDR), i);

        gpuRegs.Write32(MODS_KFUSE_FUSEWDATA, RegsToBlow[i]);
        Printf(Tee::PriDebug, "Wr 0x%x, 0x%x\n",
               gpuRegs.LookupAddress(MODS_KFUSE_FUSEWDATA), RegsToBlow[i]);

        gpuRegs.Write32(MODS_KFUSE_FUSECTRL, fuseCtrlWrite);

        Printf(Tee::PriDebug, "Wr 0x%x, 0x%x\n",
            gpuRegs.LookupAddress(MODS_KFUSE_FUSECTRL), fuseCtrlWrite);
        Printf(Tee::PriDebug, "Poll k fuse idle\n");
        Tasker::Sleep(PAUSE_MS_BETWEEN_ROWS);
        CHECK_RC(POLLWRAP_HW(&PollKFusesIdle, pSubdev, TimeoutMs));
    }

    // stop the fuse source
    CHECK_RC(pFuseSrc->Toggle(FuseSource::TO_LOW));
    CHECK_RC(POLLWRAP_HW(&PollKFusesIdle, pSubdev, TimeoutMs));

    Printf(GetPrintPriority(), "KFuse blow completed.\n");

    // verify whether we have the written the right data:
    for (UINT32 i = 0; i < m_FuseSpec.NumOfFuseReg; i++)
    {
        UINT32 FuseReg;
        CHECK_RC(GetKFuseWord(i, &FuseReg, TimeoutMs));
        Printf(GetPrintPriority(), "Final Fuse value at row %d: 0x%x\n", i, FuseReg);
        if (FuseReg != RegsToBlow[i])
        {
            Printf(Tee::PriNormal,"Expected data 0x%0x didn't match 0x%8x at row%d\n",
                   RegsToBlow[i], FuseReg, i);
            CHECK_RC(RC::FUSE_VALUE_OUT_OF_RANGE);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Static wrapper to PollFuseIdle that can be passed as a callback
//-----------------------------------------------------------------------------
bool GpuFuse::PollFusesIdleWrapper(void *pWrapperParam)
{
    MASSERT(pWrapperParam);
    GpuFuse* pThis = (GpuFuse*)pWrapperParam;

    if (!pThis->IsWaitForIdleEnabled())
        return true;

    return (pThis->PollFusesIdle());
}
//-----------------------------------------------------------------------------
/* virtual */ bool GpuFuse::PollFusesIdle()
{
    GpuSubdevice *pSubDev = (GpuSubdevice *)this->GetDevice();
    bool retval = pSubDev->Regs().Test32(MODS_FUSE_FUSECTRL_STATE_IDLE);
    return retval;
}
//-----------------------------------------------------------------------------
bool GpuFuse::PollFuseSenseDone(void *pParam)
{
    MASSERT(pParam);
    GpuSubdevice *pSubDev = (GpuSubdevice*)pParam;
    Platform::Delay(1);
    bool retval = pSubDev->Regs().Test32(MODS_FUSE_FUSECTRL_FUSE_SENSE_DONE_YES);
    return retval;
}

bool GpuFuse::PollKFusesIdle(void *pParam)
{
    MASSERT(pParam);
    GpuSubdevice *pSubdev = (GpuSubdevice*)pParam;

    UINT32 KfuseState = pSubdev->Regs().Read32(MODS_KFUSE_FUSECTRL);
    // see bug 484140 - HW needs to update the manual...
    bool retval = ((KfuseState & 0xd000) == 0);

    return retval;
}

//-----------------------------------------------------------------------------
JS_CLASS(GpuFuseXml);

static SObject GpuFuseXml_Object
(
    "GpuFuseXml",
    GpuFuseXmlClass,
    0,
    0,
    "Gpu Fuse Xml class"
);

JS_STEST_LWSTOM(GpuFuseXml, GetFuseInfo, 4,
                "Get fuse type and expected values")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    JavaScriptPtr pJS;
    string        FileName;
    string        ChipSku;
    string        FuseName;
    JSObject     *pRetArray;
    if ((NumArguments != 4) ||
        (OK != pJS->FromJsval(pArguments[0], &FileName)) ||
        (OK != pJS->FromJsval(pArguments[1], &ChipSku)) ||
        (OK != pJS->FromJsval(pArguments[2], &FuseName)) ||
        (OK != pJS->FromJsval(pArguments[3], &pRetArray)))
    {
        JS_ReportError(pContext,
                       "Usage: FuseXml.GetFuseInfo(FileName, ChipSku, FuseName, RetArray)");
        return JS_FALSE;
    }

    RC rc;
    const FuseUtil::FuseDefMap *pFuseDefMap;
    const FuseUtil::SkuList    *pSkuList;
    const FuseUtil::MiscInfo   *pMiscInfo;
    FuseUtil::FuseInfo          Info;

    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(FileName));
    C_CHECK_RC(pParser->ParseFile(FileName, &pFuseDefMap, &pSkuList, &pMiscInfo));

    C_CHECK_RC(FuseUtil::SearchFuseInfo(ChipSku, FuseName, *pSkuList, &Info));
    C_CHECK_RC(pJS->SetElement(pRetArray, FuseUtil::INFO_ATTRIBUTE, Info.Attribute));
    C_CHECK_RC(pJS->SetElement(pRetArray, FuseUtil::INFO_STR_VAL, Info.StrValue));
    C_CHECK_RC(pJS->SetElement(pRetArray, FuseUtil::INFO_VAL, Info.Value));
    if (Info.Attribute & FuseUtil::FLAG_BITCOUNT)
    {
        for (UINT32 i = 0; i < Info.Vals.size(); i++)
        {
            CHECK_RC(pJS->SetElement(pRetArray,
                                     FuseUtil::INFO_VAL_LIST+i,
                                     Info.Vals[i]));
        }
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(GpuFuseXml, GetFbConfig, 2,
                "Get valid/invalid Fb configuration - include FBP/FBIO/FBIO_SHIFT")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    JavaScriptPtr pJS;
    string        FileName;
    JSObject     *pRetArray;
    if ((NumArguments != 2) ||
        (OK != pJS->FromJsval(pArguments[0], &FileName)) ||
        (OK != pJS->FromJsval(pArguments[1], &pRetArray)))
    {
        JS_ReportError(pContext,
                       "Usage: FuseXml.GetFbConfig(FileName, RetArray)");
        return JS_FALSE;
    }
    RC rc;
    const FuseUtil::FuseDefMap *pFuseDefMap;
    const FuseUtil::SkuList    *pSkuList;
    const FuseUtil::MiscInfo   *pMiscInfo;
    const FuseDataSet          *pFuseDataSet;
    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(FileName));
    C_CHECK_RC(pParser->ParseFile(FileName, &pFuseDefMap, &pSkuList,
                                  &pMiscInfo, &pFuseDataSet));
    RETURN_RC(pFuseDataSet->ExportFbConfigToJs(&pRetArray));
}
