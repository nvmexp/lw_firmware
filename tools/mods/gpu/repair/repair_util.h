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
#include "core/include/memlane.h"
#include "core/include/rc.h"
#include "core/include/utility.h"
#include "gpu/repair/hbm/hbm_wir.h"
#include "gpu/repair/repair_types.h"
#include "gpu/repair/mem_repair_config.h"

#include <vector>

//!
//! \def CONCAT
//! \brief Concatenates two strings.
//!
#ifdef CONCAT
#   warning Redefining CONCAT macro
#   undef CONCAT
#endif
#ifdef CONCAT2
#   warning Redefining CONCAT2 macro
#   undef CONCAT2
#endif

#define CONCAT2(a, b) a ## b
#define CONCAT(a, b) CONCAT2(a, b)

//!
//! \def UNIQUE_IDENT(ident)
//! \brief Creates a unique idenitifer using the prefix \a ident.
//!
#define UNIQUE_IDENT(ident) CONCAT(Ident, __COUNTER__)

// Probably not the best place to put this
static constexpr UINT32 s_UnknownPseudoChannel = UINT32_MAX;

namespace Repair
{
    RC GetLanesFromFileFormat(const string& fileContent, vector<Memory::FbpaLane>* pLanes);
    bool GetLaneFromString(const string& s, Memory::FbpaLane* pLane);
    RC GetLanes(const string& laneDesc, vector<Memory::FbpaLane>* pLanes);
    RC GetRowErrorsFromFileFormat(const string& fileContent, vector<Repair::RowError>* pRowErrors);
    RC GetRowErrors(const string& rowDesc, vector<Repair::RowError>* pRowErrors);

    Memory::Hbm::SupportedHbmModels
    VendorIndepSupportedHbmModels(std::initializer_list<Memory::Hbm::SpecVersion> specs);

    //!
    //! \brief Return the number of 32 bit chunks that can contain the given
    //! number of bits. The last 32 bit chunk may be partially filled.
    //!
    //! \param bitLength Number of contiguous bits.
    //!
    constexpr UINT32 Num32BitChunks(UINT32 bitLength)
    {
        // constexpr UINT32 uint32BitWidth = sizeof(UINT32) * CHAR_BIT; // @c++14 MSVC
        return Utility::AlignUp<sizeof(UINT32) * CHAR_BIT>(bitLength)
            / (sizeof(UINT32) * CHAR_BIT);
    }

    bool CmpRowErrorByRow(const RowError& a, const RowError& b);
    bool IsEqualRowErrorByRow(const RowError& a, const RowError& b);

    using Settings = Repair::Settings;

    //!
    //! \brief Write/print memory repair sequence based on settings
    //!
    //! \param settings repair settings
    //! \param regs register access object
    //! \param address ModsGpuRegAddress address
    //! \param regIndex Register index
    //! \param value register value
    //!
    //! \return void.
    //!
    void Write32
    (
        const Settings &settings,
        RegHal &regs,
        ModsGpuRegAddress address,
        UINT32 regIndex,
        UINT32 value
     );

    //!
    //! \brief Write/print memory repair sequence based on settings
    //!
    //! \param settings repair settings
    //! \param regs register access object
    //! \param address ModsGpuRegAddress address
    //! \param value register value
    //!
    //! \return void.
    //!
    void Write32
    (
        const Settings &settings,
        RegHal &regs,
        ModsGpuRegAddress address,
        UINT32 value
    );

    //!
    //! \brief Write/print memory repair sequence based on settings
    //!
    //! \param settings repair settings
    //! \param regs register access object
    //! \param field ModsGpuRegField field
    //! \param regIndex Register index
    //! \param value register value
    //!
    //! \return void.
    //!
    void Write32
    (
        const Settings &settings,
        RegHal &regs,
        ModsGpuRegField field,
        UINT32 regIndex,
        UINT32 value
     );

    //!
    //! \brief Write/print memory repair sequence based on settings
    //!
    //! \param settings repair settings
    //! \param regs register access object
    //! \param field ModsGpuRegField field
    //! \param value register value
    //!
    //! \return void.
    //!
    void Write32
    (
        const Settings &settings,
        RegHal &regs,
        ModsGpuRegField field,
        UINT32 value
    );

    //!
    //! \brief Write/print memory repair sequence based on settings
    //!
    //! \param settings repair settings
    //! \param regs register access object
    //! \param value ModsGpuRegValue value
    //! \param regIndex Register index
    //!
    //! \return void.
    //!
    void Write32
    (
        const Settings &settings,
        RegHal &regs,
        ModsGpuRegValue value,
        UINT32 regIndex
     );

    //!
    //! \brief Write/print memory repair sequence based on settings
    //!
    //! \param settings repair settings
    //! \param regs register access object
    //! \param value ModsGpuRegValue value
    //! \param value register value
    //!
    //! \return void.
    //!
    void Write32
    (
        const Settings &settings,
        RegHal &regs,
        ModsGpuRegValue value
     );

    //!
    //! \brief Read/print memory repair sequence based on settings
    //!
    //! \param settings repair settings
    //! \param regs register access object
    //! \param address ModsGpuRegAddress address
    //! \param regIndex Register index
    //!
    //! \return Register value.
    //!
    UINT32 Read32
    (
        const Settings &settings,
        RegHal &regs,
        ModsGpuRegAddress address,
        UINT32 regIndex
    );
};
