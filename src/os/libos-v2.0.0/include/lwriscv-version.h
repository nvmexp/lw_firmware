/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_VERSION_H_
#define LWRISCV_VERSION_H_

#define _MK_LWRISCV_VERSION(maj, min) ((maj << 16) | (min))

#define LWRISCV_1_0   _MK_LWRISCV_VERSION(1, 0)
#define LWRISCV_1_1   _MK_LWRISCV_VERSION(1, 1)
#define LWRISCV_2_0   _MK_LWRISCV_VERSION(2, 0)
#define LWRISCV_2_2   _MK_LWRISCV_VERSION(2, 2)

#if defined(LIBOS_EXOKERNEL_TU10X)
#   define LWRISCV_VERSION LWRISCV_1_0
#elif defined(LIBOS_EXOKERNEL_GA100)
#   define LWRISCV_VERSION LWRISCV_1_1
#elif defined(LIBOS_EXOKERNEL_GA10X)
#   define LWRISCV_VERSION LWRISCV_2_0
#elif defined(LIBOS_EXOKERNEL_GH100)
#   define LWRISCV_VERSION LWRISCV_2_2
#endif

#endif
