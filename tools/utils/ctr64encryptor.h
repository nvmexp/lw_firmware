/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013, 2015 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef CTR64ENCRYPTOR_H
#define CTR64ENCRYPTOR_H

#include <cstddef>

#include "core/include/types.h"

#include <boost/type_traits.hpp>
#include <boost/utility/enable_if.hpp>

namespace AES
{
    static const size_t B = 16;
    static const size_t DW = 4;
    static const size_t RK = 44;

    void RijndaelEncrypt(const UINT32 rk[AES::RK], const UINT32 pt[AES::DW], UINT32 ct[AES::DW]);
    void CalcRoundKey(const UINT32 key[AES::DW], UINT32 rk[AES::RK]);
}

class CTR64Encryptor
{
private:

public:
    static const size_t blockSize = AES::B;

    CTR64Encryptor();

    void Encrypt(const UINT08* in, UINT08* out, const UINT32 byteCount);
    void SetContentKey(const UINT32 key[AES::DW]);
    void SetInitializatiolwector(const UINT32 iv[AES::DW]);

    const UINT32* GetContentKey() const
    {
        return m_contentKey;
    }

    const UINT32* GetInitializatiolwector() const
    {
        return m_initializatiolwector;
    }

    void StartOver();

private:
    template <class T, class U, class V>
    static void XOR128(
              T *dst,
        const U *op1,
        const V *op2,
        typename boost::enable_if<boost::is_integral<T>, T >::type* dummy1 = 0,
        typename boost::enable_if<boost::is_integral<U>, U >::type* dummy2 = 0,
        typename boost::enable_if<boost::is_integral<V>, V >::type* dummy3 = 0
    )
    {
              UINT32* dstPtr = reinterpret_cast<UINT32*>(&dst[0]);
        const UINT32* op1Ptr = reinterpret_cast<const UINT32*>(&op1[0]);
        const UINT32* op2Ptr = reinterpret_cast<const UINT32*>(&op2[0]);

        dstPtr[0] = op1Ptr[0] ^ op2Ptr[0];
        dstPtr[1] = op1Ptr[1] ^ op2Ptr[1];
        dstPtr[2] = op1Ptr[2] ^ op2Ptr[2];
        dstPtr[3] = op1Ptr[3] ^ op2Ptr[3];
    }

    static void PadBlock(const UINT08* pt, const UINT32 bytes, UINT08 block[AES::B]);

    UINT32 m_contentKey[AES::DW];
    UINT32 m_initializatiolwector[AES::DW];
    UINT32 m_lwrCounterBlock[AES::DW];
    UINT32 m_roundKey[AES::RK];
};

#endif // CTR64ENCRYPTOR_H
