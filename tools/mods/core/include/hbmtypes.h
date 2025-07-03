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

#include "core/include/hbm_model.h"
#include "core/include/memtypes.h"
#include "core/include/types.h"
#include "core/utility/strong_typedef_helper.h"
#include <vector>

namespace Memory
{
    namespace Hbm
    {
        //!
        //! \brief HBM site.
        //!
        struct Site : public NumericIdentifier
        {
            using NumericIdentifier::NumericIdentifier;
        };

        //!
        //! \brief HBM stack (aka. stack ID and SID).
        //!
        //! Equivalent to a memory rank.
        //!
        struct Stack : public NumericIdentifier
        {
            using NumericIdentifier::NumericIdentifier;

            explicit constexpr Stack(const Rank& rank) : Stack(rank.Number()) {}

            //!
            //! \brief Implicit colwersion to Rank.
            //!
            operator Rank() const { return Rank(Number()); }
        };

        //!
        //! \brief HBM channel.
        //!
        struct Channel : public NumericIdentifier
        {
            using NumericIdentifier::NumericIdentifier;
        };
    } // namespace Hbm
} // namespace Memory
