/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _RMT_RANDOM_H
#define _RMT_RANDOM_H

#include "core/include/utility.h"

namespace rmt
{
    //! Abstract base class for randomazible resources.
    class Randomizable
    {
    public:
        virtual ~Randomizable() {}

        //! Randomize configuration from a seed.
        virtual void Randomize(const UINT32 seed) = 0;

        //! Check if the current configuration is valid.
        //!
        //! @returns NULL if valid.
        //! @returns An explanation if invalid.
        //!
        //! @sa Randomizable::CreateRandom
        virtual const char *CheckValidity() const = 0;

        //! Check if the current configuration is feasible.
        //!
        //! @returns NULL if feasible.
        //! @returns An explanation if infeasible.
        //!
        //! @sa Randomizable::CreateRandom
        virtual const char *CheckFeasibility() const { return NULL; }

        //! Print details of current configuration.
        virtual void PrintParams(const INT32 pri) const = 0;

        //! Get the name of the type that should be used for logging.
        virtual const char *GetTypeName() const = 0;

        //! Get unique object ID that should be used for logging.
        virtual UINT32 GetID() const = 0;

        //! Create/allocate the resource, with possibility of failure.
        //! @sa Randomizable::CreateRandom
        virtual RC Create() = 0;

        RC CreateRandom(const UINT32 seed);

        //! Utility to destroy randomizable objects (with added logging).
        static struct Deleter
        {
            void operator ()(Randomizable *pObj);
        } deleter;
    };

    //! Utility to construct ::Random with a seed. Why does this not exist?
    class RandomInit : public Random
    {
    public:
        RandomInit(const UINT32 seed)
        {
            SeedRandom(seed);
        }
    };
}

#endif
