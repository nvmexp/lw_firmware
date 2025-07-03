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

#include "gpu/repair/hbm/gpu_interface/volta_hbm_interface.h"

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief The Ampere interface to HBM using the FB PRIV path (IEEE1500
    //! bridge).
    //!
    class AmpereHbmInterface : public VoltaHbmInterface
    {
    public:
        AmpereHbmInterface(GpuSubdevice *pSubdev) : VoltaHbmInterface(pSubdev) {}

        RC ClearPrivError(RegHal* pRegs) const override;

    private:
        //!
        //! \brief Wait for the PRIV ring to complete its current command, if
        //! it has one.
        //!
        //! \param maxPollAttempts Maximum number of times to poll. 1us delay
        //! between each poll attempt.
        //!
        RC PollPrivRingCmdIdle
        (
            RegHal* pRegs,
            UINT32 maxPollAttempts
        ) const;
    };
} // namespace Hbm
} // namespace Memory
