/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLMODULES_H
#define INCLUDED_UTLMODULES_H

#include "mdiag/tests/test.h"
#include "mdiag/smc/smcresourcecontroller.h"
#include "utlkwargsmgr.h"

#include "mdiag/utl/utlpython.h"

// This macro is the entry point for exporting all of the
// C++ classes and functions that can be used by a UTL Python script.
// Each new UTL class should implement a static Register(py::module) function
// to export any desired functions/variables.
//
// See http://pybind11.readthedocs.io/en/stable/index.html for dolwmention
// on the pybind11 APIs.
//
PYBIND11_EMBEDDED_MODULE(utl, module)
{
    UtlUtility::Register(module);
    BaseUtlEvent::Register(module);
    UtlTest::Register(module);
    UtlEngine::Register(module);
    UtlTsg::Register(module);
    UtlChannel::Register(module);
    UtlSurface::Register(module);
    UtlVidmemAccessBitBuffer::Register(module);
    UtlPerfmonBuffer::Register(module);
    UtlRegCtrl::Register(module);
    UtlGpuRegCtrl::Register(module);
    UtlGpu::Register(module);
    UtlThread::Register(module);
    UtlGpuPartition::Register(module);
    UtlSmcEngine::Register(module);
    UtlVfTest::Register(module);
    UtlLwrrentSriovFunction::Register(module);
    UtlPFLwrrentFunction::Register(module);
    UtlVFLwrrentFunction::Register(module);
    UtlUserTestPy::Register(module);
    UtlMutex::Register(module);
    UtlMmu::Register(module);
    UtlVaSpace::Register(module);
    UtlMemCtrl::Register(module);
    UtlFbInfo::Register(module);
    UtlRawMemory::Register(module);
    UtlPmaChannel::Register(module);
    UtlSelwreProfiler::Register(module);
    UtlInterrupt::Register(module);
    UtlSubCtx::Register(module);
    UtlDpc::Register(module);
    UtlVectorEvent::Register(module);

#if defined(INCLUDE_LWSWITCH)
    UtlLwSwitch::Register(module);
    UtlLwSwitchRegCtrl::Register(module);
#endif

    UtlDebug::Register(module);

    // Add decorator to all callables in the utl embedded module
    py::module::import("sys").attr("path");
    string libDir = LwDiagUtils::DefaultFindFile("utllib.py", true);
    libDir = LwDiagUtils::StripFilename(libDir.c_str());
    py::exec(Utility::StrPrintf("sys.path.append('%s')", libDir.c_str()));
    // Temporarily disable kwargs checking to resolve TOT test breaks before bringup
    // module = py::module::import("utllib").attr("utl_module_decorator")(module);
    py::exec("sys.path.pop()");
}

PYBIND11_EMBEDDED_MODULE(utl_global, module)
{
    module.def("UtlCheckUserKwargs", &UtlKwargsMgr::CheckUserKwargs);
}

#endif
