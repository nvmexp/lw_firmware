/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   gfe_bigint.c
 * @brief  Functions that use the bigint library
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "bigint.h"
#include "sha256.h"
#include "gfe/gfe_mthds.h"

static void _gfeSwapEndianness(void* pOutData, const void* pInData,
                                const LwU32 size)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeSwapEndianness");

// 
// Swapping endianness, works in place or out-of-place
// 
void
_gfeSwapEndianness
(
    void* pOutData,
    const void* pInData,
    const LwU32 size
)
{
    LwU32 i;

    for (i = 0; i < size / 2; i++)
    {
        LwU8 b1 = ((LwU8*)pInData)[i];
        LwU8 b2 = ((LwU8*)pInData)[size - 1 - i];
        ((LwU8*)pOutData)[i]            = b2;
        ((LwU8*)pOutData)[size - 1 - i] = b1;
    }
}

/* Defines to reuse the same buffers in DMEM */
#define qModulus        qnModulus
#define nModulus        qnModulus
#define dwPExponent     dwPQExponent
#define dwQExponent     dwPQExponent
#define ekmModP         buf_x
#define ekmModQ         buf_x
#define diff            buf_x
#define qh              buf_x
#define m1              buf_y
#define qIlwModP        buf_y
#define Q               buf_y
#define enc_km          buf_z
#define m2              buf_z
#define h               buf_w

// Decryption of Km: RSA-OAEP
// Ekm is input buffer
// Km is the output buffer
void
gfeRsaSign
(
    const LwU8 *p,
    const LwU8 *q,
    const LwU8 *dP,
    const LwU8 *dQ,
    const LwU8 *qIlw,
    const LwU8 *Ekm,
    LwU8 *Km,
    const LwU8 *n
)
{
    BigIntModulus pModulus;
    BigIntModulus qnModulus;
    LwU32 dwPQExponent[GFE_SIZE_RX_PRIV_EXPONENT_32];
    LwU8 buf_x[RSAOAEP_BUFFERSIZE];
    LwU8 buf_y[RSAOAEP_BUFFERSIZE];
    LwU8 buf_z[RSAOAEP_BUFFERSIZE];
    LwU8 buf_w[RSAOAEP_BUFFERSIZE];

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));

    memset(m1, 0x0, RSAOAEP_BUFFERSIZE);
    memset(ekmModP, 0x0, RSAOAEP_BUFFERSIZE);
    bigIntModulusInit(&pModulus, p, GFE_SIZE_PRIME_8);
    memcpy((LwU8 *)dwPExponent, dP, GFE_SIZE_RX_PRIV_EXPONENT_8);
    _gfeSwapEndianness(dwPExponent, dwPExponent, GFE_SIZE_RX_PRIV_EXPONENT_8);
    _gfeSwapEndianness(enc_km, Ekm, RSAOAEP_BUFFERSIZE);
    bigIntMod((LwU32 *)ekmModP, (LwU32 *)enc_km, &pModulus, GFE_SIZE_RX_MODULUS_32);
    bigIntPowerMod((LwU32*)m1, (LwU32*)ekmModP, dwPExponent, &pModulus, GFE_SIZE_RX_PRIV_EXPONENT_32);

    memset(ekmModQ, 0x0, RSAOAEP_BUFFERSIZE);
    bigIntModulusInit(&qModulus, q, GFE_SIZE_PRIME_8);
    memcpy((LwU8 *)dwQExponent, dQ, GFE_SIZE_RX_PRIV_EXPONENT_8);
    _gfeSwapEndianness(dwQExponent, dwQExponent, GFE_SIZE_RX_PRIV_EXPONENT_8);
    bigIntMod((LwU32 *)ekmModQ, (LwU32 *)enc_km, &qModulus, GFE_SIZE_RX_MODULUS_32);
    memset(m2, 0x0, RSAOAEP_BUFFERSIZE);
    bigIntPowerMod((LwU32*)m2, (LwU32*)ekmModQ, dwQExponent, &qModulus, GFE_SIZE_RX_PRIV_EXPONENT_32);

    memset(diff, 0x0, RSAOAEP_BUFFERSIZE);
    bigIntSubtractMod((LwU32 *)diff, (LwU32 *)m1, (LwU32 *)m2, &pModulus);
    
    memset(h, 0x0, RSAOAEP_BUFFERSIZE);
    memset(qIlwModP, 0x0, RSAOAEP_BUFFERSIZE);
    memcpy((LwU8 *)qIlwModP, qIlw, GFE_SIZE_RX_PRIV_EXPONENT_8);
    _gfeSwapEndianness(qIlwModP, qIlwModP, GFE_SIZE_RX_PRIV_EXPONENT_8);
    bigIntMultiplyMod((LwU32 *)h, (LwU32 *)diff, (LwU32 *)qIlwModP, &pModulus);
    
    memset(Q, 0x0, RSAOAEP_BUFFERSIZE);
    memset(qh, 0x0, RSAOAEP_BUFFERSIZE);
    memcpy((LwU8 *)Q, q, GFE_SIZE_RX_PRIV_EXPONENT_8);
    _gfeSwapEndianness(Q, Q, GFE_SIZE_RX_PRIV_EXPONENT_8); 
    bigIntModulusInit(&nModulus, n, GFE_SIZE_RX_MODULUS_8);
    bigIntMultiplyMod((LwU32 *)qh, (LwU32 *)h, (LwU32 *)Q, &nModulus);     
    bigIntAdd((LwU32 *)Km, (LwU32 *)qh, (LwU32 *)m2, GFE_SIZE_RX_MODULUS_32);
    _gfeSwapEndianness(Km, Km, RSAOAEP_BUFFERSIZE);

    // Clear stack since we copied part of the RSA key into stack variables
    memset(buf_x, 0, sizeof(buf_x));
    memset(buf_y, 0, sizeof(buf_y));
    memset(buf_z, 0, sizeof(buf_z));
    memset(buf_w, 0, sizeof(buf_w));
    memset((LwU8*)&pModulus, 0, sizeof(BigIntModulus));
    memset((LwU8*)&qnModulus, 0, sizeof(BigIntModulus));
    memset((LwU8*)dwPQExponent, 0, sizeof(dwPQExponent));

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(libBigInt));

    return;
}
