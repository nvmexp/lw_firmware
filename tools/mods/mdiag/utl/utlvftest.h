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

#ifndef INCLUDED_UTLVFTEST_H
#define INCLUDED_UTLVFTEST_H

#include "gpu/vmiop/vmiopelwmgr.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"
#include "core/include/types.h"
#include "utlpython.h"

class UtlVfTest;

// This class represents a limited Function ability from the point of view 
// of a UTL script. It will dynamicaly determine the doable function due to
// the which mode you are working on, Physical Fucntion or Virtual Function.
// Some function have the common ability as the PolicyManager side, so the 
// common parts have been abstracted into MDiagUtils namespace.
// UtlLwrrentSriovFunction is a base function. The main idea is to deduce
// the corretly child class due to the mode.
//
class UtlLwrrentSriovFunction
{
public:
    static void Register(py::module module);
    
    // This virtual is for pybind11 side to automatic upcasting
    virtual ~UtlLwrrentSriovFunction() = default;
};

class UtlPFLwrrentFunction : public UtlLwrrentSriovFunction
{
public:
    static void Register(py::module module);

    UtlVfTest * GetVfTest(py::kwargs kwargs);
    UINT32 GetVfCount();
    void VirtualFunctionReset(py::kwargs kwargs);
    void Rulwf(py::kwargs kwargs);
    void ClearFLRPendingBit(py::kwargs kwargs);
    void SendProcEvent(py::kwargs kwargs);
    void WaitProcEvent(py::kwargs kwargs);
    void WaitFlrDone(py::kwargs kwargs);
    void WaitVfDone(py::kwargs kwargs);
    void SendVmmuInfo();
    string ReadProcMessage(py::kwargs kwargs);
};

class UtlVFLwrrentFunction : public UtlLwrrentSriovFunction
{
public:
    static void Register(py::module module);

    void SendProcEvent(py::kwargs kwargs);
    void WaitProcEvent(py::kwargs kwargs);
    void ExitLwrrentProcess();
    UINT32 GetGfid();
    string ReadProcMessage();
};

class UtlVfTest
{
public:
    static void Register(py::module module);

    UtlVfTest(VmiopMdiagElwManager::VFSetting * pVfSetting);
    static void CreateUtlVfTests(const vector<VmiopMdiagElwManager::VFSetting> & vfSettings);
    const string GetVfTestDirectory() { return GetVfConfig()->vftestDir; }
    const UINT32 GetGFID() { return GetVfConfig()->gfid; }
    const UINT32 GetSeqId() { return GetVfConfig()->seqId; }
    void SetGpuPartition(py::kwargs kwargs);
    VmiopMdiagElwManager::VFSetting * GetVfSetting() { return m_pVfSetting; }
    void Run();
    static UtlVfTest * GetUtlVfTest(py::kwargs kwargs);
    static void FreeVfTests();
    static UINT32 GetVfCount();
    static UtlVfTest* GetFromSeqId(UINT32 seqId);
    static UtlVfTest* GetFromGfid(UINT32 gfid);
private:
    VmiopElwManager::VfConfig * GetVfConfig() { return m_pVfSetting->pConfig; }
    VmiopMdiagElwManager::VFSetting * m_pVfSetting;
    static vector<unique_ptr<UtlVfTest> > s_UtlVfTests;
};

#endif
