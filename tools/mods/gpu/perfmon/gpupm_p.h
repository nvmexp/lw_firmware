/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2009,2016,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file gpupm_p.h -- Private perfmon interfaces
// Performance monitor API

#ifndef INCLUDED_GPUPM_P_H
#define INCLUDED_GPUPM_P_H

#ifndef INCLUDED_GPUPM_H
#include "gpu/include/gpupm.h"
#endif

//--------------------------------------------------------------------
//! \brief Base class for all GpuPerfmon Experiments
//!
class GpuPerfmon::Experiment
{
public:
    //! This enum lists the Performance Monitor Hardware type used by the
    //! experiment
    enum PmType
    {
        Gpc,
        Fb,
        Sys,
        Unknown
    };
    Experiment(GpuPerfmon *pPerfmon, GpuPerfmon::ExperimentType type, PmType pmType);
    virtual ~Experiment() { }
    GpuPerfmon *   GetPerfmon() const { return m_pPerfmon; }
    GpuPerfmon::ExperimentType GetType() const { return m_Type; }
    PmType         GetPmType() const { return m_PmType; }

    // Pure virtual functions that all experiments must implement
    virtual RC Start() = 0;
    virtual bool AclwmulateData() = 0;
    virtual RC End() = 0;
    virtual void GetResults(GpuPerfmon::Results * pResults) const;
    virtual bool IsConflicting(GpuPerfmon::Experiment *pExp) const = 0;

protected:
    void SetStarted(bool bStarted) { m_bStarted = bStarted; }
    bool IsStarted() const { return m_bStarted; }
    GpuPerfmon::Results *GetResultsPtr() { return &m_Results; }
private:
    GpuPerfmon *m_pPerfmon;           //!< Permon used for this experiment
    GpuPerfmon::ExperimentType m_Type;//!< Type of experiment
    PmType      m_PmType;             //!< Perfomance monitor harware used
    bool        m_bStarted;           //!< True if the experiment has started
    GpuPerfmon::Results m_Results;    //!< Results of the experiment
};


//--------------------------------------------------------------------
//! \brief Base class for all L2Cache Experiments
//!
class L2CacheExperiment : public GpuPerfmon::Experiment
{
public:
    L2CacheExperiment
    (
        GpuPerfmon *pPerfmon,
        GpuPerfmon::Experiment::PmType pmType,
        UINT32 fbpNum,
        UINT32 l2slice
    );
    virtual ~L2CacheExperiment() { }
    virtual bool IsConflicting(Experiment *pExp) const;

    // Pure virtual functions that all L2CacheExperiments must implement
    virtual RC Start();
    virtual bool AclwmulateData();
    virtual RC End();

    // Common internal variables needed to start/end any experiment
    virtual UINT32 GetL2Slice() const { return m_L2Slice; }
    virtual UINT32 GetFbp() const { return m_FbpNum; }
    virtual UINT32 GetPmIdx() { return m_PmIdx; }
    virtual void SetPmIdx(UINT32 idx) { m_PmIdx = idx; }
    virtual void SetFbp(UINT32 fbp) { m_FbpNum = fbp; }
    virtual void SetL2Slice(UINT32 l2slice) { m_L2Slice = l2slice; }

    // default is to return the current Perfmon index.
    virtual UINT32 CalcPmIdx();

protected:
    void ResetStats();
    void AclwmulateStats(UINT32 HitCount, UINT32 MissCount);

private:
    UINT32 m_PmIdx;
    UINT32 m_FbpNum;  //!< FBP Number to monitor
    UINT32 m_L2Slice; //!< L2 Cache Slice to monitor
};

#endif
