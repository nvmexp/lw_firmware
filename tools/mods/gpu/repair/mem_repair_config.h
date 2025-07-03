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

#pragma once

#include "core/include/hbmtypes.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"
#include "gpu/utility/hbmdev.h"
#include "gpu/repair/gddr/gddr_model.h"

namespace Repair
{
    //!
    //! \brief Flags to configure memory repair sequence exelwtion.
    //!
    enum class MemRepairSeqMode : UINT32
    {
        Lane      = 1 << 0, //!< (soft) Lane repair (SLR)
        Row       = 1 << 1, //!< (soft) Row repair (SRR)
        Blacklist = 1 << 2, //!< Blacklisting pages
        Hard      = 1 << 3, //!< Do hard repair
        ForceHard = 1 << 4  //!< Force hard repair despite post-soft repair failures
    };
    MemRepairSeqMode operator&(MemRepairSeqMode lhs, MemRepairSeqMode rhs);
    MemRepairSeqMode& operator&=(MemRepairSeqMode& lhs, MemRepairSeqMode rhs);
    MemRepairSeqMode operator|(MemRepairSeqMode lhs, MemRepairSeqMode rhs);
    MemRepairSeqMode& operator|=(MemRepairSeqMode& lhs, MemRepairSeqMode rhs);

    //!
    //! \brief Check is the given flags and set in the given memory repair sequence
    //! mode.
    //!
    //! \param mode Memory repair sequence mode.
    //! \param flags Flags to check for membership.
    //!
    //! \return True if the \a flags are set in \a mode, false otherwise.
    //!
    bool IsSet(MemRepairSeqMode value, MemRepairSeqMode flags);

    static MemRepairSeqMode s_DefaultMemRepairSeqMode = MemRepairSeqMode::Lane |
                                                        MemRepairSeqMode::Hard;

    //!
    //! \brief Run settings.
    //!
    struct Settings
    {
        bool runningInstInSys;  //!< True if MODS is running with -inst_in_sys

        struct Modifiers
        {
            UINT32 hbmResetDurationMs     = 0;
            bool   forceHtoJ              = false;
            bool   pseudoHardRepair       = false;
            bool   readHbmLanes           = false;
            bool   skipHbmFuseRepairCheck = false;
            bool   ignoreHbmFuseRepairCheckResult = false;
            bool   printRegSeq            = false;
            Repair::MemRepairSeqMode memRepairSeqMode = Repair::s_DefaultMemRepairSeqMode;
        } modifiers;

        string ToString() const;
    };

    //!
    //! \brief GPU initialization configuration.
    //!
    struct InitializationConfig
    {
        //! Initialize MODS GPU tests
        bool initializeGpuTests = false;
        //! HBM device initialization method
        Gpu::HbmDeviceInitMethod hbmDeviceInitMethod = Gpu::HbmDeviceInitMethod::DoNotInitHbm;
        //! True to skip RM init, false o/w
        bool skipRmStateInit = false;

        bool operator==(const InitializationConfig& o) const;
        bool operator!=(const InitializationConfig& o) const;
    };

    using RawEcid = vector<UINT32>;

    //!
    //! \brief State of the system.
    //!
    //! All state relevant to repairs are tracked here. This object is used to
    //! persist data across GPU and MODS resets.
    //!
    struct SystemState
    {
        // Initialization applies to all GPUs enabled the system, and is not
        // specific to the current GPU being repaired.
        struct Initialization
        {
            //! True if the GPU is initialized, false o/w
            bool                 isInitialized = false;
            //! Type of reset that was last performed
            ::Gpu::ResetMode     resetMode     = ::Gpu::ResetMode::HotReset;
            //! Configuration of the GPU during latest initialization
            InitializationConfig config;
        } init; //!< GPU initialization info

        struct Gpu
        {
            GpuSubdevice* pSubdev = nullptr;
            //! Raw ECID of the GPU (needed to locate GPU across resets)
            RawEcid       rawEcid;
        } lwrrentGpu; //!< Data for the GPU lwrrently being repaired

        GpuDevMgr*   pGpuDevMgr    = nullptr;
        UINT32       numGpuSubdevs = 0; //!< Number of GPU subdevices on the system
        set<RawEcid> processedRawEcids; //!< GPU raw ECIDs that have been processed during the run
        map<RawEcid, Memory::Hbm::Model>  hbmModels;
        map<RawEcid, Memory::Gddr::Model> gddrModels;

        //!
        //! \brief Return true if the current GPU is set, false otherwise.
        //!
        bool IsLwrrentGpuSet() const;
    };
} // Repair
