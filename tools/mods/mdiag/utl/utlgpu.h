/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLGPU_H
#define INCLUDED_UTLGPU_H

class GpuDevice;
class GpuSubdevice;
class UtlGpuPartition;
class UtlSmcEngine;
class UtlEngine;
class UtlSurface;
class UtlMemCtrl;
class UtlFbInfo;
class UtlChannel;

// This class represents the C++ backing for the UTL Python Gpu object.
// This class is a wrapper around a pair of GpuDevice and GpuSubdevice
// pointers.  There will be one Gpu object for every unique pair of
// GpuDevice and GpuSubdevice pointers.
//

#include "ctrl/ctrl0080.h"
#include "mdiag/smc/smcresourcecontroller.h"
#include "utlpython.h"
#include "core/include/lwrm.h"

class UtlGpuRegCtrl;

class UtlGpu
{
public:
    static void Register(py::module module);
    
    enum class ResetType : UINT32
    {
        Software = LW0080_CTRL_BIF_RESET_FLAGS_TYPE_SW_RESET,
        SecondaryBus = LW0080_CTRL_BIF_RESET_FLAGS_TYPE_SBR,
        Fundamental = LW0080_CTRL_BIF_RESET_FLAGS_TYPE_FUNDAMENTAL
    };

    enum class L2OperationTargetMemory
    {
        All,
        Vidmem
    };

    enum class L2OperationType
    {
        Writeback, 
        Evict
    };

    GpuDevice* GetGpudevice() const { return m_GpuDevice; }
    GpuSubdevice* GetGpuSubdevice() const { return m_GpuSubdevice; }

    UtlGpuPartition * CreateGpuPartition(GpuPartition * pGpuPartition);
    UtlGpuPartition * GetGpuPartition(SmcResourceController::SYS_PIPE syspipe);
    UtlGpuPartition * GetGpuPartition(GpuPartition * pGpuPartition);
    UtlGpuPartition * GetGpuPartitionByName(string name);
    const vector<UtlGpuPartition*>& GetGpuPartitionList() const
        { return m_UtlGpuPartitions; }

    void AddSurface(UtlSurface* pUtlSurface);

    // All below function is explored to python call
    // the version to handle argument from python script
    py::object CreateGpuPartitionPy(py::kwargs kwargs);
    UtlGpuPartition * GetGpuPartitionPy(py::kwargs kwargs);
    py::list GetGpuPartitionListPy() const;
    UINT32 GetSmCount();
    UINT32 GetTpcCountPy(py::kwargs kwargs);
    UINT32 GetPesCountPy(py::kwargs kwargs);
    UINT32 GetGpcCountPy();
    UINT32 GetGpcMask();
    UtlGpuRegCtrl* GetRegCtrl();
    UtlMemCtrl* GetMemCtrl(py::kwargs kwargs);
    void SetGpuPartitionMode(py::kwargs kwargs);
    map<SmcResourceController::PartitionFlag, UINT32> GetAvailablePartitionTypeCount(py::kwargs kwargs);
    void Reset(py::kwargs kwargs);
    void RecoverFromReset();
    void PreemptRunlist(py::kwargs kwargs);
    void ResubmitRunlist(py::kwargs kwargs);
    vector<UINT32> GetSupportedClasses();
    py::list GetSurfaces();
    string GetChipName() const;
    UINT32 GetIrq();

    bool HasFeature(py::kwargs kwargs);

    LWGpuResource* GetGpuResource();
    SmcResourceController * FetchSmcResourceController();

    static void CreateGpus();
    static void FreeGpus();
    static py::list GetGpusPy();
    static const vector<UtlGpu*>& GetGpus() { return s_PythonGpus; }
    static UtlGpu* GetFromGpuSubdevicePtr(GpuSubdevice* pGpuSubdevice);

    void RemoveGpuPartition(py::kwargs kwargs);

    py::list GetEngines(py::kwargs kwargs);

    vector<std::pair<PHYSADDR, UINT64>> GetUprRegions() const;
    UINT32 GetGpuId() const;
    UINT32 GetDeviceInst() const;
    UINT32 GetSubdeviceInst() const;

    void TlbIlwalidatePy(py::kwargs kwargs);
    RC TlbIlwalidate(LwRm* pLwRm, LwRm::Handle hVaSpace);
    void L2Operation(py::kwargs kwargs);
    UtlFbInfo* GetFbInfo();
    UINT32 GetLwlinkEnabledMask();
    void FbFlush();
    void SetRcRecovery(py::kwargs kwargs);
    std::pair<PHYSADDR, UINT64> GetRunlistPool();
    bool IsSmcEnabled();
    UINT32 GetMmuEngineIdStart();

    py::object GetPdbConfigurePy(py::kwargs kwargs);
    RC GetPdbConfigure
    (
        UtlChannel* pPdbTargetChannel,
        UtlSurface* pSurface,
        bool shouldIlwalidateReplay,
        bool tlbIlwalidateBar1,
        bool tlbIlwalidateBar2,
        UINT64* pdbPhyAddrAlign,
        UINT32* pdbAperture,
        LwRm* pLwRm
    );

    bool CheckConfigureLevelPy(py::kwargs kwargs);
    RC CheckConfigureLevel
    (
        UINT32 tlbIlwalidateLevelNum,
        bool * isValid,
        LwRm* pLwRm
    ) const;

    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlGpu() = delete;
    UtlGpu(UtlGpu&) = delete;
    UtlGpu& operator=(UtlGpu&) = delete;

private:
    UtlGpu(GpuDevice* gpuDevice, GpuSubdevice* gpuSubdevice);

    UINT32 GetSmPerTpc();
    UINT32 GetTpcCount();
    UtlSurface* GetSurface(const string& name);

    GpuDevice* m_GpuDevice;
    GpuSubdevice* m_GpuSubdevice;

    // This map has ownership of the UtlGpu objects.
    static map<GpuSubdevice*, unique_ptr<UtlGpu>> s_Gpus;

    // This is the list that is passed to Python for the GetGpus function.
    // The m_Gpus member can't be used, otherwise Python will take
    // ownership of the UtlGpu objects due to the unique_ptr.
    static vector<UtlGpu*> s_PythonGpus;

    vector<UtlGpuPartition *> m_UtlGpuPartitions;

    vector<UtlSurface*> m_Surfaces;

    unique_ptr<UtlFbInfo> m_pUtlFbInfo;
};

#endif
