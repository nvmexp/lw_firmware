/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/* ------------------------ LW Includes ------------------------------------ */
#include <lwtypes.h>
#include <sections.h>
#include <lwrtos.h>

// Kernel-mode tls
sysKERNEL_DATA LwrtosTaskPrivate kernelTls;

sysKERNEL_CODE void tlsInit(void)
{
    kernelTls.pHeapMspace = NULL;
    kernelTls.bIsPrivileged = LW_TRUE;
    kernelTls.bIsKernel = LW_TRUE;
    kernelTls.pvObject = NULL;
    kernelTls.taskHandle = NULL;

    // Set tp register to kernelTls pointer. This register (tp)
    // is never touched by GCC and is meant to keep thread pointer data.
    asm volatile("mv tp, %0" : : "r"(&kernelTls));
}
