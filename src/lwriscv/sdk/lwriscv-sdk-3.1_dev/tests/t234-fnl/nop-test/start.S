/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <dev_riscv_csr_64.h>

.align 6
.section ".text.startup","ax",@progbits
.global startup
startup:
	la    t0, startup
	csrw  mtvec, t0
	csrwi 0x7d8, 0     // mopt
	j    startup
