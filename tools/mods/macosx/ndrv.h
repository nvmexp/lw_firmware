/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2008,2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------
//------------------------------------------

#ifndef INCLUDED_NDRV_H
#define INCLUDED_NDRV_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

/**
 * @MODS NDRV interface
 *
 * - Interface to OSX NDRV. This uses IO Service interface
 *   to ask NDRV to do stuff..
 *
 * Todo: more univeral interface, in case we need more NDRV calls
 */

RC NdrvInit();
RC NdrvClose();
RC NdrvReleaseCoreChannel(bool release);
RC NdrvGamma(bool disabled);

#endif // !INCLUDED_NDRV_H
