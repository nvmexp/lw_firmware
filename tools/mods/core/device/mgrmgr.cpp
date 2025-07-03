/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2014,2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/mgrmgr.h"

namespace DevMgrMgr
{
    GpuDevMgr *d_GraphDevMgr = nullptr;
    DevMgr *d_PexDevMgr = nullptr;
    DevMgr *d_HBMDevMgr = nullptr;
    TestDeviceMgr *d_TestDeviceMgr = nullptr;
};
