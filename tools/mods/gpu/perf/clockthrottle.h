/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CLOCKTHROTTLE_H
#define INCLUDED_CLOCKTHROTTLE_H

#ifndef INCLUDED_GPUSBDEV_H
#include "gpu/include/gpusbdev.h"
#endif
#ifndef INCLUDED_TEE_H
#include "core/include/tee.h"
#endif

//! ClockThrottle controls the "pulse width modulation" gpu clock slowdown feature.
//!
class ClockThrottle
{
public:
    struct Params
    {
        UINT32      PulseHz;    //!< Pulse rate
        FLOAT32     DutyPct;    //!< Percent of each pulse spent at full clocks
        FLOAT32     SlowFrac;   //!< Low clock fraction, i.e. .25 for DIV4 clocks
        UINT32      GradStepDur;//!< GRAD_STEP_DURATION in clocks, 0 for square-wave stepping
    };
    static void PrintParams(Tee::Priority pri, const Params & params);

    virtual RC Grab();
    virtual RC Set
    (
        const Params * pRequested,
        Params *       pActual
    );
    virtual RC Release();

    virtual void SetDebug( bool dbg ) { }; //provide default null impl.
    virtual void SetDomainMask(UINT32 newMask) {}
    virtual UINT32 GetDomainMask() const { return 0; }

    // for the frequency shmoo mode
    virtual string GetLwrrFreqLabel();
    virtual UINT32 GetLwrrFreqIdx()      { return 0; }
    virtual UINT32 GetLwrrFreqInt()      { return 0; }
    virtual UINT32 GetNumFreq()          { return 1; }

protected:
    ClockThrottle(GpuSubdevice * pGpuSub);
    virtual ~ClockThrottle();

    void SaveReg(UINT32 addr);
    bool NeedRestore() const;
    GpuSubdevice * GpuSub() const { return m_pGpuSub; }

    //! Read and print current value of all regs in m_SavedRegs.
    void DumpRegs(Tee::Priority pri);

private:
    struct Reg
    {
        UINT32 Addr;
        UINT32 Value;
    };
    GpuSubdevice *  m_pGpuSub;
    bool            m_NeedRestore;
    bool            m_SavedLWClockThermalSlowdownAllowed;
    vector<Reg>     m_SavedRegs;

    //! This class is created only by its owning GpuSubdevice object.
    static ClockThrottle * Factory(GpuSubdevice * pGpuSub);
    friend class GpuSubdevice;
};

#endif // INCLUDED_CLOCKTHROTTLE_H
