/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef CC_RMPROXY_H
#define CC_RMPROXY_H

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <liblwriscv/mpu.h>
#include "rpc.h"
#endif

#define RMPROXY_CODE_VA_BASE 0x2000000
#define RMPROXY_DATA_VA_BASE 0x3000000

#endif // CC_RMPROXY_H
