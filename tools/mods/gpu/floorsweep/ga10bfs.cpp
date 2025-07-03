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

#include "ga10bfs.h"

//------------------------------------------------------------------------------
RC GA10BFs::ReadFloorsweepingArgumentsImpl(ArgDatabase& fsArgDb)
{
    RC rc;

    // Read pre-GA10B floorsweeping arguments
    CHECK_RC(GA10xFs::ReadFloorsweepingArgumentsImpl(fsArgDb));
    for (UINT32 i = 0; i < m_FsFbpL2SliceParamPresent.size(); i++)
    {
        if (m_FsFbpL2SliceParamPresent[i])
        {
            Printf(Tee::PriError, "L2 slice floorsweeping is not supported on this device\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return rc;
}

UINT32 GA10BFs::PceMask() const
{
    const UINT32 pceCount = m_pSub->Regs().LookupAddress(MODS_CE_PCE2LCE_CONFIG__SIZE_1);
    return (1U << pceCount) - 1;
}

//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(GA10BFs);
