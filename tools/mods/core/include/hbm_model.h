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

namespace Memory
{
    namespace Hbm
    {
        //!
        //! \brief HBM specification version.
        //!
        enum class SpecVersion : UINT32
        {
            V2,  //!< HBM2
            V2e, //!< HBM2e
            All,
            Unknown = _UINT32_MAX
        };
        string ToString(SpecVersion specVersion);

        //!
        //! \brief HBM Vendor.
        //!
        enum class Vendor : UINT32
        {
            Samsung,
            SkHynix,
            Micron,
            All,
            Unknown = _UINT32_MAX
        };
        string ToString(Vendor vendor);

        //!
        //! \brief HBM physcial die version.
        //!
        enum class Die : UINT32
        {
            B,
            X,
            Mask,
            All,
            Unknown = _UINT32_MAX
        };
        string ToString(Die die);

        //!
        //! \brief HBM stack height.
        //!
        enum class StackHeight : UINT32
        {
            Hi4, //!< 4HI
            Hi8, //!< 8HI
            All,
            Unknown = _UINT32_MAX,
        };
        string ToString(StackHeight stackHeight);

        //!
        //! \brief HBM model revision.
        //!
        enum class Revision : UINT32
        {
            A,
            B,
            C,
            D,
            E,
            F,
            A1,
            A2,
            V3,
            V4,
            All,
            Unknown = _UINT32_MAX
        };
        string ToString(Revision rev);

        //!
        //! \brief A specific model of HBM.
        //!
        struct Model
        {
            SpecVersion spec     = SpecVersion::Unknown; //!< HBM specification version
            Vendor      vendor   = Vendor::Unknown;      //!< Vendor
            Die         die      = Die::Unknown;         //!< HBM die version
            StackHeight height   = StackHeight::Unknown; //!< Stack height of the HBM
            Revision    revision = Revision::Unknown;    //!< Revision
            string      name     = "unknown";            //!< Human readable name of the revision

            Model() {}
            Model
            (
                SpecVersion specVersion,
                Vendor vendor,
                Die die,
                StackHeight stackHeight,
                Revision revision,
                const string& name
            ) : spec(specVersion),
                vendor(vendor),
                die(die),
                height(stackHeight),
                revision(revision),
                name(name)
            {}

            string ToString() const;
        };
    } // namespace Hbm
} // namespace Memory
