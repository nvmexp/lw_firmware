/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/display.h"

#include "utl.h"
#include "utldpc.h"
#include "utlgpu.h"
#include "utlkwargsmgr.h"
#include "utlsurface.h"
#include "utlutil.h"

map<GpuDevice*, unique_ptr<UtlDpc>> UtlDpc::s_Dpcs;

void UtlDpc::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();

    kwargs->RegisterKwarg(
        "Dpc.Create",
        "gpu",
        false,
        "the GPU object the display test will run on; defaults to GPU 0 if not specified");

    kwargs->RegisterKwarg(
        "Dpc.Create",
        "dpcFileName",
        true,
        "a string representing a file path to a DPC config file");

    kwargs->RegisterKwarg(
        "Dpc.Create",
        "externalTriggerMode",
        false,
        "Set to True to control the start and end of the test with Dpc.ExternalTriggerStart and ExternalTriggerEnd (defaults to False)."
        "  Otherwise the display test is run to completion during the Dpc.Create fuction.");

    kwargs->RegisterKwarg(
        "Dpc.Create",
        "ignoreModePossibility",
        false,
        "Set to True to tell RM to ignore whether or not the display mode is possible (defaults to False)."
        "  Equivalent to the -disp_ignore_imp command-line argument.");

    kwargs->RegisterKwarg("Dpc.SetDisplaySurfaces", "lumaSurface", true, "a utl.Surface object that will be used as the luma surface for a display test");
    kwargs->RegisterKwarg("Dpc.SetDisplaySurfaces", "chromaSurface", true, "a utl.Surface object that will be used as the chroma surface for a display test");

    py::class_<UtlDpc> dpc(module, "Dpc",
        "The Dpc class is used to create an object that represents the interface to a DPC display test.");

    dpc.def_static("Create", &UtlDpc::Create,
        UTL_DOCSTRING("Dpc.Create",
            "Creates and initializes a display test based on a DPC configuration file."
            "  The test is also run to completion unless externalTriggerMode is set to True."),
        py::return_value_policy::reference);
    dpc.def("SetDisplaySurfaces", &UtlDpc::SetDisplaySurfaces,
        UTL_DOCSTRING("Dpc.SetDisplaySurfaces",
            "This function assigns surfaces to be used as part of the display test."));
    dpc.def("ExternalTriggerStart", &UtlDpc::ExternalTriggerStart,
        UTL_DOCSTRING("Dpc.ExternalTriggerStart",
            "This function triggers the start of a display test that was created with externalTriggerMode = True."));
    dpc.def("ExternalTriggerEnd",
        &UtlDpc::ExternalTriggerEnd,
        UTL_DOCSTRING("Dpc.ExternalTriggerEnd",
            "This function triggers the end of a display test that was created with externalTriggerMode = True."
            "  Display test CRC checking is also done after the test completes."
            "  Returns utl.Status.Succeeded if the display test passed, otherwise returns utl.Status.FailedCrc."));
}

UtlDpc* UtlDpc::Create(py::kwargs kwargs)
{
    UtlGpu* gpu = nullptr;
    if (!UtlUtility::GetOptionalKwarg<UtlGpu*>(&gpu, kwargs, "gpu",
        "Dpc.Create"))
    {
        // If the user doesn't specify a GPU, use GPU 0 by default.
        gpu = UtlGpu::GetGpus()[0];
    }

    if (s_Dpcs.count(gpu->GetGpudevice()) > 0)
    {
        UtlUtility::PyError("GPU already has DPC object.");
    }

    string dpcFileName = UtlUtility::GetRequiredKwarg<string>(kwargs,
        "dpcFileName", "Dpc.Create");

    bool externalTriggerMode = false;
    UtlUtility::GetOptionalKwarg<bool>(
        &externalTriggerMode,
        kwargs,
        "externalTriggerMode",
        "Dpc.Create");

    bool ignoreImp = false;
    UtlUtility::GetOptionalKwarg<bool>(
        &ignoreImp,
        kwargs,
        "ignoreModePossibility",
        "Dpc.Create");

    UtlGil::Release gil;

    unique_ptr<UtlDpc> utlDpc(new UtlDpc());

    UtlDpc* rawPtr = utlDpc.get();
    s_Dpcs[gpu->GetGpudevice()] = move(utlDpc);

    rawPtr->m_Dpc->SetBoundGpuDevice(gpu->GetGpudevice());
    rawPtr->m_Dpc->SetPerformCrcCountCheck(false);
    rawPtr->m_Dpc->SetWaitForNotifier(true);
    rawPtr->m_Dpc->SetExtTriggerMode(externalTriggerMode);

    Display* display = gpu->GetGpudevice()->GetDisplay();
    display->SetIgnoreIMP(ignoreImp);

    RC rc = rawPtr->m_Dpc->Setup(true, dpcFileName);
    UtlUtility::PyErrorRC(rc, "Dpc.Create failed to load DPC file %s",
        dpcFileName.c_str());

    if (externalTriggerMode)
    {
        rc = rawPtr->m_Dpc->ExtTriggerModeSetup();
        UtlUtility::PyErrorRC(rc, "Dpc.Create failed to setup external trigger mode.");
    }
    else
    {
        rc = rawPtr->m_Dpc->Run();
        UtlUtility::PyErrorRC(rc, "Dpc.Create failed during display test run.");
    }

    return rawPtr;
}

UtlDpc::UtlDpc()
{
    m_Dpc = make_unique<DispPipeConfig::DispPipeConfigurator>();
}

void UtlDpc::FreeAll()
{
    s_Dpcs.clear();
}

void UtlDpc::SetDisplaySurfaces(py::kwargs kwargs)
{
    UtlSurface* lumaSurface = UtlUtility::GetRequiredKwarg<UtlSurface*>(
        kwargs,
        "lumaSurface",
        "Dpc.SetDisplaySurfaces");

    UtlSurface* chromaSurface = UtlUtility::GetRequiredKwarg<UtlSurface*>(
        kwargs,
        "chromaSurface",
        "Dpc.SetDisplaySurfaces");

    if (lumaSurface == nullptr)
    {
        UtlUtility::PyError("lumaSurface argument to Dpc.SetDisplaySurfaces is not a valid Surface object.");
    }
    
    if (chromaSurface == nullptr)
    {
        UtlUtility::PyError("chromaSurface argument to Dpc.SetDisplaySurfaces is not a valid Surface object.");
    }

    m_Dpc->SetLumaSurface(lumaSurface->GetRawPtr()->GetSurf2D());
    m_Dpc->SetChromaSurface(chromaSurface->GetRawPtr()->GetSurf2D());
    m_Dpc->SetExtSurfaceMode(true);
}

void UtlDpc::ExternalTriggerStart()
{
    if (!m_Dpc->GetExtTriggerMode())
    {
        UtlUtility::PyError("Dpc.ExternalTriggerStart can only be used on a Dpc object that was created with externalTriggerMode = True.");
    }
    else if (m_ExternalTriggerStartCalled)
    {
        UtlUtility::PyError("Dpc.ExternalTriggerStart called more than once.");
    }
    m_ExternalTriggerStartCalled = true;

    UtlGil::Release gil;

    RC rc = m_Dpc->ExtTriggerModeStart();
    UtlUtility::PyErrorRC(rc, "Dpc.ExternalTriggerStart failed.");
}

Test::TestStatus UtlDpc::ExternalTriggerEnd()
{
    if (!m_Dpc->GetExtTriggerMode())
    {
        UtlUtility::PyError("Dpc.ExternalTriggerEnd can only be used on a Dpc object that was created with externalTriggerMode = True.");
    }
    else if (!m_ExternalTriggerStartCalled)
    {
        UtlUtility::PyError("Dpc.ExternalTriggerEnd called before Dpc.ExternalTriggerStart.");
    }
    else if (m_ExternalTriggerEndCalled)
    {
        UtlUtility::PyError("Dpc.ExternalTriggerEnd called more than once.");
    }
    m_ExternalTriggerEndCalled = true;

    Test::TestStatus status = Test::TestStatus::TEST_SUCCEEDED;
    UtlGil::Release gil;

    RC rc = m_Dpc->ExtTriggerModeEndTestAndCheckCrcs();
    if (rc != OK)
    {
        status = Test::TestStatus::TEST_FAILED_CRC;
    }

    rc.Clear();
    rc = m_Dpc->Cleanup();
    UtlUtility::PyErrorRC(rc, "Dpc.ExternalTriggerEndAndCrcCheck failed during cleanup.");

    return status;
}
