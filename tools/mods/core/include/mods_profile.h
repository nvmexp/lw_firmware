/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "utility.h"

//! Creates local variable which profiles a portion of MODS by measuring time
//! between its construction and destruction.
//!
//! This allows us to measure things globally in MODS and then report them
//! as perf values.
//!
//! Usage:
//! @code{.cpp}
//! PROFILE(CRC);
//! @endcode
#define PROFILE(what) Utility::Profile __MODS_UTILITY_CATNAME(__profile, __LINE__)\
                                       (Utility::ProfileType::what)

namespace Utility
{
    enum class ProfileType
    {
        CRC,

        // This comes last, just to have the number of supported things
        // to profile.
        NUM_PROFILE_TYPES
    };

    //! Built-in profiler implementation, used by the PROFILE macro.
    class Profile
    {
        public:
            explicit Profile(ProfileType what);
            ~Profile();

            Profile(const Profile&)            = delete;
            Profile& operator=(const Profile&) = delete;

            //! Returns total time spent doing a particular activity,
            //! such as CRC.
            static UINT64 GetTotalTimeNs(ProfileType what);

        private:
            ProfileType m_What;
            UINT64      m_StartTime;
    };
}
