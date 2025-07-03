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
#ifndef INCLUDED_GV10XPM_H
#define INCLUDED_GV10XPM_H
#include "gpu/include/gpupm.h"
#include "gpu/perfmon/gm10xpm.h"
//------------------------------------------------------------------------------------------------
// This is a thin wrapper around the GF100GpuPerfmon to address register address spacing changes
// on Volta. The algorithms for running the experiments are still the same.
class GV100GpuPerfmon : public GpuPerfmon
{
public:
    GV100GpuPerfmon(GpuSubdevice *pSubdev);
    virtual ~GV100GpuPerfmon() { };
    virtual UINT32 GetFbpPmmOffset();
    virtual RC CreateL2CacheExperiment
    (
        UINT32 FbpNum,
        UINT32 L2Slice,
        const GpuPerfmon::Experiment **pHandle
    );
};

//------------------------------------------------------------------------------
//! \brief GVLit1L2CacheExperiment Cache experiment
//!
//! This L2 Cache experiment aclwmulates statistics for all operations on the
//! specified slice, for all GVLit1 GPUs (GV100).
class GVLit1L2CacheExperiment : public GML2CacheExperiment
{
public:
    GVLit1L2CacheExperiment
    (
        UINT32 fbpNum,
        UINT32 l2Slice,
        GpuPerfmon* pPerfmon
    );
    virtual ~GVLit1L2CacheExperiment() { }
        //return the offset from the base FBP PMM register to the register for subsequent FBPs
    virtual UINT32 CalcPmIdx();

};
#endif

