/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef GSPSW_H
#define GSPSW_H

/*!
 * @file gspsw.h
 */
#include "lwuproc.h"
#include "lwrtos.h"
#include "lwoslayer.h"
#include "flcntypes.h"
#include "lwmisc.h"
#include "config/gsp-config.h"

#include <sections.h>

/*!
 * Following macros default to their respective OS_*** equivalents.
 * We keep them forked to allow fast change of GSP implementations.
 */
#define GSP_HALT()                     OS_HALT()

#endif // GSPSW_H

