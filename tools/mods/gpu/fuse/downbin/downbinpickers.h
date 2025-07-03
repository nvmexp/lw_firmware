/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"

//!
//! \brief Pickers are used when deciding how to select a given group or element
//!        to downbin in a FS set.
//!        They are supposed to leave all valid group / element choices
//!        available for successive pickers to chose from. Applying a succession of pickers
//!        should eventually leave you with the best choices of groups / elements to disable
//!        to meet a given SKU requirement
//!
class DownbinPicker
{
public:
    DownbinPicker() = default;
    virtual ~DownbinPicker() = default;

    virtual string GetPickerName() const = 0;
};

enum class PickPriority
{
    LEVEL0, // Highest Priority
    LEVEL1,
    LEVEL2,
    LEVEL3  // Lowest Priority
};

//-------------------------------------------------------------------------------------------------

//!
//! \brief Group Pickers help choose a set of groups to
//!        1. Disable a group (DisablePicker) OR
//!        2. Disable an element from a FS set (ReducePicker)
//!
class DownbinGroupPicker : public DownbinPicker
{
public:
    DownbinGroupPicker() = default;

    virtual RC Pick
    (
        const FsMask& inGroupMask,
        FsMask *pOutGroupMask
    ) const = 0;
};

//-------------------------------------------------------------------------------------------------

//!
//! \brief Element Pickers help choose a set of elements that can be disabled from
//!
class DownbinElementPicker : public DownbinPicker
{
public:
    DownbinElementPicker() = default;


    virtual RC Pick
    (
        UINT32 grpIdx,
        const FsMask& inElementMask,
        FsMask *pOutElementMask
    ) const = 0;
};

//-------------------------------------------------------------------------------------------------

//!
//! \brief Helper class to manage different pickers at varying priorities.
//!        The pickers are placed in the queue based on Pick priority
//!
class PickerQueue
{
   //TODO   
};
