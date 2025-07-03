/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

#include <lwtypes.h>
#include "libos.h"

LibosStatus LibosPartitionCreate(LwU8 partitionId, LibosPartitionHandle *partition);
LibosStatus LibosPartitionMemoryGrantPhysical(LibosPartitionHandle partition, LwU64 base, LwU64 size);
LibosStatus LibosPartitionMemoryAllocate(LibosPartitionHandle partition, LwU64 *base, LwU64 size, LwU64 alignment);
LibosStatus LibosPartitionSpawn(LibosPartitionHandle partition, LwU64 memoryStart, LwU64 memorySize);
