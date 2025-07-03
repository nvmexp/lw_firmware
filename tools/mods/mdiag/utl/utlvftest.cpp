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

#include "utlvftest.h"
#include "gpu/vmiop/vmiopelwmgr.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "mdiag/sysspec.h"
#include "mdiag/utl/utlutil.h"
#include "mdiag/vmiopmods/vmiopmdiagelw.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"
#include "mdiag/utl/utlgpupartition.h"

vector<unique_ptr<UtlVfTest> > UtlVfTest::s_UtlVfTests;

void UtlLwrrentSriovFunction::Register(py::module module)
{
    py::class_<UtlLwrrentSriovFunction>(module, "LwrrentSriovFunction",
        "The UtlLwrrentSriovFunction class is used for objects that represent SRIOV PF or VF. This objects will deduce the current process is working or which model. This ability also isolate some APIs.");
}

void UtlPFLwrrentFunction::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("PfLwrrentFunction.ClearFLRPendingBit", "vfTest", true, "target VF test");
    kwargs->RegisterKwarg("PfLwrrentFunction.GetVfTest", "fromYmlFileSeqId", false, "sequence ID of test");
    kwargs->RegisterKwarg("PfLwrrentFunction.GetVfTest", "fromYmlFileGFID", false, "GFID of test");
    kwargs->RegisterKwarg("PfLwrrentFunction.Rulwf", "vfTest", true, "VF test to run");
    kwargs->RegisterKwarg("PfLwrrentFunction.SendProcEvent", "vfTest", true, "target VF test");
    kwargs->RegisterKwarg("PfLwrrentFunction.SendProcEvent", "message", true, "message to send");
    kwargs->RegisterKwarg("PfLwrrentFunction.VirtualFunctionReset", "vfTest", true, "target VF test");
    kwargs->RegisterKwarg("PfLwrrentFunction.WaitFlrDone", "vfTest", true, "target VF test");
    kwargs->RegisterKwarg("PfLwrrentFunction.WaitProcEvent", "vfTest", true, "target VF test");
    kwargs->RegisterKwarg("PfLwrrentFunction.WaitProcEvent", "message", true, "message to wait for");
    kwargs->RegisterKwarg("PfLwrrentFunction.WaitVfDone", "vfTest", true, "target VF test");
    kwargs->RegisterKwarg("PfLwrrentFunction.ReadProcMessage", "vfTest", true, "target VF test");

    py::class_<UtlPFLwrrentFunction, UtlLwrrentSriovFunction> pfLwrrentFunction(module, "PfLwrrentFunction");
    pfLwrrentFunction.def("GetVfTest", &UtlPFLwrrentFunction::GetVfTest,
        UTL_DOCSTRING("PfLwrrentFunction.GetVfTest", "This function support via GFID or SeqId to query the vfTest.\n"
        "VfTest::GetVfTest(fromYmlFileSeqId = \n"
        "VfTest::GetVfTest(fromYmlFileGFID = "),
        py::return_value_policy::reference);
    pfLwrrentFunction.def("VirtualFunctionReset", &UtlPFLwrrentFunction::VirtualFunctionReset,
        UTL_DOCSTRING("PfLwrrentFunction.VirtualFunctionReset", "This function will trigger the specified VF virtual function level reset."));
    pfLwrrentFunction.def("Rulwf", &UtlPFLwrrentFunction::Rulwf,
        UTL_DOCSTRING("PfLwrrentFunction.Rulwf", "This function will launch a sperated Mods process for specified VF"));
    pfLwrrentFunction.def("ClearFLRPendingBit", &UtlPFLwrrentFunction::ClearFLRPendingBit,
        UTL_DOCSTRING("PfLwrrentFunction.ClearFLRPendingBit", "This function will clear the specified VF pending bit."));
    pfLwrrentFunction.def("SendProcEvent", &UtlPFLwrrentFunction::SendProcEvent,
        UTL_DOCSTRING("PfLwrrentFunction.SendProcEvent", "This function will send a message to the specified VF."));
    pfLwrrentFunction.def("WaitProcEvent", &UtlPFLwrrentFunction::WaitProcEvent,
        UTL_DOCSTRING("PfLwrrentFunction.WaitProcEvent", "This function will wait a message from the specified VF."));
    pfLwrrentFunction.def("WaitFlrDone", &UtlPFLwrrentFunction::WaitFlrDone,
        UTL_DOCSTRING("PfLwrrentFunction.WaitFlrDone", "This function will wait FLR done."));
    pfLwrrentFunction.def("WaitVfDone", &UtlPFLwrrentFunction::WaitVfDone,
        UTL_DOCSTRING("PfLwrrentFunction.WaitVfDone", "This function will wait VF done.")); 
    pfLwrrentFunction.def("SendVmmuInfo", &UtlPFLwrrentFunction::SendVmmuInfo,
        UTL_DOCSTRING("PfLwrrentFunction.SendVmmuInfo", "This function will send vmmu info to CPU Model for ATS purpose."));
    pfLwrrentFunction.def("GetVfCount", &UtlPFLwrrentFunction::GetVfCount,
        UTL_DOCSTRING("PfLwrrentFunction.GetVfCount", "This function returns the number of VFs."));
    pfLwrrentFunction.def("ReadProcMessage", &UtlPFLwrrentFunction::ReadProcMessage,
        UTL_DOCSTRING("PfLwrrentFunction.ReadProcMessage", "This function returns the message from the specified VF."));
}

void UtlVFLwrrentFunction::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("VfLwrrentFunction.SendProcEvent", "message", true, "message to send");
    kwargs->RegisterKwarg("VfLwrrentFunction.WaitProcEvent", "message", true, "message to wait for");

    py::class_<UtlVFLwrrentFunction, UtlLwrrentSriovFunction> vfLwrrentFunction(module, "VfLwrrentFunction");
    vfLwrrentFunction.def("SendProcEvent", &UtlVFLwrrentFunction::SendProcEvent,
        UTL_DOCSTRING("VfLwrrentFunction.SendProcEvent", "This function will send a message to PF."));
    vfLwrrentFunction.def("WaitProcEvent", &UtlVFLwrrentFunction::WaitProcEvent,
        UTL_DOCSTRING("VfLwrrentFunction.WaitProcEvent", "This function will wait a message from the PF."));
    vfLwrrentFunction.def("ExitLwrrentProcess", &UtlVFLwrrentFunction::ExitLwrrentProcess,
        "This function will exit VF process without triggering any hw transaction.");
    vfLwrrentFunction.def("GetGfid", &UtlVFLwrrentFunction::GetGfid,
        "This function will return the gfid for current function.");
    vfLwrrentFunction.def("ReadProcMessage", &UtlVFLwrrentFunction::ReadProcMessage,
        "This function will return the message from the PF.");
}

void UtlVfTest::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("VfTest.SetGpuPartition", "gpuPartition", true, "partition that VF would run on");

    py::class_<UtlVfTest> vfTest(module, "VfTest");
    vfTest.def_property_readonly("gfid", &UtlVfTest::GetGFID);
    vfTest.def_property_readonly("seqId", &UtlVfTest::GetSeqId);
    vfTest.def("SetGpuPartition", &UtlVfTest::SetGpuPartition,
        "This function support to set a gpupartition to a specified vftest.");
}

UtlVfTest::UtlVfTest
(
    VmiopMdiagElwManager::VFSetting * pVfSetting
) :
    m_pVfSetting(pVfSetting)
{

}

/*static*/void UtlVfTest::CreateUtlVfTests(const vector<VmiopMdiagElwManager::VFSetting> & vfSettings)
{
    for (const auto &vfSetting : vfSettings)
    {
        s_UtlVfTests.push_back(make_unique<UtlVfTest>(
                    const_cast<VmiopMdiagElwManager::VFSetting *>(&vfSetting)));
    }
}

void UtlVfTest::SetGpuPartition(py::kwargs kwargs)
{
    UtlGpuPartition * pUtlGpuPartition = UtlUtility::GetRequiredKwarg<UtlGpuPartition *>(kwargs, "gpuPartition",
                                "VfTest.SetGpuPartition");
    UtlGil::Release gil;

    UINT32 swizzId =  pUtlGpuPartition->GetSwizzId();
    m_pVfSetting->swizzid = swizzId;
}

/*static*/UtlVfTest * UtlVfTest::GetUtlVfTest(py::kwargs kwargs)
{
    static const UINT32 UNDEFINED_VALUE = ~0x0;

    UINT32 fromYmlFileSeqId = UNDEFINED_VALUE;
    UINT32 fromYmlFileGFID = UNDEFINED_VALUE;
    
    UtlUtility::GetOptionalKwarg<UINT32>(
                &fromYmlFileSeqId, kwargs, "fromYmlFileSeqId", "PfLwrrentFunction.GetVfTest");
    UtlUtility::GetOptionalKwarg<UINT32>(
                    &fromYmlFileGFID, kwargs, "fromYmlFileGFID", "PfLwrrentFunction.GetVfTest");
    
    UtlGil::Release gil;

    if ((fromYmlFileSeqId == UNDEFINED_VALUE && fromYmlFileGFID == UNDEFINED_VALUE) ||
            (fromYmlFileSeqId != UNDEFINED_VALUE && fromYmlFileGFID != UNDEFINED_VALUE))
    {
        UtlUtility::PyError("This function require fromYmlFileSeqId and fromYmlFileGFID if and only if one.");
    }

    UtlVfTest * pUtlVfTest = nullptr;

    if (fromYmlFileSeqId != UNDEFINED_VALUE)
    {
        pUtlVfTest = GetFromSeqId(fromYmlFileSeqId);
    }
    else if (fromYmlFileGFID != UNDEFINED_VALUE)
    {
        pUtlVfTest = GetFromGfid(fromYmlFileGFID);
    }
 
    if (pUtlVfTest == nullptr)
    {
        UtlUtility::PyError("Can't find any matched keywords fromYmlFileSeqId or fromYmlFileGFID."
                            "Please double confirm the argument fromYmlFileSeqId or fromYmlFileGFID.");
    }

    return pUtlVfTest;
}

void UtlVfTest::Run()
{
    // Making sure that UTL does not launch VFs when Policy manager launches VFs. 
    if (UtlUtility::IsPolicyFileLaunchingVFs())
        UtlUtility::PyError("Cannot launch VFs from UTL as Policy Manager is launching the VFs.");
    
    vector<VmiopElwManager::VfConfig> vfConfigs;
    vector<VmiopMdiagElwManager::VFSetting*> vfSettings;
    VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();

    vfConfigs.push_back(*GetVfConfig());
    vfSettings.push_back(m_pVfSetting);
    RC rc = pVmiopMdiagElwMgr->RulwfTests(vfConfigs, vfSettings);
    UtlUtility::PyErrorRC(rc, "Fail to run vf test whose GFID = %d.", GetGFID());
}

/*static*/void UtlVfTest::FreeVfTests()
{
    s_UtlVfTests.clear();
}

/*static*/ UINT32 UtlVfTest::GetVfCount()
{
    return s_UtlVfTests.size();
}

/*static*/ UtlVfTest* UtlVfTest::GetFromSeqId(UINT32 seqId)
{
    for (const auto & utlVfTest : s_UtlVfTests)
    {
        if (utlVfTest->GetSeqId() == seqId)
        {
            return utlVfTest.get();
        }
    }

    return nullptr;
}

/*static*/ UtlVfTest* UtlVfTest::GetFromGfid(UINT32 gfid)
{
    for (const auto & utlVfTest : s_UtlVfTests)
    {
        if (utlVfTest->GetGFID() == gfid)
        {
            return utlVfTest.get();
        }
    }

    return nullptr;
}

/**************************************************/
/*  UtlLwrrentFunction
 *
 **************************************************/
UtlVfTest * UtlPFLwrrentFunction::GetVfTest(py::kwargs kwargs)
{
    return UtlVfTest::GetUtlVfTest(kwargs);
}

UINT32 UtlPFLwrrentFunction::GetVfCount()
{
    return UtlVfTest::GetVfCount();
}

void UtlPFLwrrentFunction::VirtualFunctionReset(py::kwargs kwargs)
{
    UtlVfTest * pUtlVfTest = UtlUtility::GetRequiredKwarg<UtlVfTest *>(kwargs, "vfTest",
            "PfLwrrentFunction.VirtualFunctionReset");

    UtlGil::Release gil;

    if (pUtlVfTest == nullptr)
    {
        UtlUtility::PyError("Can't get the vftest correctly.");    
    }

    UINT32 GFID = pUtlVfTest->GetGFID();
    VmiopMdiagElwManager * pVmiopMgr = VmiopMdiagElwManager::Instance();
    VmiopMdiagElw * pVmiopMdiagElw = pVmiopMgr->GetVmiopMdiagElw(GFID);

    RC rc = MDiagUtils::VirtFunctionReset(
            pVmiopMdiagElw->GetPcieBus(),
            pVmiopMdiagElw->GetPcieDomain(),
            pVmiopMdiagElw->GetPcieFunction(),
            pVmiopMdiagElw->GetPcieDevice()
            );
}

void UtlPFLwrrentFunction::Rulwf(py::kwargs kwargs)
{
    UtlVfTest * pUtlVfTest  = UtlUtility::GetRequiredKwarg<UtlVfTest *>(kwargs, "vfTest",
            "PfLwrrentFunction.Rulwf");

    UtlGil::Release gil;

    if (pUtlVfTest)
        pUtlVfTest->Run();
    else
        UtlUtility::PyError("Can't find the vfTest in the yml file. "
                "Please specify corrected vfTest argument.");
}

void UtlPFLwrrentFunction::ClearFLRPendingBit(py::kwargs kwargs)
{
    vector<UINT32> data;

    MDiagUtils::ReadFLRPendingBits(&data);

    UtlVfTest * pUtlVfTest  = UtlUtility::GetRequiredKwarg<UtlVfTest *>(kwargs, "vfTest",
            "PfLwrrentFunction.ClearFLRPendingBit");

    if (pUtlVfTest == nullptr)
        UtlUtility::PyError("Can't get the vftest correctly.");

    UtlGil::Release gil;

    UINT32 GFID = pUtlVfTest->GetGFID();
    MDiagUtils::UpdateFLRPendingBits(GFID, &data);

    MDiagUtils::ClearFLRPendingBits(data);
}

void UtlPFLwrrentFunction::SendProcEvent(py::kwargs kwargs)
{
    RC rc;

    UtlVfTest * pUtlVfTest  = UtlUtility::GetRequiredKwarg<UtlVfTest *>(kwargs, "vfTest",
            "PfLwrrentFunction.SendProcEvent");
    string message = UtlUtility::GetRequiredKwarg<string>(kwargs, "message",
            "PfLwrrentFunction.SendProcEvent");

    UtlGil::Release gil;

    UINT32 GFID = pUtlVfTest->GetGFID();
    VmiopMdiagElwManager * pVmiopMgr = VmiopMdiagElwManager::Instance();
    VmiopMdiagElw * pVmiopMdiagElw = pVmiopMgr->GetVmiopMdiagElw(GFID);

    if (pVmiopMdiagElw == nullptr)
    {
        ErrPrintf("Can't find this VmiopMdiagElw whose GFID = %d\n", GFID);
        MASSERT(0);
    }

    rc = pVmiopMdiagElw->SendProcMessage(message.size(), message.c_str());

    UtlUtility::PyErrorRC(rc, "Can't send the message %s to VF whose GFID = %d correctly.",
        message.c_str(), GFID);
}

void UtlPFLwrrentFunction::WaitProcEvent(py::kwargs kwargs)
{
    RC rc;

    UtlVfTest * pUtlVfTest  = UtlUtility::GetRequiredKwarg<UtlVfTest *>(kwargs, "vfTest",
            "PfLwrrentFunction.WaitProcEvent");
    string message = UtlUtility::GetRequiredKwarg<string>(kwargs, "message",
            "PfLwrrentFunction.WaitProcEvent");

    UtlGil::Release gil;

    if (pUtlVfTest == nullptr)
        UtlUtility::PyError("Can't get the vftest correctly.");

    UINT32 GFID = pUtlVfTest->GetGFID();
    set<UINT32> gfidsToPoll {GFID};
    rc = MDiagUtils::PollSharedMemoryMessage(gfidsToPoll, message);

    UtlUtility::PyErrorRC(rc, "Can't wait the message %s from VF whose GFID = %d correctly.",
        message.c_str(), GFID);
}

void UtlPFLwrrentFunction::WaitFlrDone(py::kwargs kwargs)
{
    RC rc;

    UtlVfTest * pUtlVfTest  = UtlUtility::GetRequiredKwarg<UtlVfTest *>(kwargs, "vfTest",
            "PfLwrrentFunction.WaitFlrDone");

    UtlGil::Release gil;

    if (pUtlVfTest == nullptr)
        UtlUtility::PyError("Can't get the vftest correctly.");

    UINT32 GFID = pUtlVfTest->GetGFID();
    rc = MDiagUtils::WaitFlrDone(GFID);

    UtlUtility::PyErrorRC(rc, "Can't wait flr done from VF whose GFID = %d correctly.", GFID);
}

void UtlPFLwrrentFunction::WaitVfDone(py::kwargs kwargs)
{
    RC rc;

    UtlVfTest * pUtlVfTest  = UtlUtility::GetRequiredKwarg<UtlVfTest *>(kwargs, "vfTest",
            "PfLwrrentFunction.WaitVfDone");

    UtlGil::Release gil;

    if (pUtlVfTest == nullptr)
        UtlUtility::PyError("Can't get the vftest correctly.");

    UINT32 GFID = pUtlVfTest->GetGFID();
    VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
    rc = pVmiopMdiagElwMgr->WaitPluginThreadDone(GFID);

    UtlUtility::PyErrorRC(rc, "Can't wait VF done whose GFID = %d correctly.",
        GFID);
}

void UtlPFLwrrentFunction::SendVmmuInfo()
{
    RC rc;

    UtlGil::Release gil;

    VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
    rc = pVmiopMdiagElwMgr->SendVmmuInfo();

    UtlUtility::PyErrorRC(rc, "Can't config Ats Vmmu info correctly in CPU Model");
}

string UtlPFLwrrentFunction::ReadProcMessage(py::kwargs kwargs)
{
    string msg = "";

    UtlVfTest * pUtlVfTest  = UtlUtility::GetRequiredKwarg<UtlVfTest *>(kwargs, "vfTest",
            "PfLwrrentFunction.ReadProcMessage");
    UtlGil::Release gil;

    if (pUtlVfTest == nullptr)
        UtlUtility::PyError("Can't get the vftest correctly.");

    UINT32 GFID = pUtlVfTest->GetGFID();
    VmiopMdiagElwManager * pVmiopMgr = VmiopMdiagElwManager::Instance();
    VmiopMdiagElw * pVmiopMdiagElw = pVmiopMgr->GetVmiopMdiagElw(GFID);

    if (pVmiopMdiagElw == nullptr)
    {
        ErrPrintf("Can't find this VmiopMdiagElw whose GFID = %d\n", GFID);
        MASSERT(0);
    } 

    RC rc = pVmiopMdiagElw->ReadProcMessage(msg);
    UtlUtility::PyErrorRC(rc, "PF fails to read message.");

    return msg;
}

void UtlVFLwrrentFunction::SendProcEvent(py::kwargs kwargs)
{
    RC rc;

    string message = UtlUtility::GetRequiredKwarg<string>(kwargs, "message",
            "VfLwrrentFunction.SendProcEvent");

    UtlGil::Release gil;

    VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
    UINT32 GFID = pVmiopMdiagElwMgr->GetGfidAttachedToProcess();
    VmiopMdiagElw * pVmiopMdiagElw = pVmiopMdiagElwMgr->GetVmiopMdiagElw(GFID);

    if (pVmiopMdiagElw == nullptr)
    {
        ErrPrintf("Can't find this VmiopMdiagElw whose GFID = %d\n", GFID);
        MASSERT(0);
    }

    rc = pVmiopMdiagElw->SendProcMessage(message.size(), message.c_str());

    UtlUtility::PyErrorRC(rc, "Can't send the message %s to PF correctly.",
        message.c_str());
}

void UtlVFLwrrentFunction::WaitProcEvent(py::kwargs kwargs)
{
    RC rc;

    VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
    UINT32 GFID = pVmiopMdiagElwMgr->GetGfidAttachedToProcess();
    string message = UtlUtility::GetRequiredKwarg<string>(kwargs, "message",
            "VfLwrrentFunction.WaitProcEvent");
    
    UtlGil::Release gil;

    set<UINT32> gfidsToPoll {GFID};
    rc = MDiagUtils::PollSharedMemoryMessage(gfidsToPoll, message);
 
    UtlUtility::PyErrorRC(rc, "Can't wait the message %s from PF correctly.",
        message.c_str());
}

void UtlVFLwrrentFunction::ExitLwrrentProcess()
{
    RC rc;

    // The below functions may yield to another thread.
    // So need release gil first here.
    UtlGil::Release gil;

    Platform::SetForcedTermination();

    rc = MDiagUtils::TagTrepFilePass();
    UtlUtility::PyErrorRC(rc, "Can't exit Current VF process.");

    Utility::ExitMods(rc, Utility::ExitQuickAndDirty);
    UtlUtility::PyErrorRC(rc, "Can't exit Current VF process.");
}

UINT32 UtlVFLwrrentFunction::GetGfid()
{
    VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
    return pVmiopMdiagElwMgr->GetGfidAttachedToProcess();
}

string UtlVFLwrrentFunction::ReadProcMessage()
{
    string msg = "";

    VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
    UINT32 GFID = pVmiopMdiagElwMgr->GetGfidAttachedToProcess();
    VmiopMdiagElw * pVmiopMdiagElw = pVmiopMdiagElwMgr->GetVmiopMdiagElw(GFID);

    if (pVmiopMdiagElw == nullptr)
    {
        ErrPrintf("Can't find this VmiopMdiagElw whose GFID = %d\n", GFID);
        MASSERT(0);
    }

    RC rc = pVmiopMdiagElw->ReadProcMessage(msg);
    UtlUtility::PyErrorRC(rc, "VF fails to read message.");

    return msg;
}
