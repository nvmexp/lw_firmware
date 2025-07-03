/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef IO_H
#define IO_H
#include <stdint.h>

#include "engine.h"

uint32_t localRead(uint32_t offset);
void localWrite(uint32_t offset, uint32_t val);

#endif // IO_H
