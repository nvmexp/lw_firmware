/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2014,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_BITOP_H
#define INCLUDED_BITOP_H

#ifdef MATS_STANDALONE
#include "fakemods.h"
#else
#include "core/include/massert.h"
#include "core/include/types.h"
#endif

namespace BitOp
{

    template<int msb, int lsb>
        inline UINT32 bits(UINT32 Value)
        {
            return (UINT32(Value >> lsb)) & ~(~static_cast<UINT32>(0U)<<(msb-lsb+1));
        }

    template<int ibit>
        inline UINT32 bit(UINT32 Value)
        {
            return bits<ibit,ibit>(Value);
        }

    template<int msb, int lsb>
        inline UINT64 bits(UINT64 Value)
        {
            return (UINT64(Value >> lsb)) & ~(~static_cast<UINT64>(0U)<<(msb-lsb+1));
        }

    template<int ibit>
        inline UINT64 bit(UINT64 Value)
        {
            return bits<ibit,ibit>(Value);
        }

    template<int count>
        inline UINT64 maskoff_lower_bits(UINT64 Value)
        {
            return Value & ~((static_cast<UINT64>(1U) << count) - 1);
        }

    // extract the reverse bits in the positions between msb and lsb inclusive
    template<int lsb, int msb>
        UINT32 bitsr(UINT32 src)
        {
            ct_assert(lsb >= 0 && msb >= lsb && 32 > msb);
            UINT32 temp = src >> lsb;
            UINT32 output = 0;
            for (int pos=0; pos < msb-lsb+1; ++pos)
            {
                output = (output<<1)|(temp&1);
                temp = temp>>1;
            }
            return output;
        }

    template<int lsb, int msb>
        UINT64 bitsr(UINT64 src)
        {
            ct_assert(lsb >= 0 && msb >= lsb && 64 > msb);
            UINT64 temp = src >> (UINT64)lsb;
            UINT64 output = 0;
            for (int pos=0; pos < msb-lsb+1; ++pos)
            {
                output = (output<<(UINT64)1)|(temp&(UINT64)1);
                temp = temp>>(UINT64)1;
            }
            return output;
        }
};

#endif // INCLUDED_BITOP_H
