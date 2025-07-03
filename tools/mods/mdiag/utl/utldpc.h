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

#pragma once

#include "gpu/display/dpc/dpc_configurator.h"
#include "mdiag/tests/test.h"
#include "utlpython.h"

class GpuDevice;

// This class represents an interface to a display test specified by the
// -disp_cfg_json command-line argument.  The class acts as a wrapper
// around a DispPipeConfig::DispPipeConfigurator object, but does not own it.
//
class UtlDpc
{
public:
    static void Register(py::module module);
    static void FreeAll();

    static UtlDpc* Create(py::kwargs kwargs);
    void SetDisplaySurfaces(py::kwargs kwargs);
    void ExternalTriggerStart();
    Test::TestStatus ExternalTriggerEnd();

private:
    UtlDpc();
    UtlDpc(UtlDpc&) = delete;
    UtlDpc& operator=(UtlDpc&) = delete;

    unique_ptr<DispPipeConfig::DispPipeConfigurator> m_Dpc;
    bool m_ExternalTriggerStartCalled = false;
    bool m_ExternalTriggerEndCalled = false;

    static map<GpuDevice*, unique_ptr<UtlDpc>> s_Dpcs;
};
