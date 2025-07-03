/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  flcntypes.h
 * @brief Master header file defining types that may be used in falcon's
 *        application. This file defines all types not already defined
 *        in lwtypes.h (from the SDK).
 */

#ifndef FLCNTYPES_H
#define FLCNTYPES_H

#include <lwtypes.h>
#include "flcnifcmn.h"
#include "flcnretval.h"
#include "lwrangetypes.h"

typedef union
{
    LwU64       data;
    RM_FLCN_U64 parts;
} FLCN_U64;

// Generic helper macros
#define LW_DELTA(a, b)                              \
    (LW_MAX((a), (b)) - LW_MIN((a), (b)))

#endif // FLCNTYPES_H

