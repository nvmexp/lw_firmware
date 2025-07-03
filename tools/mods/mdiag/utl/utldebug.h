/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLDEBUG_H
#define INCLUDED_UTLDEBUG_H

#include "mdiag/utl/utlpython.h"

namespace UtlDebug
{
    void Register(py::module module);

    void Init();

    void Terminate(py::kwargs kwargs);
}

#endif
