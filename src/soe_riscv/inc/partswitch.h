/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef PARTSWITCH_H
#define PARTSWITCH_H

#define PARTSWITCH_SHARED_CARVEOUT_VA   0x40000
#define PARTSWITCH_SHARED_CARVEOUT_PA   LW_RISCV_AMAP_DMEM_START + 0x1000
#define PARTSWITCH_SHARED_CARVEOUT_SIZE 0x1000

#endif // PARTSWITCH_H
