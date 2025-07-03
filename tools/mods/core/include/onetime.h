/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "tracy.h"

#ifdef __cplusplus

#ifndef INCLUDED_STL_STRING
#define INCLUDED_STL_STRING
#include <string>
#endif

#include "core/include/bitflags.h"
#include "core/include/types.h"

#include <time.h>

namespace Mods
{
    //!
    //! \brief Stage in the exelwtion of MODS.
    //!
    enum class Stage : UINT08
    {
        Start = 1 << 0,
        End   = 1 << 1
    };
    BITFLAGS_ENUM(Stage);
}

//!
//! Special one-time initialization and shutdown. These routines will be the
//! absolute first and last to be exelwted, and should only be used in cases
//! where it is absolutely necessary!
//!
class OneTimeInit
{
public:
    //!
    //! \brief Date of Birth
    //!
    //! Ex: Wed Feb 29 12:20:26 2012
    //!
    static std::string GetDOB();

    //!
    //! \brief Date of Birth
    //!
    //! Ex: 1330473600
    //!
    static time_t GetDOBUnixTime();

    //!
    //! \brief Date of Death
    //!
    //! Ex: Wed Feb 29 12:22:26 2012
    //!
    static std::string GetDOD();

    //!
    //! \brief Time alive in seconds
    //!
    //! \param addHMS Appends hours, minutes, and seconds in the format \a
    //! h:m:s.
    //!
    static std::string GetAge(bool addHMS);

    //!
    //! \brief Associate the current time with the given MODS stage.
    //!
    //! \param stage MODS stage.
    //!
    static void Timestamp(Mods::Stage stage);

    //!
    //! \brief Print MODS stage (start/end) timestamp.
    //!
    //! NOTE: Adds appropriate MLE header/footer data for the start/end stage.
    //!
    //! \param modsStage MODS stage.
    //! \param skipMleHeader Will skip printing the MLE header, if it would be
    //! printed for the given stage.
    //!
    //! \see SetModsTimeOfDeath Time of death printed for the \a end stage will
    //! set set to the last time this was called.
    //!
    static void PrintStageTimestamp
    (
        Mods::Stage modsStage,
        bool skipMleHeader = false
    );

    //!
    //! \brief Get UTC to local offset "-8:00".
    //!
    static std::string UtcOffsetStr();

    OneTimeInit();
    ~OneTimeInit();

private:
    static long    s_OneTimeRefCount;
    static time_t  s_OneTimeStartTime;
    static time_t  s_OneTimeEndTime;
    static UINT64  s_OneTimeStartTimeNs;
    static FLOAT64 s_OneTimeElapsed;

};

// Nifty counter to be included from all (C++) source files so that we can do a
// one time initialization and shutdown (before and after static constructor
// and destructor calls).

static OneTimeInit s_OneTimeInit;

#endif // __cplusplus
