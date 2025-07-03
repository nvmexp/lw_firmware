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

#include "gp104fs.h"
#include "pascal/gp104/hwproject.h"
#include "pascal/gp104/dev_top.h"

//------------------------------------------------------------------------------
GP104Fs::GP104Fs(GpuSubdevice *pSubdev):
    GP10xFs(pSubdev)
{
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 GP104Fs::HalfFbpaMask() const
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
// | GP100 no | GP102 yes | GP104 yes | GP107 no | GP108 no  |
void GP104Fs::ApplyFloorsweepingSettings()
{
    bool fsCeParamPresent = m_FsCeParamPresent;
    // Don't let the base class update this field. Its field size is too large.
    m_FsCeParamPresent = false;
    GP10xFs::ApplyFloorsweepingSettingsInternal();
    m_FsCeParamPresent = fsCeParamPresent;

    // All floorsweeping registers should have a write with ack on non-hardware
    // platforms. see http://lwbugs/1043132

    //The number of lwenc engines for GP102/GP104 is 2
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

// | GP100 no | GP102 yes | GP104 yes | GP107 yes | GP108 ??? |
void GP104Fs::RestoreFloorsweepingSettings()
{
    if (m_FsHalfFbpaParamPresent)
    {
        m_pSub->Regs().Write32Sync(MODS_FUSE_CTRL_OPT_HALF_FBPA, m_SavedHalfFbpaMask);
    }

    return(GP10xFs::RestoreFloorsweepingSettings());
}

//------------------------------------------------------------------------------
// | GP100 no  | GP102 yes | GP104 yes | GP107 yes | GP108 ??? |
void GP104Fs::SaveFloorsweepingSettings()
{
    m_SavedHalfFbpaMask =  m_pSub->Regs().Read32(MODS_FUSE_CTRL_OPT_HALF_FBPA);
    return(GP10xFs::SaveFloorsweepingSettings());
}

//------------------------------------------------------------------------------
// | GP100 no  | GP102 yes | GP104 yes | GP107 yes | GP108 ??? |
void GP104Fs::ResetFloorsweepingSettings()
{
    UINT32 fbpaEnableMask = m_pSub->Regs().Read32(MODS_FUSE_OPT_HALF_FBPA_ENABLE);
    m_pSub->Regs().Write32(MODS_FUSE_CTRL_OPT_HALF_FBPA, fbpaEnableMask);

    GP10xFs::ResetFloorsweepingSettings();
}

CREATE_GPU_FS_FUNCTION(GP104Fs);

