/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _SMUTIL_H_
#define _SMUTIL_H_

#include "core/include/lwrm.h"
#include "gpu/tests/rmtest.h"

#include "gpu/tests/gputestc.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"
#include "core/include/rc.h"

class SMUtil
{
public:

    SMUtil();
    virtual ~SMUtil();

    RC ReadWarpGlobalMaskValues(GpuSubdevice *pSubdev, LwRm::Handle hCh, UINT32 *warpValue, UINT32 *globalValue, LwBool isCtxsw);
    RC ReadWarpGlobalMaskValuesVolta(GpuSubdevice *pSubdev, LwRm::Handle hCh, UINT32 *warpValue, UINT32 *globalValue, LwBool isCtxsw);
    RC WriteWarpGlobalMaskValues(GpuSubdevice *pSubdev, LwRm::Handle hCh, UINT32 warpValue, UINT32 globalValue, LwBool isCtxsw);
    RC WriteWarpGlobalMaskValuesVolta(GpuSubdevice *pSubdev, LwRm::Handle hCh, UINT32 warpValue, UINT32 globalValue, LwBool isCtxsw);
};

#endif // _SMUTIL_H_
