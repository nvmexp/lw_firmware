/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLMEMCTRL_H
#define INCLUDED_UTLMEMCTRL_H

#include "utlpython.h"
#include "core/include/lwrm.h"
#include "utlgpu.h"

class GpuSubdevice;

// Represents an abstract Memory Controller. This class will implement the 
// common functionalities (Alloc/Free for now) of all derived Memory Controller 
// classes. Each Gpu can have one MemoryController of each type.
// Lwrrently supported Memory Controllers are- MmioPriv and MmioUser
class UtlMemCtrl
{
public:
    static void Register(py::module module);
    
    enum class MemCtrlType
    {
        MMIO_PRIV,
        MMIO_USER,
        FBMEM
    };

    enum { INVALID = ~0ULL }; 

    static UtlMemCtrl* GetMemCtrl
    (
        MemCtrlType memCtrlType, 
        UtlGpu* pUtlGpu,
        LwRm* pLwRm,
        UINT64 address = INVALID,
        UINT64 memSize = INVALID
    );
    static void FreeMemCtrls();
    UtlMemCtrl(UtlGpu* pUtlGpu, LwRm* pLwRm, string name);
    virtual ~UtlMemCtrl();

    virtual void Alloc() = 0;

    void AllocPy();
    void FreePy();
    UINT64 GetGpuVAPy();
    string GetName() { return m_Name; }
    void WritePy(py::kwargs kwargs);
    UINT32 ReadPy(py::kwargs kwargs);

    UtlMemCtrl() = delete;
    UtlMemCtrl(UtlMemCtrl&) = delete;
    UtlMemCtrl& operator=(UtlMemCtrl&) = delete;

    // Each GPU can have its own memory object of all types
    typedef tuple<UtlGpu*, LwRm*, MemCtrlType> ResourceKey;

protected:
    GpuSubdevice* GetGpuSubdevice() { return m_pUtlGpu->GetGpuSubdevice(); }
    GpuDevice* GetGpuDevice() { return m_pUtlGpu->GetGpudevice(); }

    UtlGpu* m_pUtlGpu;
    LwRm* m_pLwRm;
    LwRm::Handle m_Handle;
    LwRm::Handle m_CtxDma;
    void* m_CpuPointer;
    UINT64 m_GpuVA;

private:
    virtual void Free();
    static map<ResourceKey, UtlMemCtrl*> s_MMIOMemCtrlMap;
    static vector<unique_ptr<UtlMemCtrl>> s_MemCtrls;
    string m_Name;
};

// This is a concrete MMIO Privileged class. It is an interface to the 
// privileged BAR1 MMIO page.
class UtlMmioPriv : public UtlMemCtrl
{
public:
    UtlMmioPriv(UtlGpu* pUtlGpu, LwRm* pLwRm);

    virtual void Alloc();
};

// This is a concrete MMIO User class. It is an interface to the user BAR1 MMIO
// page.
class UtlMmioUser : public UtlMemCtrl
{
public:
    UtlMmioUser(UtlGpu* pUtlGpu, LwRm* pLwRm);

    virtual void Alloc();
};

class UtlFbMem : public UtlMemCtrl
{
public:
    UtlFbMem(UtlGpu* pUtlGpu, LwRm* pLwRm, UINT64 gpuPA, UINT64 memSize);
    virtual ~UtlFbMem();

    virtual void Alloc();
    virtual void Free();

private:
    UINT64 m_GpuPA;
    UINT64 m_MemSize;
};

#endif
