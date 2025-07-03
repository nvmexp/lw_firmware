/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "ga10xfs.h"

class ArgReader;

class GA10BFs : public GA10xFs
{
public:
    using GA10xFs::GA10xFs;

protected:
    RC ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb) override;

private:
    UINT32 PceMask() const override;
};
