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

#include "ampere_hbm_interface.h"

using namespace Memory;

RC Hbm::AmpereHbmInterface::ClearPrivError(RegHal* pRegs) const
{
    MASSERT(pRegs);

    // NOTE: To clear the PRIV error, RING_COMMAND_ACK_INTERRUPT must be sent
    // to acknowledge the error has been recieved.

    RC rc;
    constexpr UINT32 maxPollAttempts = 10;

    MASSERT(pRegs->IsSupported(MODS_PPRIV_SYS_PRIV_ERROR_CODE_VALUE_I));
    if (pRegs->Test32(MODS_PPRIV_SYS_PRIV_ERROR_CODE_VALUE_I))
    {
        // No error to clear
        return rc;
    }

    // Wait for any previous command
    CHECK_RC(PollPrivRingCmdIdle(pRegs, maxPollAttempts));

    // Clear PRIV error by ACKing it
    pRegs->Write32(MODS_PPRIV_MASTER_RING_COMMAND_CMD_ACK_INTERRUPT);

    // Wait for command completion
    CHECK_RC(PollPrivRingCmdIdle(pRegs, maxPollAttempts));

    if (!pRegs->Test32(MODS_PPRIV_SYS_PRIV_ERROR_CODE_VALUE_I))
    {
        Printf(Tee::PriError, "PRIV error was not cleared!\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC Hbm::AmpereHbmInterface::PollPrivRingCmdIdle
(
    RegHal* pRegs,
    UINT32 maxPollAttempts
) const
{
    MASSERT(pRegs);

    UINT32 pollAttempt = 0;
    MASSERT(pRegs->IsSupported(MODS_PPRIV_MASTER_RING_COMMAND_CMD_NO_CMD));
    while (!pRegs->Test32(MODS_PPRIV_MASTER_RING_COMMAND_CMD_NO_CMD))
    {
        pollAttempt++;
        if (pollAttempt >= maxPollAttempts)
        {
            Printf(Tee::PriError, "Timeout while polling for PRIV ring ACK "
                   "command completion\n");
            return RC::TIMEOUT_ERROR;
        }

        SleepUs(1);
    }

    return RC::OK;
}
