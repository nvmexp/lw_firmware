/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DRIVERS_MM_H
#define DRIVERS_MM_H

#include <lwmisc.h>
#include <lwtypes.h>
#include <lwrtos.h>

#include "drivers/mpu.h"
#include "drivers/vm.h"

FLCN_STATUS mmVirtToPhysFb(LwU64 gspPtr, LwU64 *pFbOffset);
FLCN_STATUS mmInitTaskMemory(const char *pName,
                             TaskMpuInfo *pTaskMpu,
                             MpuHandle *pMpuHandles,
                             LwLength mpuEntryCount,
                             LwU64 codeSectionIndex,
                             LwU64 dataSectionIndex);

#endif // DRIVERS_MM_H
