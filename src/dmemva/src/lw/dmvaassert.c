/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dmvaassert.h"
#include "dmvaregs.h"
#include "dmvautils.h"
#include "dmvacommon.h"

void uAssertImpl(LwBool predicate, LwU32 line)
{
    if (!predicate)
    {
        DMVAREGWR(MAILBOX0, DMVA_ASSERT_FAIL);
        DMVAREGWR(MAILBOX1, line);
        halt();
    }
}
