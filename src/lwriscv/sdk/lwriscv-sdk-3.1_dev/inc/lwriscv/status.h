/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LWRISCV_STATUS_H
#define LWRISCV_STATUS_H

#if LWRISCV_CONFIG_USE_STATUS_lwrv
#include <lwriscv/status-rv.h>
#elif LWRISCV_CONFIG_USE_STATUS_fsp
#include <lwriscv/status-fsp.h>
#elif LWRISCV_CONFIG_USE_STATUS_lwstatus
#include <lwriscv/status-lw.h>
#else
#error "Set LWRISCV_CONFIG_USE_STATUS_(lwrv|fsp|lw) to pick return code types."
#endif

#endif // LWRISCV_STATUS_H
