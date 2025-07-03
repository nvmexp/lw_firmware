/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @note    This file is branched directly from the VBIOS tree for data
 *          structures that need to be shared between VBIOS microcodes and
 *          driver microcodes. Check the P4 file history to see from where it
 *          was branched.
 *
 *          This means that the file should almost never be directly modified.
 *          If updates to this file are needed for new functionality, changes
 *          to the original source file from the VBIOS tree should be merged
 *          into this file.
 *
 *          Additional definitions that are common but that are not pulled
 *          directly from the VBIOS headers should be placed in addendum files.
 */

#if !defined(__CERT20_H__INCLUDED_)
#define __CERT20_H__INCLUDED_

#include "crypto.h"

#if defined(LW_FALCON) || defined(GCC_FALCON)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

/*----------------------------------------------------*/
/*                                                    */
/* Cert 2.0 structures and defines                    */
/*                                                    */
/*----------------------------------------------------*/

#define MAX_NAME_LENGTH                32
#define IMAGE_VERSION_STRING_LENGTH    20
#define MAX_EXTENSION_ENTRY_NUM        20

#define BCRT20_SHA256_SIZE             CRYPTO_SHA256_SIZE
#define BCRT20_DMHASH_SIZE             CRYPTO_DMHASH_SIZE
#define BCRT20_FLW128HASH_SIZE         CRYPTO_FLW128HASH_SIZE
#define BCRT20_AES128_SIZE             CRYPTO_AES128_SIZE
#define BCRT20_3AES128_SIG_SIZE        CRYPTO_3AES128_SIG_SIZE
#define BCRT20_RSA_MODULUS             CRYPTO_RSA_MODULUS
#define BCRT20_RSA1K_SIG_SIZE          CRYPTO_RSA1K_SIG_SIZE
#define BCRT20_RSA1K_SIG_BITS_SIZE     CRYPTO_RSA1K_SIG_BITS_SIZE
#define BCRT20_RSA2K_SIG_SIZE          CRYPTO_RSA2K_SIG_SIZE
#define BCRT20_RSA3K_MODULUS           CRYPTO_RSA3K_MODULUS
#define BCRT20_RSA3K_SIG_SIZE          CRYPTO_RSA3K_SIG_SIZE
#define BCRT20_RSA3K_SIG_BITS_SIZE     CRYPTO_RSA3K_SIG_BITS_SIZE
#define BCRT20_RSA4K_SIG_SIZE          CRYPTO_RSA4K_SIG_SIZE
#define BCRT20_RSA4K_SIG_BITS_SIZE     CRYPTO_RSA4K_SIG_BITS_SIZE
#define BCRT20_ECC256_SIG_SIZE         CRYPTO_ECC256_SIG_SIZE
#define BCRT20_RSAPADDING_TYPE_PKCS1   CRYPTO_RSAPADDING_TYPE_PKCS1
#define BCRT20_RSAPADDING_TYPE_PSS     CRYPTO_RSAPADDING_TYPE_PSS

#define MAX_CERT20_CERT_COUNT          5
#define MAX_CERT20_SIG_COUNT           5
#define MAX_CERT20_HAT_COUNT           20
#define MAX_CERT20_VDPA_COUNT          1
#define MAX_CERT20_FORMATTER_LENGTH    256
#define MAX_CERT20_FORMATTER_DIGITS    8

#define MAX_EXT_IMAGE_INSTANCES        2

#define BCRT20_RAW_ECID_SIZE           16


/* Certificate main body  */

/* 64bit TimeStamp in little endian format. */
//typedef struct {
//    LwU32 lower_integer;     //  seconds since 1970 Jan 1.  lower 32 bit Rolls over 2^32 seconds = 136 years.  No Unix Year 2038 problem
//    LwU32 upper_integer;     //
////    LwU32 fraction;        //  fractional second,  resolution of 2^-32 seconds (233 picoseconds)
////    LwU32 timeZone;        //  Time Zone information
//}BCRT20VALIDITY;

typedef struct {
   char pubKeyArray[BCRT20_RSA_MODULUS];
   LwU32 Exponent;            /*  Store Key length, Algorithm Info and etc.   */
} BCRT20RSA1KPUBKEY_S, *BCRT20RSA1KPUBKEY_SPTR;

typedef struct {
   char pubKeyArray[BCRT20_RSA3K_MODULUS];
   LwU32 Exponent;            /*  Store Key length, Algorithm Info and etc.   */
} BCRT20RSA3KPUBKEY_S, *BCRT20RSA3KPUBKEY_SPTR;


/* Defines for algoType field */
#define    LW_BCRT20_ALGOTYPE_HASHTYPE                     7:0
#define    LW_BCRT20_ALGOTYPE_HASHTYPE_DMOVERAES128          1
#define    LW_BCRT20_ALGOTYPE_HASHTYPE_SHA256                2
#define    LW_BCRT20_ALGOTYPE_CRYPTOTYPE                  15:8
#define    LW_BCRT20_ALGOTYPE_CRYPTOTYPE_RSA1024             1
#define    LW_BCRT20_ALGOTYPE_CRYPTOTYPE_RSA3072             2
#define    LW_BCRT20_ALGOTYPE_CRYPTOTYPE_ECDSA256            3
#define    LW_BCRT20_ALGOTYPE_CRYPTOTYPE_RSAPSS3072          4
#define    LW_BCRT20_ALGOTYPE_HASHTYPE_ALT               23:16
#define    LW_BCRT20_ALGOTYPE_HASHTYPE_ALT_DMOVERAES128      1
#define    LW_BCRT20_ALGOTYPE_HASHTYPE_ALT_SHA256            2
#define    LW_BCRT20_ALGOTYPE_CRYPTOTYPE_ALT             31:24
#define    LW_BCRT20_ALGOTYPE_CRYPTOTYPE_ALT_AES128          1
#define    LW_BCRT20_ALGOTYPE_CRYPTOTYPE_ALT_3AES128         2

/* Defines for flag field */
#define    LW_BCRT20_FLAG_EXTENSION_PRESENT                0:0
#define    LW_BCRT20_FLAG_EXTENSION_PRESENT_NO               0
#define    LW_BCRT20_FLAG_EXTENSION_PRESENT_YES              1

#define    LW_BCRT20_FLAG_PUBKEY_PRESENT                   1:1
#define    LW_BCRT20_FLAG_PUBKEY_PRESENT_NO                  0
#define    LW_BCRT20_FLAG_PUBKEY_PRESENT_YES                 1

#define    LW_BCRT20_FLAG_PROD_ALTSIG_PRESENT              2:2
#define    LW_BCRT20_FLAG_PROD_ALTSIG_PRESENT_NO             0
#define    LW_BCRT20_FLAG_PROD_ALTSIG_PRESENT_YES            1

#define    LW_BCRT20_FLAG_DEBUG_ALTSIG_PRESENT             3:3
#define    LW_BCRT20_FLAG_DEBUG_ALTSIG_PRESENT_NO            0
#define    LW_BCRT20_FLAG_DEBUG_ALTSIG_PRESENT_YES           1


#define    LW_BCRT20_TYPE_CERT                            23:0
#define    LW_BCRT20_TYPE_CERT_ROOT                          1
#define    LW_BCRT20_TYPE_CERT_MASTER                        2
#define    LW_BCRT20_TYPE_CERT_LWD                           3
#define    LW_BCRT20_TYPE_CERT_AED                           4
#define    LW_BCRT20_TYPE_CERT_LWA                           5
#define    LW_BCRT20_TYPE_CERT_PP                            6
#define    LW_BCRT20_TYPE_CERT_PD                            7
#define    LW_BCRT20_TYPE_CERT_XOC                           8
#define    LW_BCRT20_TYPE_CERT_LWL                           9
#define    LW_BCRT20_TYPE_CERT_BASELINE                     10
#define    LW_BCRT20_TYPE_CERT_PSDL                         11
#define    LW_BCRT20_TYPE_CERT_HULK                         12      // Type 12 is for Post Devinit Licenses
#define    LW_BCRT20_TYPE_CERT_LWF_ENG                      13
#define    LW_BCRT20_TYPE_CERT_PARTNER_READ_ONLY            14
#define    LW_BCRT20_TYPE_CERT_HULK_PRE_DEVINIT             15      // New HULK type for pre devinit licenses

#define    VENDOR_CERT_BOOL( x ) ( ( x == LW_BCRT20_TYPE_CERT_LWD) || \
                                   ( x == LW_BCRT20_TYPE_CERT_AED) || \
                                   ( x == LW_BCRT20_TYPE_CERT_LWA) || \
                                   ( x == LW_BCRT20_TYPE_CERT_PP)  || \
                                   ( x == LW_BCRT20_TYPE_CERT_PD)  || \
                                   ( x == LW_BCRT20_TYPE_CERT_XOC) \
                                   )

#define    IS_TYPE_VENDOR_CERT(x) VENDOR_CERT_BOOL(x)

#define    IS_TYPE_MASTER_CERT( x ) ( ( x == LW_BCRT20_TYPE_CERT_MASTER) )

#define    IS_TYPE_ROOT_CERT( x ) ( ( x == LW_BCRT20_TYPE_CERT_ROOT) )

#define    IS_TYPE_LICENSE_CERT( x ) ( ( x == LW_BCRT20_TYPE_CERT_HULK ) || (x == LW_BCRT20_TYPE_CERT_HULK_PRE_DEVINIT ) )

#define    IS_TYPE_VBIOS_CERT( x ) ( IS_TYPE_ROOT_CERT(x) || \
                                     IS_TYPE_MASTER_CERT(x) || \
                                     IS_TYPE_VENDOR_CERT(x) \
                                   )

#define    LW_BCRT20_TYPE_INSTANCE                        31:24    // Only used for KEY ID

typedef struct {
  LwU32           version;                         /* Version of Certificate structure  */
  LwU32           size;                            /* Size of this struct               */
  LwU32           serialNumber;                    /* serial Number of the Certificate  */
  LwU32           sizeOfCertificate;               /* size of this certificate          */
  LwU32           type;                            /* Certificate Type                  */
  LwU32           algoType;                        /* Algorithm Type (DM Hash + RSA)    */
  LwU32           flag;                            /* Flags for additional info         */
  LwU32           issuerID;                        /* Issuer's ID                       */
  LwU32           certOwnerID;                     /* Certificate owner's ID            */
  //char            certOwnerName[MAX_NAME_LENGTH];  /* Certificate owner name string, null ended, Informational only should match with CertOwnerID */
  LwU32           notBefore;                       /* Not valid before this time        */
  LwU32           notAfter;                        /* Not Valid after this date         */
  LwU8            pubKeyArray[BCRT20_RSA_MODULUS];
  LwU32           Exponent;                                  /*  Store Key length, Algorithm Info and etc.   */
//  BCRT20RSA1KPUBKEY  pubKeyRSA1K;                /* RSA 1024 Public Key               */
} BCRT20CertMainRSA1024_S, *BCRT20CertMainRSA1024_SPTR;

typedef struct {
  LwU32           version;                         /* Version of Certificate structure  */
  LwU32           size;                            /* Size of this struct               */
  LwU32           serialNumber;                    /* serial Number of the Certificate  */
  LwU32           sizeOfCertificate;               /* size of this certificate          */
  LwU32           type;                            /* Certificate Type                  */
  LwU32           algoType;                        /* Algorithm Type (DM Hash + RSA)    */
  LwU32           flag;                            /* Flags for additional info         */
  LwU32           issuerID;                        /* Issuer's ID                       */
  LwU32           certOwnerID;                     /* Certificate owner's ID            */
  //char            certOwnerName[MAX_NAME_LENGTH];  /* Certificate owner name string, null ended, Informational only should match with CertOwnerID */
  LwU32           notBefore;                       /* Not valid before this time        */
  LwU32           notAfter;                        /* Not Valid after this date         */
} BCRT20CertMainNoPKCS_S, *BCRT20CertMainNoPKCS_SPTR;

#define MAX_CERT20_MAIN_STRUCT_SIZE               sizeof( BCRT20CertMainNoPKCS_S ) + sizeof( BCRT20RSA3KPUBKEY_S ) + 0x10    // add 0x10 bytes for possible future small expansion in size
#define MIN_CERT20_MAIN_STRUCT_SIZE               sizeof( BCRT20CertMainNoPKCS_S )

/* Extension Linked list,  should locate right after BCRT20CertMain
 * Extersions are used  to define NTP Server, Security Level,  Debug Certificate Validity,
 * Vendor Privilege and other Lw Policy related information
*/

typedef struct {
    LwU32    version;            /*  arbitrary extension version */
    LwU32    size;               /*  size of this structure  */
    LwU32    entry_size;         /*  entry control header struct size */
    LwU32    entryNum;           /*  number of extension entries */
}BCRT20CertExtHeader_S, *BCRT20CertExtHeader_SPTR;

#define LW_BCRT20_EXT_FLAG_FIRST_ENTRY                1
#define LW_BCRT20_EXT_FLAG_LAST_ENTRY                 2

#define  LW_BCRT20_CERT_EXTENSION_TYPE_NTP                               1
#define  LW_BCRT20_CERT_EXTENSION_TYPE_BIOSMOD_SELWRITY_LEVEL            2
#define  LW_BCRT20_CERT_EXTENSION_TYPE_EXPIRATION_DAYS                   3
#define  LW_BCRT20_CERT_EXTENSION_TYPE_VENDOR_PRIVILEGE                  4
#define  LW_BCRT20_CERT_EXTENSION_TYPE_VENDOR_NAME_INFO                  5
#define  LW_BCRT20_CERT_EXTENSION_TYPE_ECID_HASH                         6
#define  LW_BCRT20_CERT_EXTENSION_TYPE_GPU_PERSONALITY                   7      // UGPU
#define  LW_BCRT20_CERT_EXTENSION_TYPE_PSDL                              8      // Private Security Debug License
#define  LW_BCRT20_CERT_EXTENSION_TYPE_RANDOM_NUMBER                     9      // For AES signature
#define  LW_BCRT20_CERT_EXTENSION_TYPE_GPUID                            10      // GPUID
#define  LW_BCRT20_CERT_EXTENSION_TYPE_DEVID                            11      // DevID

typedef struct {
    LwU32    prev_cert_ext;      /*  offset of previous cert extension. 0 if first one  */
    LwU32    next_cert_ext;      /*  offset of next cert extension   */
    LwU32    ext_type;           /*  Cert Extension type, NTP Serve, Security Level and etc   */
    LwU32    flag;               /*  Mark the beginning, end of the link list and etc. */
}BCRT20CertExtEntry_S, *BCRT20CertExtEntry_SPTR;

// Type 3 is Debug Certificate EXPIRATION_DAYS
#define CERT20_EXT_TYPE3_STRUCT_SIZE  4
typedef struct {
    LwU32  days;           /* EXPIRATION_DAYS  */
}BCRT20CertExtType3_S, *BCRT20CertExtType3_SPTR;

//  Type 4 is VENDOR_PRIVILEGE
#define CERT20_EXT_TYPE4_STRUCT_SIZE  4
typedef struct {
    LwU32  vp;           /* Vendor privilege  */
}BCRT20CertExtType4_S, *BCRT20CertExtType4_SPTR;

// type 5, vendor name.

// Type 6 is for ECID.  it is a 32byte field
#define CERT20_EXT_TYPE6_MAX_EXT_NUM                             125                              // number of ECID could one Cert have

#define    LW_BCRT20_EXT6_ECID_DESC_ALGO                        15:0
#define    LW_BCRT20_EXT6_ECID_DESC_ALGO_SHA2                      1                              // ECID encoded by SHA2
#define    LW_BCRT20_EXT6_ECID_DESC_ALGO_DMHASH                    2                              // ECID encoded by dmhash(davies meyer hash)
#define    LW_BCRT20_EXT6_ECID_DESC_ALGO_RAW                       3                              // ECID raw data

#define CERT20_EXT_TYPE6_STRUCT_SHA2_SIZE  36
typedef struct {
    LwU32 ecid_desc;
    char  ecid[32];   /* SHA2 hash of ECID */
}BCRT20CertExtType6SHA2ECID_S, *BCRT20CertExtType6SHA2ECID_SPTR;

typedef struct {
    LwU32 ecid_desc;
    char  ecid[16];   /* DM Hash of ECID */
}BCRT20CertExtType6DMHashECID_S, *BCRT20CertExtType6DMHashECID_SPTR;

// Type 7 is GPU Personality

#define CERT20_EXT_TYPE7_MAX_EXT_NUM       8

#define CERT20_EXT_TYPE7_STRUCT_VER0_SIZE  8

#define CERT20_EXT_TYPE7_PERSONALITY_GeForce  1
#define CERT20_EXT_TYPE7_PERSONALITY_Quadro   2
#define CERT20_EXT_TYPE7_PERSONALITY_Tesla    3
#define CERT20_EXT_TYPE7_PERSONALITY_HULK     4

#define LW_CERT20_EXT7_PROC_FLAG_LOG_REGOVERRIDE            0:0
#define LW_CERT20_EXT7_PROC_FLAG_LOG_REGOVERRIDE_YES          1       // Record register overrode as a log. Replacing CERT20_EXT_TYPE7_PROC_FLAG_LOG_REGOVERRIDE
#define LW_CERT20_EXT7_PROC_FLAG_LOG_REGOVERRIDE_NO           0       // Record register overrode as a log. Replacing CERT20_EXT_TYPE7_PROC_FLAG_LOG_REGOVERRIDE
#define LW_CERT20_EXT7_PROC_FLAG_BSI_REGOVERRIDE            1:1
#define LW_CERT20_EXT7_PROC_FLAG_BSI_REGOVERRIDE_YES          1       // Processing Register Overrides to BRSS struc in BSI memory. Replacing CERT20_EXT_TYPE7_PROC_FLAG_PROCESS_BSI_REGOVERRIDE
#define LW_CERT20_EXT7_PROC_FLAG_BSI_REGOVERRIDE_NO           0       // Processing Register Overrides to BRSS struc in BSI memory. Replacing CERT20_EXT_TYPE7_PROC_FLAG_PROCESS_BSI_REGOVERRIDE

#define LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_ECID            2:2       //   Flag used to indicate status of ECID matching
#define LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_ECID_YES          1
#define LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_ECID_NO           0

#define LW_CERT20_EXT7_PROC_FLAG_ID_ECID_EXT_PRESENT        3:3       //   Flag used to indicate if ext6 for ECID exists
#define LW_CERT20_EXT7_PROC_FLAG_ID_ECID_EXT_PRESENT_YES      1
#define LW_CERT20_EXT7_PROC_FLAG_ID_ECID_EXT_PRESENT_NO       0


#define LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_DEVID           4:4       //   Flag used to indicate status of DevID matching
#define LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_DEVID_YES         1
#define LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_DEVID_NO          0

#define LW_CERT20_EXT7_PROC_FLAG_ID_PDI_EXT_PRESENT        5:5       //   Flag used to indicate if ext14 for PDI exists
#define LW_CERT20_EXT7_PROC_FLAG_ID_PDI_EXT_PRESENT_YES      1
#define LW_CERT20_EXT7_PROC_FLAG_ID_PDI_EXT_PRESENT_NO       0

#define LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_PDI            6:6       //   Flag used to indicate status of PDI matching
#define LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_PDI_YES          1
#define LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_PDI_NO           0

typedef struct {
    LwU8    major_personality; //  1 = VdChip, 2 = Lwdqro, 3 = Tesla.  2 bytes
    LwU16   minor_personality;
    LwU8    reserved;            //
    LwU16   reg_override_num;    //  number of register overrides
    LwU16   feat_table_version;  //  feature table data structure Version. 2 bytes
}BCRT20CertExtType7_S, *BCRT20CertExtType7_SPTR;


#define EXT_T7_QUADRO_VGPU_DEFAULT     0    // feature kept as default
#define EXT_T7_QUADRO_VGPU_ENABLE      1    // enalbe feature
#define EXT_T7_QUADRO_VGPU_DISABLE     2    // disable feature
#define EXT_T7_QUADRO_VGPU_MASK        3    //  mask

#define CERT20_EXT_TYPE7_QUADRO_STRUCTVER1_SIZE  4
typedef struct {
    LwU32    features;           /* vgpu and etc.  */
}BCRT20CertExtType7QuadroVer1_S, *BCRT20CertExtType7QuadroVer1_SPTR;

#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_VBIOS_SIG                 0:0
#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_VBIOS_SIG_YES             1
#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_VBIOS_SIG_NO              0

#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_DEVINIT_SIG               1:1
#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_DEVINIT_SIG_YES           1
#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_DEVINIT_SIG_NO            0

#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_ALLOW_EEPROM_ERASE             2:2
#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_ALLOW_EEPROM_ERASE_YES         1
#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_ALLOW_EEPROM_ERASE_NO          0

#define    LW_BCRT20_EXT7_FEAT1_HULK_STRICT_ID_MATCH                    3:3
#define    LW_BCRT20_EXT7_FEAT1_HULK_STRICT_ID_MATCH_YES                1
#define    LW_BCRT20_EXT7_FEAT1_HULK_STRICT_ID_MATCH_NO                 0

#define    LW_BCRT20_EXT7_FEAT1_HULK_EXELWTE_AT_PRIV_LEVEL_0            4:4
#define    LW_BCRT20_EXT7_FEAT1_HULK_EXELWTE_AT_PRIV_LEVEL_0_YES        1
#define    LW_BCRT20_EXT7_FEAT1_HULK_EXELWTE_AT_PRIV_LEVEL_0_NO         0

#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_BYPASS_SMBPBI_WP               5:5
#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_BYPASS_SMBPBI_WP_YES           1
#define    LW_BCRT20_EXT7_FEAT1_HULK_LWF_BYPASS_SMBPBI_WP_NO            0

#define    LW_BCRT20_EXT7_FEAT1_HULK_FEATURE_TYPE                       31:24
#define    LW_BCRT20_EXT7_FEAT1_HULK_FEATURE_TYPE_INST_IN_SYS           1
#define    LW_BCRT20_EXT7_FEAT1_HULK_FEATURE_TYPE_I1500_DATA_UNLOCK     2
#define    LW_BCRT20_EXT7_FEAT1_HULK_FEATURE_TYPE_TREX_LWLINK_RESET     4



#define CERT20_EXT_TYPE7_FEAT_STRUCTVER1_SIZE  4
typedef struct {
    LwU32    features;           /* hulk and etc.  */
}BCRT20CertExtType7FeatVer1_S, *BCRT20CertExtType7FeatVer1_SPTR;

#define CERT20_EXT_TYPE7_SW_FEATURE_SIZE  12

#define CERT20_EXT_TYPE7_REGOVERRIDE_TYPE         31:28
#define CERT20_EXT_TYPE7_REGOVERRIDE_TYPE_BAR0    0
#define CERT20_EXT_TYPE7_REGOVERRIDE_TYPE_BSI     0x8
#define CERT20_EXT_TYPE7_OPCODE                   27:24
#define CERT20_EXT_TYPE7_OPCODE_RMW               0         // Read modify write
#define CERT20_EXT_TYPE7_OPCODE_RBV               1         // Read back verify
#define CERT20_EXT_TYPE7_OPCODE_RMW_RBV           2         // Read modify write with Read back verify
#define CERT20_EXT_TYPE7_OPCODE_DELAY             3         // Delay in ns

//  note:  if CERT20_EXT_TYPE7_REGOVERRIDE_TYPE field is set with _BSI,  _REGOVERRIDE_ADDR field is a don't care
#define CERT20_EXT_TYPE7_REGOVERRIDE_ADDR         23:0

typedef struct {
    LwU32    addr;               /* Level 3 register address  */
    LwU32    mask;               /* register mask  */
    LwU32    data;               /* register data  */
}BCRT20CertExtType7RegOverride_S, *BCRT20CertExtType7RegOverride_SPTR;

// Personality Request. Note: Not part of Certificate, used in UGPU
typedef struct {
    LwU8    major_personality; //  1 = VdChip, 2 = Lwdqro, 3 = Tesla.  2 bytes
    LwU16   minor_personality;
    LwU8    reserved;            //
}  BCRT20GPUPersReq_S, * BCRT20GPUPersReq_SPTR;


typedef struct {
        /*  Certificate Private Key Signature,  should located at right after all the extention. */
        char sig[BCRT20_RSA1K_SIG_SIZE];
} BCRT20RSA1KSIG_S, *BCRT20RSA1KSIG_SPTR;

typedef struct {
    /*  Certificate AES HW Key Signature,  should located at right after PKC sig. */
    char sig[BCRT20_SHA256_SIZE];
} BCRT20AESSIG_S, *BCRT20AESSIG_SPTR;


// Type 10 GPU IDs
#define CERT20_EXT_TYPE10_GPUID_NO_GPUID                   0
#define CERT20_EXT_TYPE10_GPUID_GM20X                      1
#define CERT20_EXT_TYPE10_GPUID_GM206                      2
#define CERT20_EXT_TYPE10_GPUID_GP100                      3
#define CERT20_EXT_TYPE10_GPUID_GP104                      4
#define CERT20_EXT_TYPE10_GPUID_TU10X                      7
#define CERT20_EXT_TYPE10_GPUID_TU10X_RSA3K                8
#define CERT20_EXT_TYPE10_GPUID_TU11X                      9
#define CERT20_EXT_TYPE10_GPUID_GENERAL_RSAPSS_3K         10

#define CERT20_EXT_TYPE10_STRUCT_SIZE  4
typedef struct {
    LwU32   gpuid;
}  BCRT20CertExtType10_S, * BCRT20CertExtType10_SPTR;

// Type 11 Dev IDs
#define CERT20_EXT_TYPE11_MAX_EXT_NUM                             16                              // number of DevID could one Cert have
#define    LW_BCRT20_EXT11_DEVID                                 15:0

#define CERT20_EXT_TYPE11_STRUCT_SIZE  4
typedef struct {
    LwU32   devid;
}  BCRT20CertExtType11_S, * BCRT20CertExtType11_SPTR;


// Type 14 PDI
#define CERT20_EXT_TYPE14_MAX_EXT_NUM    128  // number of PDI's one Cert can have LWBUG:2516609

typedef struct {
    LwU64 pdi;
}BCRT20CertExtType14PDI_S, *BCRT20CertExtType14PDI_SPTR;

/*---------------------------------------------------------------------

    VDPA Table structures
----------------------------------------------------------------------*/

#define CERT20_FMT_CHAR         'b'
#define CERT20_FMT_REPEAT       'r'
#define CERT20_FMT_REPEAT_END   'R'
#define CERT20_FMT_SKIP         'i'


/* VDPA table header structures */
#define  LW_BCRT20_VDPA_HEADER_IMG_ID_VER_SIZE_VERSION    15:0
#define  LW_BCRT20_VDPA_HEADER_IMG_ID_VER_SIZE_VERSION_1    1
#define  LW_BCRT20_VDPA_HEADER_IMG_ID_VER_SIZE_SIZE       31:16

#define  LW_BCRT20_VDPA_HEADER_IMG_ID_IMGDEVID_DEVID      15:0

typedef struct {
    LwU32      ver_size;                                              /* Version and Size field */
    LwU32      imgDevID;                                              /* DevID in VBIOS image.  Only lower 16bit used now.  */
}BCRT20_VDPA_IMG_ID_S, *BCRT20_VDPA_IMG_ID_SPTR;

// ID struct variant 1,  with GUID
typedef struct {
    LwU32      ver_size;                                              /* Version and Size field */
    LwU32      imgDevID;                                              /* DevID in VBIOS image.  Only lower 16bit used now.  */
    LwU8       dmhash[BCRT20_DMHASH_SIZE];                            /* DM HASH of build version and magic number.  */
}BCRT20_VDPA_IMG_ID_1_S, *BCRT20_VDPA_IMG_ID_1_SPTR;

#define  LW_BCRT20_VDPA_HEADER_LOC_CTRL_SIZE_SIZE             19:0
#define  LW_BCRT20_VDPA_HEADER_LOC_CTRL_SIZE_INSTANCE         23:20
#define  LW_BCRT20_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE        31:24
#define  LW_BCRT20_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE_X86    0x00
#define  LW_BCRT20_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE_IFR    0x4E
#define  LW_BCRT20_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE_EXT    0xE0
#define  LW_BCRT20_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE_NBSI   0x70

#define  VDPA_HEADER_LOC_CTRL_TOTAL                             5    // Total structure array count. Lwrrently 5
#define  VDPA_HEADER_LOC_CTRL_IFR_IDX                           0    // array index for IFR section location information
#define  VDPA_HEADER_LOC_CTRL_X86_IDX                           1    // array index for X86 PCI ROM location information
#define  VDPA_HEADER_LOC_CTRL_EXT_IDX                           2    // array index for extended PCI ROM location information (used for all instances assumed to be contiguous)
#define  VDPA_HEADER_LOC_CTRL_NBSI_IDX                          3    // array index for NBSI PCI ROM location information,  Internal Cert Block not include
#define  VDPA_HEADER_LOC_CTRL_INTBLK_IDX                        4    // array index for Internal Cert Block information

typedef struct {
    LwU32      offset;                                               /* offset into image.  Note,  IFR area is not counted for PCI ROM offset */
    LwU32      size;                                                 /* size of the IFR or PCI ROM image.  Code type is at upper 8bits */
}BCRT20_VDPA_LOC_CTRL_S, *BCRT20_VDPA_LOC_CTRL_SPTR;


// Defines for VDPA table header:

#define LW_CRT20_VDPA_HEADER_MAX_SIZE                          128

#define LW_CRT20_VDPA_HEADER_VERSION                           7:0      // Header struct version
#define LW_CRT20_VDPA_HEADER_VERSION_V1                        1
#define LW_CRT20_VDPA_HEADER_VERSION_V2                        2
#define LW_CRT20_VDPA_HEADER_VERSION_LOC_VERSION               15:8     // LOC struct version
#define LW_CRT20_VDPA_HEADER_VERSION_LOC_VERSION_V1            1
#define LW_CRT20_VDPA_HEADER_VERSION_LOC_ARRAY_SIZE            23:16     // LOC struct array size
#define LW_CRT20_VDPA_HEADER_VERSION_LOC_ARRAY_SIZE_LWRRENT    VDPA_HEADER_LOC_CTRL_TOTAL     // LOC struct array size

#define LW_CRT20_VDPA_HEADER_ALGO_HASH                         7:0
#define LW_CRT20_VDPA_HEADER_ALGO_HASH_DMHASH                  1
#define LW_CRT20_VDPA_HEADER_ALGO_HASH_SHA256                  2
#define LW_CRT20_VDPA_HEADER_ALGO_CRYPTO                      15:8
#define LW_CRT20_VDPA_HEADER_ALGO_CRYPTO_AES128                1
#define LW_CRT20_VDPA_HEADER_ALGO_CRYPTO_RSA1024               2
#define LW_CRT20_VDPA_HEADER_ALGO_CRYPTO_RSA3072               3
#define LW_CRT20_VDPA_HEADER_ALGO_CRYPTO_ECDSA256              4

#define LW_CRT20_VDPA_HEADER_ENTRY_NORMAL                      7:0     // VDPA entry number used to colwer VBIOS image signature check.  Lwrrently _MAJOR_NORMAL Type
#define LW_CRT20_VDPA_HEADER_ENTRY_INTBLK                     15:8     // Internal Block Type, or _MAJOR_INT_BLK type VDPA entry number.  For supporting Internal Signature.
#define LW_CRT20_VDPA_HEADER_ENTRY_INTFMT                     23:16    // Internal with formatter Type, or _MAJOR_INT_FMT type VDPA entry number.  For supporting Internal Signature.

#define LW_CRT20_VDPA_HEADER_SECZONE_INFO_SECZONE_NUM         7:0     // Total Security Zones supported.
#define LW_CRT20_VDPA_HEADER_SECZONE_INFO_KEYSET_INDEX       15:8     // Security zone key set index,  used to change keys for security zone feature. It's value comes from BIOSMOD Script's user selection
#define LW_CRT20_VDPA_HEADER_SECZONE_INFO_SIG_SIZE           31:16    // one signature size for a security zone.  It includes signature header struct and signature


#define LW_CRT20_VDPA_HEADER_FLAG_VDPA_SIGNED                 0:0      // Flag to indicate VDPA table has been finalized by Lwflash
#define LW_CRT20_VDPA_HEADER_FLAG_VDPA_SIGNED_NO               0
#define LW_CRT20_VDPA_HEADER_FLAG_VDPA_SIGNED_YES              1
#define LW_CRT20_VDPA_HEADER_FLAG_REBRAND_ENABLED             1:1      // Flag to indicate rebrand enabled
#define LW_CRT20_VDPA_HEADER_FLAG_REBRAND_ENABLED_NO           0
#define LW_CRT20_VDPA_HEADER_FLAG_REBRAND_ENABLED_YES          1
#define LW_CRT20_VDPA_HEADER_FLAG_PROJ_ID_PRESENT             2:2      // Flag to indicate proj_id is present
#define LW_CRT20_VDPA_HEADER_FLAG_PROJ_ID_PRESENT_NO           0
#define LW_CRT20_VDPA_HEADER_FLAG_PROJ_ID_PRESENT_YES          1

typedef struct {
    LwU32                   version;            /*  VDPA header version, location struct version, location array number */
    LwU32                   size;               /*  size of this structure                */
    LwU32                   algo;               /*  Algorithm control                     */
    LwU32                   entryNum1;          /*  entry numbers for different type of VDPA table  */
    LwU32                   entryNum2;          /*  entry numbers for different type of VDPA table - reserved for future use */
    LwU32                   blocksize;          /*  total VDPA block size, include this header,  all VDPA entries and signature */
    BCRT20_VDPA_IMG_ID_S    id;                 /*  ID to match the underlying VBIOS image*/
    BCRT20_VDPA_LOC_CTRL_S  loc[VDPA_HEADER_LOC_CTRL_TOTAL];  /*  Has offset and size information for each IFR or ROM location, it is used to help uCode get around VBIOS image file */
    LwU32                   flag;               /*  With VDPA table signed, GPU rebrand enabled and etc.   */
    LwU32                   rsvd;               /*  Reserved field                           */
} BCRT20_VDPA_HEADER_S, *BCRT20_VDPA_HEADER_SPTR;

#define  LW_BCRT2X_MAX_PROJECTID_LENGTH             6
#define  LW_BCRT2X_MAX_PARTNER_LENGTH              20
#define  LW_BCRT2X_MAX_SESSIONID_LENGTH            20

typedef  struct {
         LwU8     projectId[LW_BCRT2X_MAX_PROJECTID_LENGTH];
         LwU8     partner[LW_BCRT2X_MAX_PARTNER_LENGTH];
         LwU8     sessionid[LW_BCRT2X_MAX_SESSIONID_LENGTH];
} BCRT2X_PROJ_ID_S, *BCRT2X_PROJ_ID_SPTR;


// VDPA header version 1,  variant 1 with proj_id, for Cert Block Version 2.0 and 2.1 ( Pascal and Volta  )
typedef struct {
    LwU32                   version;            /*  VDPA header version, location struct version, location array number */
    LwU32                   size;               /*  size of this structure                */
    LwU32                   algo;               /*  Algorithm control                     */
    LwU32                   entryNum1;          /*  entry numbers for different type of VDPA table  */
    LwU32                   entryNum2;          /*  entry numbers for different type of VDPA table - reserved for future use */
    LwU32                   blocksize;          /*  total VDPA block size, include this header,  all VDPA entries and signature */
    BCRT20_VDPA_IMG_ID_S    id;                 /*  ID to match the underlying VBIOS image*/
    BCRT20_VDPA_LOC_CTRL_S  loc[VDPA_HEADER_LOC_CTRL_TOTAL];  /*  Has offset and size information for each IFR or ROM location, it is used to help uCode get around VBIOS image file */
    LwU32                   flag;               /*  With VDPA table signed, GPU rebrand enabled and etc.   */
    LwU32                   rsvd;               /*  Reserved field                           */
    BCRT2X_PROJ_ID_S        proj_id;            /*  VBIOS ROM's project id,  partner name and SESSIONID name,  only for Cert2.0, Cert2.1. From Cert2.2, they will be move to BCRT2X_METADATA_HEADER_S  */
} BCRT20_VDPA_HEADER_1_S, *BCRT20_VDPA_HEADER_1_SPTR;

// VDPA header version 2, with BCRT20_VDPA_IMG_ID_1_S, for Cert Block Version 2.2 (Turing and later )
//    Note:  proj_id moved out of VDPA header for Cert2.2
typedef struct {
    LwU32                   version;            /*  VDPA header version, location struct version, location array number */
    LwU32                   size;               /*  size of this structure                */
    LwU32                   algo;               /*  Algorithm control                     */
    LwU32                   entryNum1;          /*  entry numbers for different type of VDPA table  */
    LwU32                   entryNum2;          /*  entry numbers for different type of VDPA table - reserved for future use */
    LwU32                   blocksize;          /*  VDPA block size, include this header,  all VDPA entries and runtime signature of VDPA table */
    LwU32                   total_blocksize;    /*  total Internal blocksize, = blocksize + all Security zone signatures */
    LwU32                   seczone_info;       /*  information related to security zone, Including key set index, single key structure size, total security zones.  */
    LwU32                   seczone_info1;      /*  Additional information related to security zone. lwrrently not used  */
    BCRT20_VDPA_IMG_ID_1_S  id;                 /*  IDs to match the underlying VBIOS image*/
    BCRT20_VDPA_LOC_CTRL_S  loc[VDPA_HEADER_LOC_CTRL_TOTAL];  /*  Has offset and size information for each IFR or ROM location, it is used to help uCode get around VBIOS image file */
    LwU32                   flag;               /*  With VDPA table signed, GPU rebrand enabled and etc.   */
    LwU32                   rsvd;               /*  Reserved field                           */
} BCRT20_VDPA_HEADER_V2_S, *BCRT20_VDPA_HEADER_V2_SPTR;


/* VDPA table entries.  */
//  Note: VDPA table entries should located right after VDPA header.
//        VDPA entry data structure define is decided by these two fields: _TYPE_MAJOR  and _TYPE_VERSION
#define LW_BCRT20_VDPA_ENTRY_TYPE_MAJOR                    7:0       // VDPA type major
#define LW_BCRT20_VDPA_ENTRY_TYPE_MAJOR_NORMAL              1        // Normal Type.  Indicate entry struct format is BCRT20_VDPA_ENTRY_NORMAL_S
#define LW_BCRT20_VDPA_ENTRY_TYPE_MAJOR_INT_BLK             2        // Internal Block. Indicate entry struct format is BCRT20_VDPA_ENTRY_INT_BLK_S
#define LW_BCRT20_VDPA_ENTRY_TYPE_MAJOR_NORMAL_FMT          5        // Normal Type.  Indicate entry struct format is BCRT20_VDPA_ENTRY_NORMAL_FMT_S
#define LW_BCRT20_VDPA_ENTRY_TYPE_MAJOR_INT_FMT             6        // Internal with formatter. Indicate entry struct format is BCRT20_VDPA_ENTRY_INT_FMT_S
#define LW_BCRT20_VDPA_ENTRY_TYPE_MINOR                   15:8       // VDPA type MINOR
#define LW_BCRT20_VDPA_ENTRY_TYPE_MINOR_IFR                 1        // Major type _NORMAL and minor type _MINOR_IFR indicates a IFR entry.
#define LW_BCRT20_VDPA_ENTRY_TYPE_MINOR_NORMAL              2        // Major type _NORMAL and minor type _MINOR_NORMAL indicates a Normal entry.
#define LW_BCRT20_VDPA_ENTRY_TYPE_MINOR_CERT_BLK            3        // Major type _NORMAL and minor type _MINOR_CERT_BLK indicates Cert Block entry.
#define LW_BCRT20_VDPA_ENTRY_TYPE_MINOR_SPT                 1        // Major type _MAJOR_INT_BLK and minor type set to _MINOR_SPT indicates a SPT entry.

#define LW_BCRT20_VDPA_ENTRY_TYPE_USAGE                   23:16      // usage of the entry,  i.e. Perf Table,  VDE Table and etc.

#define LW_BCRT20_VDPA_ENTRY_TYPE_VERSION                 31:24      // Version of this struct.
#define LW_BCRT20_VDPA_ENTRY_TYPE_VERSION_VER_0             0        // Version 0
#define LW_BCRT20_VDPA_ENTRY_TYPE_VERSION_VER_1             1        // Version 1


#define LW_BCRT20_VDPA_ENTRY_NORMAL_SIZE_SIZE                             19:0
#define LW_BCRT20_VDPA_ENTRY_NORMAL_SIZE_INSTANCE                         23:20
#define LW_BCRT20_VDPA_ENTRY_NORMAL_SIZE_CODE_TYPE                        31:24
#define LW_BCRT20_VDPA_ENTRY_NORMAL_SIZE_CODE_TYPE_X86    0x00
#define LW_BCRT20_VDPA_ENTRY_NORMAL_SIZE_CODE_TYPE_EXT    0xE0
#define LW_BCRT20_VDPA_ENTRY_NORMAL_SIZE_CODE_TYPE_NBSI   0x70

#define LW_BCRT20_VDPA_NORMAL_ENTRIES_MAX_NUMBER                             84
#define LW_BCRT20_VDPA_NORMAL_ENTRIES_MAX_SIZE                               LW_BCRT20_VDPA_NORMAL_ENTRIES_MAX_NUMBER * sizeof(BCRT20_VDPA_ENTRY_NORMAL_BASE_S)


typedef struct {
    LwU32      type;             /*  VBIOS Image type of this entry.  */
    LwU32      offset_start;     /* VBIOS image staring offset for this entry */
    LwU32      size;             /* data block size,  PCI ROM code type and instance data */
    char       sig_hash[BCRT20_DMHASH_SIZE];   /* Image Hash data from offset_start to offset_start + size  */
}BCRT20_VDPA_ENTRY_NORMAL_BASE_S, *BCRT20_VDPA_ENTRY_NORMAL_BASE_SPTR;

// Note: struct needed when merge HAT and VDPA
#define LW_BCRT20_VDPA_ENTRY_NORMAL_FMT_ATTRIB_ALGO                            7:0   // Crypto Algo Control
#define LW_BCRT20_VDPA_ENTRY_NORMAL_FMT_ATTRIB_ALGO_DMHASH128                    0
#define LW_BCRT20_VDPA_ENTRY_NORMAL_FMT_ATTRIB_ALGO_AES128                       1
#define LW_BCRT20_VDPA_ENTRY_NORMAL_FMT_ATTRIB_FMT_LEN                        19:8   // Formatter string length
typedef struct {
    LwU32      type;             /* VBIOS Image type of this entry.  */
    LwU32      offset_start;     /* VBIOS image staring offset for this entry */
    LwU32      size;             /* max block size formatter colwers,  PCI ROM code type and instance data  */
    LwU32      fmt_attrib;       /* formater string size,  flags and etc.  */
    // char*      formatter;     /* Formatter string.  its variable length is in attrib.LW_BCRT20_VDPA_ENTRY_INT_FMT_ATTRIB_FMT_LEN field  */
    // char       sig_hash[BCRT20_DMHASH_SIZE];   // Image Hash/Sig,  start of this field is at
                                                  //   sizeof( BCRT20_VDPA_ENTRY_INT_FMT_BASE_S) +
                                                  //   REF_VAL( LW_BCRT20_VDPA_ENTRY_INT_FMT_ATTRIB_FMT_LEN, BCRT20_VDPA_ENTRY_INT_FMT_BASE_S.attrib )
}BCRT20_VDPA_ENTRY_NORMAL_FMT_BASE_S, *BCRT20_VDPA_ENTRY_NORMAL_FMT_BASE_SPTR;

typedef enum {
    LW_BCRT2X_SEC_ZONE_0   = 0,  // 0 for non-secure  not selwred by any LS signature
    LW_BCRT2X_SEC_ZONE_1,        // security zone 1,  selwred by LS Signature generated from the LS key assigned to zone 1, dedicated to ID matching, VDPA entries count
    LW_BCRT2X_SEC_ZONE_2,        // security zone 2,  selwred by LS Signature generated from the LS key assigned to zone 2, dedicated to meta data
    LW_BCRT2X_SEC_ZONE_3,        // security zone 3,  selwred by LS Signature generated from the LS key assigned to zone 3
    LW_BCRT2X_SEC_ZONE_4,        // security zone 4,  selwred by LS Signature generated from the LS key assigned to zone 4
    LW_BCRT2X_SEC_ZONE_5,        // security zone 5,  selwred by LS Signature generated from the LS key assigned to zone 5
    LW_BCRT2X_SEC_ZONE_6,        // security zone 6,  selwred by LS Signature generated from the LS key assigned to zone 6
    LW_BCRT2X_SEC_ZONE_7,        // security zone 7,  selwred by LS Signature generated from the LS key assigned to zone 7
    LW_BCRT2X_SEC_ZONE_8,        // security zone 8,  selwred by LS Signature generated from the LS key assigned to zone 8
    LW_BCRT2X_SEC_ZONE_9,        // security zone 9,  selwred by LS Signature generated from the LS key assigned to zone 9
    LW_BCRT2X_SEC_ZONE_LAST,     // Used to track number of defined Security Zones in this enum. Keep it at the end
} LW_BCRT2X_GFW_LS_SIG_SELWRITY_ZONE_TYPE;

// structure used to control the matching between security zone and the key it used
typedef  struct
{
    LW_BCRT2X_GFW_LS_SIG_SELWRITY_ZONE_TYPE  secZoneId;
    LwU32  keySeedSelect;
}SecZoneIDnKeySeedPair;

#define LW_BCRT20_VDPA_INTBLK_ENTRIES_MAX_NUMBER                             96
#define LW_BCRT20_VDPA_INTBLK_ENTRIES_MAX_SIZE                               LW_BCRT20_VDPA_INTBLK_ENTRIES_MAX_NUMBER * sizeof(BCRT20_VDPA_ENTRY_INT_BLK_BASE_S)

#define LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_SIZE                             19:0
#define LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_INSTANCE                         23:20
#define LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE                        31:24
#define LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE_X86                    0x00
#define LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE_EXT                    0xE0
#define LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE_NBSI                   0x70


#define LW_BCRT20_VDPA_ENTRY_INT_BLK_FLAG_ALGO                              7:0     // Crypto Algo Control
#define LW_BCRT20_VDPA_ENTRY_INT_BLK_FLAG_ALGO_DMHASH128                      0
#define LW_BCRT20_VDPA_ENTRY_INT_BLK_FLAG_ALGO_AES128                         1

#define LW_BCRT20_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE                          15:8    // security zone for this entry
typedef struct {
    LwU32      type;             /* VBIOS Image type of this entry.  */
    LwU32      offset_start;     /* VBIOS image staring offset for this entry */
    LwU32      size;             /* data block size,  PCI ROM code type and instance data  */
    LwU32      flag;             /* general purpose flags for this entry,  with algo type, security zone and etc. */
    char       sig_hash[BCRT20_DMHASH_SIZE];   /* Image Hash data from offset_start to offset_start + size */
}BCRT20_VDPA_ENTRY_INT_BLK_BASE_S, *BCRT20_VDPA_ENTRY_INT_BLK_BASE_SPTR;

#define LW_BCRT20_VDPA_ENTRY_INT_FMT_ATTRIB_ALGO                            7:0   // Crypto Algo Control
#define LW_BCRT20_VDPA_ENTRY_INT_FMT_ATTRIB_ALGO_DMHASH128                    0
#define LW_BCRT20_VDPA_ENTRY_INT_FMT_ATTRIB_ALGO_AES128                       1
#define LW_BCRT20_VDPA_ENTRY_INT_FMT_ATTRIB_FMT_LEN                        19:8   // Formatter string length
typedef struct {
    LwU32      type;             /* VBIOS Image type of this entry.  */
    LwU32      offset_start;     /* VBIOS image staring offset for this entry */
    LwU32      size;             /* max block size formatter colwers,  PCI ROM code type and instance data  */
    LwU32      fmt_attrib;       /* formater string size,  flags and etc.  */
    // char*      formatter;     /* Formatter string.  its variable length is in attrib.LW_BCRT20_VDPA_ENTRY_INT_FMT_ATTRIB_FMT_LEN field  */
    // char       sig_hash[BCRT20_DMHASH_SIZE];   // Image Hash/Sig,  start of this field is at
                                                  //   sizeof( BCRT20_VDPA_ENTRY_INT_FMT_BASE_S) +
                                                  //   REF_VAL( LW_BCRT20_VDPA_ENTRY_INT_FMT_ATTRIB_FMT_LEN, BCRT20_VDPA_ENTRY_INT_FMT_BASE_S.attrib )
}BCRT20_VDPA_ENTRY_INT_FMT_BASE_S, *BCRT20_VDPA_ENTRY_INT_FMT_BASE_SPTR;


/* VDPA Table signature  */
typedef struct {
    char  vdpa_sig[BCRT20_AES128_SIZE];
}BCRT20_VDPA_SIG_S, *BCRT20_VDPA_SIG_SPTR;

/* ----------------------------------------------------------------------
   Cert Block Structures and Defines
-------------------------------------------------------------------------*/

#define LW_BCRT2X_CERT_CONTROL_HEADER_MAX_SIZE                        48

#define LW_BCRT2X_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE              0:0

#define LW_BCRT2X_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE              0:0
#define LW_BCRT2X_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE_YES            1
#define LW_BCRT2X_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE_NO             0
#define LW_BCRT2X_CERT_CONTROL_HEADER_EEPROM_MAX                 1048576

typedef  struct {
         LwU32    version;            // BCRT2X CTRL Structure Version
         LwU32    size;               // size of this structure
         LwU32    feature_mask;       // specify BIOS Cert feature supported that can not be implied by CTRL Structure Version,  Such as FRTS that roll out before this control structure.
         LwU32    rsvd;               // Reserved field
         LwU32    size_certblock;     // size of cert block.  PCI and NBSI containers for cert block is not included in this size
         LwU32    metadata_offset;    // offset of metadata structure. offset starts from start of BIOS Cert block,  or the instance of this header
         LwU32    cert_offset;        // Offset of cert linked list.  offset starts from start of BIOS Cert block,  or the instance of this header
         LwU32    hat_offset;         // Offset of start of hat table
         LwU32    signature_offset;   // Offset of start of external signature block
         LwU32    int_blk_offset;     // Offset of Internal block
} BCRT2X_CERT_CONTROL_HEADER_S, *BCRT2X_CERT_CONTROL_HEADER_SPTR;


#define  LW_BCRT2X_METADATA_HEADER_MAX_SIZE        64

#define  LW_BCRT2X_METADATA_HEADER_VERSION        7:0
#define  LW_BCRT2X_METADATA_HEADER_VERSION_VER_1    1
typedef  struct {
         LwU32    version;           // Metadata header version
         LwU32    size;              // size of this structure
         BCRT2X_PROJ_ID_S  proj_id;  // IDs for VBIOS ROM's project,  partner and SESSIONID
} BCRT2X_METADATA_HEADER_S, *BCRT2X_METADATA_HEADER_SPTR;

// Assess this requirement for packing.
//   All structures following are either 32-bit quantities or
//     64-bit quantities following even multiples of 32-bit parameters.
//   These should naturally align under normal cirlwmstances.

// x509 cert link list support
#define MAX_X509_ALLOWED_CERT_COUNT 50

#define X509CERT_CTRL_VERSION   0x0100
#define ANY_SIZE        1       // this is just place holder

#define X509CERT_TYPE_MASTER           BIT(0)
#define X509CERT_TYPE_CHIP             BIT(1)
#define X509CERT_TYPE_FEATURE          BIT(2)
#define X509CERT_TYPE_DEBUG            BIT(3)
#define X509CERT_TYPE_DEBUG_AE         BIT(4)
#define X509CERT_TYPE_LW_AUTHENTICATE  BIT(5)
#define X509CERT_TYPE_PARTNER          BIT(6)
#define X509CERT_TYPE_PARTNER_DEBUG    BIT(7)
#define X509CERT_TYPE_XOC              BIT(8)
#define X509CERT_TYPE_ILWALID          0x0

#define X509CERT_TYPE_VENDOR_MASK      ( X509CERT_TYPE_DEBUG | X509CERT_TYPE_DEBUG_AE | X509CERT_TYPE_LW_AUTHENTICATE | X509CERT_TYPE_PARTNER | X509CERT_TYPE_PARTNER_DEBUG | X509CERT_TYPE_XOC )

#define X509CERT_FORMAT_PEM                          0x1
#define X509CERT_FORMAT_DER                          0x2
#define LW_BCRT20_CERT_FORMAT_CERT20_MINOR_VER     27:24        // Cert Minor Version
#define LW_BCRT20_CERT_FORMAT_CERT20_MINOR_VER_0       0
#define LW_BCRT20_CERT_FORMAT_CERT20_MINOR_VER_1       1
#define LW_BCRT20_CERT_FORMAT_CERT20_MINOR_VER_2       2
#define LW_BCRT20_CERT_FORMAT_CERT20_MAJOR_VER     31:28        // Cert Major version,  reserved for after Cert2.0
#define BCRT20_CERT_FORMAT_CERT20                 BIT(31)       // Added for identify Cert2.0.

#define BCRT_FLAG_FIRST_X509       BIT(0)
#define BCRT_FLAG_LAST_X509        BIT(1)
#define BCRT_FLAG_SYSTEM_MASK     ( BCRT_FLAG_FIRST_X509 | BCRT_FLAG_SYSTEM_MASK )
#define BCRT_FLAG_X509_VINTAGE     BIT(31)

typedef  struct _BCRT_X509_HEADER {
         LwU32    version;            // X509CERT_CTRL_VERSION
         LwU32    size;               // size of this structure
         LwU32    prev_cert;          // offset of previous x509 cert. 0 if first one
         LwU32    next_cert;          // offset of next x509 cert or beginning of next block (hash filed) for the last x509 cert
         LwU32    cert_type;          // Verdor cert | Master Cert | Feature Cert
         LwU32    cert_format;        // Encoding format  DER | PEM
         LwU32    flag;               // Set BCRT_LAST_X509 flag if reached last x509 Cert record.
} BCRT_X509_HEADER;

// ---- end of x509 cert link list support

// Cert2.0 cert link list support
#define MAX_ALLOWED_CERT_COUNT 50

#define BCRT_CTRL_VERSION   0x0200
#define ANY_SIZE        1       // this is just place holder

#define LW_BCRT20_CERTBLK_CERT_TYPE                 15:0
#define LW_BCRT20_CERTBLK_CERT_TYPE_ILWALID            0
#define LW_BCRT20_CERTBLK_CERT_TYPE_ROOT               1
#define LW_BCRT20_CERTBLK_CERT_TYPE_MASTER             2
#define LW_BCRT20_CERTBLK_CERT_TYPE_LWD                3
#define LW_BCRT20_CERTBLK_CERT_TYPE_AED                4
#define LW_BCRT20_CERTBLK_CERT_TYPE_LWA                5
#define LW_BCRT20_CERTBLK_CERT_TYPE_PP                 6
#define LW_BCRT20_CERTBLK_CERT_TYPE_PD                 7
#define LW_BCRT20_CERTBLK_CERT_TYPE_XOC                8
#define LW_BCRT20_CERTBLK_CERT_TYPE_LWL                9
#define LW_BCRT20_CERTBLK_CERT_TYPE_INTERNAL          10

#define BCRT20_FLAG_FIRST_CERT       BIT(0)
#define BCRT20_FLAG_LAST_CERT        BIT(1)
#define BCRT20_FLAG_ZBX_SIGNING_CERT BIT(2)
#define BCRT20_FLAG_PROJ_ID_PRESENT  BIT(3)
#define BCRT20_FLAG_SYSTEM_MASK     ( BCRT20_FLAG_FIRST_CERT | BCRT20_FLAG_LAST_CERT | BCRT20_FLAG_ZBX_SIGNING_CERT )
#define BCRT20_FLAG_CERT_VINTAGE     BIT(31)

// Macro to colwert Cert1.0 style Certificate type to Cert2.0 style Certificate Type
#define COLWERT_CERTTYPE_1TO2(x) \
    x & X509CERT_TYPE_MASTER? LW_BCRT20_CERTBLK_CERT_TYPE_MASTER : \
    x & X509CERT_TYPE_DEBUG ? LW_BCRT20_CERTBLK_CERT_TYPE_LWD : \
    x & X509CERT_TYPE_DEBUG_AE ? LW_BCRT20_CERTBLK_CERT_TYPE_AED : \
    x & X509CERT_TYPE_LW_AUTHENTICATE ? LW_BCRT20_CERTBLK_CERT_TYPE_LWA : \
    x & X509CERT_TYPE_PARTNER ? LW_BCRT20_CERTBLK_CERT_TYPE_PP : \
    x & X509CERT_TYPE_PARTNER_DEBUG ? LW_BCRT20_CERTBLK_CERT_TYPE_PD : \
    x & X509CERT_TYPE_XOC ? LW_BCRT20_CERTBLK_CERT_TYPE_XOC : LW_BCRT20_CERTBLK_CERT_TYPE_ILWALID


typedef  struct {
         LwU32    version;            // BCRT20_CTRL_VERSION
         LwU32    size;               // size of this structure
         LwU32    prev_cert;          // offset of previous x509 cert. 0 if first one
         LwU32    next_cert;          // offset of next x509 cert or beginning of next block (hash filed) for the last x509 cert
         LwU32    cert_type;          // Verdor cert | Master Cert | Feature Cert
         LwU32    cert_format;        //
         LwU32    flag;               // Set BCRT20_FLAG_LAST_CERT flag if reached last Cert record.
} BCRT20_CERT_HEADER;

// end of Cert2.0 cert link list support

#define BCRT_SIGNATURE_HEADER_VERSION      0x1
#define BCRT_SIGNATURE_HEADER_VERSION_V2   0x2

#define BCRT_DIGEST_ALGO_DMHash            BIT(0)        // Lwrrently DM Hash is only one supported
#define BCRT_DIGEST_ALGO_SHA1              BIT(1)
#define LW_BCRT_SIG_HEADER_ALGO_CRYPTO                     15:8
#define LW_BCRT_SIG_HEADER_ALGO_CRYPTO_AES                   1
#define LW_BCRT_SIG_HEADER_ALGO_CRYPTO_RSA1024               2
#define LW_BCRT_SIG_HEADER_ALGO_CRYPTO_RSA2048               3
#define LW_BCRT_SIG_HEADER_ALGO_CRYPTO_RSA3072               4
#define LW_BCRT_SIG_HEADER_ALGO_CRYPTO_ECC256                5
#define BCRT_DIGEST_ALGO_IMAGE_FLW128      BIT(16)       // image hash for VBIOS runtime check, Lwrrently FLW only


#define BCRT_SIG_FORMAT_BINARY  BIT(0)     // Deprecated
#define BCRT_SIG_FORMAT_HEX     BIT(1)     // Deprecated

#define BCRT_SIG_FLAG_LWR_SEPARATE              BIT(0)        // For LWR Signing, when set different individual ROMs will use diffrent set of x508 Certs for signing.
#define BCRT_SIG_FLAG_FLW_ABSENT                BIT(1)        // Indicate FLW hash is absent in cert block.
#define BCRT_SIG_FLAG_INTERNAL_BLOCK_PRESENT    BIT(2)        // Indicate the presence of Internal Cert Block.  VDPA table and Internal Signatures are part of Internal cert block.
#define BCRT_SIG_FLAG_INTERNAL_BLOCK            BIT(3)        // Indicate this is internal block

typedef  struct _BCRT_SIGNATURE_HEADER {
        LwU32    version;
        LwU32    size;               // size of this structure
        LwU32    algo;               // Key and hash algo
        LwU32    rsvd0;              //
        LwU32    sig_size;           // size of the signature, With RSA 1024bit key, size = 128bytes
        LwU32    vbios_hash_size;    // Lwrrently FLW-128 hash, size = 128/8 = 16
        LwU64    timestamp;          // The timestamp of the cert block generating with seconds elapse since 1970 Jan 1.
        LwU32    flag;

} BCRT_SIGNATURE_HEADER;

#define LW_BCRT20_SIG_HEADER_ALGO_HASH                        7:0     // Hash used for signature of this block
#define LW_BCRT20_SIG_HEADER_ALGO_HASH_DMHASH                  1
#define LW_BCRT20_SIG_HEADER_ALGO_HASH_SHA256                  2
#define LW_BCRT20_SIG_HEADER_ALGO_CRYPTO                     15:8
#define LW_BCRT20_SIG_HEADER_ALGO_CRYPTO_AES                   1
#define LW_BCRT20_SIG_HEADER_ALGO_CRYPTO_RSA1024               2
#define LW_BCRT20_SIG_HEADER_ALGO_CRYPTO_RSA2048               3
#define LW_BCRT20_SIG_HEADER_ALGO_CRYPTO_RSA3072               4
#define LW_BCRT20_SIG_HEADER_ALGO_CRYPTO_ECC256                5

#define LW_BCRT20_SIG_HEADER_FLAG_INTBLK_CRYPTO                     15:8

#define LW_BCRT20_SIG_HEADER_FLAG_LWR_SEPARATE               0:0     // Deprecated
#define LW_BCRT20_SIG_HEADER_FLAG_FLW_ABSENT                 1:1     // Indicate FLW hash is absent in cert block.
#define LW_BCRT20_SIG_HEADER_FLAG_FLW_ABSENT_YES              1      // Yes FLW hash is absent in cert block.
#define LW_BCRT20_SIG_HEADER_FLAG_FLW_ABSENT_NO               0      // No FLW hash is not absent in cert block.
#define LW_BCRT20_SIG_HEADER_FLAG_INTBLK_PRESENT             2:2     // Indicate the presence of Internal Cert Block.  VDPA table and Internal Signatures are part of Internal cert block.
#define LW_BCRT20_SIG_HEADER_FLAG_INTBLK_PRESENT_NO           0      // No, there is no internal block in Cert Block
#define LW_BCRT20_SIG_HEADER_FLAG_INTBLK_PRESENT_YES          1      // Yes, internal block is present in Cert Block
#define LW_BCRT20_SIG_HEADER_FLAG_INTBLK_BLOCK               3:3     // Indicate this is internal block
#define LW_BCRT20_SIG_HEADER_FLAG_INTBLK_BLOCK_NO             0      // No,   It is not internal block
#define LW_BCRT20_SIG_HEADER_FLAG_INTBLK_BLOCK_YES            1      // Yes,  It is internal block
#define LW_BCRT20_SIG_HEADER_FLAG_INTBLK_SIG_VALID           4:4     // Indicate this is internal block
#define LW_BCRT20_SIG_HEADER_FLAG_INTBLK_SIG_VALID_NO         0      // No,   The signature is not valid
#define LW_BCRT20_SIG_HEADER_FLAG_INTBLK_SIG_VALID_YES        1      // Yes,  The signature is valid
#define LW_BCRT20_SIG_HEADER_FLAG_NEXT_SIG_AVAILABLE         5:5     // Indicate there is another signature behind this signature
#define LW_BCRT20_SIG_HEADER_FLAG_NEXT_SIG_AVAILABLE_NO       0      // No,   The signature is the last one
#define LW_BCRT20_SIG_HEADER_FLAG_NEXT_SIG_AVAILABLE_YES      1      // Yes,  there is signature behind this signature
#define LW_BCRT20_SIG_HEADER_FLAG_SEC_ZONE                  15:8     // Security Zone for this signature.  Only used for LS signature
#define LW_BCRT20_SIG_HEADER_FLAG_PKC_KEY_INDEX             31:16    // This index used to find the public key for the PKC signature in the key chain

typedef  struct {
        LwU32    version;
        LwU32    size;               // size of this structure
        LwU32    algo;               // Hash/Crypto used for the signature that controlled by this header
        LwU64    timestamp;          // The timestamp of the cert block generating with seconds elapse since 1970 Jan 1.
        LwU32    flag;               // security zone, PKC key index if used, and etc.
        LwU32    rsvd0;
} BCRT20_SIGNATURE_HEADER_V2;

typedef struct {
        BCRT20_SIGNATURE_HEADER_V2  sigheader;
        LwU32                       sig[8];          // sig[0..3] Production Signature,  sig[4..7] Debug Signature
} BCRT2X_LS_AES128_SIG_STRUCT_S, *BCRT2X_LS_AES128_SIG_STRUCT_SPTR;


#define LW_BCRT_HAT_HEADER_VERSION_V1                      0x00000001
// Version 2 requires support for the new HAT entry info field,
// specifically BASE_INSTANCE and BASE_CODE_TYPE as the IMAGE_INDEX
// and TYPE fields are backwards compatible
#define LW_BCRT_HAT_HEADER_VERSION_V2                      0x00000002

#define LW_BCRT_HAT_HEADER_COUNT_NORM_ENTRY_COUNT           7:0
#define LW_BCRT_HAT_HEADER_COUNT_FMT_ENTRY_COUNT           15:8
// This is the structure used to put header into BIOS Cert image
typedef  struct {
        LwU32    version;
        LwU32    size;               // size of this structure
        LwU32    count;              // number of BCRT_HAT_ENTRY entries follows this header,  both Normal Entries and Entries with Formatter
        LwU32    flag;               // Lwrrently reserved. Should set to 0
} BCRT_HAT_HEADER;

// This is for packed Runtime ROM to track preservation areas
#define LW_BCRT_HASH_INFO_IMAGE_INDEX                             7:0

// In HAT version 2, type is now an enumeration instead of bit flags. The
// initial values were defined to be backwards compatible. New types may use
// any unused 8-bit value.

// TYPE_NBSI_BCRT is for hashable area inside BIOS Cert block.
// Note: If a BCRT_HAT_ENTRY entry with this type, the start_offset would be
//       the offset from end of NBSI_GLOB_HEADER structure. This provision is
//       for cases of users adding NBSI blockes for a signed VBIOS.

#define LW_BCRT_HASH_INFO_TYPE                                   15:8
#define LW_BCRT_HASH_INFO_TYPE_PRESERVATION                0x00000001
#define LW_BCRT_HASH_INFO_TYPE_PRESERVATION_SIGNATURE      0x00000002
#define LW_BCRT_HASH_INFO_TYPE_PRESERVATION_FLW_HASH       0x00000004
#define LW_BCRT_HASH_INFO_TYPE_IFR                         0x00000007
#define LW_BCRT_HASH_INFO_TYPE_NORMAL                      0x00000008
#define LW_BCRT_HASH_INFO_TYPE_NBSI_BCRT                   0x00000010
#define LW_BCRT_HASH_INFO_TYPE_NBSI_VDPA                   0x00000011
#define LW_BCRT_HASH_INFO_TYPE_INTERNAL_NORMAL             0x00000012
#define LW_BCRT_HASH_INFO_TYPE_INTERNAL_FOMATTER           0x00000013


#define LW_BCRT_HASH_INFO_RESERVED                              19:16

// BASE_INSTANCE specifies which oclwrence of a PCI ROM image with
// BASE_CODE_TYPE to use as the relative base for start_offset.
#define LW_BCRT_HASH_INFO_BASE_INSTANCE                         23:20

// BASE_CODE_TYPE specifies the code type of the PCI ROM image to use as the
// relative base for start_offset.
#define LW_BCRT_HASH_INFO_BASE_CODE_TYPE                        31:24

typedef  struct {
        LwU32    start_offset;       // starting offset relative to base codeType/instance
        LwU32    size;               // size of this hash area
        LwU32    info;               // hash area information
} BCRT_BASE_HAT_ENTRY;

#define LW_BCRT_HASH_INFO_FMT_ATTRIB                           7:0
#define LW_BCRT_HASH_INFO_FMT_ATTRIB_NORMAL                      0        // formatter is used to describ the areas that covered by hash
#define LW_BCRT_HASH_INFO_FMT_ATTRIB_ILWERT                      1        // formatter is used to describ the areas that skips the hash coverage

// BCRT_FMT_HAT_ENTRY is with a fomatter,  or format string at end of it. When LW_BCRT_HASH_INFO_TYPE with _INTERNAL_FOMATTER, this structure is used
//                    The formatter is used to describe VBIOS internal block with VDPA table,
//                    in which data need to be signed and not signed are mixed.
typedef  struct {
        LwU32    start_offset;       // starting offset relative to base codeType/instance
        LwU32    size;               // size of this hash area
        LwU32    info;               // hash area information
        LwU32    fmt_len;            // length of format string
        LwU32    fmt_attrib;         // formatter's attribute and etc.
        //LwU8   fmt[length of format string with arbitrary length];  //  fmt_len has the value of "length of format string with arbitrary length"
} BCRT_FMT_HAT_ENTRY, *BCRT_FMT_HAT_ENTRY_PTR;

// BCRT_FMT_HAT_ENTRY_PAIR is used for construction of BCRT_FMT_HAT_ENTRY and its formatter during implementation.
typedef  struct {
       BCRT_FMT_HAT_ENTRY  fmt_hat_entry;
       LwU8                formatter[MAX_CERT20_FORMATTER_LENGTH];
} BCRT_FMT_HAT_ENTRY_PAIR, *BCRT_FMT_HAT_ENTRY_PAIR_PTR;


#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_SIZE                 15:0
#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_ALGO                 23:16
#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_ALGO_RSA               0      // use value 0 for backward compatibility
#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_ALGO_ECC               1      // Lwrve type: prime256v1
#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE                 31:24
#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE_IMAGE             0      // use value 0 for backward compatibility
#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE_INTERNAL          1
#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE_SIZE_256        256
#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE_SIZE_1K         1024
#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE_SIZE_2K         1024*2
#define LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE_SIZE_3K         1024*3

// defines used to BIOSMOD script SELWRITYZONE keyword related parameters
#define LW_BCRT2X_SELWRITYZONESELECTION_ALGO_HASH                        7:0
#define LW_BCRT2X_SELWRITYZONESELECTION_ALGO_HASH_DMHASH                  1
#define LW_BCRT2X_SELWRITYZONESELECTION_ALGO_HASH_SHA256                  2
#define LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO                      15:8
#define LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO_AES128                1
#define LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO_RSA1024               2
#define LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO_RSA2048               3
#define LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO_RSA3072               4
#define LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO_ECDSA256              5

#define LW_BCRT2X_SELWRITYZONESELECTION_FLAG_KEYSET_INDEX                7:0
#define LW_BCRT2X_SELWRITYZONESELECTION_FLAG_KEYSET_INDEX_0               0
#define LW_BCRT2X_SELWRITYZONESELECTION_FLAG_KEYSET_INDEX_1               1


#if defined(LW_FALCON) || defined(GCC_FALCON)
#pragma pack()
#else
#pragma pack(pop)
#endif

#endif    // __CERT20_H__INCLUDED_
