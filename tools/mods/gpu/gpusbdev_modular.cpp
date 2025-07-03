/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/include/gpusbdev.h"
#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "core/include/version.h"

//! \brief Force most gpu tests to fail if run on the wrong mods version.
//!
//! It is important that we use the same mods branch in chip, board, and system
//! testing as we used for the original GPU qualification tests.
//! Folks tend to cheat on this, if we don't enforce the rule.
//!
//! If IsPlatformOK is true, this test is one that is used by the MCP folks
//! to test bus operations for motherboards.  They tend to use any old GPU,
//! so we cut them some slack.
//!
//! This function will be a little bit different in every sw branch.
//!
RC GpuSubdevice::ValidateSoftwareTree()
{
    RC rc = OK;

    // Reasons to allow the test to run, all false initially.
    bool isCorrectGpuForSoftwareBranch = false;
    bool isUncheckedPlatform = false;

    // We still want to regress everything with old GPUs on PVS, so use
    // Xp::IsRemoteConsole() as a check for PVS.
    if (( (Xp::GetOperatingSystem() != Xp::OS_LINUX) &&
          (Xp::GetOperatingSystem() != Xp::OS_MAC) &&
          (Xp::GetOperatingSystem() != Xp::OS_MACRM)) ||
        Xp::IsRemoteConsole() ||
        IsEmOrSim())
    {
        isUncheckedPlatform = true;
    }

    // For each LW device found, make sure it complies with the table at
    // http://swwiki/index.php/MODS/What_version_of_MODS_should_I_use%3F

    // Enable this in release branches
    /*
    if (DeviceId() == Gpu::Gxxxx)
    {
        isCorrectGpuForSoftwareBranch = true;
    }
    */
    bool bIgnoreBranchCheck = false;
    JavaScriptPtr pJs;
    (void)pJs->GetProperty(pJs->GetGlobalObject(), "g_IgnoreBranchCheck", &bIgnoreBranchCheck);

    // When running with only an unexpired bypass file skip the branch check, however
    // when running on the lwpu network it is specifically required to request
    // that the branch check be ignored so that by default internal checking will
    // match external unless explicitly requested.  This can be accomplished either
    // through a command line option or bypass.bin
    const bool bSkipBranchCheck =
        (Utility::GetSelwrityUnlockLevel() == Utility::SUL_BYPASS_UNEXPIRED) ||
        (Utility::GetSelwrityUnlockLevel() == Utility::SUL_LWIDIA_NETWORK_BYPASS) ||
        ((Utility::GetSelwrityUnlockLevel() == Utility::SUL_LWIDIA_NETWORK) &&
         bIgnoreBranchCheck);

    if (g_FieldDiagMode == FieldDiagMode::FDM_629 ||
        bSkipBranchCheck ||
        isUncheckedPlatform ||
        isCorrectGpuForSoftwareBranch)
    {
        rc = OK;
    }
    else
    {
        Printf(Tee::PriHigh, Tee::GetTeeModuleCoreCode(), Tee::SPS_WARNING,
               "MODS chips_a.X is not the correct version for %s.\n",
               Gpu::DeviceIdToString(DeviceId()).c_str());
        Printf(Tee::PriHigh, Tee::GetTeeModuleCoreCode(), Tee::SPS_WARNING,
               "You need to use an older MODS release.\n");
        rc = RC::UNSUPPORTED_DEVICE;
    }

    return rc;
}
