/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl0080gr.cpp
//! \brief LW0080_CTRL_CMD GR tests
//!

#include "gpu/tests/rmtest.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputestc.h"
#include "core/include/refparse.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gralloc.h"

class ProdTest;
//
// Here, As we are changing the value of registers through instance memory so
// it is advisery to choose those register with which the risk of
// malfunctioning is low after updating register data.
//

//
// We are taking two different registers because
// LW_PGRAPH_PRI_SCC_DEBUG register is not present with FERMI
// and vice-versa.
//

class Ctrl0080GrTest: public RmTest
{
public:
    Ctrl0080GrTest();
    virtual ~Ctrl0080GrTest();

    virtual string     IsTestSupported();

    virtual RC       Setup();

    virtual RC       Run();
    virtual RC       Cleanup();
    virtual void     FreeCtxOverrideCh();
    virtual RC       CreateCtxOverrideCh();
private:
    virtual RC       TestSetContextOverride(RefManualRegister const *refManReg);
    virtual RC       ExecClassMethods();

    Notifier         m_notifier;
    FLOAT64          m_TimeoutMs;
    LwRm::Handle     m_hObject2;
    LwRm::Handle     m_hDynMem;
    Channel *        m_pCh;
    LwRm::Handle     m_hCh;
    ThreeDAlloc      m_3dAlloc_First;
    ThreeDAlloc      m_3dAlloc_Second;
};

