/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef BROM_H
#define BROM_H

#include "engine-io.h"

void bromDumpStatus(Engine *pEngine);

void bromBoot(Engine *pEngine);
void bromConfigurePmbBoot(Engine *pEngine);
LwBool bromWaitForCompletion(Engine *pEngine, LwU32 timeoutMs);
#endif // BROM_H
