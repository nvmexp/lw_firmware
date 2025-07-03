/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/crypto.h"
#include "core/include/massert.h"
#include <string.h>

//------------------------------------------------------------------------------
// AES encryption routines -- Reference: FIPS-197
//------------------------------------------------------------------------------
RC Crypto::XtsAesEncSect(XTSAESKey &Key,
                         UINT64 Tweak,
                         UINT32 DataSize,
                         const UINT08* pRawData,
                         UINT08* pEncData)
// routine cloned from Annex C in 1619.pdf
{
    RC rc;
    UINT32 i,j; // local counters
    UINT08 t[AES_BLK_BYTES]; // tweak value
    //PP after GF addition and token after the GF multiplication
    UINT08 pp[AES_BLK_BYTES], token[AES_BLK_BYTES];
    const UINT08 *x=pRawData;
    UINT08 *cx=pEncData; // read/write pointers
    UINT08 cIn,cOut; // "carry" bits for LFSR shifting

    if(DataSize%AES_BLK_BYTES) // data unit must be multiple of 16 bytes
    {
        Printf(Tee::PriError, "XTS-AES DataSize in multiples of 16 only.\n");
        return RC::SOFTWARE_ERROR;
    }

    if(!pRawData || !pEncData)
    {
        Printf(Tee::PriError, "Input pointer pRawData/pEncData is invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    for(j=0;j<AES_BLK_BYTES;j++)
    {
        // colwert sector number to tweak plaintext
        t[j] = (UINT08) (Tweak & 0xff);
        Tweak = Tweak >> 8; // also note that t[] is padded with zeroes
    }

    CHECK_RC(EncAes128(Key.keys[1],t,token)); // encrypt the tweak

    // now encrypt the data unit, AES_BLK_BYTES at a time
    for(i=0;i<DataSize;i+=AES_BLK_BYTES, x+=AES_BLK_BYTES, cx+=AES_BLK_BYTES)
    {
        // merge the tweak into the input block
        for(j=0;j<AES_BLK_BYTES;j++)
            pp[j] = x[j] ^ token[j];

        // encrypt one block
        CHECK_RC(EncAes128(Key.keys[0],pp,cx));

        // merge the tweak into the output block
        for(j=0;j<AES_BLK_BYTES;j++)
            cx[j] = cx[j] ^ token[j];

        // Multiply t by  to get next token
        cIn = 0;
        for(j=0;j<AES_BLK_BYTES;j++)
        {
            cOut = (token[j] >> 7) & 1;
            token[j] = ((token[j] << 1) + cIn) & 0xff;
            cIn = cOut;
        }
        if(cOut)
            token[0] ^= AES_GF_128_FDBK;
    }

    return OK;
}

RC Crypto::XtsAesDecSect(XTSAESKey &Key,
                         UINT64 Tweak,
                         UINT32 DataSize,
                         const UINT08* pEncData,
                         UINT08* pDecData)
{
    RC rc;
    UINT32 i,j; // local counters
    UINT08 t[AES_BLK_BYTES]; // tweak value
    //PP after GF addition and token after the GF multiplication
    UINT08 pp[AES_BLK_BYTES], token[AES_BLK_BYTES];
    const UINT08 *x=pEncData;
    UINT08 *cx=pDecData; // read/write pointers
    UINT08 cIn,cOut; // "carry" bits for LFSR shifting

    if(DataSize%AES_BLK_BYTES) // data unit is multiple of 16 bytes
    {
        Printf(Tee::PriError, "XTS-AES DataSize in multiples of 16 only.\n");
        return RC::SOFTWARE_ERROR;
    }

    if(!pEncData || !pDecData)
    {
        Printf(Tee::PriError, "Input pointer pEncData/pDecData is invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    for(j=0;j<AES_BLK_BYTES;j++)
    {
        // colwert sector number to tweak plaintext
        t[j] = (UINT08) (Tweak & 0xff);
        Tweak = Tweak >> 8; // also note that t[] is padded with zeroes
    }

    CHECK_RC(EncAes128(Key.keys[1],t,token)); // encrypt the tweak

    // now encrypt the data unit, AES_BLK_BYTES at a time
    for(i=0;i<DataSize;i+=AES_BLK_BYTES, x+=AES_BLK_BYTES, cx+=AES_BLK_BYTES)
    {
        // merge the tweak into the input block
        for(j=0;j<AES_BLK_BYTES;j++)
            pp[j] = x[j] ^ token[j];

        // encrypt one block
        CHECK_RC(DecAes128(Key.keys[0],pp,cx));

        // merge the tweak into the output block
        for(j=0;j<AES_BLK_BYTES;j++)
            cx[j] = cx[j] ^ token[j];

        // Multiply t by  to get next token
        cIn = 0;
        for(j=0;j<AES_BLK_BYTES;j++)
        {
            cOut = (token[j] >> 7) & 1;
            token[j] = ((token[j] << 1) + cIn) & 0xff;
            cIn = cOut;
        }
        if(cOut)
            token[0] ^= AES_GF_128_FDBK;
    }
    return OK;
}

RC Crypto::EncAes128(AESKey128 &Key, const UINT08* pText, UINT08* pCipher, UINT32 BlkSize)
{
    RC rc;
    // standard procedure for AES block encryption: Sec. 5.1 in FIPS-197 standard
    // BlkSize must be 128 bit
    if(BlkSize != AES_BLK_BYTES)
    {
        Printf(Tee::PriError, "AES block encryption BlkSize != 128-bit.\n");
        return RC::SOFTWARE_ERROR;
    }

    if(!pText || !pCipher)
    {
        Printf(Tee::PriError, "Input pointer pText/pCipher invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    // callwlate the key expansion
    AESKey128 exKey[AES_BLK_NR128+1];
    CHECK_RC(AesKeyExpansion((UINT08*)&Key, (UINT08*)exKey, false));

    // perform Cipher using the expanded keys
    CHECK_RC(AesCipher(pText,pCipher,(UINT08*)exKey));

    return OK;
}

RC Crypto::DecAes128(AESKey128 &Key, const UINT08* pCipher, UINT08* pText, UINT32 BlkSize)
{
    RC rc;
    // standard procedure for AES block decryption: Sec. 5.3 in FIPS-197 standard
    // BlkSize must be 128 bit
    if(BlkSize != AES_BLK_BYTES)
    {
        Printf(Tee::PriError, "AES block encryption BlkSize != 128-bit.\n");
        return RC::SOFTWARE_ERROR;
    }

    if(!pText || !pCipher)
    {
        Printf(Tee::PriError, "Input pointer pText/pCipher invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    // callwlate the key expansion
    AESKey128 exKey[AES_BLK_NR128+1];
    CHECK_RC(AesKeyExpansion((UINT08*)&Key, (UINT08*)exKey, true));

    // perform Cipher using the expanded keys
    CHECK_RC(AesIlwCipher(pCipher,pText,(UINT08*)exKey));

    return OK;
}

//helper routine for GF(2^8) algebra
UINT08 Crypto::Gf8Multiplier(UINT08 P1, UINT08 P2)
{
    UINT08 aa = P1, bb = P2, r = 0, t;

    while(aa)
    {
        if(aa&1)
            r ^= bb;
        t = bb & 0x80;
        bb <<= 1;
        if(t)
            bb ^= AES_GF_8_FDBK;
        aa >>= 1;
    }

    return r;
}

UINT08 Crypto::Gf8Power(UINT08 Alpha, UINT32 Exp)
{
    UINT32 exponent = Exp%(AES_GF_SIZE(8)-1);
    UINT08 prod = 1, i;
    UINT08 prodArray[AES_GF_SIZE(8)-1];
    memset(prodArray,0,AES_GF_SIZE(8)-1);

    if(exponent < 2)
    {
        if(exponent)
            prod = Alpha;
        else
            prod = 1;
    }
    else
    {
        //mark the factorized term a^k for a^exponent
        prodArray[exponent] = 1;
        for(i=exponent; i>=2; i--)
        {
            if(prodArray[i])
            {
                prodArray[i/2] = 1;
                if(i%2)
                    prodArray[i/2+i%2] = 1;
            }
        }
        prodArray[1] = Alpha;
        prodArray[0] = 1;

        //set the intial a^0 to a^7 and compute the marked value a^k
        for(i=0; i<=exponent; i++)
        {
            //for exponents greater or equal to 8
            if(prodArray[i] > 0)
                prodArray[i] = Gf8Multiplier(prodArray[i/2],prodArray[i/2+i%2]);
        }
        prod = prodArray[exponent];
/*      prod=Alpha;
        for(i=2;i<=exponent;i++)
            prod = Gf8Multiplier(prod,Alpha);*/
    }

    return prod;
}

//AES Key expansion routines
RC Crypto::AesKeyExpansion(const UINT08 *pKey, UINT08 *pExKey, bool IsIlw, UINT32 KeyLen)
{
    UINT32 i;
    UINT32 *pDword = (UINT32*)pExKey;
    UINT32 round, maxRound;

    switch(KeyLen)
    {
        case sizeof(AESKey128):
            maxRound = AES_BLK_NR128;
            break;
        case sizeof(AESKey192):
            maxRound = AES_BLK_NR192;
            break;
        case sizeof(AESKey256):
            maxRound = AES_BLK_NR256;
            break;
        default:
            maxRound = 0;
            break;
    }

    if(!maxRound)
    {
        Printf(Tee::PriError, "Encryption KeyLen is not within supported list.\n");
        return RC::SOFTWARE_ERROR;
    }
    if(!pKey || !pExKey)
    {
        Printf(Tee::PriError, "Input pointer pText/pCipher/pExKey invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    //copy the Key to the first KeyLen/4 DWs in pExKey
    memcpy(pExKey, pKey, KeyLen);
    i = KeyLen/4;
    pDword += KeyLen/4;

    for(; pDword < (UINT32*)(pExKey+AES_BLK_BYTES*(maxRound+1)); pDword++, i++ )
    {
        //Printf(Tee::PriDebug, "Round %i\n", i);

        UINT32 tmp = *(pDword-1);

        //Printf(Tee::PriDebug, "tmp: 0x[%02x %02x %02x %02x]\n",
        //       tmp&0xff, (tmp>>8)&0xff, (tmp>>16)&0xff, (tmp>>24)&0xff);

        if(i%(KeyLen/4)==0)
        {
            AesRotWord(tmp);
            AesSubWord(tmp);
            tmp ^= AesGetRcon(i/(KeyLen/4));
        }
        else if((KeyLen/4) > 6 && i%(KeyLen/4) == 4)
        {
            AesSubWord(tmp);
        }
        *pDword = *(pDword-KeyLen/4) ^ tmp;
    }

    //modify key schedule if is de-ciphering
    for(round=1, pDword=(UINT32*)(pExKey+AES_BLK_BYTES); round<maxRound && IsIlw; round++, pDword += AES_BLK_DWORDS)
        AesIlwMixCol((UINT08*)pDword);

    return OK;
}

RC Crypto::AesSubWord(UINT32 &Word)
{
    UINT32 i;
    UINT08* pByte = (UINT08*)&Word;

    for(i=0; i<4; i++, pByte++)
    {
        //updating each byte by table lookup in S-Matrix
        UINT32 x = (*pByte)>>4&0x0f;
        UINT32 y = (*pByte)&0x0f;

        (*pByte) = Sbox[x][y];
    }

    //Printf(Tee::PriDebug, "\tAfter SubWord: 0x[%02x %02x %02x %02x]\n",
    //       Word&0xff, (Word>>8)&0xff, (Word>>16)&0xff, (Word>>24)&0xff);

    return OK;
}

RC Crypto::AesRotWord(UINT32 &Word)
{
    UINT32 tmpWord = 0;
    tmpWord = Word << 24;
    Word >>= 8;
    Word |= tmpWord;

    //Printf(Tee::PriDebug, "\tAfter RotWord: 0x[%02x %02x %02x %02x]\n",
    //       Word&0xff, (Word>>8)&0xff, (Word>>16)&0xff, (Word>>24)&0xff);

    return OK;
}

UINT32 Crypto::AesGetRcon(UINT32 Index)
{
    UINT32 rcon = 0;
    MASSERT(Index>0);

    rcon = Gf8Power(0x02, Index-1);

    //Printf(Tee::PriDebug, "\tRcon[%i]: 0x[%02x %02x %02x %02x]\n", Index,
    //       rcon&0xff, (rcon>>8)&0xff, (rcon>>16)&0xff, (rcon>>24)&0xff);

    return rcon;
}

//AES Encryption routines
RC Crypto::AesCipher(const UINT08* pText, UINT08 *pCipher, const UINT08 *pExKey, UINT32 KeyLen)
{
    RC rc;
    const UINT32 *pRoundKey = (const UINT32*)pExKey;
    UINT32 round, maxRound;

    switch(KeyLen)
    {
        case sizeof(AESKey128):
            maxRound = AES_BLK_NR128;
            break;
        case sizeof(AESKey192):
            maxRound = AES_BLK_NR192;
            break;
        case sizeof(AESKey256):
            maxRound = AES_BLK_NR256;
            break;
        default:
            maxRound = 0;
            break;
    }

    if(!maxRound)
    {
        Printf(Tee::PriError, "Encryption KeyLen is not within supported list.\n");
        return RC::SOFTWARE_ERROR;
    }
    if(!pText || !pCipher || !pExKey)
    {
        Printf(Tee::PriError, "Input pointer pText/pCipher/pExKey invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    memcpy(pCipher, pText, AES_BLK_BYTES);

    //Printf(Tee::PriDebug, "Init State:\n");
    //Memory::Print08(pCipher, AES_BLK_BYTES, Tee::PriDebug);
    //Printf(Tee::PriDebug, "RoundKey 0:\n");
    //Memory::Print08((void*)pRoundKey, AES_BLK_BYTES, Tee::PriDebug);

    //Printf(Tee::PriDebug, "Round 0 State:\n");
    CHECK_RC(AesAddRoundKey(pCipher, pRoundKey)); // See Sec. 5.1.4 in FIPS-197

    for(round=1, pRoundKey += AES_BLK_DWORDS; round < maxRound; round++, pRoundKey += AES_BLK_DWORDS)
    {

        //Printf(Tee::PriDebug, "RoundKey %i:\n", round);
        //Memory::Print08((void*)pRoundKey, AES_BLK_BYTES, Tee::PriDebug);

        CHECK_RC(AesSubBytes(pCipher)); // See Sec. 5.1.1 in FIPS-197
        CHECK_RC(AesShiftRows(pCipher)); // See Sec. 5.1.2 in FIPS-197
        CHECK_RC(AesMixCol(pCipher)); // See Sec. 5.1.3 in FIPS-197

        //Printf(Tee::PriDebug, "Round %i State:\n", round);
        CHECK_RC(AesAddRoundKey(pCipher, pRoundKey));
    }

    //Printf(Tee::PriDebug, "Last RoundKey:\n");
    //Memory::Print08((void*)pRoundKey, AES_BLK_BYTES, Tee::PriDebug);

    CHECK_RC(AesSubBytes(pCipher));
    CHECK_RC(AesShiftRows(pCipher));

    //Printf(Tee::PriDebug, "Output State:\n");
    CHECK_RC(AesAddRoundKey(pCipher, pRoundKey));

    return OK;
}

RC Crypto::AesSubBytes(UINT08 *pState)
{
    UINT32 i;
    //UINT08 *pStart = pState;

    if(!pState)
    {
        Printf(Tee::PriError, "Input pointer pState invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    for(i=0; i<AES_BLK_BYTES; pState++, i++)
    {
        //updating each byte by table lookup in S-Matrix
        UINT32 x = (*pState)>>4&0x0f;
        UINT32 y = (*pState)&0x0f;
        (*pState) = Sbox[x][y];
    }

    //Printf(Tee::PriDebug, "\tAfter SubBytes:\n");
    //Memory::Print08(pStart, AES_BLK_BYTES, Tee::PriDebug);

    return OK;
}

RC Crypto::AesShiftRows(UINT08 *pState)
{
    UINT32 i, c;
    //UINT08 *pStart = pState;

    //ith row in pState: pState[i],pState[i+4],...,pState[i+4*x],...

    if(!pState)
    {
        Printf(Tee::PriError, "Input pointer pState invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    for(i=1, pState+=1; i<4; i++, pState++)
    {
        UINT08 tmpBytes[AES_BLK_DWORDS];
        for(c=0; c<AES_BLK_DWORDS; c++)
            tmpBytes[c] = pState[4*((c+i)%AES_BLK_DWORDS)];
        for(c=0; c<AES_BLK_DWORDS; c++)
            pState[4*c] = tmpBytes[c];
    }

    //Printf(Tee::PriDebug, "\tAfter ShiftRows:\n");
    //Memory::Print08(pStart, AES_BLK_BYTES, Tee::PriDebug);

    return OK;
}

RC Crypto::AesMixCol(UINT08 *pState)
{
    UINT32 i, c;
    //UINT08 *pStart=pState;

    //ith col in pState: pState[4*i+0],...,pState[4*i+3]

    if(!pState)
    {
        Printf(Tee::PriError, "Input pointer pState invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    for(c=0; c<AES_BLK_DWORDS; c++, pState+=4)
    {
        //callwlate 4 elements on the same column
        UINT08 bytes[4];
        for(i=0; i<4; i++)
        {
            bytes[i] = 0;
            UINT32 j;
            for(j=0; j<4; j++)
                bytes[i] ^= Gf8Multiplier(McolMat[(j-i+3)%4],*(pState+j));
        }
        //assigning bytes in column c
        for(i=0; i<4; i++)
            *(pState+i) = bytes[i];
    }

    //Printf(Tee::PriDebug, "\tAfter MixCol:\n");
    //Memory::Print08(pStart, AES_BLK_BYTES, Tee::PriDebug);

    return OK;
}

RC Crypto::AesAddRoundKey(UINT08 *pState, const UINT32 *pRoundKey)
{
    UINT32 i;

    if(!pState || !pRoundKey)
    {
        Printf(Tee::PriError, "Input pointer pState/pRoundKey invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    //each column of pState is a DWORD to XOR with the *pRoundKey
    for(i=0; i<AES_BLK_DWORDS; i++)
        *(UINT32*)(pState+4*i) ^= *(pRoundKey+i);

    //Printf(Tee::PriDebug, "\tAfter AddRoundKey:\n");
    //Memory::Print08(pState, AES_BLK_BYTES, Tee::PriDebug);

    return OK;
}

//AES Decryption routines
RC Crypto::AesIlwCipher(const UINT08* pCipher, UINT08 *pText, const UINT08 *pExKey, UINT32 KeyLen)
{
    //using the more efficient EqIlwCipher routine in Sec. FIPS-197
    RC rc;
    const  UINT32 *pRoundKey = (const UINT32*)pExKey;
    UINT32 round, maxRound;

    switch(KeyLen)
    {
        case sizeof(AESKey128):
            maxRound = AES_BLK_NR128;
            break;
        case sizeof(AESKey192):
            maxRound = AES_BLK_NR192;
            break;
        case sizeof(AESKey256):
            maxRound = AES_BLK_NR256;
            break;
        default:
            maxRound = 0;
            break;
    }

    if(!maxRound)
    {
        Printf(Tee::PriError, "Encryption KeyLen is not within supported list.\n");
        return RC::SOFTWARE_ERROR;
    }
    if(!pText || !pCipher || !pExKey)
    {
        Printf(Tee::PriError, "Input pointer pText/pCipher/pExKey invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    memcpy(pText, pCipher, AES_BLK_BYTES);

    pRoundKey = (const UINT32*)(pExKey + maxRound*AES_BLK_BYTES);

    //Printf(Tee::PriDebug, "Init State:\n");
    //Memory::Print08(pText, AES_BLK_BYTES, Tee::PriDebug);
    //Printf(Tee::PriDebug, "RoundKey 0:\n");
    //Memory::Print08((void*)pRoundKey, AES_BLK_BYTES, Tee::PriDebug);

    //Printf(Tee::PriDebug, "Round 0 State:\n");
    CHECK_RC(AesAddRoundKey(pText, pRoundKey));

    for(round=maxRound-1, pRoundKey -= AES_BLK_DWORDS; round>0; round--, pRoundKey -= AES_BLK_DWORDS)
    {
        //Printf(Tee::PriDebug, "RoundKey %i:\n", maxRound - round);
        //Memory::Print08((void*)pRoundKey, AES_BLK_BYTES, Tee::PriDebug);

        CHECK_RC(AesIlwSubBytes(pText));
        CHECK_RC(AesIlwShiftRows(pText));
        CHECK_RC(AesIlwMixCol(pText));

        //Printf(Tee::PriDebug, "Round %i State:\n", maxRound - round);
        CHECK_RC(AesAddRoundKey(pText, pRoundKey));
    }

    //Printf(Tee::PriDebug, "Last RoundKey:\n");
    //Memory::Print08((void*)pRoundKey, AES_BLK_BYTES, Tee::PriDebug);

    CHECK_RC(AesIlwSubBytes(pText));
    CHECK_RC(AesIlwShiftRows(pText));

    //Printf(Tee::PriDebug, "Output State:\n");
    CHECK_RC(AesAddRoundKey(pText, pRoundKey));

    return OK;
}

RC Crypto::AesIlwSubBytes(UINT08 *pState)
{
    UINT32 i;
    //UINT08 *pStart = pState;

    if(!pState)
    {
        Printf(Tee::PriError, "Input pointer pState invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    for(i=0; i<AES_BLK_BYTES; pState++, i++)
    {
        //updating each byte by table lookup in S-Matrix
        UINT32 x = (*pState)>>4&0x0f;
        UINT32 y = (*pState)&0x0f;
        (*pState) = IlwSbox[x][y];
    }

    //Printf(Tee::PriDebug, "\tAfter IlwSubBytes:\n");
    //Memory::Print08(pStart, AES_BLK_BYTES, Tee::PriDebug);

    return OK;
}

RC Crypto::AesIlwShiftRows(UINT08 *pState)
{
    UINT32 i, c;
    //UINT08 *pStart=pState;

    if(!pState)
    {
        Printf(Tee::PriError, "Input pointer pState invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    for(i=1, pState+=1; i<4; i++, pState++)
    {
        UINT08 tmpBytes[AES_BLK_DWORDS];
        for(c=0; c<AES_BLK_DWORDS; c++)
            tmpBytes[c] = pState[4*((c-i+AES_BLK_DWORDS)%AES_BLK_DWORDS)];
        for(c=0; c<AES_BLK_DWORDS; c++)
            pState[4*c] = tmpBytes[c];
    }

    //Printf(Tee::PriDebug, "\tAfter IlwShiftRows:\n");
    //Memory::Print08(pStart, AES_BLK_BYTES, Tee::PriDebug);

    return OK;
}

RC Crypto::AesIlwMixCol(UINT08 *pState)
{
    UINT32 c, i;
    //UINT08 *pStart = pState;

    if(!pState)
    {
        Printf(Tee::PriError, "Input pointer pState invalid.\n");
        return RC::SOFTWARE_ERROR;
    }

    for(c=0; c<AES_BLK_DWORDS; c++, pState+=4)
    {
        //callwlate 4 elements on the same column
        UINT08 bytes[4];
        for(i=0; i<4; i++)
        {
            bytes[i] = 0;
            UINT32 j;
            for(j=0; j<4; j++)
                bytes[i] ^= Gf8Multiplier(IlwMcolMat[(j-i+3)%4],*(pState+j));
        }
        //assigning bytes in column c
        for(i=0; i<4; i++)
            *(pState+i) = bytes[i];
    }

    //Printf(Tee::PriDebug, "\tAfter IlwMixCol:\n");
    //Memory::Print08(pStart, AES_BLK_BYTES, Tee::PriDebug);

    return OK;
}

