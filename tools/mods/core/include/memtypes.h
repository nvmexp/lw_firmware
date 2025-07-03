/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013,2015,2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Part of namespace Memory, split off to allow it to be used by
// header files that don't want to pull in all the cruft in memory.h.
// C++ allows namespaces to be reopened and added-to later.

#pragma once

#include "core/include/types.h"
#include "core/utility/char_utility.h"
#include "core/utility/strong_typedef_helper.h"

#ifdef MATS_STANDALONE
#   include "fakemods.h"
#else
#   include "core/include/massert.h"
#endif

#include <cctype>
#include <limits>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

namespace Memory
{
    //!
    //! \brief Contains functionality for an abstract frame buffer component
    //! (FBP, FBPA, FBIO, etc.).
    //!
    //! \tparam Type name of the component being represented.
    //!
    template<class T>
    struct FbComponent
        : type_safe::strong_typedef<FbComponent<T>, UINT32>,
          type_safe::strong_typedef_op::equality_comparison<FbComponent<T>>,
          type_safe::strong_typedef_op::relational_comparison<FbComponent<T>>,
          type_safe::strong_typedef_op::increment<FbComponent<T>>,
          type_safe::strong_typedef_op::decrement<FbComponent<T>>
    {
        using type_safe::strong_typedef<FbComponent<T>, UINT32>::strong_typedef;

        static T FromLetter(UINT32 componentLetter)
        {
            return T(static_cast<UINT32>(CharUtility::ToUpperCase(componentLetter) - 'A'));
        }

        constexpr UINT32 Number() const { return static_cast<UINT32>(*this); }

        //!
        //! \brief Return the letter associated with the FBPA number.
        //!
        char Letter() const
        {
            const UINT32 n = Number();
            MASSERT(n < 26 && "Only supports single letter representation");
            return 'A' + n;
        }

        friend ostream& operator<<(ostream& os, const FbComponent& component)
        {
            return os << component.Number() << " (aka '" << component.Letter() << "')";
        }
    };

    //!
    //! \brief Hardware FBP.
    //!
    struct HwFbp : public FbComponent<HwFbp>
    {
        using FbComponent::FbComponent;
    };

    //!
    //! \brief Virtual FBP.
    //!
    struct VirtFbp : public FbComponent<VirtFbp>
    {
        using FbComponent::FbComponent;
    };

    //!
    //! \brief Hardware FBPA.
    //!
    struct HwFbpa : public FbComponent<HwFbpa>
    {
        using FbComponent::FbComponent;
    };

    //!
    //! \brief Virtual FBPA.
    //!
    struct VirtFbpa : public FbComponent<VirtFbpa>
    {
        using FbComponent::FbComponent;
    };

    //!
    //! \brief Hardware FBIO.
    //!
    struct HwFbio : public FbComponent<HwFbio>
    {
        using FbComponent::FbComponent;
    };

    //!
    //! \brief Virtual FBIO.
    //!
    struct VirtFbio : public FbComponent<VirtFbio>
    {
        using FbComponent::FbComponent;
    };

    //!
    //! \brief Contains funtionality for an abstract memory channel.
    //!
    //! \tparam Type name of the component being represented.
    //!
    template<typename T>
    struct FbChannelComponent : public FbComponent<T>
    {
        constexpr FbChannelComponent() : FbComponent<T>::FbComponent() {}
        explicit constexpr FbChannelComponent(UINT32 n) : FbComponent<T>::FbComponent(n) {}

        // For lane callwlation (subp * busWidth)
        friend UINT32 operator*(const FbChannelComponent& subp, UINT32 n)
        {
            return static_cast<UINT32>(subp) * n;
        }

        friend UINT32 operator*(UINT32 n, const FbChannelComponent& subp)
        {
            return subp * n;
        }
    };

    //!
    //! \brief FBIO subpartition.
    //!
    struct FbioSubp : public FbChannelComponent<FbioSubp>
    {
        using FbChannelComponent::FbChannelComponent;
    };

    //!
    //! \brief FBPA subpartition.
    //!
    struct FbpaSubp : public FbChannelComponent<FbpaSubp>
    {
        using FbChannelComponent::FbChannelComponent;
    };

    //!
    //! \brief Hardware LTC (aka. L2 cache).
    //!
    struct HwLtc : public FbComponent<HwLtc>
    {
        using FbComponent::FbComponent;
    };

    //!
    //! \brief Virtual LTC (aka. L2 cache).
    //!
    struct VirtLtc : public FbComponent<VirtLtc>
    {
        using FbComponent::FbComponent;
    };

    //!
    //! \brief Memory rank.
    //!
    struct Rank : public NumericIdentifier
    {
        using NumericIdentifier::NumericIdentifier;
    };

    //!
    //! \brief Pseudo channel.
    //!
    struct PseudoChannel : public NumericIdentifier
    {
        using NumericIdentifier::NumericIdentifier;
    };

    //!
    //! \brief Memory bank.
    //!
    struct Bank : public NumericIdentifier
    {
        using NumericIdentifier::NumericIdentifier;
    };

    enum Location
    {
        //! GPU framebuffer
        Fb,
        //! Sysmem, CPU-cached with cache snooping
        Coherent,
        //! Sysmem, not cached on the CPU
        NonCoherent,
        //! Sysmem, CPU-cached, no cache snooping, requires explicit flushing
        CachedNonCoherent,
        //! On GPU - FB, on CheetAh - NonCoherent
        Optimal,
    };

    const char * GetMemoryLocationName(Memory::Location loc);

    enum AddressModel
    {
        Segmentation,   // one context DMA per surface
        Paging,         // one context DMA for all surfaces
        OptimalAddress, // paging if supported, segmentation if not
    };

    const char * GetMemoryAddressModelName(Memory::AddressModel model);

    // Mostly will use DefaultSys.  We will want this level of control to test
    // BR02, however
    enum SystemModel
    {
        PciExpressModel = 1,
        PciModel,
        OptimalModel,
    };

    //! Page attributes.  These match enums defined in the rm's osCore.h.
    enum Attrib
    {
        CA = 0,  // Cached
        UC = 1,  // UnCached
        WC = 2,  // Write-Combined
        WT = 3,  // Write-Through
        WP = 4,  // Write-Protect
        WB = 5,  // Write-Back
        DE = 6,  // Default
    };

    //! Page protection flags
    enum Protect
    {
        ProtectDefault = 0x0,
        Readable       = 0x1,
        Writeable      = 0x2,
        Exelwtable     = 0x4,
        ReadWrite      = Readable | Writeable, // shorthand for common case
        ProcessShared  = 0x8,  // only for shared memory between vgpu plugin and guest RM
        ProtectIlwalid = ~0U
    };

    enum DmaProtocol
    {
        Default = 0x00000000, // LWOS03_FLAGS_PROTOCOL_DEFAULT
        Safe    = 0x00000001, // LWOS03_FLAGS_PROTOCOL_SAFE
        Fast    = 0x00000002  // LWOS03_FLAGS_PROTOCOL_FAST
    };
    //!< Matches LWOS03_FLAGS_PROTOCOL

    //! Virtual memory allocation flags
    enum VirtualAllocFlags
    {
         VirualAllocDefault = 0
        ,Shared             = (1 << 0)
        ,Anonymous          = (1 << 1)
        ,Fixed              = (1 << 2)
        ,Private            = (1 << 3)
    };

    //! Virtual memory free method
    enum VirtualFreeMethod
    {
        Decommit
       ,Release
       ,VirtualFreeDefault
    };

    //! VirtualAdvice
    enum VirtualAdvice
    {
         Normal
        ,Fork
        ,DontFork
        ,VirtualAdviceDefault
    };

    // TODO(stewarts): replace with ErrorType
    //!
    //! \brief Source/cause of a blacklist
    //!
    //! NOTE: User exposed type via file (see g_FailingPages). Values must be
    //! consistent.
    //!
    enum class BlacklistSource : UINT08
    {
        MemError = 0,
        Sbe = 1,
        Dbe = 2,
    };

    //!
    //! \brief A single row in memory.
    //!
    struct Row
    {
        std::string rowName; // TODO(stewarts): Derive name from other fields
        HwFbpa hwFbio;       // TODO(stewarts): this is a misnomer! should be called hwFbpa
        FbpaSubp subpartition;
        PseudoChannel pseudoChannel;
        Rank rank;
        Bank bank;
        UINT32 row;

        Row() = default;
        Row
        (
            std::string rowName,
            HwFbpa hwFbpa,
            FbpaSubp subpartition,
            PseudoChannel pseudoChannel,
            Rank rank,
            Bank bank,
            UINT32 row
        ) : rowName(rowName),
            hwFbio(hwFbpa),
            subpartition(subpartition),
            pseudoChannel(pseudoChannel),
            rank(rank),
            bank(bank),
            row(row)
        {}

        friend bool operator<(const Row& a, const Row& b)
        {
            return std::tie(a.hwFbio, a.subpartition, a.pseudoChannel, a.rank, a.bank, a.row)
                < std::tie(b.hwFbio, b.subpartition, b.pseudoChannel, b.rank, b.bank, b.row);
        }

        friend bool operator==(const Row& a, const Row& b)
        {
            return std::tie(a.hwFbio, a.subpartition, a.pseudoChannel, a.rank, a.bank, a.row)
                == std::tie(b.hwFbio, b.subpartition, b.pseudoChannel, b.rank, b.bank, b.row);
        }
    };

    enum class RepairedLanesSource : UINT08
    {
        UNKNOWN = 0,
        GPU = 1,
        HBM_FUSES = 2
    };

    //!
    //! \brief Memory interface.
    //!
    enum class Interface
    {
        HOST2JTAG,
        IEEE1500,
        UNKNOWN,
    };

    //!
    //! \brief Memory error type.
    //!
    //! NOTE: For compatibility with places where BlacklistSource is used, the
    //! enum values here are the same as those in BlacklistSource when
    //! applicable.
    //!
    enum class ErrorType : UINT08
    {
        MEM_ERROR = 0, //!< Generic memory error.
        SBE       = 1, //!< Single bit error.
        DBE       = 2, //!< Double bit error.
        NONE      = 3  //!< No error.
    };

    // TODO(stewarts): For historical reasons, failing pages are stored using
    // 'blacklistsource' as the type. Row remapping necessitates using these
    // values in the JS despite taking a Memory::ErrorType. A later CL should
    // merge BlacklistSource into the more generic Memory::ErrorType.
    static_assert(
        static_cast<UINT08>(ErrorType::MEM_ERROR) == static_cast<UINT08>(BlacklistSource::MemError)
        && static_cast<UINT08>(ErrorType::SBE) == static_cast<UINT08>(BlacklistSource::Sbe)
        && static_cast<UINT08>(ErrorType::DBE) == static_cast<UINT08>(BlacklistSource::Dbe),
        "Enum values in Memory::ErrorType that are present in "
        "Memory::BlacklistSource do not match");

    string ToString(ErrorType errType);

    //!
    //! \brief DEVICE_ID value for a DRAM chip (GDDR/HBM)
    //!
    struct DramDevId
    {
        UINT32         index;     //! << Identifier for the DRAM chip
        vector<UINT32> rawDevId;  //! << DEVICE_ID result
    };

    //!
    //! \brief Types of repair that can be performed.
    //!
    enum class RepairType : UINT08
    {
        UNKNOWN,
        HARD,    //!< Hardware repair: permanent physical repair
        SOFT     //!< Software repair: temporary repair
    };
    string ToString(RepairType repairType);
} // namespace Memory

// Granted, this is an abuse of this header file.  Really, this header file
// should just get renamed.  This header is pretty much just the stuff that's
// common between Platform and Xp.
namespace Platform
{
    //! Simple IRQL scheme.  For now, we only support two levels: normal (all
    //! interrupts permitted), and elevated (no interrupts permitted).
    enum Irql
    {
        NormalIrql,
        ElevatedIrql,
    };
}
