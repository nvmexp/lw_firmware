/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012,2014,2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GPUMTEST_H
#define INCLUDED_GPUMTEST_H

#include "gputest.h"
#include "core/include/memerror.h"
#include "gpu/utility/gpuutils.h"
#include <memory>
#include <vector>

/**
 * All MODS GPU memory tests should inherit from the GpuMemTest base class.
 */
class GpuMemTest : public GpuTest
{
public:
    GpuMemTest();
    virtual ~GpuMemTest();

    virtual RC Run() = 0;

    virtual RC InitFromJs();
    virtual void PrintJsProperties(Tee::Priority pri);

    virtual RC Setup();
    virtual RC Cleanup();
    virtual bool IsSupported();
    virtual bool IsTestType(TestType tt);
    virtual RC EndLoop(UINT32 loop = 0xffffffff);

    // Legacy JS interfaces use names "Start" and "End" instead of
    // "StartLocation" and "EndLocation" as in the C++ code.
    UINT32 GetStart()         { return GetStartLocation(); }
    RC     SetStart(UINT32 i) { return SetStartLocation(i); }
    UINT32 GetEnd()           { return GetEndLocation(); }
    RC     SetEnd(UINT32 i)   { return SetEndLocation(i); }

    RC SetStartLocation(UINT32 end);
    UINT32 GetStartLocation();
    RC SetEndLocation(UINT32 end);
    UINT32 GetEndLocation();

    SETGET_PROP(MinFbMemPercent, UINT32);
    SETGET_PROP(MaxFbMb, FLOAT64);
    SETGET_PROP(ChunkSizeMb, FLOAT64);
    SETGET_PROP(L2ModeToTest, UINT32);
    SETGET_PROP(NumRechecks, UINT32);
    SETGET_PROP(IntermittentError, bool);
    SETGET_PROP(AutoRefreshValue, UINT32);
    SETGET_PROP(DumpLaneRepair, string);

protected:
    FLOAT64 m_ChunkSizeMb;

    RC AllocateEntireFramebuffer
    (
        bool BlockLinear,
        UINT32 hChannel
    );
    RC AllocateEntireFramebuffer2
    (
        bool BlockLinear,
        UINT64 minPageSize,
        UINT64 maxPageSize,
        UINT32 hChannel,
        bool contiguous
    );
    GpuUtility::MemoryChunks * GetChunks() { return &m_MemoryChunks; }
    void PrintIntermittentError(RC rc);
    const GpuUtility::MemoryChunks * GetChunks() const
        { return &m_MemoryChunks; }
    const GpuUtility::MemoryChunkDesc *GetChunk(UINT64 FbOffset) const;
    SETGET_PROP(DidResolveMemErrorResult, bool);

private:
    UINT32      m_MinFbMemPercent;
    FLOAT64     m_MaxFbMb;
    UINT32      m_EndFbMb;
    UINT32      m_StartFbMb;
    UINT32      m_L2ModeToTest;
    UINT32      m_NumRechecks;
    UINT32      m_AutoRefreshValue;
    UINT32      m_OrgAutoRefreshValue;
    bool        m_IntermittentError;
    vector<UINT32>  m_OrgL2Mode;
    string      m_DumpLaneRepair;

    GpuUtility::MemoryChunks  m_MemoryChunks;
    Callbacks::Handle m_hPreRunCallbackMemError;
    Callbacks::Handle m_hPostRunCallbackMemError;

    RC OnPreRun(const CallbackArguments &args);
    RC OnPostRun(const CallbackArguments &args);
    RC DumpLaneRepairFile();
};

extern SObject GpuMemTest_Object;

#endif // INCLUDED_GPUMEMTEST_H
