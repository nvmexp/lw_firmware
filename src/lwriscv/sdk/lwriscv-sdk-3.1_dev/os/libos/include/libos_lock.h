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

// TODO: Ignoring Coverity error for now since pragma once should ideally
// save us. However, it is not good design to have relwrsive includes and
// we should fix this going forward.
// Note that the relwrsive include path is : libos.h -> libos_lock.h -> libos.h

// coverity[include_relwrsion]
#include "libos.h"

LibosStatus LibosLockAsyncAcquire(LibosLockId lock, LibosShuttleId shuttle);
LibosStatus LibosLockSyncAcquire(LibosLockId lock, LwU64 timeout);
LibosStatus LibosLockRelease(LibosLockId lock);
