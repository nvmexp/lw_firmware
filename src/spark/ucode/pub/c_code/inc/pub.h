/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: pub.h
 */

#ifndef PUB_H
#define PUB_H

#include <falcon-intrinsics.h>
#include "pubutils.h"
#include "rmlsfm.h"
#include "lwuproc.h"
#include "../lwctassert.h"
//
// Function prototypes
// TODO: Need a better way
//

#define OVLINIT_NAME            ".imem_pubinit"
#define OVL_NAME                ".imem_pub"
#define OVL_END_NAME            ".imem_pub_end"

void        hs_wrapper_entry(void)                                                              ATTR_OVLY(OVLINIT_NAME);
void        hsEntrySetSp(void)                                                                  ATTR_OVLY(".imem_pub");

#endif
