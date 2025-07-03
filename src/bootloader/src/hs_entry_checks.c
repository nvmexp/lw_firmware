/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: hs_entry_checks.c
 */

#include "lwuproc.h"
#include "lwctassert.h"
#include "hsutils.h"
#include "hs_entry_checks.h"

/*!
 * Exception handler which will reside in the secure section. This subroutine will be exelwted whenever exception is triggered.
 */
void hsExceptionHandlerSelwreHang(void)
{
    // coverity[no_escape]
    while(1);
}

