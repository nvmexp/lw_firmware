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

#ifndef INCLUDED_MGRMGR_H
#define INCLUDED_MGRMGR_H

#ifndef INCLUDED_GPUMGR_H
#include "devmgr.h"
#endif

class GpuDevMgr;
class TestDeviceMgr;

/*
 * This it the device manager manager.
 */
namespace DevMgrMgr
{
    extern GpuDevMgr *d_GraphDevMgr;
    extern DevMgr *d_PexDevMgr;
    extern DevMgr *d_HBMDevMgr;
    extern TestDeviceMgr *d_TestDeviceMgr;
}

#endif // INCLUDED_DEVMGR_H
