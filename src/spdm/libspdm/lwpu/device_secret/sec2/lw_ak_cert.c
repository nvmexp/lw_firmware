/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libSPDM copyright, as file is LW-authored uses on libSPDM code.

/*!
 * @file   lw_ak_cert.c
 * @brief  Create and fill in AK certificate.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "lwtypes.h"
#include "lw_utils.h"
/* ------------------------- Application Includes --------------------------- */
#include "lw_crypt.h"
#include "lw_device_secret.h"
#include "spdm_device_secret_lib.h"
#include "lw_apm_rts.h"
#include "seapi.h"
#include "x509_cert/x509_ak_nonca_cert.h"
#include "sign_cert.h"

/* ------------------------ Macros and Defines ----------------------------- */
/*
 * Struct used for ECDSA sign or ECDSA verify
 * Simular to bootrom_api_parameters.h which we don't want to include here.
 * All numbers are little-endian in a word.
 */
typedef struct
{
    LwU32 hash[ECC_P256_INTEGER_SIZE_IN_DWORDS];  // little-endian in a word
} ECC_P256_HASH;

typedef struct
{
    LwU32 privKey[ECC_P256_PRIVKEY_SIZE_IN_DWORDS];  // little-endian in a word
} ECC_P256_PRIVKEY;

typedef struct
{
    LwU32 R[ECC_P256_INTEGER_SIZE_IN_DWORDS];  // little-endian in a word
    LwU32 S[ECC_P256_INTEGER_SIZE_IN_DWORDS];  // little-endian in a word
} ECC_P256_SIGNATURE;

typedef struct
{
    LwU32 Qx[ECC_P256_INTEGER_SIZE_IN_DWORDS];  // little-endian in a word
    LwU32 Qy[ECC_P256_INTEGER_SIZE_IN_DWORDS];  // little-endian in a word
} ECC_P256_PUBKEY;

#define  SIG_SIZE_IN_BYTES   (ECC_P256_INTEGER_SIZE_IN_BYTES * 2) 

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS _spdm_fill_AK_cert(Certificate_0 *pCert, LwU32 *publicAk, LwU32 publicAkSize)
    GCC_ATTRIB_SECTION("imem_spdm", "_spdm_fill_AK_cert");

/* ------------------------- Global Variables ------------------------------- */
LwU32 g_publicEK[ECC_P256_PUBKEY_SIZE_IN_DWORDS]
    GCC_ATTRIB_SECTION("dmem_spdm", "g_publicEK");

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief Writes a single Integer in Asn1 format.
 *    This is modeled after funciton Put_Asn1_Integer in:
 *    \hw\gpu_ip\unit\falcon\6.0_ghlit1\defs\rv_brom\spark\common\hal\rv_x509.adb
 *
 * @param[in]  intData      Pointer to integer to write to ASN1 container.
 * @param[in]  container    Pointer to ANS1 container to write integer into.
 * @param[out] totalLen     Alwtal length written to Asn1 container.
 *
 * @return LW_TRUE if integer written successfully, LW_FALSE otherwise.
 */
boolean
put_asn1_integer
(
    LwU8   *intData,        // Int       => Sig.R,
    LwU8   *container,      // Container => Sig_Integer_Container ( Sig_Asn1_Data (Sig_Asn1_Len .. Sig_Integer_Container'Last + Sig_Asn1_Len)),
    LwU32  *totalLen        // Total_Len => Container_Len);
)
{
    LwU32   startPos;
    LwU32   intLen;
    LwU32   i;
    boolean status = LW_TRUE;

    startPos = 0;
    intLen   = SIG_SIZE_IN_BYTES / 2;

    // Find the first non '00' byte position
    for (i = 0 ;  i < SIG_SIZE_IN_BYTES / 2 ; i++)
    {
        if (intData[i] != 0)
        {
            startPos = i;
            break;
        }
    }

    //
    // If it's a negative number, we need to append 0 in front of it.
    // see rfc5280 Appendix B.  ASN.1 Notes
    //
    if (intData[startPos] > 127)
    { // Neg Number
        container[0] = 0x02;
        container[1] = (LwU8) (intLen - startPos+1);
        container[2] = 0x00;
        for (i = startPos ; i < intLen  ;  i++)
            container[i + 3 - startPos] = intData[i];
        *totalLen = 3 + intLen - startPos;
    } // Neg Number
    else
    { // Pos Number
        container[0] = 0x02;
        container[1] = (LwU8) (intLen - startPos);
        for (i = startPos ; i < intLen  ;  i++)
            container[i + 2 - startPos] = intData[i];
        *totalLen = 2 + intLen - startPos;
    } // Pos Number

    return status;
}

/*!
 * @brief Fills in the certificates signature in ASN1 format.
 *    This is modeled after funciton Fill_Cert_Sig (and some Sign_Ak_Ca_Cert) in:
 *    \hw\gpu_ip\unit\falcon\6.0_ghlit1\defs\rv_brom\spark\common\hal\rv_x509.adb
 *
 * @param[in]  pCert            Pointer to AK certificate structure.
 * @param[in]  pSignature       Pointer to signature to write to cert.
 * @param[out] sigAsn1Data      Container in the cert that sig is written to.
 * @param[in]  sizeAnsiData     Size of ASN1 container not to be exceeded.
 * @param[out] pSigAsn1Length   Alwtal length written to ASN1 container.
 *
 * @return LW_TRUE if signature written successfully, LW_FALSE otherwise.
 */
boolean
fill_cert_sig_asn1
(
    Certificate_0  *pCert0,
    LwU8           *pSignature,
    LwU8           *sigAsn1Data,
    LwU32           sizeAsn1Data,
    LwU32          *pSigAsn1Length,
    LwU32          *neededExtraBytes
)
{
    LwU32       i;
    LwU32       containerLength;
    LwS32       sigAsn1LenDiffer;
    LwU8        origSigFieldSize;

    // Bit string padding
    sigAsn1Data[0]  = 0;
    // Sequence tag
    sigAsn1Data[1]  = 0x30;
    // Sequence len
    sigAsn1Data[2]  = 0;

    // Track length used.
    *pSigAsn1Length = 3;
    origSigFieldSize = pCert0->Val2.Len;

    put_asn1_integer
        (
            pSignature,         // Int       => Sig.R,
            &sigAsn1Data[3],    // Container => Sig_Integer_Container ( Sig_Asn1_Data (Sig_Asn1_Len .. Sig_Integer_Container'Last + Sig_Asn1_Len)),
            &containerLength    // Total_Len => Container_Len);
        );
    *pSigAsn1Length = *pSigAsn1Length + containerLength;

    put_asn1_integer
        (
            pSignature + (SIG_SIZE_IN_BYTES/2),     // Int       => Sig.S,
            &sigAsn1Data[*pSigAsn1Length],          // Container => Sig_Integer_Container (Sig_Asn1_Data (Sig_Asn1_Len .. Sig_Integer_Container'Last + Sig_Asn1_Len)),
            &containerLength                        // Total_Len => Container_Len);
        );
    *pSigAsn1Length = *pSigAsn1Length + containerLength;

    // Adjust Sequence Len
    sigAsn1Data[2] = (LwU8)*pSigAsn1Length - 3;

    // scrub garbage data at the  tail of signature
    for (i = *pSigAsn1Length; i < sizeAsn1Data; i++)
    {
        sigAsn1Data[i] = 0;
    }

    //
    // back to code in Sign_Ak_Ca_Cert
    //
    //if (pCert0->Val2.Len != 105)  // CPB is 104??
    sigAsn1LenDiffer = pCert0->Val2.Len - *pSigAsn1Length;
    if (sigAsn1LenDiffer > 98  && sigAsn1LenDiffer < 0xFFFE)
    {
        return LW_FALSE;
    }
    // Update actual length.
    pCert0->Val2.Len = (LwU8)*pSigAsn1Length;

    // Ak_Cert_Nonca_Rec.Len (2) := Ak_Cert_Nonca_Rec.Len (2) - Sig_Asn1_Len_Differ;
    pCert0->Len[2] = pCert0->Len[2] - (LwU8)sigAsn1LenDiffer;

    // May need to adjust the size of the entire cert, if the field grew
    *neededExtraBytes = 0;
    if (*pSigAsn1Length > origSigFieldSize)
    {
        *neededExtraBytes = *pSigAsn1Length - origSigFieldSize;
    }
    
    return LW_TRUE;
}

/*!
 * @brief Fills in many of the Certificate fields, such as cert serial number,
 *        Issuer.sn, AK Public key, and Authority Key Identifier.
 *    This code is modeled after funciton Fill_Ak_Nonca_TbsCert in:
 *    \hw\gpu_ip\unit\falcon\6.0_ghlit1\defs\rv_brom\spark\common\hal\rv_x509.adb
 *    The rather odd accessor funcitons (such as pCert->Val0.Val1.Val0) come from that 
 *    and the generated include file x509_cert/x509_ak_nonca_cert.h.
 *                 
 * @param[in] pCert           Pointer to AK certificate structure.
 * @param[in] publicAk       Public AK, must have size ECC_P256_PUBKEY_SIZE_IN_DWORDS.
 * @param[in] publicAkSize  Size of Public_AK in bytes, must be = ECC_P256_PUBKEY_SIZE_IN_BYTES.

 *
 * @return FLCN_OK if AK cert created successfully, relevant error otherwise.
 */
static FLCN_STATUS
_spdm_fill_AK_cert
(
    Certificate_0  *pCert,
    LwU32           *publicAk,
    LwU32           publicAkSize
)
{
    LwU8            sha1Hash[32];
    FLCN_STATUS     status = FLCN_OK;
    LwU8            keyPlusOneByte[ECC_P256_PUBKEY_SIZE_IN_BYTES+1];

    // Validate have correct size pub key.
    if (publicAkSize != ECC_P256_PUBKEY_SIZE_IN_BYTES)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto _spdm_fill_AK_cert_exit;
    }

    // Get Public EK
    if (signCertGetEkPublic(g_publicEK) != FLCN_OK)
    {
        status = FLCN_ERROR;
        goto _spdm_fill_AK_cert_exit;
    }

    //
    // Compute SHA1 (04 || AK_pub_NonCA), for the next two fields (Issuer.sn, and cert serial number)
    //
    keyPlusOneByte[0] = 0x4;
    copy_mem(&keyPlusOneByte[1], publicAk, ECC_P256_PUBKEY_SIZE_IN_BYTES);
    if (!sha1_hash_all(&keyPlusOneByte[0], ECC_P256_PUBKEY_SIZE_IN_BYTES+1, &sha1Hash[0]))
    {
        status = FLCN_ERROR;
        goto _spdm_fill_AK_cert_exit;
    }

    //
    // Issuer.sn
    // SHA1 (04 || EK_Pub)
    // APM-TODO CONFCOMP-653: This will need to be matching Issuer.sn of EK certificate.
    // Lwrrently, leave as generated value, which matches for PID1.
    //
    // copy_mem(&pCert->Val0.Val3.Val0.Val0.Val1.Val0[0], &ekSha1Hash[0], SHA_1_HASH_SIZE_BYTE);
    
    //
    // AK Public key.
    // Subject.public_key = BitString(0||4||Key_Pub)
    // 0 means zero padding of bitstring
    // 4 means uncompressed key
    //
    pCert->Val0.Val6.Val1.Val0[0] = 0x00;
    pCert->Val0.Val6.Val1.Val0[1] = 0x04;
    copy_mem(&pCert->Val0.Val6.Val1.Val0[2], (LwU8 *)&publicAk[0], ECC_P256_PUBKEY_SIZE_IN_BYTES);
    
    //
    // cert serial number
    // SHA1 (04 || AK_pub_NonCA), uses SHA1 hash from above.
    // For sha1Hash[0], AND with 0x7F, then OR with 0x40. This will ensure
    // first byte is positive and nonzero, necessary for valid DER encoding.
    //
    // APM-TODO CONFCOMP-653: Update serial number here and in Subject field.
    // sha1Hash[0]  = (sha1Hash[0] & 0x7F) | 0x40;
    // copy_mem(&pCert->Val0.Val1.Val0[0], &sha1Hash[0], SHA_1_HASH_SIZE_BYTE);

    //
    // Authority Key Identifier
    // APM-TODO CONFCOMP-653: Fill in Authority Key Identifier.
    // For now, we have hardcoded in x509_ak_nonca_cert.h
    //

    //
    // TSG-DICE_FWID - broken up into two parts
    // TCG-DICE-FWID.CompositeDeviceID.SubjectPublicKeyInfo.ecPubKey = EK_priv * Gx || EK_priv * Gy
    // TCG-DICE-FWID.CompositeDeviceID.FWID.fwid = GH100 BROM implementation is SHA384 hash of various attributes of FMC ??????
    // TODO - this will ahve to be done in HS, as requires private EK.
    //

_spdm_fill_AK_cert_exit:
    return status;
}

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Completes the AK Certificate by:
 *            Filling in fields of the Certificate
 *            SHA 256 hash Certificate
 *            ECC Sign the Cert
 *    This code is modeled after funciton Fill_Ak_Nonca_TbsCert in:
 *    \hw\gpu_ip\unit\falcon\6.0_ghlit1\defs\rv_brom\spark\common\hal\rv_x509.adb
 *    The rather odd accessor funcitons (such as pCert->Val0.Val1.Val0) come from that 
 *    and the generated include file x509_cert/x509_ak_nonca_cert.h.
 *
 * @param[in/out] ppCertChain     Pointer to pointer to the certificate chain returned
 *                                   in *ppCertChain
 * @param[in/out] pCertChainSize  Pointer to size of certificate chain.   Size returned 
 *                                   in *pCertChainSize
 *
 * @return LW_TRUE if AK cert created successfully, LW_FALSE otherwise.
 */
boolean
spdm_generate_ak_certificate
(
    uint8  **ppCertChain,
    uint32  *pCertChainSize
)
{
    LwU32               publicAkSize = ECC_P256_PUBKEY_SIZE_IN_BYTES;
    Certificate_0      *pCert0 = (Certificate_0  *)&Ak_Cert_Structure.Fmc_X509_Ak_Nonca_Cert_Ro_Data;
    ECC_P256_SIGNATURE  signatureMSB;
    ECC_P256_SIGNATURE  signatureLSB;
    ECC_P256_HASH       sha256Hash;
    LwU32               publicAk[ECC_P256_PUBKEY_SIZE_IN_DWORDS];
    FLCN_STATUS         flcnStatus;
    boolean             status;
    LwU32               sigAnsLength;
    LwU32               neededExtraBytes;

    status = LW_FALSE;

    // Get info which will be needed to fill fields.
    if (!spdm_device_secret_get_public_ak((uint8 *)&publicAk, &publicAkSize))
    {
        goto spdm_generate_ak_certificate_exit;
    }

    // Fill in fields of the Ak cert.
    if (_spdm_fill_AK_cert(pCert0, publicAk, publicAkSize) != FLCN_OK)
    {
        goto spdm_generate_ak_certificate_exit;
    }

    // Compute SHA256 digest of the certificate and put in spdm header (root_hash)
    if (!sha256_hash_all(pCert0, Certificate_0_Len_In_Byte, (LwU8 *)&sha256Hash.hash[0]))
    {
        goto spdm_generate_ak_certificate_exit;
    }
    copy_mem(Ak_Cert_Structure.root_hash, &sha256Hash.hash[0], sizeof(Ak_Cert_Structure.root_hash));

    // Fill in size of the certificate and put in spdm header (length)
    Ak_Cert_Structure.length = Certificate_0_Len_In_Byte;

    //
    // Sign the Cert with EK private. Will have to be HS, unless SE engine can use directly from SE keyslot.
    // For certificate sign, just want to hash the actual certificate
    //
    if (!sha256_hash_all(&(pCert0->Val0), TbsCertificate_0_0_Len_In_Byte, (LwU8 *)&sha256Hash.hash[0]))
    {
        goto spdm_generate_ak_certificate_exit;
    }
    
    // Swap endianess on hash
    if (!change_endianness((LwU8 *)&sha256Hash.hash[0],
                           (LwU8 *)&sha256Hash.hash[0],
                           ECC_P256_INTEGER_SIZE_IN_BYTES))
    {
        goto spdm_generate_ak_certificate_exit;
    }

    flcnStatus = signCertEkSignHash((LwU32 *)&sha256Hash.hash[0], (LwU32 *)&signatureLSB);
    if (flcnStatus != FLCN_OK)
    {
        goto spdm_generate_ak_certificate_exit;
    }
    
    // Fill in Cert Signature, comes from SE as LSB, so reverse to temp.
    if (!change_endianness((LwU8 *)&signatureMSB.R[0],
                           (LwU8 *)&signatureLSB.R[0],
                           ECC_P256_INTEGER_SIZE_IN_BYTES))
    {
        goto spdm_generate_ak_certificate_exit;
    }
    if (!change_endianness((LwU8 *)&signatureMSB.S[0],
                           (LwU8 *)&signatureLSB.S[0],
                           ECC_P256_INTEGER_SIZE_IN_BYTES))
    {
        goto spdm_generate_ak_certificate_exit;
    }
    if (!fill_cert_sig_asn1(pCert0, (LwU8 *)&signatureMSB.R[0], &pCert0->Val2.Val0[0],
                            sizeof(pCert0->Val2.Val0), &sigAnsLength, &neededExtraBytes))
    {
        goto spdm_generate_ak_certificate_exit;
    }
    
    //
    // Check size if certificate had to be enlarged.
    // Note:
    // If we have a template certificate where both signature halves (R & S)
    // start with a non-negative byte, neither sig halve will be prepended with extra zero
    // and will be two bytes shorter than the maximum size a signature will be when it has
    // both sig halves starting with a negative byte (so both halves would be prepended with 
    // extra zero).
    // Therefore, the most the cert should ever have to "grow" is two bytes.  Verify addional
    // bytes needed and also check not overflowing buffer.
    //
    if (neededExtraBytes > 2  ||  
        Certificate_0_Len_In_Byte + neededExtraBytes > sizeof(Ak_Cert_Structure.Fmc_X509_Ak_Nonca_Cert_Ro_Data))
    {
        goto spdm_generate_ak_certificate_exit;
    }

    // Set return pointer and size.
    *ppCertChain    = (uint8 *)&Ak_Cert_Structure;
    *pCertChainSize = Certificate_0_Len_In_Byte + OFFSET_OF(CERT_STRUCT, Fmc_X509_Ak_Nonca_Cert_Ro_Data) + 
                      neededExtraBytes;
    status = LW_TRUE;

spdm_generate_ak_certificate_exit:
    return status;
}
