/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: gsphs.h
 */

#ifndef GSPHS_H
#define GSPHS_H

#include <falcon-intrinsics.h>
#include <flcnretval.h>
#include <lwtypes.h>
#include <lwmisc.h>
#include <lwuproc.h>

#include "gsphsif.h"

extern void enterHs(LwU32 cmd, LwU32 arg);

#define csbRd32(addr)                    falc_ldx_i ((LwU32*)(addr), 0)
#define csbWr32(addr, data)        falc_stxb_i((LwU32*)(addr), 0, data)

#endif // GSPHS_H
