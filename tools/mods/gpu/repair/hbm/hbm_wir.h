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

#pragma once

#include "core/include/bitflags.h"
#include "core/include/hbmtypes.h"

#include <map>
#include <set>
#include <type_traits>
#include <vector>

//!
//! \file hbm_wir.h
//!
//! \brief HBM WIR related declarations.
//!

namespace Memory
{
namespace Hbm
{
    //!
    //! \brief Defines a set of supported items.
    //!
    //! \tparam T Enum. Must have 'All' defined.
    //!
    template<typename T>
    class SupportSet
    {
    public:
        using const_iterator = typename std::set<T>::const_iterator;

        //!
        //! \brief Constructor.
        //!
        //! \param v List of supported items.
        //!
        explicit SupportSet(std::initializer_list<T> v)
        {
            for (const T& e : v)
            {
                if (e == T::All)
                {
                    SetAllSupported();
                    return;
                }
            }

            m_Supported = std::move(std::set<T>(v));
        }

        SupportSet(const SupportSet& o)
        {
            this->m_AllSupported = o.m_AllSupported;
            this->m_Supported = o.m_Supported;
        }

        SupportSet(SupportSet&& o)
            : m_AllSupported(std::move(o.m_AllSupported)), m_Supported(o.m_Supported) {}

        SupportSet& operator=(const SupportSet& o)
        {
            if (this != &o)
            {
                *this = SupportSet(o);
            }
            return *this;
        }

        SupportSet& operator=(SupportSet&& o)
        {
            if (this != &o)
            {
                *this = std::move(o);
            }
            return *this;
        }

        //!
        //! \brief Set all items to supported.
        //!
        void SetAllSupported() { m_AllSupported = true; m_Supported.clear(); }

        //!
        //! \brief Add a supported item.
        //!
        void AddSupported(const T& t) { m_Supported.insert(t); }

        //!
        //! \brief Return true if the given item is supported, false otherwise.
        //!
        bool IsSupported(const T& t) const
            { return (m_AllSupported ? true : (m_Supported.count(t) != 0)); }

        //!
        //! \brief Return true if all items are supported, false otherwise.
        //!
        bool IsAllSupported() const { return m_AllSupported; }

        const_iterator begin() const { return m_Supported.begin(); }
        const_iterator end() const { return m_Supported.end(); }

    private:
        bool m_AllSupported = false; //!< All items are supported.
        std::set<T> m_Supported;     //!< Supported items.
    };

    //!
    //! \brief Supported HBM models.
    //!
    class SupportedHbmModels
    {
    public:
        //!
        //! \brief Constructor.
        //!
        //! \param specs List of supported HBM specifications.
        //! \param vendors List of supported vendor.
        //! \param dies List of supported dies.
        //! \param stackHeights List of supported stack heights.
        //! \param revisions List of supported revisions.
        //!
        SupportedHbmModels(std::initializer_list<SpecVersion> specs,
                           std::initializer_list<Vendor> vendors,
                           std::initializer_list<Die> dies,
                           std::initializer_list<StackHeight> stackHeights,
                           std::initializer_list<Revision> revisions);
        SupportedHbmModels(const SupportedHbmModels& o);
        SupportedHbmModels(SupportedHbmModels&& o);

        //!
        //! \brief Return true if the given model is supported.
        //!
        bool IsSupported(const Model& hbmModel) const;
        string ToString() const;

    private:
        SupportSet<SpecVersion> m_SpecVersions; //!< Supported HBM specification versions.
        SupportSet<Vendor>      m_Vendors;      //!< Supported vendors.
        SupportSet<Die>         m_Dies;         //!< Supported dies.
        SupportSet<StackHeight> m_StackHeights; //!< Supported stack heights.
        SupportSet<Revision>    m_Revisions;    //!< Supported revisions.
    };

    //!
    //! \brief Wrapper Instruction Register (WIR).
    //!
    struct Wir
    {
    public:
        enum Type
        {
            // HBM2 spec
            BYPASS,
            HBM_RESET,
            SOFT_REPAIR,
            HARD_REPAIR,
            SOFT_LANE_REPAIR,
            HARD_LANE_REPAIR,
            MODE_REGISTER_DUMP_SET, // aka MRS
            DEVICE_ID,

            // Samsung
            TEST_MODE_REGISTER_SET, // aka TMRS
            ENABLE_FUSE_SCAN,

            // SK Hynix/Micron
            PPR_INFO,

            // Micron
            WSO_MUX,

            // Micron
            MISC,
        };

        enum class Flags : UINT32
        {
            None = 0,                   //!< Used for type safe zeroing of flags
            SingleChannelOnly = 1 << 0, //!< Supports single channel operation only
        };

        using InstrEncoding = UINT08; //!< WIR instruction encoding
        using ChannelSelect = UINT08; //!< WIR channel selection
        enum class RegType : UINT08   //!< Register R/W availability
        {
            Read  = 1 << 0,
            Write = 1 << 1
        };
        friend constexpr RegType operator|(const RegType& lhs, const RegType& rhs)
        {
            using UnderlyingT = std::underlying_type<RegType>::type;
            return static_cast<RegType>(static_cast<UnderlyingT>(lhs) | static_cast<UnderlyingT>(rhs));
        }
        friend constexpr UINT32 operator&(const RegType& lhs, const RegType& rhs)
        {
            using UnderlyingT = std::underlying_type<RegType>::type;
            return static_cast<UnderlyingT>(lhs) & static_cast<UnderlyingT>(rhs);
        }
        static string ToString(RegType regType);

        //!
        //! \brief Channel select special values.
        //!
        //! Ref: HBM2 Spec revision 2.02, section 13.2.2.1, table 78
        //!
        enum ChannelSelectSpecialValues : ChannelSelect
        {
            CHANNEL_SELECT_ILWALID = 0xf0, // channel select is only 4 bits
            CHANNEL_SELECT_ALL = 0x0f
        };

        Wir
        (
            const char* name,
            Type wirType,
            InstrEncoding instr,
            RegType regType,
            UINT32 wdrLength,
            const SupportedHbmModels& supportedHbmModels,
            Flags flags
        );

        Wir
        (
            const char* name,
            Type wirType,
            InstrEncoding instr,
            RegType regType,
            UINT32 wdrLength,
            const SupportedHbmModels& supportedHbmModels
        );

        //!
        //! \brief Encode the WIR with the given channel select for writing to
        //! the HBM.
        //!
        //! \param channelSel The channel(s) that this WIR should operate on.
        //!
        UINT32 Encode(ChannelSelect channelSel) const;

        //!
        //! \brief Return if the given HBM model is supported.
        //!
        //! \param hbmModel HBM model.
        //!
        bool IsSupported(const Model& hbmModel) const
        { return m_SupportedHbmModels.IsSupported(hbmModel); }

        //!
        //! \brief Return true if the WIR is read capable.
        //!
        bool IsRead() const { return (m_regType & RegType::Read) != 0; }

        //!
        //! \brief Return true if the WIR is write capable.
        //!
        bool IsWrite() const { return (m_regType & RegType::Write) != 0; }

        //!
        //! \brief Return true if the WIR is read and write capable.
        //!
        bool IsReadWrite() const { return IsRead() && IsWrite(); }

        //!
        //! \brief Return the name of the WIR.
        //!
        string ToString() const { return m_name; }

        //!
        //! \brief Return a full description of the WIR and its properties.
        //!
        string FullToString() const;

        const string& GetName()      const { return m_name;      }
        Type          GetType()      const { return m_type;      }
        InstrEncoding GetInstr()     const { return m_instr;     }
        RegType       GetRegType()   const { return m_regType;   }
        UINT32        GetWdrLength() const { return m_wdrLength; }
        Flags         GetFlags()     const { return m_flags;     }

    private:
        string        m_name;      //!< Name of the WIR
        Type          m_type;      //!< WIR type
        InstrEncoding m_instr;     //!< Opcode for the instruction
        RegType       m_regType;   //!< Read/write capability of the instruction
        UINT32        m_wdrLength; //!< Length of the associate WDR
        Flags         m_flags;     //!< Flags corresponding to special features or
                                   //!< requirements of the WIR

        SupportedHbmModels m_SupportedHbmModels; //!< HBM models that support this WIR
    };
    BITFLAGS_ENUM(Wir::Flags);

    //!
    //! \brief Wrapper Data Register (WDR) data.
    //!
    class WdrData
    {
    protected:
        //! Data for the wrapper data register.
        std::vector<UINT32> m_Data;

    public:
        using Container = decltype(m_Data);
        using iterator = Container::iterator;
        using const_iterator = Container::const_iterator;
        using size_type = Container::size_type;

        WdrData() {}
        WdrData(std::initializer_list<UINT32> data) : m_Data(data) {}

        void           clear()       { return m_Data.clear(); }
        bool           empty() const { return m_Data.empty(); }
        iterator       begin()       { return m_Data.begin(); }
        const_iterator begin() const { return m_Data.begin(); }
        iterator       end()         { return m_Data.end();   }
        const_iterator end()   const { return m_Data.end();   }
        UINT32&        back()        { return m_Data.back();  }
        const UINT32&  back()  const { return m_Data.back();  }

        size_type size() const { return m_Data.size(); }
        void resize(size_type count, const UINT32& value) { m_Data.resize(count, value); }

        UINT32& operator[](size_type pos) { return m_Data[pos]; }
        const UINT32& operator[](size_type pos) const { return m_Data[pos]; }

        //!
        //! \brief Access the underlying vector.
        //!
        const std::vector<UINT32>& GetRaw() const { return m_Data; }

        string ToString() const;
    };

    //! Maps a WIR type to a WIR object
    using WirMap = map<Wir::Type, const Wir*>;
} // namespace Hbm
} // namespace Memory
