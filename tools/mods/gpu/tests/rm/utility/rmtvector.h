/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RM_UTILITY_VECTOR_H_
#define RM_UTILITY_VECTOR_H_

#include <cstring>
#include <vector>

namespace rmt
{
    //!
    //! \brief      Vector
    //!
    //! \details    This class is a drop-in replacement for std::vector, which
    //!             it extends.  A handful of colwenience functions are added
    //!             to improve the degree of code reuse.  Objects of this class
    //!             take no more space than std::vector objects.
    //!
    template <class T> struct Vector: public std::vector<T>
    {
        //! \brief      Copy each element from that to this
        inline void push_back_all(const std::vector<T> that)
        {
            this->insert(std::vector<T>::end(), that.begin(), that.end());
        }

        //! \brief      This vector as a C-style array
        inline T *c_array()
        {
            return std::vector<T>::empty()? NULL: &std::vector<T>::front();
        }

        //! \brief      This vector as a C-style array
        inline const T *c_array() const
        {
            return std::vector<T>::empty()? NULL: &std::vector<T>::front();
        }

        //!
        //! \brief      Fill vector elements with specified byte.
        //!
        //! \details    Since this function does not change the size of this
        //!             vector, you probably want to call 'resize' first.
        //!
        inline T *memset(LwU8 val = 0)
        {
            return (T*) ::memset(&std::vector<T>::front(), val, std::vector<T>::size() * sizeof(T));
        }
    };
};

#endif // RM_UTILITY_VECTOR_H_

