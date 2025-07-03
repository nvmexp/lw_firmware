/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    hdcp22wired_aes.c
 * @brief   Library to implement AES functions for HDCP22.
 *
 */

// TODO: Change the file name to hdcp22wired_sw_aes.c

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_aes.h"

/* ------------------------ Function Definitions ---------------------------- */
#if !defined(HDCP22_USE_SCP_GEN_DKEY)
static void _aesMixColumns(LwU8 a[4][AES_MAXBC], LwU8 AES_BC)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAes", "_aesMixColumns");
static void _aesShiftRows(LwU8 a[4][AES_MAXBC], LwU8 d, LwU8 AES_BC)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAes", "_aesShiftRows");
static void _aesSubstitution(LwU8 a[4][AES_MAXBC], LwU8 box[256], LwU8 AES_BC)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAes", "_aesSubstitution");
static void _aesAddRoundKey(LwU8 a[4][AES_MAXBC], LwU8 rk[4][AES_MAXBC], LwU8 AES_BC)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAes", "_aesAddRoundKey");
static LwU8 _aesmul(LwU8 a, LwU8 b)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAes", "_aesmul");
static LwS32 _aesrijndaelKeySched(unsigned char k[4][AES_MAXKC], LwS32 keyBits, LwS32 blockBits, unsigned char W[AES_MAXROUNDS+1][4][AES_MAXBC])
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAes", "_aesrijndaelKeySched");
static LwS32 _aesrijndaelEncrypt(unsigned char a[4][AES_MAXBC], LwS32 keyBits, LwS32 blockBits, unsigned char rk[AES_MAXROUNDS+1][4][AES_MAXBC])
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAes", "_aesrijndaelEncrypt");

/* ------------------------ Static Variables -------------------------------- */
static LwU8 aesShifts[3][4][2] = {
    { { 0, 0 },
      { 1, 3 },
      { 2, 2 },
      { 3, 1 } },

    { { 0, 0 },
      { 1, 5 },
      { 2, 4 },
      { 3, 3 } },

    { { 0, 0 },
      { 1, 7 },
      { 3, 5 },
      { 4, 4 } }
};

static LwU8 aesLogtable[256] = {
      0,   0,  25,   1,  50,   2,  26, 198,  75, 199,  27, 104,  51, 238, 223,   3,
    100,   4, 224,  14,  52, 141, 129, 239,  76, 113,   8, 200, 248, 105,  28, 193,
    125, 194,  29, 181, 249, 185,  39, 106,  77, 228, 166, 114, 154, 201,   9, 120,
    101,  47, 138,   5,  33,  15, 225,  36,  18, 240, 130,  69,  53, 147, 218, 142,
    150, 143, 219, 189,  54, 208, 206, 148,  19,  92, 210, 241,  64,  70, 131,  56,
    102, 221, 253,  48, 191,   6, 139,  98, 179,  37, 226, 152,  34, 136, 145,  16,
    126, 110,  72, 195, 163, 182,  30,  66,  58, 107,  40,  84, 250, 133,  61, 186,
     43, 121,  10,  21, 155, 159,  94, 202,  78, 212, 172, 229, 243, 115, 167,  87,
    175,  88, 168,  80, 244, 234, 214, 116,  79, 174, 233, 213, 231, 230, 173, 232,
     44, 215, 117, 122, 235,  22,  11, 245,  89, 203,  95, 176, 156, 169,  81, 160,
    127,  12, 246, 111,  23, 196,  73, 236, 216,  67,  31,  45, 164, 118, 123, 183,
    204, 187,  62,  90, 251,  96, 177, 134,  59,  82, 161, 108, 170,  85,  41, 157,
    151, 178, 135, 144,  97, 190, 220, 252, 188, 149, 207, 205,  55,  63,  91, 209,
     83,  57, 132,  60,  65, 162, 109,  71,  20,  42, 158,  93,  86, 242, 211, 171,
     68,  17, 146, 217,  35,  32,  46, 137, 180, 124, 184,  38, 119, 153, 227, 165,
    103,  74, 237, 222, 197,  49, 254,  24,  13,  99, 140, 128, 192, 247, 112,   7,
};

static LwU8 aesAlogtable[256] = {
      1,   3,   5,  15,  17,  51,  85, 255,  26,  46, 114, 150, 161, 248,  19,  53,
     95, 225,  56,  72, 216, 115, 149, 164, 247,   2,   6,  10,  30,  34, 102, 170,
    229,  52,  92, 228,  55,  89, 235,  38, 106, 190, 217, 112, 144, 171, 230,  49,
     83, 245,   4,  12,  20,  60,  68, 204,  79, 209, 104, 184, 211, 110, 178, 205,
     76, 212, 103, 169, 224,  59,  77, 215,  98, 166, 241,   8,  24,  40, 120, 136,
    131, 158, 185, 208, 107, 189, 220, 127, 129, 152, 179, 206,  73, 219, 118, 154,
    181, 196,  87, 249,  16,  48,  80, 240,  11,  29,  39, 105, 187, 214,  97, 163,
    254,  25,  43, 125, 135, 146, 173, 236,  47, 113, 147, 174, 233,  32,  96, 160,
    251,  22,  58,  78, 210, 109, 183, 194,  93, 231,  50,  86, 250,  21,  63,  65,
    195,  94, 226,  61,  71, 201,  64, 192,  91, 237,  44, 116, 156, 191, 218, 117,
    159, 186, 213, 100, 172, 239,  42, 126, 130, 157, 188, 223, 122, 142, 137, 128,
    155, 182, 193,  88, 232,  35, 101, 175, 234,  37, 111, 177, 200,  67, 197,  84,
    252,  31,  33,  99, 165, 244,   7,   9,  27,  45, 119, 153, 176, 203,  70, 202,
     69, 207,  74, 222, 121, 139, 134, 145, 168, 227,  62,  66, 198,  81, 243,  14,
     18,  54,  90, 238,  41, 123, 141, 140, 143, 138, 133, 148, 167, 242,  13,  23,
     57,  75, 221, 124, 132, 151, 162, 253,  28,  36, 108, 180, 199,  82, 246,   1,
};

static LwU8 S[256] = {
     99, 124, 119, 123, 242, 107, 111, 197,  48,   1, 103,  43, 254, 215, 171, 118,
    202, 130, 201, 125, 250,  89,  71, 240, 173, 212, 162, 175, 156, 164, 114, 192,
    183, 253, 147,  38,  54,  63, 247, 204,  52, 165, 229, 241, 113, 216,  49,  21,
      4, 199,  35, 195,  24, 150,   5, 154,   7,  18, 128, 226, 235,  39, 178, 117,
      9, 131,  44,  26,  27, 110,  90, 160,  82,  59, 214, 179,  41, 227,  47, 132,
     83, 209,   0, 237,  32, 252, 177,  91, 106, 203, 190,  57,  74,  76,  88, 207,
    208, 239, 170, 251,  67,  77,  51, 133,  69, 249,   2, 127,  80,  60, 159, 168,
     81, 163,  64, 143, 146, 157,  56, 245, 188, 182, 218,  33,  16, 255, 243, 210,
    205,  12,  19, 236,  95, 151,  68,  23, 196, 167, 126,  61, 100,  93,  25, 115,
     96, 129,  79, 220,  34,  42, 144, 136,  70, 238, 184,  20, 222,  94,  11, 219,
    224,  50,  58,  10,  73,   6,  36,  92, 194, 211, 172,  98, 145, 149, 228, 121,
    231, 200,  55, 109, 141, 213,  78, 169, 108,  86, 244, 234, 101, 122, 174,   8,
    186, 120,  37,  46,  28, 166, 180, 198, 232, 221, 116,  31,  75, 189, 139, 138,
    112,  62, 181, 102,  72,   3, 246,  14,  97,  53,  87, 185, 134, 193,  29, 158,
    225, 248, 152,  17, 105, 217, 142, 148, 155,  30, 135, 233, 206,  85,  40, 223,
    140, 161, 137,  13, 191, 230,  66, 104,  65, 153,  45,  15, 176,  84, 187,  22,
};

static LwU32 rcon[30] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36, 0x6c, 0xd8,
    0xab, 0x4d, 0x9a, 0x2f, 0x5e, 0xbc, 0x63, 0xc6, 0x97, 0x35, 0x6a, 0xd4,
    0xb3, 0x7d, 0xfa, 0xef, 0xc5, 0x91,
};

/* ------------------------ Private Functions ------------------------------- */
/*
 * @brief  Returns the Multiple of 2 numbers.
 */
static LwU8
_aesmul
(
    LwU8 a,
    LwU8 b
)
{
    if (a && b)
    {
        LwU16 tmp = aesLogtable[a] + aesLogtable[b];
        return aesAlogtable[tmp%255];
    }
    else
    {
        return 0;
    }
}

static void
_aesAddRoundKey
(
    LwU8 a[4][AES_MAXBC],
    LwU8 rk[4][AES_MAXBC],
    LwU8 AES_BC
)
{
    LwS32 i;
    LwS32 j;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < AES_BC; j++)
        {
            a[i][j] ^= rk[i][j];
        }
    }
}

static void
_aesSubstitution
(
    LwU8 a[4][AES_MAXBC],
    LwU8 box[256],
    LwU8 AES_BC
)
{
    LwS32 i;
    LwS32 j;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < AES_BC; j++)
        {
            a[i][j] = box[a[i][j]];
        }
    }
}

static void
_aesShiftRows
(
    LwU8 a[4][AES_MAXBC],
    LwU8 d,
    LwU8 AES_BC
)
{
    LwU8    tmp[AES_MAXBC];
    LwU8    i;
    LwU8    j;

    for (i = 1; i < 4; i++)
    {
        for (j = 0; j < AES_BC; j++)
        {
            tmp[j] = a[i][(j + aesShifts[AES_SC][i][d]) % AES_BC];
        }
        for (j = 0; j < AES_BC; j++)
        {
            a[i][j] = tmp[j];
        }
    }
}

static void
_aesMixColumns
(
    LwU8 a[4][AES_MAXBC],
    LwU8 AES_BC
)
{

    LwU8 b[4][AES_MAXBC];
    LwS32 i;
    LwS32 j;

    for (j = 0; j < AES_BC; j++)
    {
        for (i = 0; i < 4; i++)
        {
            b[i][j] = _aesmul(2,a[i][j])
                        ^ _aesmul(3,a[(i + 1) % 4][j])
                        ^ a[(i + 2) % 4][j]
                        ^ a[(i + 3) % 4][j];
        }
    }

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < AES_BC; j++)
        {
            a[i][j] = b[i][j];
        }
    }
}

static LwS32
_aesrijndaelEncrypt
(
    LwU8 a[4][AES_MAXBC],
    LwS32 keyBits,
    LwS32 blockBits,
    LwU8 rk[AES_MAXROUNDS+1][4][AES_MAXBC]
)
{
    LwS32 r;
    LwS32 AES_BC;
    LwS32 ROUNDS;

    switch (blockBits)
    {
        case 128:
        {
            AES_BC = 4;
            break;
        }
        case 192:
        {
            AES_BC = 6;
            break;
        }
        case 256:
        {
            AES_BC = 8;
            break;
        }
        default:
        {
            return (-2);
        }
    }

    switch (keyBits >= blockBits ? keyBits : blockBits)
    {
        case 128:
        {
            ROUNDS = 10;
            break;
        }
        case 192:
        {
            ROUNDS = 12;
            break;
        }
        case 256:
        {
            ROUNDS = 14;
            break;
        }
        default :
        {
            return (-3);
        }
    }

    // Begin with a key addition
    _aesAddRoundKey(a, rk[0], AES_BC);

    // ROUNDS-1 ordinary rounds
    for (r = 1; r < ROUNDS; r++)
    {
        _aesSubstitution(a, S, AES_BC);
        _aesShiftRows(a, 0, AES_BC);
        _aesMixColumns(a, AES_BC);
        _aesAddRoundKey(a, rk[r], AES_BC);
    }

    // Last round is special: there is no _aesMixColumns
    _aesSubstitution(a, S, AES_BC);
    _aesShiftRows(a, 0, AES_BC);
    _aesAddRoundKey(a, rk[ROUNDS], AES_BC);
    return 0;
}

static LwS32
_aesrijndaelKeySched
(
    LwU8 k[4][AES_MAXKC],
    LwS32 keyBits,
    LwS32 blockBits,
    LwU8 W[AES_MAXROUNDS+1][4][AES_MAXBC]
)
{
    LwS32  KC;
    LwS16  AES_BC;
    LwS16  ROUNDS;
    LwS32  i;
    LwS32  j;
    LwS32  t;
    LwS32  rconpointer = 0;
    LwU8   tk[4][AES_MAXKC];

    switch (keyBits)
    {
        case 128:
        {
            KC = 4;
            break;
        }
        case 192:
        {
            KC = 6;
            break;
        }
        case 256:
        {
            KC = 8;
            break;
        }
        default:
        {
            return (-1);
        }
    }

    switch (blockBits)
    {
        case 128:
        {
            AES_BC = 4;
            break;
        }
        case 192:
        {
            AES_BC = 6;
            break;
        }
        case 256:
        {
            AES_BC = 8;
            break;
        }
        default:
        {
            return (-2);
        }
    }

    switch (keyBits >= blockBits ? keyBits : blockBits)
    {
        case 128:
        {
            ROUNDS = 10;
            break;
        }
        case 192:
        {
            ROUNDS = 12;
            break;
        }
        case 256:
        {
            ROUNDS = 14;
            break;
        }
    }

    for (j = 0; j < KC; j++)
    {
        for (i = 0; i < 4; i++)
        {
            tk[i][j] = k[i][j];
        }
    }

    t = 0;
    // Copy values LwS32o round key array.
    for (j = 0; (j < KC) && (t < (ROUNDS+1)*AES_BC); j++, t++)
    {
        for (i = 0; i < 4; i++)
        {
            W[t / AES_BC][i][t % AES_BC] = tk[i][j];
        }
    }

    while (t < (ROUNDS+1)*AES_BC)
    {
        //
        // While not enough round key material callwlated
        // callwlate new values.
        //
        for (i = 0; i < 4; i++)
        {
            tk[i][0] ^= S[tk[(i+1)%4][KC-1]];
        }
        tk[0][0] ^= rcon[rconpointer++];

        if (KC != 8)
        {
            for (j = 1; j < KC; j++)
            {
                for (i = 0; i < 4; i++)
                {
                    tk[i][j] ^= tk[i][j-1];
                }
            }
        }
        else
        {
            for (j = 1; j < KC/2; j++)
            {
                for (i = 0; i < 4; i++)
                {
                    tk[i][j] ^= tk[i][j-1];
                }
            }

            for (i = 0; i < 4; i++)
            {
                tk[i][KC/2] ^= S[tk[i][KC/2 - 1]];
            }

            for (j = KC/2 + 1; j < KC; j++)
            {
                for (i = 0; i < 4; i++)
                {
                    tk[i][j] ^= tk[i][j-1];
                }
            }
        }
        // Copy values LwS32o round key array.
        for (j = 0; (j < KC) && (t < (ROUNDS+1)*AES_BC); j++, t++)
        {
            for (i = 0; i < 4; i++)
            {
                W[t / AES_BC][i][t % AES_BC] = tk[i][j];
            }
        }
    }
    return 0;
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * hdcp22AesGenerateDkey128
 *
 * @brief Encrypts the Message with the Key
 *
 * @param[out] pEncryptMessage   The encrypted Message
 * @param[in]  pMessage          The message to be encrypted
 * @param[in]  pSkey             The Key with which message is to be encrypted
 * @returns    FLCN_STATUS       FLCN_OK on successfull exelwtion
 *                               Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22AesGenerateDkey128
(
    LwU8 *pMessage,
    LwU8 *pSkey,
    LwU8 *pEncryptMessage,
    LwU32 size
)
{
    LwU8        k[4][AES_MAXKC];
    LwU8        keySched[AES_MAXROUNDS+1][4][AES_MAXBC];
    LwU32       i;
    LwU8        j;
    LwU32       t;
    LwU8        block[4][AES_MAXBC];
    FLCN_STATUS status = FLCN_OK;

    swapEndianness((LwU8*)(pMessage), (LwU8*)(pMessage), HDCP22_SIZE_DKEY);
    swapEndianness((LwU8*)(pSkey), (LwU8*)(pSkey), HDCP22_SIZE_DKEY);

    for (i=0; i<16; i++)
    {
        k[i%4][i/4] = pSkey[15 - i];
    }

    if((_aesrijndaelKeySched(k, 128, 128, keySched)) < 0)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    for (i=0; i<size/16; i++)
    {
        for( j=0;j<4;j++)
        {
            for( t=0; t<4; t++)
            {
                block[t][j] = pMessage[16*i+(15 - (4*j+t))] & 0xFF;
            }
        }

        if((_aesrijndaelEncrypt(block, 128, 128, keySched)) < 0)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
        }

        for (j=0; j<4; j++)
        {
            for(t=0; t<4; t++)
            {
                pEncryptMessage[16*i+(15 - (4*j+t))] = (unsigned char)block[t][j];
            }
        }
    }

    swapEndianness((LwU8*)(pEncryptMessage), (LwU8*)(pEncryptMessage), HDCP22_SIZE_DKEY);

    return status;
}
#endif

