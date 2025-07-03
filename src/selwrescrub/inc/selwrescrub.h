/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: selwrescrub.h
 */

#ifndef SELWRESCRUB_H
#define SELWRESCRUB_H

/* ------------------------- System Includes -------------------------------- */
#include <falcon-intrinsics.h>
#include "selwrescrubutils.h"
#include "selwrescrubtypes.h"
#include "selwrescrubovl.h"
#include "lwuproc.h"
#include "rmselwrescrubif.h"
#include "mmu/mmucmn.h"
#ifndef BOOT_FROM_HS_BUILD
#include "selwrescrub_static_inline_functions.h"
#endif // BOOT_FROM_HS_BUILD

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

// WPR address alignment in MMU is 128K

#define NUM_BYTES_IN_256_MB                         (NUM_BYTES_IN_1_MB * 256)
#define NUM_BYTES_IN_768_MB                         (NUM_BYTES_IN_1_MB * 768)
#define MIN_FB_SIZE_FOR_LARGE_SYNC_SCRUB_CBC        (12 * 1024)
#define ILWALID_WPR_ADDR_LO                         (0xFFFFFFFF)
#define ILWALID_WPR_ADDR_HI                         (0x0)
#define ILWALID_MEMLOCK_RANGE_ADDR_LO               (0xFFFFFFFF)
#define ILWALID_MEMLOCK_RANGE_ADDR_HI               (0x0)

//
// defines to CLEAR/SET DISABLE_EXCEPTIONS in SEC SPR
// For more info please check Falcon_6_arch doc (Section 2.3.1)
//
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS          19:19
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_CLEAR    0x00000000
#define LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_SET      0x00000001

//
// Setting bottom limit of stack to 32KB
// That means the stack can grow from 64KB till 32KB
//
#define SELWRESCRUB_STACK_BOTTOM_LIMIT              (0x4000)

// Start Addr of DMEM
#define SELWRESCRUB_DMEM_START_ADDR                 (0x0)

#define SELWRESCRUB_PC_TRIM(pc)                     (pc & 0x0FFFFFFF)
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
// Linker Script Variable to identify start of BSS
extern LwU32 _bss_start[]                        ATTR_OVLY(".data");
extern LwU32 _selwrescrub_resident_code_start[]  ATTR_OVLY(".data");
extern LwU32 _selwrescrub_resident_code_end[]    ATTR_OVLY(".data");
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

#ifdef IS_SSP_ENABLED
void        __stack_chk_fail(void)                                                              ATTR_OVLY(SELWRE_OVL);
#endif //IS_SSP_ENABLED
#endif // SELWRESCRUB_H

