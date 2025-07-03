/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#ifndef INCLUDED_SYSMATS_H
#define INCLUDED_SYSMATS_H

#ifndef INCLUDED_RC_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_TASKER_H
#include "core/include/tasker.h"
#endif

namespace SysMats
{
   extern RC d_Rc;
   RC Run();
}

namespace ThreadTest
{
   RC RunAll();
   RC Benchmark();
}

#endif // !INCLUDED_SYSMATS_H

