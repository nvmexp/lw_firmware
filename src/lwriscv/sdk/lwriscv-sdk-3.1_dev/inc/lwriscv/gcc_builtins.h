/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_GCC_BUILTINS_H
#define LWRISCV_GCC_BUILTINS_H

#define LWRV_BUILTIN_OFFSETOF(type, member) __builtin_offsetof(type, member)
#define LWRV_BUILTIN_UNREACHABLE            __builtin_unreachable

#endif // LWRISCV_GCC_BUILTINS_H
