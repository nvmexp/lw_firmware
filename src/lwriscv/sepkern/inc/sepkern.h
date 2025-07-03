/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef __SEPKERN_H_
#define __SEPKERN_H_

#include "reg_access.h"
#include "config.h"

//! @brief Irreversibly halt the core.
__attribute__((__noreturn__)) void halt(void);
//! @brief Properly shutdown core.
__attribute__((__noreturn__)) void shutdown(void);

//! @brief Per-engine lwstomizations
void engineConfigApply(void);

#endif // __SEPKERN_H_
