/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef CC_MISC_H
#define CC_MISC_H

void ccPanic(void) __attribute__((noreturn));

// The traditional LW_ALIGN_UP will cause compiler error
#ifndef LW32_ALIGN_UP
#define LW32_ALIGN_UP(v, gran)  ((((LwU32)v) + (((LwU32)gran) - 1)) & ~((LwU32)(gran)-1))
#endif

#endif // CC_MISC_H

