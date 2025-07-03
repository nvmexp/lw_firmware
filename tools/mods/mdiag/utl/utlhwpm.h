/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLHWPM_H
#define INCLUDED_UTLHWPM_H

#include "utlpython.h"
#include "utlsurface.h"
#include "core/include/lwrm.h"
#include "mdiag/lwgpu.h"

// This class represents a logical PMA channel from the point of view of a
// UTL script.
//
class UtlPmaChannel
{
public:
    static void Register(py::module module);

    ~UtlPmaChannel();

    UINT32 GetChannelIndex() const;
    static void FreePmaChannels();
    
    // The following functions are called from the Python interpreter.
    static UtlPmaChannel* CreatePy(py::kwargs kwargs);
    void AllocatePy();
    UINT64 UpdateGetPtrPy(py::kwargs kwargs);
    UINT64 GetPutPtrPy();

    // The following functions are also called from the Python interpreter,
    // specifically as property accessor functions.
    void SetRecordBuffer(UtlSurface* pBuffer);
    UtlSurface* GetRecordBuffer() const;
    void SetMemBytesBuffer(UtlSurface* pBuffer);
    UtlSurface* GetMemBytesBuffer() const;

    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlPmaChannel() = delete;
    UtlPmaChannel(UtlPmaChannel&) = delete;
    UtlPmaChannel& operator=(UtlPmaChannel&) = delete;

private:
    // UtlPmaChannel objects should only be created by the static
    // Create function, so the constructor is private.
    UtlPmaChannel(LwRm::Handle profiler);

    LwRm::Handle m_ProfilerObject;

    UtlSurface* m_pRecordBuffer;
    UtlSurface* m_pMemBytesBuffer;

    UINT32 m_ChannelIndex;

    // All UtlPmaChannel objects are stored in this map.
    static map<UtlGpu*, vector<unique_ptr<UtlPmaChannel>>> s_PmaChannels;
};

// This class represents a logical secure profiler from the point of view of a
// UTL script. It provides APIs to manipulate HWPM resources. On GH100 there is 
// only one global profiler object per GPU which binds resources for all PMA channels.
//
class UtlSelwreProfiler
{
public:   
    static void Register(py::module module);

    ~UtlSelwreProfiler();
    static void FreeProfilers();
    LwRm::Handle GetHandle();

    enum ReservationType
    {
        HWPM_LEGACY = 0,
        PM_AREA_SMPC,
        RESERVE_TYPE_MAX 
    };

    // The following functions are called from the Python interpreter.
    static UtlSelwreProfiler* CreatePy(py::kwargs kwargs);
    void ReservePy(py::kwargs kwargs);
    void ReleasePy(py::kwargs kwargs);
    void BindPy();
    void UnbindPy();
    
    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlSelwreProfiler() = delete;
    UtlSelwreProfiler(UtlSelwreProfiler&) = delete;
    UtlSelwreProfiler& operator=(UtlSelwreProfiler&) = delete;

private:
    // UtlSelwreProfiler objects should only be created by the static
    // Create function, so the constructor is private.
    UtlSelwreProfiler(GpuSubdevice* pSubdev);

    LwRm::Handle m_ProfilerObject;

    // All UtlSelwreProfiler objects are stored in this map.
    static map<UtlGpu*, unique_ptr<UtlSelwreProfiler>> s_SelwreProfilers;

    bool m_BindState = false;
    bool m_ReserveState[RESERVE_TYPE_MAX] = {false};
};

#endif
