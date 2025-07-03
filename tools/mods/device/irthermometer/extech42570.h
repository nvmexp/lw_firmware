/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2014 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/rc.h"
#include <vector>

class Serial;

class Extech42570
{
public:
    ~Extech42570();

    RC FindInstruments(UINT32 *numInstruments);

    RC MeasureTemperature(UINT32 instrumentIdx, double *tempDegC);

private:
    vector<Serial*> m_Instruments;
};
