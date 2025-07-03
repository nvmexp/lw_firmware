/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

.section ".text.startup","ax",@progbits
.global startup
startup:

    // mscratch always contains the monitor stack
    la t0, _monitor_stack_top
    csrw mscratch, t0
    mv sp, t0
    j _ada_sk_init

