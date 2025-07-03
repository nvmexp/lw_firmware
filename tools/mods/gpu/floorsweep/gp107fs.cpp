/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gp107fs.h"
#include "pascal/gp107/hwproject.h"
#include "pascal/gp107/dev_top.h"

//------------------------------------------------------------------------------
GP107Fs::GP107Fs(GpuSubdevice *pSubdev):
    GP10xFs(pSubdev)
{
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GP107Fs::HalfFbpaMask() const
{
    return m_pSub->Regs().Read32(MODS_FUSE_STATUS_OPT_HALF_FBPA_DATA);
}

//------------------------------------------------------------------------------
// ApplyFloorsweepingSettings passed via the command line arguments.
// Note:
// Most of the internal variables are maintained in FermiFs however the register
// structure for some of these registers have been simplified. The
// simplification requires utilization of new individual m_SavedFsxxxx vars.
//
// | GP100 no | GP102 no | GP104 no | GP107 yes | GP108 yes |
void GP107Fs::ApplyFloorsweepingSettings()
{
    bool fsCeParamPresent = m_FsCeParamPresent;
    // Don't let the base class update this field. Its field size is too large.
    m_FsCeParamPresent = false;
    GP10xFs::ApplyFloorsweepingSettingsInternal();
    m_FsCeParamPresent = fsCeParamPresent;

    // All floorsweeping registers should have a write with ack on non-hardware
    // platforms. see http://lwbugs/1043132

    //The number of lwenc engines for GP107/GP108 is 1
    if (m_FsLwencParamPresent)
    {
        UINT32 regVal = m_SavedFsLwencControl;
        m_pSub->Regs().SetField(&regVal,
                                MODS_FUSE_CTRL_OPT_LWENC_DATA,
                                m_LwencMask);
        m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_LWENC, regVal);
    }

    if (m_FsHalfFbpaParamPresent)
    {
        m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_HALF_FBPA_DATA, m_HalfFbpaMask);
    }
}

//------------------------------------------------------------------------------
// | GP100 no | GP102 yes | GP104 yes | GP107 yes | GP108 ??? |
void GP107Fs::RestoreFloorsweepingSettings()
{
    if (m_FsHalfFbpaParamPresent)
    {
        m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_HALF_FBPA, m_SavedHalfFbpaMask);
    }

    return(GP10xFs::RestoreFloorsweepingSettings());
}

//------------------------------------------------------------------------------
// | GP100 no  | GP102 yes | GP104 yes | GP107 yes | GP108 ??? |
void GP107Fs::SaveFloorsweepingSettings()
{
    m_SavedHalfFbpaMask =  m_pSub->Regs().Read32(MODS_FUSE_CTRL_OPT_HALF_FBPA);
    return(GP10xFs::SaveFloorsweepingSettings());
}

//------------------------------------------------------------------------------
// | GP100 no  | GP102 yes | GP104 yes | GP107 yes | GP108 ??? |
void GP107Fs::ResetFloorsweepingSettings()
{
    UINT32 fbpaEnableMask = m_pSub->Regs().Read32(MODS_FUSE_OPT_HALF_FBPA_ENABLE);
    m_pSub->Regs().Write32(MODS_FUSE_CTRL_OPT_HALF_FBPA, fbpaEnableMask);

    GP10xFs::ResetFloorsweepingSettings();
}

//------------------------------------------------------------------------------
CREATE_GPU_FS_FUNCTION(GP107Fs);

