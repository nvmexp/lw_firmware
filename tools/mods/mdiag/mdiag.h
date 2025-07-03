/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_MDIAG_H
#define INCLUDED_MDIAG_H

class Trep;
class ArgReader;

namespace Mdiag
{
    void PostRunSemaphore();
    bool IsSmmuEnabled();
    const ArgReader* GetParam();
}

#endif
