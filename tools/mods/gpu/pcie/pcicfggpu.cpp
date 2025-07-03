/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/rc.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/pcie/pcicfggpu.h"
#include "lwmisc.h"

#include "maxwell/gm107/dev_lw_xve.h"

// PciCfgSpace subclass that saves/restores the Azalia-enable bit,
// in addition to the other registers saved/restored by the base
// class.  Needed for fermi+ chips so that resetting the GPU
// doesn't toggle the Azalia-enable setting.
//
RC PciCfgSpaceGpu::Save()
{
    RC rc;
    CHECK_RC(PciCfgSpace::Save());
    if (PciFunctionNumber() == FUNC_GPU &&
        m_pSubdev->HasFeature(GpuSubdevice::GPUSUB_HAS_AZALIA_CONTROLLER))
    {
        CHECK_RC(SaveEntry(
                LW_XVE_PRIV_XV_1,
                DRF_SHIFTMASK(LW_XVE_PRIV_XV_1_CYA_XVE_MULTIFUNC)));
    }
    return rc;
}
