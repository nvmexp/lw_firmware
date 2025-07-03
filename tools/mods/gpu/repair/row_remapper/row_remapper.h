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

#include "core/include/types.h"
#include "core/include/memtypes.h"
#ifdef MATS_STANDALONE
#   include "fakemods.h"
#else
#   include "core/include/lwrm.h"
#endif

#include <limits>

class GpuSubdevice;

class RowRemapper
{
protected:
    using MemErrT = Memory::ErrorType;

public:

    //!
    //! \brief Source of the row remap.
    //!
    enum class Source : UINT08
    {
        UNKNOWN = 0,      //!< Unknown.
        NONE    = 1 << 0, //!< None.
        FACTORY = 1 << 1, //!< Factory.
        FIELD   = 1 << 2, //!< Field.
        MODS    = 1 << 3  //!< MODS.
    };

    //!
    //! \brief The status of a single row remap that was performed.
    //!
    enum class RemapStatus : UINT08
    {
        UNKNOWN = 0,           //!< Unknown
        OK = 1,                //!< Success
        REMAPPING_PENDING = 2, //!< Row remap will take affect next boot
        TABLE_FULL = 3,        //!< Row remap table is full
        RM_INTERNAL_ERROR = 4, //!< RM experienced an internal error
    };

    //!
    //! \brief Colwert source and error to an RM row remapper source.
    //! type.
    //!
    //! \param source Source.
    //! \param error Error.
    //! \param[out] pOut RM source.
    //!
    static RC ToRmSource(Source source, MemErrT error, UINT08* pOut);

    //!
    //! \brief Colwert RM row remap source to an individual source and error
    //! type.
    //!
    //! \param rmVal RM source.
    //! \param[out] pSource Source.
    //! \param[out] pError Error.
    //!
    static RC FromRmSource(UINT08 rmVal, Source* pSource, MemErrT* pError);

    RowRemapper(const GpuSubdevice* pSubdev, LwRm* pLwRm);

    //!
    //! \brief Initialize. Must be done before use.
    //!
    RC Initialize();

    //!
    //! \brief True if row remapping is on, false otherwise.
    //!
    bool IsEnabled() const;

    //!
    //! \brief Get the number of remapped rows in the InfoROM.
    //!
    //! \param[out] pNumRows Number of remapped rows.
    //!
    RC GetNumInfoRomRemappedRows(UINT32* pNumRows) const;

    //!
    //! \brief Print the details of each remapped row in the InfoROM.
    //!
    RC PrintInfoRomRemappedRows(const char * pPrefix = nullptr) const;

    //!
    //! \brief Defines the maximum number of remapped rows allowed.
    //!
    struct MaxRemapsPolicy
    {
        static constexpr UINT32 UNSET = std::numeric_limits<UINT32>::max();
        UINT32 totalRows = UNSET;      //!< Total amount of row remaps on the GPU.
        UINT32 numRowsPerBank = UNSET; //!< Total amount of row remaps per bank.
    };

    //!
    //! \brief Check if the policy for the maximum number of remapped rows has
    //! been violated.
    //!
    RC CheckMaxRemappedRows(const MaxRemapsPolicy& policy) const;

    //!
    //! \brief Check if a remap error oclwrred
    //!
    RC CheckRemapError() const;

    //!
    //! \brief Row remap request.
    //!
    struct Request
    {
        UINT64  physicalAddr = 0;             //!< Physical address to remap
        MemErrT cause        = MemErrT::NONE; //!< Cause of the remap

        Request(UINT64 physicalAddr, MemErrT cause)
            : physicalAddr(physicalAddr), cause(cause) {}
    };

    //!
    //! \brief Perform the given remaps.
    //!
    //! \param request Row remap requests.
    //!
    RC Remap(const vector<Request>& requests) const;

    //!
    //! \brief Description of what types of remapping entries to clear.
    //!
    struct ClearSelection
    {
        Source source = Source::NONE;  //!< Source of the remap
        MemErrT cause = MemErrT::NONE; //!< Cause of the remap

        ClearSelection(Source source, MemErrT cause)
            : source(source), cause(cause) {}
    };

    //!
    //! \brief Clear remapped rows based on the given criteria.
    //!
    //! \param clearSelections Descriptions of the types of remaps to be
    //! removed. A remapped row that meets any individual clear selection will
    //! be cleared.
    //!
    RC ClearRemappedRows(const vector<ClearSelection>& clearSelections) const;

    //!
    //! \brief The number of bytes reserved as part of the reserved rows when
    //! the row remapper feature is enabled.
    //!
    UINT64 GetRowRemapReservedRamAmount() const;

private:
    const GpuSubdevice* m_pSubdev = nullptr;
    LwRm*               m_pLwRm   = nullptr;

    //! True if initialized.
    bool m_Initialized = false;
    //! True if remaps should be recorded as field remaps instead of factory.
    bool m_RowRemapAsField = false;
};

string ToString(RowRemapper::Source source);
