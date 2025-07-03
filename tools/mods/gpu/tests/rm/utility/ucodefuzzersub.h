/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UCODE_FUZZER_H
#define INCLUDED_UCODE_FUZZER_H

#ifndef LWTYPES_INCLUDED
#include "lwtypes.h"
#endif

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#include "ctrl/ctrl2080/ctrl2080ucodefuzzer.h"

//! \brief Provides an interface for communicating with the UCODE_FUZZER
//!        category of cl2080
//!
class UcodeFuzzerSub
{
public:
    UcodeFuzzerSub(GpuSubdevice *pSubdevice);

    //! Submits a synchronous command/payload to an RTOS ucode
    //!
    //! Wraps the LW2080_CTRL_CMD_UCODE_FUZZER_RTOS_CMD control call
    //! (see ctrl2080ucodefuzzer.h for details)
    //!
    //! \param ucode numeric id of the ucode
    //! \param timeoutMs timeout for the command in milliseconds
    //! \param cmd structured command data
    //! \param payload structured payload data
    //! \return RC::OK if successful
    //!         RC::LWRM_TIMEOUT if timeoutMs is exceeded
    //!
    RC RtosCommand(LwU32 ucode,
                   LwU32 timeoutMs,
                   const std::vector<LwU8>& cmd,
                   const std::vector<LwU8>& payload);

    //! Wraps LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_GET_CONTROL
    //! (see ctrl2080ucodefuzzer.h for details)
    //!
    //! \param ucode numberic id of ucode
    //! \param usedOut [OUT] "used" in control control's params
    //! \param missedOut [OUT] "missed" in control control's params
    //! \param bEnabledOut [OUT] "bEnabled" in control control's params
    //!
    RC SanitizerCovGetControl(LwU32 ucode,
                              LwU32& usedOut,
                              LwU32& missedOut,
                              LwBool& bEnabledOut);

    //! Wraps LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_SET_CONTROL
    //! (see ctrl2080ucodefuzzer.h for details)
    //!
    //! \param ucode numberic id of ucode
    //! \param used "used" in control control's params
    //! \param missed "missed" in control control's params
    //! \param bEnabled "bEnabled" in control control's params
    //!
    RC SanitizerCovSetControl(LwU32 ucode,
                              LwU32 used,
                              LwU32 missed,
                              LwBool bEnabled);

    //! Wraps LW2080_CTRL_CMD_UCODE_FUZZER_SANITIZER_COV_DATA_GET
    //! (see ctrl2080ucodefuzzer.h for details)
    //!
    //! \param ucode numeric id of ucode
    //! \param data vector to retrieve all available data into
    //!
    RC SanitizerCovDataGet(LwU32 ucode,
                           std::vector<LwU64>& data);

private:
    GpuSubdevice *m_pSubdevice;
};

#endif // INCLUDED_UCODE_FUZZER_H
