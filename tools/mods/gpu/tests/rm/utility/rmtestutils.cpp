/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/types.h"
#include "core/include/tee.h"
#include "core/include/platform.h"
#include "rmtestutils.h"
#include "gpu/tests/rmtest.h"
#include "core/include/memcheck.h"

//! \brief PollFunc:
//!
//! This function is used for poll and timeout.
//! Polling on the release condition,and  timeout
//! in case it isn't released.
//!
//! \return TRUE if the expected value is obtained.
//!
bool PollFunc(void *pArg)
{
    PollArguments *pArgLocal  = (PollArguments *)pArg;
    UINT32 data                 = MEM_RD32(pArgLocal->pAddr);

    if(data == pArgLocal->value)
    {
        Printf(Tee::PriDebug, "Poll exit Success \n");
        return true;
    }
    else
    {
        return false;
    }
}

//! \brief PollVal
//!
//! this function polls for the value passed , prints a data missmatch error if that value is not obtained.
RC PollVal(PollArguments *pArg, FLOAT64 timeoutMs)
{
    RC      rc;

    rc = POLLWRAP(&PollFunc, pArg, timeoutMs);

    UINT32 gotVal = MEM_RD32(pArg->pAddr);

    if (rc)
    {
        Printf(Tee::PriHigh,
              "PollVal: expected value is 0x%x, got value 0x%x\n",
              pArg->value, gotVal);
        return RC::DATA_MISMATCH;
    }
    return rc;
}

