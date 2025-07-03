/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLFBINFO_H
#define INCLUDED_UTLFBINFO_H

#include "utlpython.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"
#include "core/include/rc.h"

class GpuSubdevice;
class LwRm;
class UtlGpuPartition;

// This class is used to retrieve FB related info
class UtlFbInfo
{
public:
    static void Register(py::module module);
    
    UtlFbInfo(GpuSubdevice* pGpuSubdev, LwRm* pLwRm, UtlGpuPartition* pUtlGpuPartition);

    UINT64 GetPhysicalFbpMask();
    UINT32 GetPhysicalFbpaMask(py::kwargs kwargs);
    UINT32 GetPhysicalLtcMask(py::kwargs kwargs);
    UINT32 GetPhysicalLtsMask(py::kwargs kwargs);

    UtlFbInfo() = delete;
    UtlFbInfo(UtlFbInfo&) = delete;
    UtlFbInfo& operator=(UtlFbInfo&) = delete;

private:
    enum class FbInfoType : UINT32
    {
        StartAddress = LW2080_CTRL_FB_INFO_INDEX_HEAP_START,
        Size = LW2080_CTRL_FB_INFO_INDEX_HEAP_SIZE
    };

    UINT64 GetData(FbInfoType fbInfoType);
    RC GetPhysicalFbMasksRMCtrlCall(LW2080_CTRL_FB_GET_FS_INFO_PARAMS* params);

    GpuSubdevice* m_pGpuSubdev;
    LwRm* m_pLwRm;
    UtlGpuPartition* m_pUtlGpuPartition;
};

#endif
