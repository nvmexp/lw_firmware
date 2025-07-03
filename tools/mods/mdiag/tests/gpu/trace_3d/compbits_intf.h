/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _COMPBITS_INTF_H_
#define _COMPBITS_INTF_H_

// Dependency eliminated:
// This header file shall not include any ohter files.
// This header file shall be included by only 2 files: trace_3e.cpp and compbits.h.

class GpuDevice;
class TraceChannel;
class TraceSubChannel;
class ArgReader;
class MdiagSurf;

// Keep it as a clean CompBitsTest Interface only
class CompBitsTestIF
{
public:
    static CompBitsTestIF* CreateInstance(TraceSubChannel* pSubChn,
        UINT32 testType);

public:
    virtual ~CompBitsTestIF() {}

    // Passing arguments in.
    virtual RC SetArgs(MdiagSurf* pSurfCA, GpuDevice *pGpuDevice,
        TraceChannel *pChannel, const ArgReader *pParam, FLOAT64 timeoutMs) = 0;
    // Do Init and preparation.
    virtual RC Init();
    virtual RC PostRender();

protected:
    CompBitsTestIF() {}

private:
    // Private non-existing copy ctor to prevent from being copied.
    CompBitsTestIF(const CompBitsTestIF& inst);

};

#endif

