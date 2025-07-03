/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/platform.h" // FOR MEM_WR32 & MEM_RD32
#include "lwmisc.h"
#include "th500c2cdiagdriver.h"

//-----------------------------------------------------------------------------
RC Th500C2cDiagDriver::Init(void *c2cRegBase, LwU32 nrPartitions,
                                    LwU32 nrLinksPerPartition)
{
    RC status;

    status = InitInternal(nrPartitions, nrLinksPerPartition);
    if(status != RC::OK)
    {
        return status;
    }

    status = InitDevicePointer(c2cRegBase);
    if(status != RC::OK)
    {
        return status;
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
/* virtual */ RC Th500C2cDiagDriver::ReadC2CRegister(LwU32 regIdx,
                                            LwU32 *regValue)
{
    if (m_c2cRegBase != 0)
    {
        // TODO: Check if base needs to be subtracted for TH500
        *regValue = MEM_RD32(m_c2cRegBase + (regIdx  - DEVICE_BASE(LW_PC2C)));
        return RC::OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
/* virtual */ RC Th500C2cDiagDriver::WriteC2CRegister(LwU32 regIdx,
                                            LwU32 regValue)
{
    if (m_c2cRegBase != 0)
    {
        // TODO: Check if base needs to be subtracted for TH500
        MEM_WR32(m_c2cRegBase + (regIdx  - DEVICE_BASE(LW_PC2C)), regValue);
        return RC::OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

//-----------------------------------------------------------------------------
RC Th500C2cDiagDriver::InitDevicePointer(void *c2cRegBase)
{
    MASSERT(c2cRegBase != NULL);
    m_c2cRegBase = (LwU64) c2cRegBase;
    return RC::OK;
}
