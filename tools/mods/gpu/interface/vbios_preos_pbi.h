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

#include "core/include/hbmtypes.h"
#include "core/include/platform.h"
#include "core/include/rc.h"
#include "core/include/types.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpusbdev.h"

//!
//! \brief Interface to the VBIOS preOS Post-box Interface (PBI).
//!
//! VBIOS provides a PBI before RM is initialized and after it is destroyed.
//!
//! See "//sw/docs/Notebook/Reference/NBPB/GPU Post-Box Sw Arch Spec.doc" for
//! more information.
//!
namespace VbiosPreOsPbi
{
    //!
    //! \brief Retrieve the device information for each HBM site.
    //!
    //! NOTE: DEVICE_ID value is adjusted to the LSB by the preOS PBI interface.
    //!
    //! \param pSubdev GPU subdevice.
    //! \param[out] pSiteDevIds Collection of site numbers and their DEVICE_ID
    //! result. Only contains information for active HBM sites.
    //!
    RC GetDramInformation
    (
        GpuSubdevice* pSubdev,
        vector<Memory::DramDevId>* pSiteDevIds
    );
};
