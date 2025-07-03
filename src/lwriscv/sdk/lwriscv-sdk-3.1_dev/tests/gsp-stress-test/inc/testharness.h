/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef __riscvdemo__TESTHARNESS_H
#define __riscvdemo__TESTHARNESS_H

#include <lwtypes.h>
#include <liblwriscv/io.h>
#include <lwriscv/peregrine.h>

#include <lwmisc.h>

#define PET_WATCHDOG(x) localWrite(LW_PRGNLCL_FALCON_WDTMRVAL, (x))

#define CORE_HALT do {\
	__asm__ volatile ("csrrwi zero, %0, %1" : :\
					  "i"(LW_RISCV_CSR_MOPT),\
					  "i"( DRF_DEF64(_RISCV, _CSR_MOPT, _CMD, _HALT)));\
} while (0);

#define SIZE_1K  0x400
#define SIZE_4K  0x1000
#define SIZE_8K  0x2000
#define SIZE_16K  0x4000
#define SIZE_32K  0x8000
#define SIZE_64K 0x10000

#define MEMRW_GADGET_CTRL LW_PRGNLCL_FALCON_MAILBOX0
#define MEMRW_GADGET_STATUS LW_PRGNLCL_FALCON_MAILBOX1

#endif
