/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "nullfs.h"

//------------------------------------------------------------------------------
/*virtual*/ NullFs::NullFs( GpuSubdevice *pSubdev) :
    FloorsweepImpl(pSubdev)
{
}

//------------------------------------------------------------------------------
// Create a NULL floorsweep object where floorsweeping functionality is not
// supported
FloorsweepImpl *CreateNullFs(GpuSubdevice *pSubdev)
{
    return new NullFs(pSubdev);
}

