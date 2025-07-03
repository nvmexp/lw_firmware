/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or diss_Closure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "encryption_common.h"
#include "aes.h"
#include "ccm.h"

const unsigned char t_7[16 * 3] =
{
    0x41, 0xf2, 0x04, 0x66, 0xd0, 0xdc, 0xce, 0x6a, 0x6a, 0x10, 0x24, 0xa6, 0x9a, 0xac, 0x7f, 0x37,
    0xe1, 0x2d, 0x8c, 0xae, 0xa1, 0xfc, 0x4f, 0x62, 0xe7, 0xd0, 0x4c, 0x67, 0xc0, 0xe4, 0x15, 0x26,
    0xd1, 0x6f, 0x94, 0xfc, 0x72, 0xaa, 0x9e, 0x8f, 0x4a, 0x73, 0xb7, 0x4f, 0xaa, 0x36, 0xab, 0xa9
};

LwDiagUtils::EC Encryption::LoadFile(FILE* file, vector<UINT08>* pBuffer)
{
    LWDASSERT(pBuffer);

    const size_t increment = 64 * 1024;

    for (;;)
    {
        const auto oldSize = pBuffer->size();
        pBuffer->resize(oldSize + increment);
        const auto numRead = fread(&(*pBuffer)[oldSize], 1, increment, file);
        if (numRead < increment)
        {
            pBuffer->resize(oldSize + numRead);
        }
        if (ferror(file))
        {
            return LwDiagUtils::InterpretFileError();
        }
        if (feof(file))
        {
            break;
        }
    }

    return LwDiagUtils::OK;
}

void GetAESKey(UINT08* buf, const UINT08* encryptedKey)
{
    LWDASSERT(buf);
    LWDASSERT(encryptedKey);

    CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption dec(&encryptedKey[16], 16, &encryptedKey[32]);
    dec.ProcessData(buf, &encryptedKey[0], 16);
}
