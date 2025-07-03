/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2016,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "gpu/perf/clockthrottle.h"
#include "gpu/perf/thermsub.h"
#include "core/include/utility.h"

//------------------------------------------------------------------------------
ClockThrottle::ClockThrottle(GpuSubdevice * pGpuSub)
    :      m_pGpuSub(pGpuSub)
          ,m_NeedRestore(false)
          ,m_SavedLWClockThermalSlowdownAllowed(false)

{
    MASSERT(m_pGpuSub);
}

//------------------------------------------------------------------------------
/* virtual */
ClockThrottle::~ClockThrottle()
{
    Release();
}

//------------------------------------------------------------------------------
//! Reserve the clock-pulsing HW of this gpu for this object.
//! Disable RM access to clock-pulsing HW.
//! Save away all register and RM state we will modify.
/* virtual */
RC ClockThrottle::Grab()
{
    RC rc;
    Thermal * pTherm = GpuSub()->GetThermal();

    // Save the flag for whether or not RM is allowed to touch the
    // thermal slowdown HW.
    rc = pTherm->GetClockThermalSlowdownAllowed (
            &m_SavedLWClockThermalSlowdownAllowed);

    // Block the RM from touching the thermal slowdown HW.
    if (OK == rc)
        rc = pTherm->SetClockThermalSlowdownAllowed (false);

    m_NeedRestore = true;

    // Derived classes must override this function, and in their overridden
    // version call this base-class version after calling SaveReg.
    //
    // Probably, derived classes won't need to override Release at all.
    return rc;
}

//------------------------------------------------------------------------------
/* virtual */
RC ClockThrottle::Set
(
    const ClockThrottle::Params * pRequested,
    ClockThrottle::Params *       pActual
)
{
    // Derived classes should override this function.
    // This dummy implementation is to make the base class non-virtual, so that
    // we can alloc a base-class dummy object for unsupported GPUs.
    return RC::UNSUPPORTED_HARDWARE_FEATURE;
}

//------------------------------------------------------------------------------
//! Restore all RM state and registers, release clock-pulsing HW.
RC ClockThrottle::Release()
{
    RC rc;
    if (NeedRestore())
    {
        // Restore RM's ability to fiddle with thermal slowdown regs,
        // assuming it wasn't disabled before we started.
        Thermal * pTherm = GpuSub()->GetThermal();

        rc = pTherm->SetClockThermalSlowdownAllowed (
                m_SavedLWClockThermalSlowdownAllowed);

        // Restore registers saved by derived-class SaveReg calls.
        // We restore in reverse order, i.e. LIFO.
        while (m_SavedRegs.size())
        {
            Reg r = m_SavedRegs.back();
            GpuSub()->RegWr32(r.Addr, r.Value);
            m_SavedRegs.pop_back();
        }

        m_NeedRestore = false;
    }
    return rc;
}

//------------------------------------------------------------------------------
/*static*/
void ClockThrottle::PrintParams (Tee::Priority pri, const Params & params)
{
    Printf(pri, "Hz %d  Duty %f%%  Frac %.3f  Slew in %d-clock steps\n",
            params.PulseHz,
            params.DutyPct,
            params.SlowFrac,
            params.GradStepDur);
}

//------------------------------------------------------------------------------
//! Called by derived-class Grab implementations.
void ClockThrottle::SaveReg (UINT32 addr)
{
    MASSERT(m_pGpuSub);

    Reg r;
    r.Addr = addr;
    r.Value = GpuSub()->RegRd32(addr);
    m_SavedRegs.push_back(r);
    m_NeedRestore = true;
}

//------------------------------------------------------------------------------
bool ClockThrottle::NeedRestore () const
{
    return m_NeedRestore;
}

//------------------------------------------------------------------------------
string ClockThrottle::GetLwrrFreqLabel()
{
    return Utility::StrPrintf("%u", GetLwrrFreqInt());
}

//------------------------------------------------------------------------------
void ClockThrottle::DumpRegs(Tee::Priority pri)
{
    Printf(pri, "ClockThrottle::DumpRegs(): \n");
    vector<Reg>::const_iterator it;
    for (it = m_SavedRegs.begin(); it != m_SavedRegs.end(); ++it)
    {
        UINT32 r = GpuSub()->RegRd32(it->Addr);
        Printf(pri, " bar0[0x%08x] is 0x%08x\n", it->Addr, r);
    }
}

//------------------------------------------------------------------------------
typedef ClockThrottle * (ClockThrottleFactory)(GpuSubdevice *);

extern ClockThrottleFactory GP100ClockThrottleFactory;
extern ClockThrottleFactory GM20xClockThrottleFactory;
extern ClockThrottleFactory GF100ClockThrottleFactory;

static ClockThrottleFactory* FactoryArray[] =
{
    GP100ClockThrottleFactory,
    GM20xClockThrottleFactory,
    GF100ClockThrottleFactory

};

/* static */
ClockThrottle * ClockThrottle::Factory(GpuSubdevice * pGpuSub)
{
    // Try to create a ClockThrottle object by trying each possible type
    // in order of newest to oldest until one matches.

    ClockThrottle * pObj = 0;

    for (UINT32 i = 0; i < sizeof(FactoryArray)/sizeof(FactoryArray[0]); i++)
    {
        pObj = (*FactoryArray[i])(pGpuSub);
        if (pObj)
            return pObj;
    }

    // This gpu is unrecognized, create a dummy base-class object, which will
    // just return an error for all attempts to enable clock-pulsing.
    Printf(Tee::PriLow, "ClockThrottle support missing for %s.\n",
            Gpu::DeviceIdToString(pGpuSub->DeviceId()).c_str());

    return new ClockThrottle(pGpuSub);
}

