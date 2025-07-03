/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "core/include/rc.h"

namespace LwLinkErrorCollector
{
    RC Start();
    RC Stop();

    class Pause
    {
        public:
            explicit Pause();
            ~Pause();

            Pause(const Pause&)            = delete;
            Pause& operator=(const Pause&) = delete;
    };
}
