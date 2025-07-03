/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
  Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "gpu/include/gpusbdev.h"
#include "gpu/floorsweep/fslib_interface.h" // for FsLib
#include "gpu/fuse/fusetypes.h" // for FuseOverride
#include "fsmask.h"

namespace Downbin
{
    // Floorsweeping units
    enum class FsUnit : UINT08
    {
        CPC,
        GPC,
        FBIO,
        FBP,
        FBPA,
        HALF_FBPA,
        IOCTRL,
        LTC,
        L2_SLICE,
        LWDEC,
        LWENC,
        OFA,
        PERLINK,
        PES,
        ROP,
        SYSPIPE,
        TPC,
        LWJPG,
        NONE
    };

    enum class DownbinFlag : UINT32
    {
        SKIP_FS_IMPORT = 1,
        SKIP_GPU_IMPORT = 1 << 1,
        SKIP_DEFECTIVE_FUSE = 1 << 2,
        SKIP_DOWN_BIN = 1 << 3,
        IGNORE_SKU_FS = 1 << 4,
        USE_BOARD_FS = 1 << 5
    };

    struct DownbinInfo
    {
        DownbinFlag downbinFlags;
        FsLib::FsMode fsMode;
        map<string, FuseOverride> fuseOverrideList;
    };

    DownbinFlag operator&(DownbinFlag lhs, DownbinFlag rhs);
    //!
    //! \brief Check if the given flags are set in the given DownbinFlags
    //! value.
    //!
    //! \param value DownbinFlags value.
    //! \param flags flags to check for membership.
    //!
    //! \return True if the \a flags are set in \a value, false otherwise.
    //!
    bool IsDownbinFlagSet(DownbinFlag value, DownbinFlag flags);

    string ToString(FsUnit fsUnit);

    // Type of fuse masks for the floorsweeping units
    enum class FuseMaskType : UINT08
    {
        DISABLE,
        DEFECTIVE,
        RECONFIG,
        DISABLE_CP,
        BOARD
    };

    // Downbin settings
    struct Settings
    {
        bool bModifyDefective = false;
        bool bModifyReconfig  = false;
    };

    // Get the distribution of input group mask among the enabled  UGPUs
    RC GetUGpuGroupEnMasks
    (
        const GpuSubdevice &subdev,
        FsUnit groupFsUnit,
        const FsMask& inputGroupsMask,
        vector<FsMask> *pUGpuGpcEnMasks
    );
}
