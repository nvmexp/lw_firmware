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

#include "core/include/massert.h"
#include "core/include/utility.h"

#include "fsmask.h"

//------------------------------------------------------------------------------
FsMask::FsMask(UINT32 numBits)
    : m_NumBits(numBits)
{
    MASSERT(numBits <= 32);
    m_FullMask = (1UL << numBits) - 1;
}

//------------------------------------------------------------------------------
FsMask::FsMask()
    : FsMask(32)
{
}

//-----------------------------------------------------------------------------
FsMask& FsMask::operator=(const UINT32 mask)
{
    m_Mask = mask;
    return *this;
}

//------------------------------------------------------------------------------
bool FsMask::operator==(const FsMask& fsMask) const
{
    return (m_Mask == fsMask.GetMask());
}

//------------------------------------------------------------------------------
bool FsMask::operator==(const UINT32 mask) const
{
    return (m_Mask == mask);
}

//------------------------------------------------------------------------------
UINT32 FsMask::operator&(const FsMask& fsMask) const
{
    return (m_Mask & fsMask.GetMask());
}

//------------------------------------------------------------------------------
UINT32 FsMask::operator&(const UINT32 mask) const
{
    return (m_Mask & mask);
}

//------------------------------------------------------------------------------
UINT32 FsMask::operator~() const
{
    // TODO: cleanup the dowbin code to use ~ operator directly
    return ~m_Mask & m_FullMask;
}

//------------------------------------------------------------------------------
void FsMask::SetNumBits(UINT32 numBits)
{
    MASSERT(numBits <= 32);
    m_NumBits = numBits;
    m_FullMask = (1UL << numBits) - 1;
}

//------------------------------------------------------------------------------
UINT32 FsMask::GetMask() const
{
    return m_Mask;
}

//------------------------------------------------------------------------------
RC FsMask::SetMask(UINT32 mask)
{
    if (mask > m_FullMask)
    {
        Printf(Tee::PriError, "Input mask can't fit into given length (%u)\n", m_NumBits);
        return RC::BAD_PARAMETER;
    }

    m_Mask = mask;
    return RC::OK;
}

//-----------------------------------------------------------------------------
RC FsMask::SetMask(FsMask fsmask)
{
    return SetMask(fsmask.GetMask());
}

//------------------------------------------------------------------------------
RC FsMask::SetBit(UINT32 bit)
{
    if (bit >= m_NumBits)
    {
        Printf(Tee::PriError, "Bit to set should be lesser than mask length (%u)\n", m_NumBits);
        return RC::BAD_PARAMETER;
    }

    m_Mask |= (1 << bit);
    return RC::OK;
}

//------------------------------------------------------------------------------
RC FsMask::SetBits(UINT32 bitMask)
{
    if (bitMask > m_FullMask)
    {
        Printf(Tee::PriError, "Input mask can't fit into given length (%u)\n", m_NumBits);
        return RC::BAD_PARAMETER;
    }

    m_Mask |= bitMask;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC FsMask::ClearBit(UINT32 bit)
{
    if (bit >= m_NumBits)
    {
        Printf(Tee::PriError, "Bit to set should be lesser than mask length (%u)\n", m_NumBits);
        return RC::BAD_PARAMETER;
    }

    m_Mask &= ~(1 << bit);
    return RC::OK;
}

//------------------------------------------------------------------------------
RC FsMask::ClearBits(UINT32 bitMask)
{
    m_Mask &= ~bitMask;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC FsMask::SetAll()
{
    m_Mask = m_FullMask;
    return RC::OK;
}

//------------------------------------------------------------------------------
RC FsMask::ClearAll()
{
    m_Mask = 0;
    return RC::OK;
}

//------------------------------------------------------------------------------
bool FsMask::IsSet(UINT32 bit) const
{
    MASSERT(bit < m_NumBits);
    return (((m_Mask >> bit) & 0x1) == 1);
}

//------------------------------------------------------------------------------
bool FsMask::IsFull() const
{
    return m_Mask == m_FullMask;
}

//------------------------------------------------------------------------------
bool FsMask::IsEmpty() const
{
    return m_Mask == 0;
}

//------------------------------------------------------------------------------
UINT32 FsMask::GetFullMask() const
{
    return m_FullMask;
}

//------------------------------------------------------------------------------
UINT32 FsMask::GetNumSetBits() const
{
    return static_cast<UINT32>(Utility::CountBits(m_Mask));
}

//------------------------------------------------------------------------------
UINT32 FsMask::GetNumUnsetBits() const
{
    return (m_NumBits - GetNumSetBits());
}

//------------------------------------------------------------------------------
RC FsMask::SetIlwertedMask(const FsMask& mask, const FsMask & refMask)
{
    m_Mask = (~mask.GetMask()) & refMask.GetMask();

    return RC::OK;
}

//------------------------------------------------------------------------------
RC FsMask::SetIlwertedMask(const FsMask& mask)
{
    RC rc;
    FsMask fullMask; 
    fullMask.SetMask(mask.GetFullMask());
    CHECK_RC(SetIlwertedMask(mask, fullMask));
   
    return RC::OK;
}
