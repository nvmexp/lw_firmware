/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010, 2016, 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file gpupm.h -- declares GpuPerfmon.
// Performance monitor API

#ifndef INCLUDED_GPUPM_H
#define INCLUDED_GPUPM_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#ifndef INCLUDED_TASKER_H
#include "core/include/tasker.h"
#endif

#ifndef INCLUDED_STL_VECTOR
#define INCLUDED_STL_VECTOR
#include <vector>
#endif

class GpuSubdevice;

//--------------------------------------------------------------------
//! \brief Provides a top level interface to GPU performance monitoring
//!        hardware
//!
class GpuPerfmon
{
public:
    class Experiment;
    enum ExperimentType
    {
        ExpL2Cache
    };
    struct Results
    {
        ExperimentType Type;
        union
        {
            struct
            {
                UINT64 HitCount;
                UINT64 MissCount;
            } CacheResults;
        } Data;
        Results()
        {
            Type = (ExperimentType)0;
            Data.CacheResults.HitCount = 0;
            Data.CacheResults.MissCount = 0;
        }
    };
                    explicit GpuPerfmon(GpuSubdevice *pSubdev);
    virtual         ~GpuPerfmon();
    RC              AddExperiment(Experiment *pExperiment);
    RC              BeginExperiments();
    RC              DestroyExperiment(const Experiment *pExperiment);
    RC              EndExperiments();
    GpuSubdevice *  GetGpuSubdevice() const { return m_pGpuSubdev; }
    void            ShutDown();

    // Each specific subclass must implement its own version of this API
    // to account for the specifics necessary for the particular GPU
    virtual RC      CreateL2CacheExperiment(UINT32 FbpNum, UINT32 L2Slice,
                                       const Experiment **pHandle) = 0;
    virtual Tee::Priority GetPrintPri() { return m_Pri; }
    virtual UINT32  GetFbpPmmOffset();
    virtual void    GetResults(const Experiment *pHandle, Results * pResults);
    virtual void    ResetFbpPerfmon();
    virtual void    ResetFbpPmControl(UINT32 FbpNum, UINT32 PmIdx);
    virtual void    SetPrintPri(Tee::Priority pri) { m_Pri = pri; }
    virtual RC      SetupFbpExperiment(UINT32 FbpNum, UINT32 PmIdx);
    virtual void    TriggerCounters();

private:
    static void     AcquireDataHandler(void *pThis);
    void            ReadCounters();
    RC              ReservePerfMon(bool reserve);

    vector<Experiment *> m_Experiments;    //!< List of experiments to run
    GpuSubdevice        *m_pGpuSubdev;     //!< GpuSubdevice assoicated with
                                           //!< the perfmon class
    Tasker::ThreadID     m_ThreadID;       //!< Thread ID for data acquisition
    bool                 m_bStopAcquiring; //!< Flag to stop aquisition
    bool                 m_PerfmonReserved;//!< Flag indicating whether PM was
                                           //!< reserved in RM
    bool                 m_PollingError;   //!< Flag indicating that there was
                                           //!< an error when polling the perfmon
                                           //!< shadow registers.
    Tee::Priority        m_Pri;            //!< default = PriLow, increase to debug
                                           //!< experiments

};

#endif
