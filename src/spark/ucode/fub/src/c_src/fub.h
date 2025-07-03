/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
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
#include "fubutils.h"
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

int         main (int, char **)                                   GCC_ATTRIB_NO_STACK_PROTECT() ATTR_OVLY(".text");

void        fub_entry_wrapper(void)                                        ATTR_OVLY(OVLINIT_NAME);
void IV1_handler(void) ATTR_OVLY(OVL_NAME);
void _exceptionHandlerHang(void)ATTR_OVLY(OVL_NAME);
unsigned int getStackChkFailAddr(void) ATTR_OVLY(".imem_pub");
unsigned int get_stack_bottom_from_linkervar(void) ATTR_OVLY(".imem_pub");
void exterrInterruptIV1HandlerHalt(void) ATTR_OVLY(".imem_pub");
void hsEntrySetSp(void) ATTR_OVLY(".imem_pub");
void        __stack_chk_fail(void)                                                              ATTR_OVLY(OVL_NAME);

#endif
