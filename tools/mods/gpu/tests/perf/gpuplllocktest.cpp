/* LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Gpu Pll Lock Test

#include "gpu/tests/gputest.h"
#include "gpu/include/gpusbdev.h"

class GpuPllLockTest : public GpuTest
{
private:
    UINT32 m_PrintPri;
    FLOAT64 m_PllLockTimeMs;
    PStateOwner m_PStateOwner;

public:
    GpuPllLockTest();
    bool IsSupported();
    RC Setup();
    RC Run();
    RC Cleanup();

    SETGET_PROP(PrintPri, UINT32);
    SETGET_PROP(PllLockTimeMs, FLOAT64);
};

//----------------------------------------------------------------------------
// Script interface.
JS_CLASS_INHERIT(GpuPllLockTest, GpuTest, "Gpu PLL Lock test.");
CLASS_PROP_READWRITE(GpuPllLockTest, PrintPri, UINT32,
                     "UINT32: Priority of informational messages");
CLASS_PROP_READWRITE(GpuPllLockTest, PllLockTimeMs, FLOAT64,
            "Time to wait for the PLL to lock");

GpuPllLockTest::GpuPllLockTest() :
    m_PrintPri(Tee::PriLow),
    m_PllLockTimeMs(1)
{
}

//----------------------------------------------------------------------------
bool GpuPllLockTest::IsSupported()
{
    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_SUPPORTS_PLL_QUERIES))
    {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------------
RC GpuPllLockTest::Setup()
{
    RC rc;

    CHECK_RC(m_PStateOwner.ClaimPStates(GetBoundGpuSubdevice()));

    return OK;
}

RC GpuPllLockTest::Cleanup()
{
    m_PStateOwner.ReleasePStates();
    return OK;
}

//------------------------------------------------------------------------------
// Read-Modify-Write the LWPLL, HPLL and SPPLL0 registers to lock the PLLs and
// check if they can be locked.
// TODO: Add support for VPLL0, VPLL1. There is not sufficient information on
//       them right now.
RC GpuPllLockTest::Run()
{
    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();
    vector<GpuSubdevice::PLLType> PllList;
    RC rc;

    pSubdevice->GetPLLList(&PllList);

    for (UINT32 ii = 0; ii < PllList.size(); ++ii)
    {
        GpuSubdevice::PLLType Pll = PllList[ii];
        const char *PllName = pSubdevice->GetPLLName(Pll);

        if (pSubdevice->IsPLLEnabled(Pll))
        {
            // see bug 1386922
            if (pSubdevice->IsPllLockRestricted(Pll))
            {
                continue;
            }

            // Read-Modify-Write and check PLL CFG register
            const UINT32 OldRegVal = pSubdevice->GetPLLCfg(Pll);

            Printf(m_PrintPri, "Old %s CFG: 0x%08x\n", PllName, OldRegVal);

            pSubdevice->SetPLLLocked(Pll);

            // Give some time for the PLL to be locked
            Tasker::Sleep(m_PllLockTimeMs);

            UINT32 NewRegVal = pSubdevice->GetPLLCfg(Pll);
            Printf(m_PrintPri, "New %s CFG: 0x%08x\n", PllName, NewRegVal);

            if (!pSubdevice->IsPLLLocked(Pll))
            {
                Printf(Tee::PriError, "%s could not be locked", PllName);
                Printf(Tee::PriError, "[CFG: 0x%08x].\n", NewRegVal);

                // Bug 731970: Testing to see if the PLL ever gets locked by reading
                // it 100 times and printing a message if it ever gets locked.
                for(UINT32 iter = 0; iter < 100; iter ++)
                {
                    // Sleep for 100us before retry
                    Tasker::Sleep(0.1);
                    pSubdevice->GetPLLCfg(Pll);
                    if (pSubdevice->IsPLLLocked(Pll))
                        Printf(Tee::PriError, "%s does get locked[iter %d]\n",
                                             PllName, iter + 1);
                }
                rc = RC::PLL_LOCK_FAILED;
            }

            pSubdevice->SetPLLCfg(Pll, OldRegVal);
        }
    }

    return rc;
}
