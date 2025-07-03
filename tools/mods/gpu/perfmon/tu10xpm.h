/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_TU10XPM_H
#define INCLUDED_TU10XPM_H
#include "gpu/include/gpupm.h"
#include "gpu/perfmon/gm10xpm.h"
//------------------------------------------------------------------------------------------------
// This is a thin wrapper around the GF100GpuPerfmon to address register address spacing changes
// on Turing. The algorithms for running the experiments are still the same.
class TU10xGpuPerfmon : public GpuPerfmon
{
public:
    TU10xGpuPerfmon(GpuSubdevice *pSubdev);
    virtual ~TU10xGpuPerfmon() { };
    virtual UINT32 GetFbpPmmOffset();
    virtual RC SetupFbpExperiment(UINT32 FbpNum, UINT32 PmIdx);
    virtual RC CreateL2CacheExperiment
    (
        UINT32 FbpNum,
        UINT32 L2Slice,
        const GpuPerfmon::Experiment **pHandle
    );
};

//------------------------------------------------------------------------------
//! \brief TULit2L2CacheExperiment Cache experiment
//!
//! This L2 Cache experiment aclwmulates statistics for all operations on the
//! specified slice, for all TULit2 GPUs (TU10x).
class TULit2L2CacheExperiment : public GML2CacheExperiment
{
public:
    TULit2L2CacheExperiment
    (
        UINT32 fbpNum,
        UINT32 l2Slice,
        GpuPerfmon* pPerfmon
    );
    virtual ~TULit2L2CacheExperiment() { }
    // Callwlate the correct Perfmon domain index for the current FPB/LTC/SLICE
    virtual UINT32 CalcPmIdx();

};
//------------------------------------------------------------------------------
//! \brief TULit2L2CacheExperiment Cache experiment
//!
//! This L2 Cache experiment aclwmulates statistics for all operations on the
//! specified slice, for all TULit3 GPUs (TU11x).
class TULit3L2CacheExperiment : public GML2CacheExperiment
{
public:
    TULit3L2CacheExperiment
    (
        UINT32 fbpNum,
        UINT32 l2Slice,
        GpuPerfmon* pPerfmon
    );
    virtual ~TULit3L2CacheExperiment() { }
    // Callwlate the correct Perfmon domain index for the current FPB/LTC/SLICE
    virtual UINT32 CalcPmIdx();

};
#endif

