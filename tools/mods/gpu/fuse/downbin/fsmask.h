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
//! \brief Mask representing floorsweeping state
//!
class FsMask
{
public:
    explicit FsMask(UINT32 numBits);
    explicit FsMask();

    // Copyable
    FsMask(const FsMask&)             = default;
    FsMask& operator=(const FsMask&)  = default;
   
    // Assignment of mask
    FsMask& operator=(const UINT32 mask);

    // Equality operators 
    bool operator==(const FsMask& fsMask) const;
    bool operator==(const UINT32 mask) const;

    // bitwise and (&) operators 
    UINT32 operator&(const FsMask& fsMask) const;
    UINT32 operator&(const UINT32 mask) const;

    //unary ~ operator
    UINT32 operator~() const;

    // Move constructor
    FsMask(FsMask && mask)            = default;
    FsMask& operator=(FsMask && mask) = default;

    //!
    //! \brief Set number of bits in the full mask
    //!
    void SetNumBits(UINT32 numBits);

    //!
    //! \brief Return current value of the mask
    //!
    UINT32 GetMask() const;

    //!
    //! \brief Set the mask to given value
    //!
    //! \param mask   Value to set
    //!
    RC SetMask(UINT32 mask);
    RC SetMask(FsMask fsMask);

    //!
    //! \brief Set the given bit in the mask to 1
    //!
    //! \param bit   Bit index to be set
    //!
    RC SetBit(UINT32 bit);

    //!
    //! \brief Set the bits in the given bitmask to 1
    //!
    //! \param bitMask Bitmask representing bits to be set
    //!
    RC SetBits(UINT32 bitMask);

    //!
    //! \brief Clear the given bit to 0
    //!
    //! \param bitMask Bit index to be cleared
    //!
    RC ClearBit(UINT32 bit);

    //!
    //! \brief Clear the bits in the given bitmask to 0
    //!
    //! \param bitMask Bitmask representing bits to be cleared
    //!
    RC ClearBits(UINT32 bitMask);

    //!
    //! \brief Set all valid bits in the mask to 1
    //!
    RC SetAll();

    //!
    //! \brief Set all valid bits in the mask to 0
    //!
    RC ClearAll();

    //!
    //! \brief Returns if given bit index is set to 1
    //!
    //! \param bit   Bit index to check
    //!
    bool IsSet(UINT32 bit) const;

    //!
    //! \brief Returns if all valid bits in the mask are set to 1
    //!
    bool IsFull() const;

    //!
    //! \brief Returns if all valid bits in the mask are set to 0
    //!
    bool IsEmpty() const;

    //!
    //! \brief Returns the value of a fully set mask
    //!
    UINT32 GetFullMask() const;

    //!
    //! \brief Returns number of valid bits set to 1
    //!
    UINT32 GetNumSetBits() const;

    //!
    //! \brief Returns number of valid bits set to 0
    //!
    UINT32 GetNumUnsetBits() const;
 
    //!
    //! \brief Given a  mask, set the ilwerted mask as per full mask 
    //         or given bitMask
    //! \param mask    Initial mask to be ilwerted to.
    //!        refMask Reference mask with superset of set bits in
    //!                in the ilwerted mask 
    RC SetIlwertedMask(const FsMask& mask, const FsMask& refMask);
    RC SetIlwertedMask(const FsMask& mask);
private:
    // Value of the mask
    UINT32 m_Mask     = 0;

    // Number of bits
    UINT32 m_NumBits  = 0;

    // Value of fully set mask
    UINT32 m_FullMask = 0;
};
