/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/types.h"
#include "type_safe/strong_typedef.hpp"

//!
//! \brief Strong typedef that represents some fixed identifier.
//!
struct Identifier
    : type_safe::strong_typedef<Identifier, UINT32>,
      type_safe::strong_typedef_op::equality_comparison<Identifier>,
      type_safe::strong_typedef_op::relational_comparison<Identifier>
{
    using strong_typedef::strong_typedef;
};

//!
//! \brief Strong typedef that represents some numeric identifier.
//!
struct NumericIdentifier
    : type_safe::strong_typedef<NumericIdentifier, UINT32>,
      type_safe::strong_typedef_op::equality_comparison<NumericIdentifier>,
      type_safe::strong_typedef_op::relational_comparison<NumericIdentifier>,
      type_safe::strong_typedef_op::increment<NumericIdentifier>,
      type_safe::strong_typedef_op::decrement<NumericIdentifier>
{
    using strong_typedef::strong_typedef;

    constexpr UINT32 Number() const { return static_cast<UINT32>(*this); }
};

//!
//! \brief Strong typedef that represents some 64-bit numeric identifier.
//!
struct NumericIdentifier64
    : type_safe::strong_typedef<NumericIdentifier64, UINT64>,
      type_safe::strong_typedef_op::equality_comparison<NumericIdentifier64>,
      type_safe::strong_typedef_op::relational_comparison<NumericIdentifier64>,
      type_safe::strong_typedef_op::increment<NumericIdentifier64>,
      type_safe::strong_typedef_op::decrement<NumericIdentifier64>
{
    using strong_typedef::strong_typedef;

    constexpr UINT64 Number() const { return static_cast<UINT64>(*this); }
};
