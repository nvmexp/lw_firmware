
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: rng_scp.h
 */
#ifndef RNG_SCP_H
#define RNG_SCP_H

#include "acrtypes.h"

#ifndef UPROC_RISCV
# include <falcon-intrinsics.h>
#endif // UPROC_RISCV

#define SCP_REG_SIZE_IN_DWORDS (LwU32)0x4
#define SCP_ADDR_ALIGNMENT_IN_BYTES (LwU32)0x00000010

#endif
