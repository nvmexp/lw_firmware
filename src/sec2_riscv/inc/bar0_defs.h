/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BAR0_DEFS_H
#define BAR0_DEFS_H

/*!
 * @file   bar0_defs.h
 *
 * This header defines BAR0/PRIV related macro for this engine which can be used
 * by common library code.
 */

#include "lwmisc.h"
#include "dev_sec_pri.h"

/*!
 * Define of the BAR0/PRIV base of this engine so that the common reg accessor
 * functions know where to access the BAR0/PRIV registers of this engine.
 */
#define   ENGINE_PRIV_BASE   DRF_BASE(LW_PSEC)

#endif // BAR0_DEFS_H
