/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef DRIVERS_VM_H
#define DRIVERS_VM_H

#include <lwmisc.h>
#include <lwtypes.h>
#include <lwrtos.h>
// VMA management
FLCN_STATUS vmInit(LwUPtr base, LwU64 pageCount);
LwUPtr vmAllocateVaSpace(LwU64 pageCount);
void vmFreeSpace(LwUPtr virt, LwU64 pageCount);

#endif // DRIVERS_VM_H
