/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: hkd.h
 */

#ifndef HKD_H
#define HKD_H

#include <falcon-intrinsics.h>
#include <lwtypes.h>
#include "regmacros.h"
#include "hkdretval.h"
#include "hkdutils.h"

// Common defines
#define FLCN_IMEM_BLK_SIZE_IN_BYTES       (256)
#define FLCN_IMEM_BLK_ALIGN_BITS          (8) 
#define HKD_SCP_KEY_INDEX_LC128           (23) 

//
// Function prototypes
// TODO: Need a better way
//
#define OVLINIT_NAME            ".ovl1init"
#define OVL_NAME                ".ovl1"

int     main (int, char **)                                                         ATTR_OVLY(".text");
void    hkdInitErrorCodeNs(void)                                                    ATTR_OVLY(".text");
void    hkdStart(void)                                                              ATTR_OVLY(OVLINIT_NAME);
void    hkdBar0WaitIdle(void)                                                       ATTR_OVLY(OVL_NAME);
LwU32   hkdReadBar0(LwU32 addr)                                                     ATTR_OVLY(OVL_NAME);
LwU32   hkdWriteBar0(LwU32 addr, LwU32 data)                                        ATTR_OVLY(OVL_NAME);
void    hkdSwapEndianness(void *pOutData, const void *pInData, const LwU32 size)    ATTR_OVLY(OVL_NAME);
void    hkdDecryptLc128Hs(LwU8 *pLc128e, LwU8 *pOutLc128)                           ATTR_OVLY(OVL_NAME);
LwU32   falc_ldxb_i_with_halt(LwU32)                                                ATTR_OVLY(OVL_NAME); 
void    falc_stxb_i_with_halt(LwU32, LwU32)                                         ATTR_OVLY(OVL_NAME);
#ifdef HKD_PROFILE_TU11X
void    hkdEnforceAllowedChipsForTuring(void)                                       ATTR_OVLY(OVL_NAME);
#endif

#endif // HKD_H
