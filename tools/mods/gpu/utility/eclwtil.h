/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "class/cl2080.h"
#include "core/include/rc.h"
#include "core/include/types.h"
#include "ctrl/ctrl2080/ctrl2080ecc.h"
#include "ctrl/ctrlxxxx.h" // LWXXXX_CTRL_CMD_GET_FEATURES required by ctrl2080gpu.h
#include "ctrl/ctrl2080/ctrl2080gpu.h"
#include "ctrl/ctrl208f/ctrl208fgr.h"
#include "ctrl/ctrl208f/ctrl208ffb.h"
#include "ctrl/ctrl208f/ctrl208fmmu.h"
#include "ctrl/ctrl208f/ctrl208fbus.h"

#include <array>
#include <map>
#include <vector>

class GpuSubdevice;

namespace Ecc
{
    //!
    //! \brief LW2080_ECC_UNITS is the reference list for all ECC units.
    //!
    //! CAUTION: Any unit added here will be checked at runtime. Make sure any
    //! unit listed here is supported appropriately. See MODS ECC for a
    //! (non-exhaustive) list of what should be done to support a new ECC unit
    //!   Ref: https://confluence.lwpu.com/x/0w9DC
    //!
    //! Must be sequential.
    //!
    enum class Unit : UINT08
    {
        FIRST = 0,
        LRF = 0,
        CBU,
        L1,
        L1DATA,
        L1TAG,
        SHM,
        TEX,
        L2,
        DRAM,
        SM_ICACHE,
        GCC_L15,
        GPCMMU,
        HUBMMU_L2TLB,
        HUBMMU_HUBTLB,
        HUBMMU_FILLUNIT,
        GPCCS,
        FECS,
        PMU,
        SM_RAMS,
        HSHUB,
        PCIE_REORDER, 
        PCIE_P2PREQ,  
        MAX,
    };


    enum class UnitType : UINT08
    {
        FB = 0,
        L2,
        GPC,
        FALCON,
        HSHUB,
        PCIE,
        MAX
    };

    enum class Source : UINT32
    {
        UNDEFINED = 0,
        DRAM,
        GPU,
        DRAM_AND_GPU
    };

    //!
    //! \brief Correctabled and Uncorrectable ECC error counts.
    //!
    struct ErrCounts
    {
        UINT64 corr;   //!< Correctable error
        UINT64 uncorr; //!< Uncorrectable error

        ErrCounts operator+(const ErrCounts& o) const;
        ErrCounts& operator+=(const ErrCounts& o);
        ErrCounts operator-(const ErrCounts& o) const;
        ErrCounts& operator-=(const ErrCounts& o);
    };

    // Indexed by [gpc][tpc], [fbp][channel], or [fbp][l2slice] depending on unit
    struct DetailedErrCounts
    {
        vector<vector<Ecc::ErrCounts>> eccCounts;

        inline DetailedErrCounts operator+(const DetailedErrCounts& other) const
        {
            DetailedErrCounts diff = *this;
            diff += other;
            return diff;
        }
        inline DetailedErrCounts operator-(const DetailedErrCounts& other) const
        {
            DetailedErrCounts diff = *this;
            diff -= other;
            return diff;
        }
        DetailedErrCounts& operator+=(const DetailedErrCounts& other);
        DetailedErrCounts& operator-=(const DetailedErrCounts& other);
    };


    //!
    //! \brief Array of ECC units indexed by LW2080_ECC_UNITS.
    //!
    template<class T>
    using UnitList = std::array<T, static_cast<UINT32>(Unit::MAX)>;

    //! \brief Type for LW2080_CTRL_GPU_ECC_UNIT_*
    using RmGpuEclwnit = UINT08;
    //! \brief Type for LW2080_CTRL_GR_ECC_INJECT_ERROR_LOC_*
    using RmGrInjectLoc = UINT08;
    //! \brief Type for LW208F_CTRL_CMD_GR_ECC_INJECT_ERROR_TYPE_*
    using RmGrInjectType = UINT08;
    //! \brief Type of LW2080_CTRL_MMU_ECC_INJECT_ERROR_LOC_*
    using RmMmuInjectLoc = UINT08;
    //! \brief Type of LW2080_CTRL_MMU_ECC_INJECT_ERROR_TYPE_*
    using RmMmuInjectType = UINT08;
    //! \brief Type of LW2080_CTRL_FB_ECC_INJECT_ERROR_LOC_*
    using RmFbInjectLoc = UINT08;
    //! \brief Type of ECC_UINT_* (ctrl2080ecc.h)
    using RmEclwnit = UINT08;

    //!
    //! \brief Behavior of an ECC unit when an ECC error is injected.
    //!
    enum class InjectBehavior
    {
        NOP,    //!< HW will NOP (do nothing)
        INTR    //!< HW will trigger an interrupt
    };

    //!
    //! \brief Error injection information for an ECC unit.
    //!
    struct InjectLocInfo
    {
        //! Behavior when correctable error is injected
        InjectBehavior corrBehavior = InjectBehavior::NOP;
        //! Behavior when uncorrectable error is injected
        InjectBehavior uncorrBehavior = InjectBehavior::NOP;

        InjectLocInfo() = default;
        InjectLocInfo(InjectBehavior corrBehavior, InjectBehavior uncorrBehavior)
            : corrBehavior(corrBehavior), uncorrBehavior(uncorrBehavior) {}
    };

    //!
    //! \brief Types of ECC errors.
    //!
    enum class ErrorType : UINT08
    {
        CORR,
        UNCORR,
        MAX
    };

    // RM control call LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION shows the types of
    // notifiers as SBE/DBE. This is a misnomer. The notifications actually map to
    // CORR/UNCORR.
    constexpr UINT32 LW2080_NOTIFIERS_ECC_CORR = LW2080_NOTIFIERS_ECC_SBE;
    constexpr UINT32 LW2080_NOTIFIERS_ECC_UNCORR  = LW2080_NOTIFIERS_ECC_DBE;

    //!
    //! \brief Maps RM inject error location to the location information.
    //!
    using RmGrInjectLocInfoMap = std::map<RmGrInjectLoc, InjectLocInfo>;
    using RmMmuInjectLocInfoMap = std::map<RmMmuInjectLoc, InjectLocInfo>;

    const char* ToString(Unit eclwnit);
    const char* ToString(ErrorType errType);
    const char* ToString(Source eccSource);
    const char* HsHubSublocationString(UINT32 sublocIdx);

    RmGpuEclwnit     ToRmGpuEclwnit(Unit eclwnit);
    RmEclwnit        ToRmEclwnit(Unit eclwnit);
    UINT32           ToRmNotifierEvent(ErrorType errorType);

    Unit     ToEclwnitFromRmGpuEclwnit(RmGpuEclwnit rmGpuEclwnit);
    Unit     ToEclwnitFromRmEclwnit(RmEclwnit rmGpuEclwnit);
    UINT32   ToEclwnitBitFromRmGpuEclwnit(RmGpuEclwnit rmGpuEclwnit);
    bool     IsUnitEnabled(Unit eclwnit, UINT32 unitBitMask);
    Unit     GetNextUnit(Unit eclwnit);
    UnitType GetUnitType(Unit eclwnit);

    bool IsInjectLocSupported(Unit injectLoc);
    bool IsAddressReportingSupported(Unit addressLoc);

    RmGrInjectLoc  ToRmGrInjectLoc(Unit eclwnit);
    RmMmuInjectLoc ToRmMmuInjectUnit(Unit eclwnit);
    RmFbInjectLoc  ToRmFbInjectLoc(Unit eclwnit);
    LW208F_CTRL_BUS_UNIT_TYPE ToRmBusInjectUnit(Unit eclwnit);
    
    RmGrInjectType             ToRmGrInjectType(ErrorType errorType);
    RmMmuInjectType            ToRmMmuInjectType(ErrorType errorType);
    LW208F_CTRL_FB_ERROR_TYPE  ToRmFbInjectType(ErrorType errorType);
    LW208F_CTRL_BUS_ERROR_TYPE ToRmBusInjectType(ErrorType errorType);

    template<typename InjectLocT, typename InjectLocInfoMapT>
    InjectLocInfo GetInjectLocInfo(InjectLocT injectLoc, const InjectLocInfoMapT& injectLocInfoMap)
    {
        auto injectLocSearch = injectLocInfoMap.find(injectLoc);
        MASSERT(injectLocSearch != injectLocInfoMap.end());
        return injectLocSearch->second;
    }

    RC GetRC(Unit eclwnit, ErrorType errType);

    //!
    //! \brief Get virtual GPCs that are valid to collect ECC counts from.
    //!
    //! \param pSubdev GPU subdevice to check.
    //!
    vector<UINT32> GetValidVirtualGpcs(GpuSubdevice* pSubdev);
};
