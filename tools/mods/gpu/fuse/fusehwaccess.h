/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/fuse/fusetypes.h"
#include "gpu/fuse/fuseutils.h"

#include "core/include/device.h"

#include <map>
#include <memory>
#include <vector>

class Fuse;
class TestDevice;

class FuseHwAccessor
{
public:
    virtual ~FuseHwAccessor() = default;

    // Sets a pointer to the Fuse object this HwAccessor belongs to in case it needs
    // to access any general fuse functions (e.g. GetFuseValue("sw_override_en"))
    virtual void SetParent(Fuse* pFuse) = 0;

    // Sets up the prerequisites for burning fuses on the chip
    virtual RC SetupFuseBlow() = 0;
    // Resets chip state after burning fuses on the chip
    virtual RC CleanupFuseBlow() = 0;

    // Burns the given fuse data to the fuse block
    virtual RC BlowFuses(const vector<UINT32>& fuseBinary) = 0;
    // Writes the given address/value pairs to the OPT registers
    virtual RC SetSwFuses(const map<UINT32, UINT32>& swFuseWrites) = 0;
    // Writes IFF records to dedicated RAM
    virtual RC SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc) = 0;
    // Verifies IFF records
    virtual RC VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc) = 0;

    // Gets a single fuse row from this macro
    virtual RC GetFuseRow(UINT32 row, UINT32* pValue) = 0;

    // Gets the value for an OPT fuse given its address
    virtual UINT32 GetOptFuseValue(UINT32 address) = 0;

    // Ilwalidate any lwrrently cached fuses to force a
    // re-read the next time the fuse block is requested
    // TODO: let FuseEncoder observe this, so if any of the
    // fuse caches get marked dirty, the fuse decode is also
    // marked dirty
    void MarkFuseReadDirty()
    {
        m_CachedFuses.clear();
    }

    // Disables waiting for the fuse HW to go idle (used with fmodel)
    virtual void DisableWaitForIdle() = 0;

    // Checks whether or not the fusing registers are priv protected
    // (thus requiring a uCode/HULK/other security unlock to fuse)
    virtual bool IsFusePrivSelwrityEnabled() = 0;

    // Checks whether or not SW fusing is still allowed
    virtual bool IsSwFusingAllowed() = 0;

    // Trigger the hardware to re-read fuses and update the OPT registers
    virtual RC LatchFusesToOptRegs() = 0;

    // Trigger a SW Reset to have HW re-read the OPT fuses (after SW fusing)
    virtual RC SwReset() = 0;

    // Return the number of rows for this macro
    virtual RC GetNumFuseRows(UINT32 *pNumRows) const = 0;

    // Return the fuse clock period
    virtual UINT32 GetFuseClockPeriodNs() const = 0;

    // Retrieves the entire fuse block for this macro
    RC GetFuseBlock(const vector<UINT32>** ppCachedVals)
    {
        MASSERT(ppCachedVals != nullptr);

        RC rc;
        if (m_CachedFuses.empty())
        {
            CHECK_RC(ReadFuseBlock(&m_CachedFuses));
        }
        *ppCachedVals = &m_CachedFuses;
        return rc;
    }

    virtual RC SetFusingVoltage(UINT32 vddMv) = 0;

protected:
    virtual RC ReadFuseBlock(vector<UINT32>* pReadFuses) = 0;

    // Print a table of new bits and the new final row value for the fuse block
    void PrintNewFuses(const vector<UINT32>& oldBinary, const vector<UINT32>& newBinary)
    {
        Printf(FuseUtils::GetPrintPriority(), "New Bits   | Expected Final Value \n");
        Printf(FuseUtils::GetPrintPriority(), "----------   ----------\n");
        MASSERT(oldBinary.size() == newBinary.size());
        for (UINT32 i = 0; i < newBinary.size(); i++)
        {
            UINT32 newRow = newBinary[i];
            UINT32 oldRow = oldBinary[i];
            UINT32 newBits = newRow & ~oldRow;
            Printf(FuseUtils::GetPrintPriority(), "0x%08x : 0x%08x\n", newBits, newRow);
        }
    }

private:
    vector<UINT32> m_CachedFuses;
};
