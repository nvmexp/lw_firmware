/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RM_UTILITY_ARRAY_H_
#define RM_UTILITY_ARRAY_H_

#include <vector>

namespace rmt
{
    //!
    //! \brief      Fixed-Size Array
    //!
    //! \details    This class is modeled after the 2011 C++ standard, but does
    //!             not (as of yet) implement all the functionality.
    //!
    template <class T, size_t N> struct Array
    {
        //! \brief      The array
        T array[N];

        //! \brief      Subscript operator
        T &operator[](size_t n)
        {
            return array[n];
        }

        //! \brief      Subscript operator
        const T &operator[](size_t n) const
        {
            return array[n];
        }

        //! \brief      Subscript operator
        T *operator+(size_t n)
        {
            return array + n;
        }

        //! \brief      Subscript operator
        const T *operator+(size_t n) const
        {
            return array + n;
        }

        //! \brief      Is this a valid index?
        inline bool isValid(size_t n) const
        {
            return n < N;
        }

    };
};

#endif // RM_UTILITY_VECTOR_H_

