/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "riraccessor.h"

#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/fuse/fuse.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghal.h"

RirAccessor::RirAccessor(TestDevice* pDev)
    : m_pDev(pDev),
      m_pFuse(nullptr)
{
    MASSERT(pDev);
    MASSERT(dynamic_cast<GpuSubdevice*>(pDev) != nullptr);
}

void RirAccessor::SetParent(Fuse* pFuse)
{
    MASSERT(pFuse != nullptr);
    m_pFuse = pFuse;
}

RC RirAccessor::SetupFuseBlow()
{
    RC rc;
    bool bNeedsDaughterCard = !m_pDev->HasFeature(Device::GPUSUB_SUPPORTS_FUSING_WO_DAUGHTER_CARD);
    if (bNeedsDaughterCard)
    {
        FuseSource fuseSrc;
        GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
        CHECK_RC(fuseSrc.Initialize(pSubdev, "jsfunc_FuseSrcToggle"));
        CHECK_RC(fuseSrc.Toggle(FuseSource::TO_HIGH));
    }
    return rc;
}

RC RirAccessor::CleanupFuseBlow()
{
    RC rc;
    bool bNeedsDaughterCard = !m_pDev->HasFeature(Device::GPUSUB_SUPPORTS_FUSING_WO_DAUGHTER_CARD);
    if (bNeedsDaughterCard)
    {
        FuseSource fuseSrc;
        GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
        CHECK_RC(fuseSrc.Initialize(pSubdev, "jsfunc_FuseSrcToggle"));
        CHECK_RC(fuseSrc.Toggle(FuseSource::TO_LOW));
    }
    return rc;
}

RC RirAccessor::BlowFuses(const vector<UINT32>& fuseBinary)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    RegHal& regs = m_pDev->Regs();

    bool bNeedsDaughterCard = !m_pDev->HasFeature(Device::GPUSUB_SUPPORTS_FUSING_WO_DAUGHTER_CARD);

    // Clear the fuse control register before starting
    regs.Write32(MODS_FUSE_FUSECTRL, 0);

    // Set up the function to toggle the daughter card
    FuseSource fuseSrc;
    if (bNeedsDaughterCard)
    {
        GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
        CHECK_RC(fuseSrc.Initialize(pSubdev, "jsfunc_FuseSrcToggle"));
    }

    // Get the cached fuses for comparison
    const vector<UINT32>* pCachedFuses = nullptr;
    CHECK_RC(GetFuseBlock(&pCachedFuses));
    const vector<UINT32>& cachedFuses = *pCachedFuses;

    if (cachedFuses.size() != fuseBinary.size())
    {
        Printf(Tee::PriError, "RIR fuse binary is not the correct size!\n");
        return RC::SOFTWARE_ERROR;
    }

    PrintNewFuses(*pCachedFuses, fuseBinary);

    CHECK_RC(PollFusesIdle());
    bool fuseSrcHigh = false;

    // Assert RWL to indicate use of RIR
    regs.Write32Sync(MODS_FUSE_DEBUGCTRL_RWL_ENABLE);

    for (UINT32 row = 0; row < fuseBinary.size(); row++)
    {
        // Only burn bits going from 0 to 1
        UINT32 valToBurn = fuseBinary[row] & ~cachedFuses[row];
        if (valToBurn == 0)
        {
            continue;
        }

        if (!fuseSrcHigh && bNeedsDaughterCard)
        {
            CHECK_RC(fuseSrc.Toggle(FuseSource::TO_HIGH));
            fuseSrcHigh = true;
        }

        regs.Write32(MODS_FUSE_FUSEADDR_VLDFLD, row);
        regs.Write32(MODS_FUSE_FUSEWDATA, valToBurn);
        regs.Write32(MODS_FUSE_FUSECTRL_CMD_WRITE);

        CHECK_RC(PollFusesIdle());
    }

    if (fuseSrcHigh && bNeedsDaughterCard)
    {
        CHECK_RC(fuseSrc.Toggle(FuseSource::TO_LOW));
        fuseSrcHigh = false;
    }

    regs.Write32Sync(MODS_FUSE_DEBUGCTRL_RWL_DISABLE);

    MarkFuseReadDirty();

    CHECK_RC(VerifyFusing(fuseBinary));

    return rc;
}

RC RirAccessor::GetFuseRow(UINT32 row, UINT32* pValue)
{
    MASSERT(pValue);
    RC rc;
    const vector<UINT32>* pFuseBlock = nullptr;
    CHECK_RC(GetFuseBlock(&pFuseBlock));
    if (row >= pFuseBlock->size())
    {
        Printf(Tee::PriError, "Requested row outside of RIR block: %d\n", row);
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }
    *pValue = (*pFuseBlock)[row];
    return rc;
}

RC RirAccessor::ReadFuseBlock(vector<UINT32>* pReadFuses)
{
    RC rc;
    MASSERT(pReadFuses);
    pReadFuses->clear();
    RegHal& regs = m_pDev->Regs();

    // Clear the fuse control register before starting
    regs.Write32(MODS_FUSE_FUSECTRL, 0);

    // Indicate usage of RIR functionality
    regs.Write32(MODS_FUSE_DEBUGCTRL_RWL_ENABLE);

    UINT32 numRows;
    CHECK_RC(GetNumFuseRows(&numRows));
    for (UINT32 row = 0; row < numRows; row++)
    {
        regs.Write32(MODS_FUSE_FUSEADDR_VLDFLD, row);
        regs.Write32(MODS_FUSE_FUSECTRL_CMD_READ);
        CHECK_RC(PollFusesIdle());
        pReadFuses->push_back(regs.Read32(MODS_FUSE_FUSERDATA));
    }

    regs.Write32(MODS_FUSE_DEBUGCTRL_RWL_DISABLE);

    return OK;
}

RC RirAccessor::PollFusesIdle()
{
    RC rc;
    if (m_WaitForIdle)
    {
        rc = Tasker::PollHw(
                [](void *pRegs) { return static_cast<RegHal*>(pRegs)->Test32(
                                MODS_FUSE_FUSECTRL_STATE_IDLE); },
                &m_pDev->Regs(), Tasker::FixTimeout(FuseUtils::GetTimeoutMs()),
                __FUNCTION__, "FUSECTRL IDLE");
        if (rc == RC::TIMEOUT_ERROR &&
            Platform::GetSimulationMode() != Platform::Hardware)
        {
            // Ignore timeouts in simulation, which may not support
            // LW_FUSECTRL_STATE_IDLE
            rc.Clear();
        }
    }
    return rc;
}

void RirAccessor::DisableWaitForIdle()
{
    m_WaitForIdle = false;
}

RC RirAccessor::LatchFusesToOptRegs()
{
    // No OPT registers related to RIR
    return OK;
}

RC RirAccessor::VerifyFusing(const vector<UINT32>& expectedBinary)
{
    StickyRC rc;
    const vector<UINT32>* pLwrrentFuses = nullptr;
    CHECK_RC(GetFuseBlock(&pLwrrentFuses));
    MASSERT(expectedBinary.size() == pLwrrentFuses->size());

    for (UINT32 row = 0; row < pLwrrentFuses->size(); row++)
    {
        UINT32 lwrrentRowVal = (*pLwrrentFuses)[row];
        Printf(FuseUtils::GetPrintPriority(), "Final RIR value at row %d: 0x%x\n",
                                               row, lwrrentRowVal);
        if (lwrrentRowVal != expectedBinary[row])
        {
            rc = RC::VECTORDATA_VALUE_MISMATCH;
            Printf(FuseUtils::GetPrintPriority(),
                "RIR record mismatch at row %d: Expected 0x%08x != Actual 0x%08x\n",
                row, expectedBinary[row], lwrrentRowVal);
        }
    }
    return rc;
}

RC RirAccessor::GetNumFuseRows(UINT32 *pNumFuseRows) const
{
    MASSERT(pNumFuseRows);

    // We don't have any register denoting this information
    // So it needs to be hard coded based on the chip
    switch (m_pDev->GetDeviceId())
    {
#if LWCFG(GLOBAL_ARCH_AMPERE)
        case Device::LwDeviceId::GA100:
            *pNumFuseRows = 8;
            break;
#endif
        default:
            *pNumFuseRows = 4;
    }

    return RC::OK;
}
