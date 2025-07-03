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
#ifndef INCLUDED_GP10XPM_H
#define INCLUDED_GP10XPM_H
#include "gpu/include/gpupm.h"
#include "gpu/perfmon/gm10xpm.h"
//------------------------------------------------------------------------------------------------
// This is a thin wrapper around the GF100GpuPerfmon to address register address spacing changes
// on Turing. The algorithms for running the experiments are still the same.
class GP10xGpuPerfmon : public GpuPerfmon
{
public:
    GP10xGpuPerfmon(GpuSubdevice *pSubdev);
    virtual ~GP10xGpuPerfmon() { };
    virtual RC CreateL2CacheExperiment
    (
        UINT32 FbpNum,
        UINT32 L2Slice,
        const GpuPerfmon::Experiment **pHandle
    );
};
//------------------------------------------------------------------------------
//! \brief GPLit3L2CacheExperiment Cache experiment
//!
//! This L2 Cache experiment aclwmulates statistics for all operations on the
//! specified slice, for all GPLit3 GPUs (GP102/GP104/GP106/GP107/GP108).
class GPLit3L2CacheExperiment : public GML2CacheExperiment
{
public:
    GPLit3L2CacheExperiment
    (
        UINT32 fbpNum,
        UINT32 l2Slice,
        GpuPerfmon* pPerfmon
    );
    virtual ~GPLit3L2CacheExperiment() { }
};

//------------------------------------------------------------------------------
//! \brief GPLit4L2CacheExperiment Cache experiment
//!
//! This L2 Cache experiment aclwmulates statistics for all operations on the
//! specified slice, for all GPLit3 GPUs (GP102/GP104/GP106/GP107/GP108).
class GPLit4L2CacheExperiment : public GML2CacheExperiment
{
public:
    GPLit4L2CacheExperiment
    (
        UINT32 fbpNum,
        UINT32 l2Slice,
        GpuPerfmon* pPerfmon
    );
    virtual ~GPLit4L2CacheExperiment() { }
};
#endif
