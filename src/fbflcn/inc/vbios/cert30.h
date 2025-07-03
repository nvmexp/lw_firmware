#if !defined(__CERT30_H__INCLUDED_)
#define __CERT30_H__INCLUDED_

#if defined(LW_FALCON) || defined(GCC_FALCON)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

#include "cert20.h"

/*-------------------------------------------------------*/
/*                                                       */
/* Certificate structures and defines                    */
/*                                                       */
/*-------------------------------------------------------*/

#define BCRT30_MAX_NAME_LENGTH              MAX_NAME_LENGTH              //   32
#define BCRT30_IMAGE_VERSION_STRING_LENGTH  IMAGE_VERSION_STRING_LENGTH  //   20

#define BCRT30_SHA256_SIZE             BCRT20_SHA256_SIZE                //   32
#define BCRT30_DMHASH_SIZE             BCRT20_DMHASH_SIZE                //   16
#define BCRT30_FLW128HASH_SIZE         BCRT20_FLW128HASH_SIZE            //   16
#define BCRT30_AES128_SIZE             BCRT20_AES128_SIZE                //   BCRT20_DMHASH_SIZE
#define BCRT30_AES256_SIZE             (BCRT30_AES128_SIZE * 2)
#define BCRT30_3AES128_SIG_SIZE        BCRT20_3AES128_SIG_SIZE           //   32
#define BCRT30_RSA_MODULUS             BCRT20_RSA_MODULUS                //   128
#define BCRT30_RSA1K_SIG_SIZE          BCRT20_RSA1K_SIG_SIZE             //   128
#define BCRT30_RSA2K_SIG_SIZE          BCRT20_RSA2K_SIG_SIZE             //   256
#define BCRT30_RSA3K_MODULUS           BCRT20_RSA3K_MODULUS              //   384
#define BCRT30_RSA3K_SIG_SIZE          BCRT20_RSA3K_SIG_SIZE             //   384
#define BCRT30_RSA3K_SIG_BITS_SIZE     BCRT20_RSA3K_SIG_BITS_SIZE        //   8 * BCRT20_RSA3K_SIG_SIZE
#define BCRT30_RSA4K_SIG_SIZE          BCRT20_RSA4K_SIG_SIZE             //   512
#define BCRT30_RSA4K_SIG_BITS_SIZE     BCRT20_RSA4K_SIG_BITS_SIZE        //   8 * BCRT20_RSA4K_SIG_SIZE
#define BCRT30_ECC256_SIG_SIZE         BCRT20_ECC256_SIG_SIZE            //   32
#define BCRT30_RSAPADDING_TYPE_PKCS1   BCRT20_RSAPADDING_TYPE_PKCS1      //    1
#define BCRT30_RSAPADDING_TYPE_PSS     BCRT20_RSAPADDING_TYPE_PSS        //    2

#define BCRT30_MAX_CERT_COUNT           MAX_CERT20_CERT_COUNT            //   5
#define BCRT30_MAX_SIG_COUNT            MAX_CERT20_SIG_COUNT             //   5

#define BCRT30_MAX_VDPA_COUNT           MAX_CERT20_VDPA_COUNT            //   1
#define BCRT30_MAX_FORMATTER_LENGTH     MAX_CERT20_FORMATTER_LENGTH      //   256
#define BCRT30_MAX_FORMATTER_DIGITS     MAX_CERT20_FORMATTER_DIGITS      //   8

#define BCRT30_MAX_EXT_IMAGE_INSTANCES  MAX_EXT_IMAGE_INSTANCES          //   2

#define BCRT30_RAW_ECID_SIZE            BCRT20_RAW_ECID_SIZE             //   16

#define LW_BCRT30_PCI_ROM_CODE_TYPE_X86    0x00
#define LW_BCRT30_PCI_ROM_CODE_TYPE_EFI    0x03
#define LW_BCRT30_PCI_ROM_CODE_TYPE_IFR    0x4E                          // note: IFR is not a PCI ROM,  but we need a value to differentiate IFR from PCI ROM X86 Code Type
#define LW_BCRT30_PCI_ROM_CODE_TYPE_EXT    0xE0
#define LW_BCRT30_PCI_ROM_CODE_TYPE_NBSI   0x70


/* Certificate main body  */

// typedef struct {
//    char pubKeyArray[BCRT20_RSA_MODULUS];
//    LwU32 Exponent;            /*  Store Key length, Algorithm Info and etc.   */
// } BCRT20RSA1KPUBKEY_S, *BCRT20RSA1KPUBKEY_SPTR;
#define  BCRT30RSA1KPUBKEY_S     BCRT20RSA1KPUBKEY_S
#define  BCRT30RSA1KPUBKEY_SPTR  BCRT20RSA1KPUBKEY_SPTR

// typedef struct {
//    char pubKeyArray[BCRT20_RSA3K_MODULUS];
//    LwU32 Exponent;            /*  Store Key length, Algorithm Info and etc.   */
// } BCRT20RSA3KPUBKEY_S, *BCRT20RSA3KPUBKEY_SPTR;

#define  BCRT30RSA3KPUBKEY_S     BCRT20RSA3KPUBKEY_S
#define  BCRT30RSA3KPUBKEY_SPTR  BCRT20RSA3KPUBKEY_SPTR

/* Defines for algoType field */
#define    LW_BCRT30_ALGOTYPE_HASHTYPE                   LW_BCRT20_ALGOTYPE_HASHTYPE                   //   7:0
#define    LW_BCRT30_ALGOTYPE_HASHTYPE_DMOVERAES128      LW_BCRT20_ALGOTYPE_HASHTYPE_DMOVERAES128      //     1
#define    LW_BCRT30_ALGOTYPE_HASHTYPE_SHA256            LW_BCRT20_ALGOTYPE_HASHTYPE_SHA256            //     2
#define    LW_BCRT30_ALGOTYPE_CRYPTOTYPE                 LW_BCRT20_ALGOTYPE_CRYPTOTYPE                 //  15:8
#define    LW_BCRT30_ALGOTYPE_CRYPTOTYPE_RSA1024         LW_BCRT20_ALGOTYPE_CRYPTOTYPE_RSA1024         //     1
#define    LW_BCRT30_ALGOTYPE_CRYPTOTYPE_RSA3072         LW_BCRT20_ALGOTYPE_CRYPTOTYPE_RSA3072         //     2
#define    LW_BCRT30_ALGOTYPE_CRYPTOTYPE_ECDSA256        LW_BCRT20_ALGOTYPE_CRYPTOTYPE_ECDSA256        //     3
#define    LW_BCRT30_ALGOTYPE_CRYPTOTYPE_RSAPSS3072      4
#define    LW_BCRT30_ALGOTYPE_HASHTYPE_ALT               LW_BCRT20_ALGOTYPE_HASHTYPE_ALT               // 23:16
#define    LW_BCRT30_ALGOTYPE_HASHTYPE_ALT_DMOVERAES128  LW_BCRT20_ALGOTYPE_HASHTYPE_ALT_DMOVERAES128  //     1
#define    LW_BCRT30_ALGOTYPE_HASHTYPE_ALT_SHA256        LW_BCRT20_ALGOTYPE_HASHTYPE_ALT_SHA256        //     2
#define    LW_BCRT30_ALGOTYPE_CRYPTOTYPE_ALT             LW_BCRT20_ALGOTYPE_CRYPTOTYPE_ALT             // 31:24
#define    LW_BCRT30_ALGOTYPE_CRYPTOTYPE_ALT_AES128      LW_BCRT20_ALGOTYPE_CRYPTOTYPE_ALT_AES128      //     1
#define    LW_BCRT30_ALGOTYPE_CRYPTOTYPE_ALT_3AES128     LW_BCRT20_ALGOTYPE_CRYPTOTYPE_ALT_3AES128     //     2

/* Defines for flag field */                                                                           //
#define    LW_BCRT30_FLAG_EXTENSION_PRESENT              LW_BCRT20_FLAG_EXTENSION_PRESENT              //   0:0
#define    LW_BCRT30_FLAG_EXTENSION_PRESENT_NO           LW_BCRT20_FLAG_EXTENSION_PRESENT_NO           //     0
#define    LW_BCRT30_FLAG_EXTENSION_PRESENT_YES          LW_BCRT20_FLAG_EXTENSION_PRESENT_YES          //     1

#define    LW_BCRT30_FLAG_PUBKEY_PRESENT                 LW_BCRT20_FLAG_PUBKEY_PRESENT                 //   1:1
#define    LW_BCRT30_FLAG_PUBKEY_PRESENT_NO              LW_BCRT20_FLAG_PUBKEY_PRESENT_NO              //     0
#define    LW_BCRT30_FLAG_PUBKEY_PRESENT_YES             LW_BCRT20_FLAG_PUBKEY_PRESENT_YES             //     1

#define    LW_BCRT30_FLAG_PROD_ALTSIG_PRESENT            LW_BCRT20_FLAG_PROD_ALTSIG_PRESENT            //   2:2
#define    LW_BCRT30_FLAG_PROD_ALTSIG_PRESENT_NO         LW_BCRT20_FLAG_PROD_ALTSIG_PRESENT_NO         //     0
#define    LW_BCRT30_FLAG_PROD_ALTSIG_PRESENT_YES        LW_BCRT20_FLAG_PROD_ALTSIG_PRESENT_YES        //     1

#define    LW_BCRT30_FLAG_DEBUG_ALTSIG_PRESENT           LW_BCRT20_FLAG_DEBUG_ALTSIG_PRESENT           //   3:3
#define    LW_BCRT30_FLAG_DEBUG_ALTSIG_PRESENT_NO        LW_BCRT20_FLAG_DEBUG_ALTSIG_PRESENT_NO        //     0
#define    LW_BCRT30_FLAG_DEBUG_ALTSIG_PRESENT_YES       LW_BCRT20_FLAG_DEBUG_ALTSIG_PRESENT_YES       //     1


#define    LW_BCRT30_TYPE_CERT                           LW_BCRT20_TYPE_CERT                           //  23:0
#define    LW_BCRT30_TYPE_CERT_ROOT                      LW_BCRT20_TYPE_CERT_ROOT                      //     1
#define    LW_BCRT30_TYPE_CERT_MASTER                    LW_BCRT20_TYPE_CERT_MASTER                    //     2
#define    LW_BCRT30_TYPE_CERT_LWD                       LW_BCRT20_TYPE_CERT_LWD                       //     3
#define    LW_BCRT30_TYPE_CERT_AED                       LW_BCRT20_TYPE_CERT_AED                       //     4
#define    LW_BCRT30_TYPE_CERT_LWA                       LW_BCRT20_TYPE_CERT_LWA                       //     5
#define    LW_BCRT30_TYPE_CERT_PP                        LW_BCRT20_TYPE_CERT_PP                        //     6
#define    LW_BCRT30_TYPE_CERT_PD                        LW_BCRT20_TYPE_CERT_PD                        //     7
#define    LW_BCRT30_TYPE_CERT_XOC                       LW_BCRT20_TYPE_CERT_XOC                       //     8
#define    LW_BCRT30_TYPE_CERT_LWL                       LW_BCRT20_TYPE_CERT_LWL                       //     9
#define    LW_BCRT30_TYPE_CERT_BASELINE                  LW_BCRT20_TYPE_CERT_BASELINE                  //    10
#define    LW_BCRT30_TYPE_CERT_PSDL                      LW_BCRT20_TYPE_CERT_PSDL                      //    11
#define    LW_BCRT30_TYPE_CERT_HULK                      LW_BCRT20_TYPE_CERT_HULK                      //    12      // Type 12 is for Post Devinit Licenses
#define    LW_BCRT30_TYPE_CERT_LWF_ENG                   LW_BCRT20_TYPE_CERT_LWF_ENG                   //    13
#define    LW_BCRT30_TYPE_CERT_PARTNER_READ_ONLY         LW_BCRT20_TYPE_CERT_PARTNER_READ_ONLY         //    14
#define    LW_BCRT30_TYPE_CERT_HULK_PRE_DEVINIT          LW_BCRT20_TYPE_CERT_HULK_PRE_DEVINIT          //    15      // New HULK type for pre devinit licenses
#define    LW_BCRT30_TYPE_CERT_FEATURE_IST               16                                            // IST  Feature Certificate

#define    LW_BCRT30_TYPE_INSTANCE                       LW_BCRT20_TYPE_INSTANCE                       //  31:24    // Only used for KEY ID

#define    BCRT30_IS_FEATURE_CERT( x ) ( ( (x) == LW_BCRT30_TYPE_CERT_FEATURE_IST) )
#define    BCRT30_IS_VENDOR_CERT                         VENDOR_CERT_BOOL
#define    BCRT30_IS_MASTER_CERT                         MASTER_CERT_BOOL


// typedef struct {
//   LwU32           version;                         /* Version of Certificate structure  */
//   LwU32           size;                            /* Size of this struct               */
//   LwU32           serialNumber;                    /* serial Number of the Certificate  */
//   LwU32           sizeOfCertificate;               /* size of this certificate          */
//   LwU32           type;                            /* Certificate Type                  */
//   LwU32           algoType;                        /* Algorithm Type (DM Hash + RSA)    */
//   LwU32           flag;                            /* Flags for additional info         */
//   LwU32           issuerID;                        /* Issuer's ID                       */
//   LwU32           certOwnerID;                     /* Certificate owner's ID            */
//   //char            certOwnerName[MAX_NAME_LENGTH];  /* Certificate owner name string, null ended, Informational only should match with CertOwnerID */
//   LwU32           notBefore;                       /* Not valid before this time        */
//   LwU32           notAfter;                        /* Not Valid after this date         */
//   LwU8            pubKeyArray[BCRT20_RSA_MODULUS];
//   LwU32           Exponent;                                  /*  Store Key length, Algorithm Info and etc.   */
// //  BCRT20RSA1KPUBKEY  pubKeyRSA1K;                /* RSA 1024 Public Key               */
// } BCRT20CertMainRSA1024_S, *BCRT20CertMainRSA1024_SPTR;

#define    BCRT30CertMainRSA1024_S     BCRT20CertMainRSA1024_S
#define    BCRT30CertMainRSA1024_SPTR  BCRT20CertMainRSA1024_SPTR

// typedef struct {
//   LwU32           version;                         /* Version of Certificate structure  */
//   LwU32           size;                            /* Size of this struct               */
//   LwU32           serialNumber;                    /* serial Number of the Certificate  */
//   LwU32           sizeOfCertificate;               /* size of this certificate          */
//   LwU32           type;                            /* Certificate Type                  */
//   LwU32           algoType;                        /* Algorithm Type (DM Hash + RSA)    */
//   LwU32           flag;                            /* Flags for additional info         */
//   LwU32           issuerID;                        /* Issuer's ID                       */
//   LwU32           certOwnerID;                     /* Certificate owner's ID            */
//   //char            certOwnerName[MAX_NAME_LENGTH];  /* Certificate owner name string, null ended, Informational only should match with CertOwnerID */
//   LwU32           notBefore;                       /* Not valid before this time        */
//   LwU32           notAfter;                        /* Not Valid after this date         */
// } BCRT20CertMainNoPKCS_S, *BCRT20CertMainNoPKCS_SPTR;

#define    BCRT30CertMainNoPKCS_S       BCRT20CertMainNoPKCS_S
#define    BCRT30CertMainNoPKCS_SPTR    BCRT20CertMainNoPKCS_SPTR

/* Extension Linked list,  should locate right after BCRT20CertMain
 * Extersions are used  to define NTP Server, Security Level,  Debug Certificate Validity,
 * Vendor Privilege and other Lw Policy related information
*/

// typedef struct {
//     LwU32    version;            /*  arbitrary extension version */
//     LwU32    size;               /*  size of this structure  */
//     LwU32    entry_size;         /*  entry control header struct size */
//     LwU32    entryNum;           /*  number of extension entries */
// }BCRT20CertExtHeader_S, *BCRT20CertExtHeader_SPTR;

#define    BCRT30CertExtHeader_S     BCRT20CertExtHeader_S
#define    BCRT30CertExtHeader_SPTR  BCRT20CertExtHeader_SPTR

#define  LW_BCRT30_EXT_FLAG_FIRST_ENTRY                                 LW_BCRT20_EXT_FLAG_FIRST_ENTRY                         // 1
#define  LW_BCRT30_EXT_FLAG_LAST_ENTRY                                  LW_BCRT20_EXT_FLAG_LAST_ENTRY                          // 2

#define  LW_BCRT30_CERT_EXTENSION_TYPE_NTP                              LW_BCRT20_CERT_EXTENSION_TYPE_NTP                      // 1
#define  LW_BCRT30_CERT_EXTENSION_TYPE_BIOSMOD_SELWRITY_LEVEL           LW_BCRT20_CERT_EXTENSION_TYPE_BIOSMOD_SELWRITY_LEVEL   // 2
#define  LW_BCRT30_CERT_EXTENSION_TYPE_EXPIRATION_DAYS                  LW_BCRT20_CERT_EXTENSION_TYPE_EXPIRATION_DAYS          // 3
#define  LW_BCRT30_CERT_EXTENSION_TYPE_VENDOR_PRIVILEGE                 LW_BCRT20_CERT_EXTENSION_TYPE_VENDOR_PRIVILEGE         // 4
#define  LW_BCRT30_CERT_EXTENSION_TYPE_VENDOR_NAME_INFO                 LW_BCRT20_CERT_EXTENSION_TYPE_VENDOR_NAME_INFO         // 5
#define  LW_BCRT30_CERT_EXTENSION_TYPE_ECID_HASH                        LW_BCRT20_CERT_EXTENSION_TYPE_ECID_HASH                // 6
#define  LW_BCRT30_CERT_EXTENSION_TYPE_GPU_PERSONALITY                  LW_BCRT20_CERT_EXTENSION_TYPE_GPU_PERSONALITY          // 7      // UGPU
#define  LW_BCRT30_CERT_EXTENSION_TYPE_PSDL                             LW_BCRT20_CERT_EXTENSION_TYPE_PSDL                     // 8      // Private Security Debug License
#define  LW_BCRT30_CERT_EXTENSION_TYPE_RANDOM_NUMBER                    LW_BCRT20_CERT_EXTENSION_TYPE_RANDOM_NUMBER            // 9      // For AES signature
#define  LW_BCRT30_CERT_EXTENSION_TYPE_GPUID                            LW_BCRT20_CERT_EXTENSION_TYPE_GPUID                    //10      // GPUID
#define  LW_BCRT30_CERT_EXTENSION_TYPE_DEVID                            LW_BCRT20_CERT_EXTENSION_TYPE_DEVID                    //11      // DevID

// typedef struct {
//     LwU32    prev_cert_ext;      /*  offset of previous cert extension. 0 if first one  */
//     LwU32    next_cert_ext;      /*  offset of next cert extension   */
//     LwU32    ext_type;           /*  Cert Extension type, NTP Serve, Security Level and etc   */
//     LwU32    flag;               /*  Mark the beginning, end of the link list and etc. */
// }BCRT20CertExtEntry_S, *BCRT20CertExtEntry_SPTR;

#define BCRT30CertExtEntry_S     BCRT20CertExtEntry_S
#define BCRT30CertExtEntry_SPTR  BCRT20CertExtEntry_SPTR

// Type 3 is Debug Certificate EXPIRATION_DAYS
#define BCRT30_EXT_TYPE3_STRUCT_SIZE   CERT20_EXT_TYPE3_STRUCT_SIZE
// typedef struct {
//     LwU32  days;           /* EXPIRATION_DAYS  */
// }BCRT20CertExtType3_S, *BCRT20CertExtType3_SPTR;

#define BCRT30CertExtType3_S    BCRT20CertExtType3_S
#define BCRT30CertExtType3_SPTR BCRT20CertExtType3_SPTR

//  Type 4 is VENDOR_PRIVILEGE
#define BCRT30_EXT_TYPE4_STRUCT_SIZE  CERT20_EXT_TYPE4_STRUCT_SIZE
// typedef struct {
//     LwU32  vp;           /* Vendor privilege  */
// }BCRT20CertExtType4_S, *BCRT20CertExtType4_SPTR;

#define BCRT30CertExtType4_S    BCRT20CertExtType4_S
#define BCRT30CertExtType4_SPTR BCRT20CertExtType4_SPTR

// type 5, vendor name.

// Type 6 is for ECID.  it is a 32byte field
#define BCRT30_EXT_TYPE6_MAX_EXT_NUM               CERT20_EXT_TYPE6_MAX_EXT_NUM            //  125   // number of ECID could one Cert have
                                                                                           //
#define LW_BCRT30_EXT6_ECID_DESC_ALGO              LW_BCRT20_EXT6_ECID_DESC_ALGO           // 15:0
#define LW_BCRT30_EXT6_ECID_DESC_ALGO_SHA2         LW_BCRT20_EXT6_ECID_DESC_ALGO_SHA2      //    1   // ECID encoded by SHA2
#define LW_BCRT30_EXT6_ECID_DESC_ALGO_DMHASH       LW_BCRT20_EXT6_ECID_DESC_ALGO_DMHASH    //    2   // ECID encoded by dmhash(davies meyer hash)
#define LW_BCRT30_EXT6_ECID_DESC_ALGO_RAW          LW_BCRT20_EXT6_ECID_DESC_ALGO_RAW       //    3   // ECID raw data

#define BCRT30_EXT_TYPE6_STRUCT_SHA2_SIZE          CERT20_EXT_TYPE6_STRUCT_SHA2_SIZE       // 36
// typedef struct {
//     LwU32 ecid_desc;
//     char  ecid[32];   /* SHA2 hash of ECID */
// }BCRT20CertExtType6SHA2ECID_S, *BCRT20CertExtType6SHA2ECID_SPTR;
#define BCRT30CertExtType6SHA2ECID_S              BCRT20CertExtType6SHA2ECID_S
#define BCRT30CertExtType6SHA2ECID_SPTR           BCRT20CertExtType6SHA2ECID_SPTR

// typedef struct {
//     LwU32 ecid_desc;
//     char  ecid[16];   /* DM Hash of ECID */
// }BCRT20CertExtType6DMHashECID_S, *BCRT20CertExtType6DMHashECID_SPTR;
#define BCRT30CertExtType6DMHashECID_S    BCRT20CertExtType6DMHashECID_S
#define BCRT30CertExtType6DMHashECID_SPTR BCRT20CertExtType6DMHashECID_SPTR

// Type 7 is GPU Personality

#define BCRT30_EXT_TYPE7_MAX_EXT_NUM                        CERT20_EXT_TYPE7_MAX_EXT_NUM                 // 8

#define BCRT30_EXT_TYPE7_STRUCT_VER0_SIZE                   CERT20_EXT_TYPE7_STRUCT_VER0_SIZE            // 8

#define BCRT30_EXT_TYPE7_PERSONALITY_GeForce                CERT20_EXT_TYPE7_PERSONALITY_GeForce         // 1
#define BCRT30_EXT_TYPE7_PERSONALITY_Quadro                 CERT20_EXT_TYPE7_PERSONALITY_Quadro          // 2
#define BCRT30_EXT_TYPE7_PERSONALITY_Tesla                  CERT20_EXT_TYPE7_PERSONALITY_Tesla           // 3
#define BCRT30_EXT_TYPE7_PERSONALITY_HULK                   CERT20_EXT_TYPE7_PERSONALITY_HULK            // 4

#define LW_BCRT30_EXT7_PROC_FLAG_LOG_REGOVERRIDE            LW_CERT20_EXT7_PROC_FLAG_LOG_REGOVERRIDE         // 0:0
#define LW_BCRT30_EXT7_PROC_FLAG_LOG_REGOVERRIDE_YES        LW_CERT20_EXT7_PROC_FLAG_LOG_REGOVERRIDE_YES     //   1       // Record register overrode as a log. Replacing CERT20_EXT_TYPE7_PROC_FLAG_LOG_REGOVERRIDE
#define LW_BCRT30_EXT7_PROC_FLAG_LOG_REGOVERRIDE_NO         LW_CERT20_EXT7_PROC_FLAG_LOG_REGOVERRIDE_NO      //   0       // Record register overrode as a log. Replacing CERT20_EXT_TYPE7_PROC_FLAG_LOG_REGOVERRIDE
#define LW_BCRT30_EXT7_PROC_FLAG_BSI_REGOVERRIDE            LW_CERT20_EXT7_PROC_FLAG_BSI_REGOVERRIDE         // 1:1
#define LW_BCRT30_EXT7_PROC_FLAG_BSI_REGOVERRIDE_YES        LW_CERT20_EXT7_PROC_FLAG_BSI_REGOVERRIDE_YES     //   1       // Processing Register Overrides to BRSS struc in BSI memory. Replacing CERT20_EXT_TYPE7_PROC_FLAG_PROCESS_BSI_REGOVERRIDE
#define LW_BCRT30_EXT7_PROC_FLAG_BSI_REGOVERRIDE_NO         LW_CERT20_EXT7_PROC_FLAG_BSI_REGOVERRIDE_NO      //   0       // Processing Register Overrides to BRSS struc in BSI memory. Replacing CERT20_EXT_TYPE7_PROC_FLAG_PROCESS_BSI_REGOVERRIDE

#define LW_BCRT30_EXT7_PROC_FLAG_ID_MATCHED_ECID            LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_ECID         // 2:2       //   Flag used to indicate status of ECID matching
#define LW_BCRT30_EXT7_PROC_FLAG_ID_MATCHED_ECID_YES        LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_ECID_YES     //   1
#define LW_BCRT30_EXT7_PROC_FLAG_ID_MATCHED_ECID_NO         LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_ECID_NO      //   0

#define LW_BCRT30_EXT7_PROC_FLAG_ID_ECID_EXT_PRESENT        LW_CERT20_EXT7_PROC_FLAG_ID_ECID_EXT_PRESENT     // 3:3       //   Flag used to indicate if ext6 for ECID exists
#define LW_BCRT30_EXT7_PROC_FLAG_ID_ECID_EXT_PRESENT_YES    LW_CERT20_EXT7_PROC_FLAG_ID_ECID_EXT_PRESENT_YES //   1
#define LW_BCRT30_EXT7_PROC_FLAG_ID_ECID_EXT_PRESENT_NO     LW_CERT20_EXT7_PROC_FLAG_ID_ECID_EXT_PRESENT_NO  //   0


#define LW_BCRT30_EXT7_PROC_FLAG_ID_MATCHED_DEVID           LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_DEVID        // 4:4       //   Flag used to indicate status of DevID matching
#define LW_BCRT30_EXT7_PROC_FLAG_ID_MATCHED_DEVID_YES       LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_DEVID_YES    //   1
#define LW_BCRT30_EXT7_PROC_FLAG_ID_MATCHED_DEVID_NO        LW_CERT20_EXT7_PROC_FLAG_ID_MATCHED_DEVID_NO     //   0

// typedef struct {
//     LwU8    major_personality; //  1 = VdChip, 2 = Lwdqro, 3 = Tesla.  2 bytes
//     LwU16   minor_personality;
//     LwU8    reserved;            //
//     LwU16   reg_override_num;    //  number of register overrides
//     LwU16   feat_table_version;  //  feature table data structure Version. 2 bytes
// }BCRT20CertExtType7_S, *BCRT20CertExtType7_SPTR;
#define BCRT30CertExtType7_S     BCRT20CertExtType7_S
#define BCRT30CertExtType7_SPTR  BCRT20CertExtType7_SPTR

#define LW_BCRT30_EXT_T7_QUADRO_VGPU_DEFAULT     EXT_T7_QUADRO_VGPU_DEFAULT     // 0    // feature kept as default
#define LW_BCRT30_EXT_T7_QUADRO_VGPU_ENABLE      EXT_T7_QUADRO_VGPU_ENABLE      // 1    // enalbe feature
#define LW_BCRT30_EXT_T7_QUADRO_VGPU_DISABLE     EXT_T7_QUADRO_VGPU_DISABLE     // 2    // disable feature
#define LW_BCRT30_EXT_T7_QUADRO_VGPU_MASK        EXT_T7_QUADRO_VGPU_MASK        // 3    //  mask

#define LW_BCRT30_EXT_TYPE7_QUADRO_STRUCTVER1_SIZE  CERT20_EXT_TYPE7_QUADRO_STRUCTVER1_SIZE   // 4
// typedef struct {
//     LwU32    features;           /* vgpu and etc.  */
// }BCRT20CertExtType7QuadroVer1_S, *BCRT20CertExtType7QuadroVer1_SPTR;
#define BCRT30CertExtType7QuadroVer1_S     BCRT20CertExtType7QuadroVer1_S
#define BCRT30CertExtType7QuadroVer1_SPTR  BCRT20CertExtType7QuadroVer1_SPTR

#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_SKIP_VBIOS_SIG                    LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_VBIOS_SIG             // 0:0
#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_SKIP_VBIOS_SIG_YES                LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_VBIOS_SIG_YES         // 1
#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_SKIP_VBIOS_SIG_NO                 LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_VBIOS_SIG_NO          // 0

#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_SKIP_DEVINIT_SIG                  LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_DEVINIT_SIG           // 1:1
#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_SKIP_DEVINIT_SIG_YES              LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_DEVINIT_SIG_YES       // 1
#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_SKIP_DEVINIT_SIG_NO               LW_BCRT20_EXT7_FEAT1_HULK_LWF_SKIP_DEVINIT_SIG_NO        // 0

#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_ALLOW_EEPROM_ERASE                LW_BCRT20_EXT7_FEAT1_HULK_LWF_ALLOW_EEPROM_ERASE         // 2:2
#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_ALLOW_EEPROM_ERASE_YES            LW_BCRT20_EXT7_FEAT1_HULK_LWF_ALLOW_EEPROM_ERASE_YES     // 1
#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_ALLOW_EEPROM_ERASE_NO             LW_BCRT20_EXT7_FEAT1_HULK_LWF_ALLOW_EEPROM_ERASE_NO      // 0

#define LW_BCRT30_EXT7_FEAT1_HULK_STRICT_ID_MATCH                       LW_BCRT20_EXT7_FEAT1_HULK_STRICT_ID_MATCH                // 3:3
#define LW_BCRT30_EXT7_FEAT1_HULK_STRICT_ID_MATCH_YES                   LW_BCRT20_EXT7_FEAT1_HULK_STRICT_ID_MATCH_YES            // 1
#define LW_BCRT30_EXT7_FEAT1_HULK_STRICT_ID_MATCH_NO                    LW_BCRT20_EXT7_FEAT1_HULK_STRICT_ID_MATCH_NO             // 0

#define LW_BCRT30_EXT7_FEAT1_HULK_EXELWTE_AT_PRIV_LEVEL_0               LW_BCRT20_EXT7_FEAT1_HULK_EXELWTE_AT_PRIV_LEVEL_0        // 4:4
#define LW_BCRT30_EXT7_FEAT1_HULK_EXELWTE_AT_PRIV_LEVEL_0_YES           LW_BCRT20_EXT7_FEAT1_HULK_EXELWTE_AT_PRIV_LEVEL_0_YES    // 1
#define LW_BCRT30_EXT7_FEAT1_HULK_EXELWTE_AT_PRIV_LEVEL_0_NO            LW_BCRT20_EXT7_FEAT1_HULK_EXELWTE_AT_PRIV_LEVEL_0_NO     // 0

#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_BYPASS_SMBPBI_WP                  LW_BCRT20_EXT7_FEAT1_HULK_LWF_BYPASS_SMBPBI_WP           // 5:5
#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_BYPASS_SMBPBI_WP_YES              LW_BCRT20_EXT7_FEAT1_HULK_LWF_BYPASS_SMBPBI_WP_YES       // 1
#define LW_BCRT30_EXT7_FEAT1_HULK_LWF_BYPASS_SMBPBI_WP_NO               LW_BCRT20_EXT7_FEAT1_HULK_LWF_BYPASS_SMBPBI_WP_NO        // 0

#define LW_BCRT30_EXT7_FEAT1_HULK_FEATURE_TYPE                          LW_BCRT20_EXT7_FEAT1_HULK_FEATURE_TYPE                   // 31:24
#define LW_BCRT30_EXT7_FEAT1_HULK_FEATURE_TYPE_INST_IN_SYS              LW_BCRT20_EXT7_FEAT1_HULK_FEATURE_TYPE_INST_IN_SYS       // 1
#define LW_BCRT30_EXT7_FEAT1_HULK_FEATURE_TYPE_I1500_DATA_UNLOCK        LW_BCRT20_EXT7_FEAT1_HULK_FEATURE_TYPE_I1500_DATA_UNLOCK // 2


#define LW_BCRT30_CERT_EXT_TYPE7_FEAT_STRUCTVER1_SIZE                   CERT20_EXT_TYPE7_FEAT_STRUCTVER1_SIZE                    // 4
// typedef struct {
//     LwU32    features;           /* hulk and etc.  */
// }BCRT20CertExtType7FeatVer1_S, *BCRT20CertExtType7FeatVer1_SPTR;
#define BCRT30CertExtType7FeatVer1_S    BCRT20CertExtType7FeatVer1_S
#define BCRT30CertExtType7FeatVer1_SPTR BCRT20CertExtType7FeatVer1_SPTR


#define LW_BCRT30_EXT_TYPE7_SW_FEATURE_SIZE          CERT20_EXT_TYPE7_SW_FEATURE_SIZE           // 12

#define LW_BCRT30_EXT_TYPE7_REGOVERRIDE_TYPE         CERT20_EXT_TYPE7_REGOVERRIDE_TYPE          // 31:28
#define LW_BCRT30_EXT_TYPE7_REGOVERRIDE_TYPE_BAR0    CERT20_EXT_TYPE7_REGOVERRIDE_TYPE_BAR0     // 0
#define LW_BCRT30_EXT_TYPE7_REGOVERRIDE_TYPE_BSI     CERT20_EXT_TYPE7_REGOVERRIDE_TYPE_BSI      // 0x8
#define LW_BCRT30_EXT_TYPE7_OPCODE                   CERT20_EXT_TYPE7_OPCODE                    // 27:24
#define LW_BCRT30_EXT_TYPE7_OPCODE_RMW               CERT20_EXT_TYPE7_OPCODE_RMW                // 0         // Read modify write
#define LW_BCRT30_EXT_TYPE7_OPCODE_RBV               CERT20_EXT_TYPE7_OPCODE_RBV                // 1         // Read back verify
#define LW_BCRT30_EXT_TYPE7_OPCODE_RMW_RBV           CERT20_EXT_TYPE7_OPCODE_RMW_RBV            // 2         // Read modify write with Read back verify
#define LW_BCRT30_EXT_TYPE7_OPCODE_DELAY             CERT20_EXT_TYPE7_OPCODE_DELAY              // 3         // Delay in ns

//  note:  if CERT20_EXT_TYPE7_REGOVERRIDE_TYPE field is set with _BSI,  _REGOVERRIDE_ADDR field is a don't care
#define LW_BCRT30_EXT_TYPE7_REGOVERRIDE_ADDR         CERT20_EXT_TYPE7_REGOVERRIDE_ADDR          // 23:0

// typedef struct {
//     LwU32    addr;               /* Level 3 register address  */
//     LwU32    mask;               /* register mask  */
//     LwU32    data;               /* register data  */
// }BCRT20CertExtType7RegOverride_S, *BCRT20CertExtType7RegOverride_SPTR;
#define BCRT30CertExtType7RegOverride_S     BCRT20CertExtType7RegOverride_S
#define BCRT30CertExtType7RegOverride_SPTR  BCRT20CertExtType7RegOverride_SPTR

// Personality Request. Note: Not part of Certificate, used in UGPU
// typedef struct {
//     LwU8    major_personality; //  1 = VdChip, 2 = Lwdqro, 3 = Tesla.  2 bytes
//     LwU16   minor_personality;
//     LwU8    reserved;            //
// }  BCRT20GPUPersReq_S, * BCRT20GPUPersReq_SPTR;
#define BCRT30GPUPersReq_S     BCRT20GPUPersReq_S
#define BCRT30GPUPersReq_SPTR  BCRT20GPUPersReq_SPTR

// typedef struct {
//         /*  Certificate Private Key Signature,  should located at right after all the extention. */
//         char sig[BCRT20_RSA1K_SIG_SIZE];
// } BCRT20RSA1KSIG_S, *BCRT20RSA1KSIG_SPTR;
#define BCRT30RSA1KSIG_S     BCRT20RSA1KSIG_S
#define BCRT30RSA1KSIG_SPTR  BCRT20RSA1KSIG_SPTR

// typedef struct {
//     /*  Certificate AES HW Key Signature,  should located at right after PKC sig. */
//     char sig[BCRT30_SHA256_SIZE];
// } BCRT20AESSIG_S, *BCRT20AESSIG_SPTR;
#define BCRT30AESSIG_S    BCRT20AESSIG_S
#define BCRT30AESSIG_SPTR BCRT20AESSIG_SPTR

// Type 10 GPU IDs
#define LW_BCRT30_EXT_TYPE10_GPUID_NO_GPUID                  CERT20_EXT_TYPE10_GPUID_NO_GPUID              // 0
#define LW_BCRT30_EXT_TYPE10_GPUID_GM20X                     CERT20_EXT_TYPE10_GPUID_GM20X                 // 1
#define LW_BCRT30_EXT_TYPE10_GPUID_GM206                     CERT20_EXT_TYPE10_GPUID_GM206                 // 2
#define LW_BCRT30_EXT_TYPE10_GPUID_GP100                     CERT20_EXT_TYPE10_GPUID_GP100                 // 3
#define LW_BCRT30_EXT_TYPE10_GPUID_GP104                     CERT20_EXT_TYPE10_GPUID_GP104                 // 4
#define LW_BCRT30_EXT_TYPE10_GPUID_TU10X                     CERT20_EXT_TYPE10_GPUID_TU10X                 // 7
#define LW_BCRT30_EXT_TYPE10_GPUID_TU10X_RSA3K               CERT20_EXT_TYPE10_GPUID_TU10X_RSA3K           // 8
#define LW_BCRT30_EXT_TYPE10_GPUID_TU11X                     CERT20_EXT_TYPE10_GPUID_TU11X                 // 9
#define LW_BCRT30_EXT_TYPE10_GPUID_GENERAL_RSAPSS_3K         CERT20_EXT_TYPE10_GPUID_GENERAL_RSAPSS_3K     //10

#define LW_BCRT30_EXT_TYPE10_STRUCT_SIZE                     CERT20_EXT_TYPE10_STRUCT_SIZE                 // 4
// typedef struct {
//     LwU32   gpuid;
// }  BCRT20CertExtType10_S, * BCRT20CertExtType10_SPTR;
#define BCRT20CertExtType10_S      BCRT20CertExtType10_S
#define BCRT20CertExtType10_SPTR   BCRT20CertExtType10_SPTR

// Type 11 Dev IDs
#define LW_BCRT30_EXT_TYPE11_MAX_EXT_NUM        CERT20_EXT_TYPE11_MAX_EXT_NUM   //  16   // number of DevID could one Cert have
#define LW_BCRT30_EXT11_DEVID                   LW_BCRT20_EXT11_DEVID           // 15:0

#define LW_BCRT30_EXT_TYPE11_STRUCT_SIZE        CERT20_EXT_TYPE11_STRUCT_SIZE   // 4
// typedef struct {
//     LwU32   devid;
// }  BCRT20CertExtType11_S, * BCRT20CertExtType11_SPTR;
#define BCRT30CertExtType11_S     BCRT20CertExtType11_S
#define BCRT30CertExtType11_SPTR  BCRT20CertExtType11_SPTR

/*---------------------------------------------------------------------

    VDPA Table structures
----------------------------------------------------------------------*/

#define LW_BCRT30_FMT_CHAR        CERT20_FMT_CHAR          // 'b'
#define LW_BCRT30_FMT_REPEAT      CERT20_FMT_REPEAT        // 'r'
#define LW_BCRT30_FMT_REPEAT_END  CERT20_FMT_REPEAT_END    // 'R'
#define LW_BCRT30_FMT_SKIP        CERT20_FMT_SKIP          // 'i'


/* VDPA table header structures */
#define LW_BCRT30_VDPA_HEADER_IMG_ID_VER_SIZE_VERSION     LW_BCRT20_VDPA_HEADER_IMG_ID_VER_SIZE_VERSION    // 15:0
#define LW_BCRT30_VDPA_HEADER_IMG_ID_VER_SIZE_VERSION_1   LW_BCRT20_VDPA_HEADER_IMG_ID_VER_SIZE_VERSION_1  //   1

#define LW_BCRT30_VDPA_HEADER_IMG_ID_VER_SIZE_SIZE        LW_BCRT20_VDPA_HEADER_IMG_ID_VER_SIZE_SIZE       // 31:16

#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IMGDEVID_DEVID     LW_BCRT20_VDPA_HEADER_IMG_ID_IMGDEVID_DEVID      // 15:0

#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_GUID                0:0
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_GUID_NO             0         // hash not cover GUID
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_GUID_YES            1         // hash covers GUID
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_BOARDID             1:1
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_BOARDID_NO          0         // hash not cover Board ID
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_BOARDID_YES         1         // hash covers Board ID
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_MAGIC_NUM           2:2
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_MAGIC_NUM_NO        0         // hash not cover magic number
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_MAGIC_NUM_YES       1         // hash covers  magic number
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_VBIOS_VERSION       3:3
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_VBIOS_VERSION_NO    0         // hash not cover magic number
#define LW_BCRT30_VDPA_HEADER_IMG_ID_1_IDSELECT_VBIOS_VERSION_YES   1         // hash covers  magic number

// ID struct variant 1,  with selectable ID coverage by hash
typedef struct {
     LwU32      ver_size;                                              /* Version and Size field */
     LwU32      imgDevID;                                              /* DevID in VBIOS image.  Only lower 16bit used now.  */
     LwU32      idSelect;                                              /* Select ID covered by SHA256 hash in hash  */
     LwU8       hash[BCRT30_SHA256_SIZE];                              /* SHA256 HASH of build version and magic number.  */
}BCRT30_VDPA_IMG_ID_1_S, *BCRT30_VDPA_IMG_ID_1_SPTR;

#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_SIZE_SIZE           LW_BCRT20_VDPA_HEADER_LOC_CTRL_SIZE_SIZE             // 19:0
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_SIZE_INSTANCE       LW_BCRT20_VDPA_HEADER_LOC_CTRL_SIZE_INSTANCE         // 23:20
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE      LW_BCRT20_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE        // 31:24
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE_X86  LW_BCRT30_PCI_ROM_CODE_TYPE_X86        // 0x00
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE_EFI  LW_BCRT30_PCI_ROM_CODE_TYPE_EFI        // 0x03
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE_IFR  LW_BCRT30_PCI_ROM_CODE_TYPE_IFR        // 0x4E
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE_EXT  LW_BCRT30_PCI_ROM_CODE_TYPE_EXT        // 0xE0
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_SIZE_CODE_TYPE_NBSI LW_BCRT30_PCI_ROM_CODE_TYPE_NBSI       // 0x70

#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_TOTAL       VDPA_HEADER_LOC_CTRL_TOTAL                     //  5    // Total structure array count. Lwrrently 5
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_IFR_IDX     VDPA_HEADER_LOC_CTRL_IFR_IDX                   //  0    // array index for IFR section location information
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_X86_IDX     VDPA_HEADER_LOC_CTRL_X86_IDX                   //  1    // array index for X86 PCI ROM location information
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_EXT_IDX     VDPA_HEADER_LOC_CTRL_EXT_IDX                   //  2    // array index for extended PCI ROM location information (used for all instances assumed to be contiguous)
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_UEFI        3                                              // array index for UEFI PCI ROM location information
#define  LW_BCRT30_VDPA_HEADER_LOC_CTRL_INFOROM     4                                              // array index for Inforom PCI ROM location information

//typedef struct {
//    LwU32      offset;												 /* offset into image.  Note,  IFR area is not counted for PCI ROM offset */
//    LwU32      size;                                                 /* size of the IFR or PCI ROM image.  Code type is at upper 8bits */
//}BCRT20_VDPA_LOC_CTRL_S, *BCRT20_VDPA_LOC_CTRL_SPTR;
#define  BCRT30_VDPA_LOC_CTRL_S     BCRT20_VDPA_LOC_CTRL_S
#define  BCRT30_VDPA_LOC_CTRL_SPTR  BCRT20_VDPA_LOC_CTRL_SPTR


// Defines for VDPA table header:

#define LW_BCRT30_VDPA_HEADER_MAX_SIZE                        LW_CRT20_VDPA_HEADER_MAX_SIZE                        //   128

#define LW_BCRT30_VDPA_HEADER_VERSION                         LW_CRT20_VDPA_HEADER_VERSION                         //   7:0      // Header struct version
#define LW_BCRT30_VDPA_HEADER_VERSION_V1                      LW_CRT20_VDPA_HEADER_VERSION_V1                      //   1
#define LW_BCRT30_VDPA_HEADER_VERSION_V2                      LW_CRT20_VDPA_HEADER_VERSION_V2                      //   2
#define LW_BCRT30_VDPA_HEADER_VERSION_LOC_VERSION             LW_CRT20_VDPA_HEADER_VERSION_LOC_VERSION             //   15:8     // LOC struct version
#define LW_BCRT30_VDPA_HEADER_VERSION_LOC_VERSION_V1          LW_CRT20_VDPA_HEADER_VERSION_LOC_VERSION_V1          //   1
#define LW_BCRT30_VDPA_HEADER_VERSION_LOC_ARRAY_SIZE          LW_CRT20_VDPA_HEADER_VERSION_LOC_ARRAY_SIZE          //   23:16     // LOC struct array size
#define LW_BCRT30_VDPA_HEADER_VERSION_LOC_ARRAY_SIZE_LWRRENT  LW_CRT20_VDPA_HEADER_VERSION_LOC_ARRAY_SIZE_LWRRENT  //   VDPA_HEADER_LOC_CTRL_TOTAL     // LOC struct array size

#define LW_BCRT30_VDPA_HEADER_ALGO_HASH                       LW_CRT20_VDPA_HEADER_ALGO_HASH                       //   7:0
#define LW_BCRT30_VDPA_HEADER_ALGO_HASH_DMHASH                LW_CRT20_VDPA_HEADER_ALGO_HASH_DMHASH                //   1        // deprecated
#define LW_BCRT30_VDPA_HEADER_ALGO_HASH_SHA256                LW_CRT20_VDPA_HEADER_ALGO_HASH_SHA256                //   2
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO                     LW_CRT20_VDPA_HEADER_ALGO_CRYPTO                     //  15:8      // for header signature, usage flash security
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO_NONE                0                                                    //   No VDPA table signature required
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO_AES128              LW_CRT20_VDPA_HEADER_ALGO_CRYPTO_AES128              //   1
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO_RSA1024             LW_CRT20_VDPA_HEADER_ALGO_CRYPTO_RSA1024             //   2
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO_RSA2048             3
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO_RSA3072             4
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO_ECDSA256            5
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO_RSAPSS2048          6
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO_RSAPSS3072          7
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO_RSAPSS4096          8
#define LW_BCRT30_VDPA_HEADER_ALGO_CRYPTO_AES256              9

#define LW_BCRT30_VDPA_HEADER_ENTRYNUM1_NORMAL                LW_CRT20_VDPA_HEADER_ENTRY_NORMAL                    //   7:0     // Number of _MAJOR_NORMAL Type of VDPA entries in VDPA table
#define LW_BCRT30_VDPA_HEADER_ENTRYNUM1_INTBLK                LW_CRT20_VDPA_HEADER_ENTRY_INTBLK                    //  15:8     // Number of _MAJOR_INT_BLK Type of VDPA entries in VDPA table
#define LW_BCRT30_VDPA_HEADER_ENTRYNUM1_INTFMT                LW_CRT20_VDPA_HEADER_ENTRY_INTFMT                    //  23:16    // Number of _MAJOR_INT_FMT Type of VDPA entries in VDPA table

#define LW_BCRT30_VDPA_HEADER_SECZONE_INFO_SECZONE_NUM        LW_CRT20_VDPA_HEADER_SECZONE_INFO_SECZONE_NUM        //  7:0     // Total Security Zones supported.
#define LW_BCRT30_VDPA_HEADER_SECZONE_INFO_KEYSET_INDEX       LW_CRT20_VDPA_HEADER_SECZONE_INFO_KEYSET_INDEX       // 15:8     // Security zone key index. Not used in cert30.
#define LW_BCRT30_VDPA_HEADER_SECZONE_INFO_SIG_SIZE           LW_CRT20_VDPA_HEADER_SECZONE_INFO_SIG_SIZE           // 31:16    // one signature size for a security zone.  It includes signature header struct and signature

#define LW_BCRT30_VDPA_HEADER_FLAG_VDPA_SIGNED                LW_CRT20_VDPA_HEADER_FLAG_VDPA_SIGNED                //  0:0      // Flag to indicate VDPA table has been finalized by Lwflash
#define LW_BCRT30_VDPA_HEADER_FLAG_VDPA_SIGNED_NO             LW_CRT20_VDPA_HEADER_FLAG_VDPA_SIGNED_NO             //   0
#define LW_BCRT30_VDPA_HEADER_FLAG_VDPA_SIGNED_YES            LW_CRT20_VDPA_HEADER_FLAG_VDPA_SIGNED_YES            //   1
#define LW_BCRT30_VDPA_HEADER_FLAG_REBRAND_ENABLED            LW_CRT20_VDPA_HEADER_FLAG_REBRAND_ENABLED            //  1:1      // Flag to indicate rebrand enabled
#define LW_BCRT30_VDPA_HEADER_FLAG_REBRAND_ENABLED_NO         LW_CRT20_VDPA_HEADER_FLAG_REBRAND_ENABLED_NO         //   0
#define LW_BCRT30_VDPA_HEADER_FLAG_REBRAND_ENABLED_YES        LW_CRT20_VDPA_HEADER_FLAG_REBRAND_ENABLED_YES        //   1
#define LW_BCRT30_VDPA_HEADER_FLAG_PROJ_ID_PRESENT            LW_CRT20_VDPA_HEADER_FLAG_PROJ_ID_PRESENT            //  2:2      // Flag to indicate proj_id is present
#define LW_BCRT30_VDPA_HEADER_FLAG_PROJ_ID_PRESENT_NO         LW_CRT20_VDPA_HEADER_FLAG_PROJ_ID_PRESENT_NO         //   0
#define LW_BCRT30_VDPA_HEADER_FLAG_PROJ_ID_PRESENT_YES        LW_CRT20_VDPA_HEADER_FLAG_PROJ_ID_PRESENT_YES        //   1

#define  LW_BCRT30_MAX_PROJECTID_LENGTH   LW_BCRT2X_MAX_PROJECTID_LENGTH         //    6
#define  LW_BCRT30_MAX_PARTNER_LENGTH     LW_BCRT2X_MAX_PARTNER_LENGTH           //   20
#define  LW_BCRT30_MAX_SESSIONID_LENGTH   LW_BCRT2X_MAX_SESSIONID_LENGTH         //   20

// typedef  struct {
//          LwU8     projectId[LW_BCRT2X_MAX_PROJECTID_LENGTH];
//          LwU8     partner[LW_BCRT2X_MAX_PARTNER_LENGTH];
//          LwU8     sessionid[LW_BCRT2X_MAX_SESSIONID_LENGTH];
// } BCRT2X_PROJ_ID_S, *BCRT2X_PROJ_ID_SPTR;
#define BCRT30_PROJ_ID_S    BCRT2X_PROJ_ID_S
#define BCRT30_PROJ_ID_SPTR BCRT2X_PROJ_ID_SPTR

//VDPA header version 2, with BCRT30_VDPA_IMG_ID_1_S, for Cert Block Version 30
//Note:  proj_id moved out of VDPA header for Cert2.2
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
    BCRT30_VDPA_IMG_ID_1_S  id;                 /*  IDs to match the underlying VBIOS image*/
    BCRT30_VDPA_LOC_CTRL_S  loc[LW_BCRT30_VDPA_HEADER_LOC_CTRL_TOTAL];  /*  Has offset and size information for each IFR or ROM location, it is used to help uCode get around VBIOS image file */
    LwU32                   flag;               /*  With VDPA table signed, GPU rebrand enabled and etc.   */
    LwU32                   rsvd;               /*  Reserved field                           */
} BCRT30_VDPA_HEADER_V2_S, *BCRT30_VDPA_HEADER_V2_SPTR;
// BCRT20_VDPA_HEADER_S, *BCRT20_VDPA_HEADER_SPTR;

/* VDPA table entries.  */
//  Note: VDPA table entries should located right after VDPA header.
//        VDPA entry data structure define is decided by these two fields: _TYPE_MAJOR  and _TYPE_VERSION
#define LW_BCRT30_VDPA_ENTRY_TYPE_MAJOR                  LW_BCRT20_VDPA_ENTRY_TYPE_MAJOR                 //   7:0       // VDPA type major
#define LW_BCRT30_VDPA_ENTRY_TYPE_MAJOR_NORMAL           LW_BCRT20_VDPA_ENTRY_TYPE_MAJOR_NORMAL          //    1        // Normal Type.  Indicate entry struct format is BCRT20_VDPA_ENTRY_NORMAL_S
#define LW_BCRT30_VDPA_ENTRY_TYPE_MAJOR_INT_BLK          LW_BCRT20_VDPA_ENTRY_TYPE_MAJOR_INT_BLK         //    2        // Internal Block. Indicate entry struct format is BCRT20_VDPA_ENTRY_INT_BLK_S
#define LW_BCRT30_VDPA_ENTRY_TYPE_MAJOR_NORMAL_FMT       LW_BCRT20_VDPA_ENTRY_TYPE_MAJOR_NORMAL_FMT      //    5        // Normal Type.  Indicate entry struct format is BCRT20_VDPA_ENTRY_NORMAL_FMT_S
#define LW_BCRT30_VDPA_ENTRY_TYPE_MAJOR_INT_FMT          LW_BCRT20_VDPA_ENTRY_TYPE_MAJOR_INT_FMT         //    6        // Internal with formatter. Indicate entry struct format is BCRT20_VDPA_ENTRY_INT_FMT_S
#define LW_BCRT30_VDPA_ENTRY_TYPE_MINOR                  LW_BCRT20_VDPA_ENTRY_TYPE_MINOR                 //  15:8       // VDPA type MINOR
#define LW_BCRT30_VDPA_ENTRY_TYPE_MINOR_IFR              LW_BCRT20_VDPA_ENTRY_TYPE_MINOR_IFR             //    1        // Indicates an entry for data in IFR
#define LW_BCRT30_VDPA_ENTRY_TYPE_MINOR_NORMAL           LW_BCRT20_VDPA_ENTRY_TYPE_MINOR_NORMAL          //    2        // Indicates an entry for data in Normal VBIOS image.
#define LW_BCRT30_VDPA_ENTRY_TYPE_MINOR_CERT_BLK         LW_BCRT20_VDPA_ENTRY_TYPE_MINOR_CERT_BLK        //    3        // Indicates an entry for data in Cert Block.

#define LW_BCRT30_VDPA_ENTRY_TYPE_DIRT_ID                27:16                                           //  DIRT ID,  used to be LW_BCRT20_VDPA_ENTRY_TYPE_USAGE

#define LW_BCRT30_VDPA_ENTRY_TYPE_VERSION                31:28                                           //  Version of this struct.   Used to be LW_BCRT20_VDPA_ENTRY_TYPE_VERSION
#define LW_BCRT30_VDPA_ENTRY_TYPE_VERSION_VER_0          0                                               //    0        // Version 0   Used to be LW_BCRT20_VDPA_ENTRY_TYPE_VERSION_VER_0
#define LW_BCRT30_VDPA_ENTRY_TYPE_VERSION_VER_1          1                                               //    1        // Version 1   Used to be LW_BCRT20_VDPA_ENTRY_TYPE_VERSION_VER_1

//  VDPA IntBlk entry defines
#define LW_BCRT30_VDPA_INTBLK_ENTRIES_MAX_NUMBER           LW_BCRT20_VDPA_INTBLK_ENTRIES_MAX_NUMBER             // 96
#define LW_BCRT30_VDPA_INTBLK_ENTRIES_MAX_SIZE             (LW_BCRT30_VDPA_INTBLK_ENTRIES_MAX_NUMBER * sizeof(BCRT30_VDPA_ENTRY_INT_BLK_BASE_S))

#define LW_BCRT30_VDPA_ENTRY_INT_BLK_SIZE_SIZE             LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_SIZE               // 19:0
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_SIZE_INSTANCE         LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_INSTANCE           // 23:20
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE        LW_BCRT20_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE          // 31:24
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE_X86    LW_BCRT30_PCI_ROM_CODE_TYPE_X86                      // 0x00
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE_EFI    LW_BCRT30_PCI_ROM_CODE_TYPE_EFI                      // 0x03
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE_EXT    LW_BCRT30_PCI_ROM_CODE_TYPE_EXT                      // 0xE0
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE_NBSI   LW_BCRT30_PCI_ROM_CODE_TYPE_NBSI                     // 0x70
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_SIZE_CODE_TYPE_IFR    LW_BCRT30_PCI_ROM_CODE_TYPE_IFR                      // 0x4E

#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_ALGO            LW_BCRT20_VDPA_ENTRY_INT_BLK_FLAG_ALGO                // 7:0     // Crypto Algo Control
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_ALGO_SHA256     1

#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE        31:8 // security zone bitmap for this entry
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_0      LW_BCRT30_VDPA_SEC_ZONE_0      //     0
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_1      LW_BCRT30_VDPA_SEC_ZONE_1      // BIT(0)  dedicated to ID matching, MetaData and VBIOS/VDPA integrality
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_2      LW_BCRT30_VDPA_SEC_ZONE_2      // BIT(1)  dedicated to VBIOS data
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_3      LW_BCRT30_VDPA_SEC_ZONE_3      // BIT(2)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_4      LW_BCRT30_VDPA_SEC_ZONE_4      // BIT(3)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_5      LW_BCRT30_VDPA_SEC_ZONE_5      // BIT(4)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_6      LW_BCRT30_VDPA_SEC_ZONE_6      // BIT(5)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_7      LW_BCRT30_VDPA_SEC_ZONE_7      // BIT(6)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_8      LW_BCRT30_VDPA_SEC_ZONE_8      // BIT(7)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_9      LW_BCRT30_VDPA_SEC_ZONE_9      // BIT(8)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_10     LW_BCRT30_VDPA_SEC_ZONE_10     // BIT(9)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_11     LW_BCRT30_VDPA_SEC_ZONE_11     // BIT(10)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_12     LW_BCRT30_VDPA_SEC_ZONE_12     // BIT(11)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_13     LW_BCRT30_VDPA_SEC_ZONE_13     // BIT(12)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_14     LW_BCRT30_VDPA_SEC_ZONE_14     // BIT(13)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_15     LW_BCRT30_VDPA_SEC_ZONE_15     // BIT(14)
#define LW_BCRT30_VDPA_ENTRY_INT_BLK_FLAG_SEC_ZONE_16     LW_BCRT30_VDPA_SEC_ZONE_16     // BIT(15)

typedef struct {
    LwU32 type;                        /* VBIOS Image type of this entry. */
    LwU32 offset_start;                /* VBIOS image staring offset for this entry */
    LwU32 size;                        /* data block size, PCI ROM code type and instance data */
    LwU32 flag;                        /* with algo type, security zone */
    LwU32 flag1;                       /* general purpose flags */
    char sig_hash[BCRT30_SHA256_SIZE]; /* Image Hash data from offset_start to offset_start + size */
}BCRT30_VDPA_ENTRY_INT_BLK_BASE_S, *BCRT30_VDPA_ENTRY_INT_BLK_BASE_SPTR;



#define LW_BCRT30_VDPA_ENTRY_NORMAL_SIZE_SIZE            LW_BCRT20_VDPA_ENTRY_NORMAL_SIZE_SIZE           //  19:0
#define LW_BCRT30_VDPA_ENTRY_NORMAL_SIZE_INSTANCE        LW_BCRT20_VDPA_ENTRY_NORMAL_SIZE_INSTANCE       //  23:20
#define LW_BCRT30_VDPA_ENTRY_NORMAL_SIZE_CODE_TYPE       LW_BCRT20_VDPA_ENTRY_NORMAL_SIZE_CODE_TYPE      //  31:24
#define LW_BCRT30_VDPA_ENTRY_NORMAL_SIZE_CODE_TYPE_X86   LW_BCRT30_PCI_ROM_CODE_TYPE_X86  //  0x00
#define LW_BCRT30_VDPA_ENTRY_NORMAL_SIZE_CODE_TYPE_EFI   LW_BCRT30_PCI_ROM_CODE_TYPE_EFI  //  0x03
#define LW_BCRT30_VDPA_ENTRY_NORMAL_SIZE_CODE_TYPE_EXT   LW_BCRT30_PCI_ROM_CODE_TYPE_EXT  //  0xE0
#define LW_BCRT30_VDPA_ENTRY_NORMAL_SIZE_CODE_TYPE_NBSI  LW_BCRT30_PCI_ROM_CODE_TYPE_NBSI //  0x70

#define LW_BCRT30_VDPA_NORMAL_ENTRIES_MAX_NUMBER         LW_BCRT20_VDPA_NORMAL_ENTRIES_MAX_NUMBER        //  96
#define LW_BCRT30_VDPA_NORMAL_ENTRIES_MAX_SIZE           LW_BCRT20_VDPA_NORMAL_ENTRIES_MAX_SIZE          //  LW_BCRT20_VDPA_NORMAL_ENTRIES_MAX_NUMBER * sizeof(BCRT20_VDPA_ENTRY_NORMAL_BASE_S)

// Note: used for flash security
typedef struct {
    LwU32      type;             /*  VBIOS Image type of this entry.  */
    LwU32      offset_start;     /* VBIOS image staring offset for this entry */
    LwU32      size;             /* data block size,  PCI ROM code type and instance data */
    char       sig_hash[BCRT30_SHA256_SIZE];   /* Image Hash data from offset_start to offset_start + size  */
}BCRT30_VDPA_ENTRY_NORMAL_BASE_S, *BCRT30_VDPA_ENTRY_NORMAL_BASE_SPTR;

// security zone defines
#define LW_BCRT30_VDPA_SEC_ZONE_0        0         // 0 for non-secure not selwred by any signature
#define LW_BCRT30_VDPA_SEC_ZONE_1        BIT(0)    // security zone 1,  selwred by Signature root to BIOS Cert Root of Trust, dedicated to VBIOS image integrality and identity. It should cover Cert Block, including MetaData, Certs, IDs and structures of VDPA Entry
#define LW_BCRT30_VDPA_SEC_ZONE_2        BIT(1)    // security zone 2,  selwred by Signature root to BIOS Cert Root of Trust, most of VBIOS tables assigned to this zone by default during its VDPA generation process
#define LW_BCRT30_VDPA_SEC_ZONE_3        BIT(2)    // security zone 3,  selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_4        BIT(3)    // security zone 4,  selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_5        BIT(4)    // security zone 5,  selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_6        BIT(5)    // security zone 6,  selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_7        BIT(6)    // security zone 7,  selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_8        BIT(7)    // security zone 8,  selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_9        BIT(8)    // security zone 9,  selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_10       BIT(9)    // security zone 10, selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_11       BIT(10)   // security zone 11, selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_12       BIT(11)   // security zone 12, selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_13       BIT(12)   // security zone 13, selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_14       BIT(13)   // security zone 14, selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_15       BIT(14)   // security zone 15, selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_16       BIT(15)   // security zone 16, selwred by Signature root to BIOS Cert Root of Trust
#define LW_BCRT30_VDPA_SEC_ZONE_TOTAL    17     // total supported security zone

#define LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_NONE    0  // No Key Needed.  security zone not signed
#define LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWD     1  // LWD  Key signed security zone signature
#define LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWA     2  // LWA  Key signed security zone signature
#define LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PP      3  // PP   Key signed security zone signature
#define LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PD      4  // PD   Key signed security zone signature
#define LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_XOC     5  // XOC  Key signed security zone signature
#define LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_UEFI    6  // UEFI Key signed security zone signature
#define LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_IST     7  // IST  Key signed security zone signature

#define LW_BCRT30_VDPA_SEC_ZONE_KEY_TYPE_VBIOS_VENDOR_KEY ( BIT( LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWD ) | \
                                                            BIT( LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWA ) | \
                                                            BIT( LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PP  ) | \
                                                            BIT( LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PD  ) | \
                                                            BIT( LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_XOC )   \
                                                          )
#define LW_BCRT30_VDPA_SEC_ZONE_KEY_TYPE_UEFI_KEY         BIT( LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_UEFI )
#define LW_BCRT30_VDPA_SEC_ZONE_KEY_TYPE_IST_KEY          BIT( LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_IST )


// working structure used to control the matching between security zone and its key
typedef  struct
{
    LwU32  secZoneId;
    LwU32  keySelect;
}LW_BCRT30_SEC_ZONE_ID_KEY_PAIR_S, *LW_BCRT30_SEC_ZONE_ID_KEY_PAIR_SPTR;

// Note: VDPA Internal structure with formatter.  Used to describe data need to be secure are not contiguous.
//       Moved functionality from BCRT_FMT_HAT_ENTRY to this structure

#define LW_BCRT30_VDPA_ENTRY_INT_FMT_SIZE_SIZE                       19:0
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_SIZE_INSTANCE                   23:20
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_SIZE_CODE_TYPE                  31:24
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_SIZE_CODE_TYPE_X86              LW_BCRT30_PCI_ROM_CODE_TYPE_X86    // 0x00
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_SIZE_CODE_TYPE_EFI              LW_BCRT30_PCI_ROM_CODE_TYPE_EFI    // 0x03
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_SIZE_CODE_TYPE_EXT              LW_BCRT30_PCI_ROM_CODE_TYPE_EXT    // 0xE0
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_SIZE_CODE_TYPE_NBSI             LW_BCRT30_PCI_ROM_CODE_TYPE_NBSI   // 0x70

#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_ALGO                       7:0   // Crypto Algo Control
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_ALGO_DMHASH128             0
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_ALGO_SHA256                1

#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE                  31:8 // security zone bitmap for this entry
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE_0                LW_BCRT30_VDPA_SEC_ZONE_0      //     0
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE_1                LW_BCRT30_VDPA_SEC_ZONE_1      // BIT(0)
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE_2                LW_BCRT30_VDPA_SEC_ZONE_2      // BIT(1)
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE_3                LW_BCRT30_VDPA_SEC_ZONE_3      // BIT(2)
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE_4                LW_BCRT30_VDPA_SEC_ZONE_4      // BIT(3)
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE_5                LW_BCRT30_VDPA_SEC_ZONE_5      // BIT(4)
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE_6                LW_BCRT30_VDPA_SEC_ZONE_6      // BIT(5)
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE_7                LW_BCRT30_VDPA_SEC_ZONE_7      // BIT(6)
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE_8                LW_BCRT30_VDPA_SEC_ZONE_8      // BIT(7)
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG_SEC_ZONE_9                LW_BCRT30_VDPA_SEC_ZONE_9      // BIT(8)
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG1_VERIFY_SKIP              7:0                            // Control if skip hash verificatioin at certain stages of the image life cycle
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG1_VERIFY_SKIP_NONE         0                              // Don't skip any verification
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG1_VERIFY_SKIP_LWF          BIT(0)                         // Skip hash verification at Lwflash stage
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG1_VERIFY_SKIP_BOOT         BIT(1)                         // Skip hash verification at Boot
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG1_VERIFY_SKIP_LS           BIT(2)                         // Skip hash verification at LS uCode Launch, do we need this? selwrely patching LS uCode before launching?
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG1_VERIFY_SKIP_FRTS         BIT(3)                         // Skip hash verification at FRTS, do we need this? selwrely patching VBIOS image before FRTS?
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG1_FMT_ATTRIB               8:8
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG1_FMT_ATTRIB_NORMAL        0                              // formatter is used to describ the areas that covered by hash
#define LW_BCRT30_VDPA_ENTRY_INT_FMT_FLAG1_FMT_ATTRIB_ILWERT        1                              // formatter is used to describ the areas that skips the hash coverage


typedef struct {
    LwU32 type;                       /* VBIOS Image type of this entry. */
    LwU32 offset_start;               /* VBIOS image staring offset for this entry */
    LwU32 size;                       /* max block size formatter colwers, PCI ROM code type and instance data */
    LwU32 flag;                       /* with algo type, security zone */
    LwU32 flag1;                      /* general purpose flags */
    LwU32 fmt_len;                    // length of format string
    // char* formatter;               /* Formatter string located immediate after BCRT30_VDPA_ENTRY_INT_FMT_BASE_S. String length in fmt_attrib.LW_BCRT30_VDPA_ENTRY_INT_FMT_ATTRIB_FMT_LEN field */
    // char sig_hash[BCRT30_SHA256_SIZE]; // Image Hash/Sig, start of this field is at
                                       // sizeof( BCRT30_VDPA_ENTRY_INT_FMT_BASE_S) +
                                       // REF_VAL( LW_BCRT30_VDPA_ENTRY_INT_FMT_ATTRIB_FMT_LEN, BCRT20_VDPA_ENTRY_INT_FMT_BASE_S.fmt_attrib )
}BCRT30_VDPA_ENTRY_INT_FMT_BASE_S, *BCRT30_VDPA_ENTRY_INT_FMT_BASE_SPTR;

// BCRT30_VDPA_ENTRY_INT_FMT_ENTRY_S is used collect all the information for construction of VDPA with formatter entry
typedef  struct {
    BCRT30_VDPA_ENTRY_INT_FMT_BASE_S vdpa_fmt_entry;
    LwU8                formatter[BCRT30_MAX_FORMATTER_LENGTH];
    char                sig_hash[BCRT30_SHA256_SIZE];
} BCRT30_VDPA_ENTRY_INT_FMT_ENTRY_S, *BCRT30_VDPA_ENTRY_INT_FMT_ENTRY_SPTR;



/* VDPA Table signature, used for flash security  */
// typedef struct {
//     char  vdpa_sig[BCRT20_AES128_SIZE];
// }BCRT20_VDPA_SIG_S, *BCRT20_VDPA_SIG_SPTR;
#define LW_BCRT30_VDPA_SIG_S     BCRT20_VDPA_SIG_S
#define LW_BCRT30_VDPA_SIG_SPTR  BCRT20_VDPA_SIG_SPTR

/* ----------------------------------------------------------------------
   Cert Block Structures and Defines
-------------------------------------------------------------------------*/

#define LW_BCRT30_CERT_CONTROL_HEADER_MAX_SIZE                40

#define LW_BCRT30_CERT_CONTROL_HEADER_VERSION                 31:0
#define LW_BCRT30_CERT_CONTROL_HEADER_VERSION_30              0x030

#define LW_BCRT30_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE       LW_BCRT2X_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE      // 0:0

#define LW_BCRT30_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE       LW_BCRT2X_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE      // 0:0
#define LW_BCRT30_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE_YES   LW_BCRT2X_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE_YES  //   1
#define LW_BCRT30_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE_NO    LW_BCRT2X_CERT_CONTROL_HEADER_FEAT_FRTS_DISABLE_NO   //   0

typedef  struct {
    LwU32    version;            // Version of Cert Block and this structure
    LwU32    size;               // size of this structure
    LwU32    feature_mask;       // specify BIOS Cert feature supported that can not be implied by CTRL Structure Version,  Such as FRTS that roll out before this control structure.
    LwU32    metadata_offset;    // offset of metadata structure. offset starts from start of BIOS Cert block,  or the instance of this header
    LwU32    cert_offset;        // Offset of cert linked list.  offset starts from start of BIOS Cert block,  or the instance of this header
    LwU32    vdpa_offset;        // Offset of VDPA Talbe
    LwU32    size_certblock;     // size of cert block
} BCRT30_CERT_CONTROL_HEADER_S, *BCRT30_CERT_CONTROL_HEADER_SPTR;
//#define LW_BCRT30_CERT_CONTROL_HEADER_S     BCRT2X_CERT_CONTROL_HEADER_S
//#define LW_BCRT30_CERT_CONTROL_HEADER_SPTR  BCRT2X_CERT_CONTROL_HEADER_SPTR

#define  LW_BCRT30_METADATA_HEADER_MAX_SIZE       LW_BCRT2X_METADATA_HEADER_MAX_SIZE       //  64

#define  LW_BCRT30_METADATA_HEADER_VERSION        LW_BCRT2X_METADATA_HEADER_VERSION        // 7:0
#define  LW_BCRT30_METADATA_HEADER_VERSION_VER_1  LW_BCRT2X_METADATA_HEADER_VERSION_VER_1  //   1
//typedef  struct {
//         LwU32    version;           // Metadata header version
//         LwU32    size;              // size of this structure
//         BCRT2X_PROJ_ID_S  proj_id;  // IDs for VBIOS ROM's project,  partner and SESSIONID
//} BCRT2X_METADATA_HEADER_S, *BCRT2X_METADATA_HEADER_SPTR;
#define BCRT30_METADATA_HEADER_S     BCRT2X_METADATA_HEADER_S
#define BCRT30_METADATA_HEADER_SPTR  BCRT2X_METADATA_HEADER_SPTR

// BIOS Cert initial BIOSMOD script signing support

  // X509CERT keyword
  //    <cert_file_name>,   // null ended file name string
  //    <cert format>,      // BIOS Cert Version
  //    <cert flag>         //
#define LW_BCRT30_CERT_FORMAT_RSVD                23:0
#define LW_BCRT30_CERT_FORMAT_MINOR_VER    LW_BCRT20_CERT_FORMAT_CERT20_MINOR_VER   //  27:24        // Cert Minor Version
#define LW_BCRT30_CERT_FORMAT_MINOR_VER_0  LW_BCRT20_CERT_FORMAT_CERT20_MINOR_VER_0 //      0
#define LW_BCRT30_CERT_FORMAT_MINOR_VER_1  LW_BCRT20_CERT_FORMAT_CERT20_MINOR_VER_1 //      1
#define LW_BCRT30_CERT_FORMAT_MINOR_VER_2  LW_BCRT20_CERT_FORMAT_CERT20_MINOR_VER_2 //      2
#define LW_BCRT30_CERT_FORMAT_MAJOR_VER    LW_BCRT20_CERT_FORMAT_CERT20_MAJOR_VER   //  31:28        // Cert Major version,  reserved for after Cert2.0
#define LW_BCRT30_CERT_FORMAT_MAJOR_VER_1  0                                        // Cert1.0  backward compatible with legacy definition
#define LW_BCRT30_CERT_FORMAT_MAJOR_VER_2  8                                        // Cert2.0  backward compatible with legacy definition // BIT(31)       // Used for identify Cert2.0, keep it for legacy code
#define LW_BCRT30_CERT_FORMAT_MAJOR_VER_3  3                                        // Cert3.0

  // SIGLWBIOS keyword
  //     <Vendor PrivKey file name>,  // filename of the private key.  "placeholder" if using key in HSM or signature generated externally.
  //     <Param>,                     // Hash Algo, BIOS Cert Version, Key source
  //     <SigType>                    // Key size, Key Algorithm, Key Type

#define LW_BCRT30_SIGLWBIOS_PRIVKEY_FILENAME_PLACEHOLDER   "placeholder"

#define LW_BCRT30_SIGLWBIOS_PARAM_DIGEST_ALGO           7:0
#define LW_BCRT30_SIGLWBIOS_PARAM_DIGEST_ALGO_DMHash    BCRT_DIGEST_ALGO_DMHash  // BIT(0) Note: Deprecated
#define LW_BCRT30_SIGLWBIOS_PARAM_DIGEST_ALGO_SHA1      BCRT_DIGEST_ALGO_SHA1    // BIT(1) Note: Deprecated
#define LW_BCRT30_SIGLWBIOS_PARAM_DIGEST_ALGO_SHA256    3
#define LW_BCRT30_SIGLWBIOS_PARAM_MINOR_VER             11:8
#define LW_BCRT30_SIGLWBIOS_PARAM_MINOR_VER_0           0
#define LW_BCRT30_SIGLWBIOS_PARAM_MAJOR_VER             15:12
#define LW_BCRT30_SIGLWBIOS_PARAM_MAJOR_VER_LEGACY      0
#define LW_BCRT30_SIGLWBIOS_PARAM_MAJOR_VER_3           3
#define LW_BCRT30_SIGLWBIOS_PARAM_KEY_SOURCE            19:16                    // Signature generating source
#define LW_BCRT30_SIGLWBIOS_PARAM_KEY_SOURCE_LOCAL      0                        // Signature generated using private key stored locally
#define LW_BCRT30_SIGLWBIOS_PARAM_KEY_SOURCE_HSM        1                        // Signature generated using private key stored in HSM.  Note: Use "placeholder" as Priv Key File Name
#define LW_BCRT30_SIGLWBIOS_PARAM_KEY_SOURCE_EXTERNAL   2                        // Signature generated externally. Note: Use "placeholder" as Priv Key File Name


#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_SIZE            LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_SIZE           // 15:0
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_SIZE_256        LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE_SIZE_256  // 256
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_SIZE_1K         LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE_SIZE_1K   // 1024
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_SIZE_2K         LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE_SIZE_2K   // 1024*2
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_SIZE_3K         LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE_SIZE_3K   // 1024*3
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_ALGO            LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_ALGO           // 23:16
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_ALGO_RSA        LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_ALGO_RSA       //   0      // use value 0 for backward compatibility
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_ALGO_ECC        LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_ALGO_ECC       //   1      // Lwrve type: prime256v1
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_ALGO_RSAPSS     2                                              // RSA with PSS padding
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_TYPE            LW_BCRT20_SIGLWBIOS_SIGTYPE_KEY_TYPE           // 31:24
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_TYPE_UNDEFINED  0
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_TYPE_LWD        LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWD         // 1  // LWD  Key signed security zone or data blob signature
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_TYPE_LWA        LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWA         // 2  // LWA  Key signed security zone or data blob signature
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_TYPE_PP         LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PP          // 3  // PP   Key signed security zone or data blob signature
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_TYPE_PD         LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PD          // 4  // PD   Key signed security zone or data blob signature
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_TYPE_XOC        LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_XOC         // 5  // XOC  Key signed security zone or data blob signature
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_TYPE_UEFI       LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_UEFI        // 6  // UEFI Key signed security zone or data blob signature
#define LW_BCRT30_SIGLWBIOS_SIGTYPE_KEY_TYPE_IST        LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_IST         // 7  // IST  Key signed security zone or data blob signature


  // SELWRITYZONE keyword
#define LW_BCRT30_SELWRITYZONESELECTION_ALGO_HASH               LW_BCRT2X_SELWRITYZONESELECTION_ALGO_HASH             // 7:0
#define LW_BCRT30_SELWRITYZONESELECTION_ALGO_HASH_DMHASH        LW_BCRT2X_SELWRITYZONESELECTION_ALGO_HASH_DMHASH      //  1
#define LW_BCRT30_SELWRITYZONESELECTION_ALGO_HASH_SHA256        LW_BCRT2X_SELWRITYZONESELECTION_ALGO_HASH_SHA256      //  2
#define LW_BCRT30_SELWRITYZONESELECTION_ALGO_CRYPTO             LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO           // 15:8
#define LW_BCRT30_SELWRITYZONESELECTION_ALGO_CRYPTO_AES128      LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO_AES128    //  1
#define LW_BCRT30_SELWRITYZONESELECTION_ALGO_CRYPTO_RSA1024     LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO_RSA1024   //  2
#define LW_BCRT30_SELWRITYZONESELECTION_ALGO_CRYPTO_RSA2048     LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO_RSA2048   //  3
#define LW_BCRT30_SELWRITYZONESELECTION_ALGO_CRYPTO_RSA3072     LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO_RSA3072   //  4
#define LW_BCRT30_SELWRITYZONESELECTION_ALGO_CRYPTO_ECDSA256    LW_BCRT2X_SELWRITYZONESELECTION_ALGO_CRYPTO_ECDSA256  //  5
#define LW_BCRT30_SELWRITYZONESELECTION_ALGO_CRYPTO_RSAPSS3072  6

#define LW_BCRT30_SELWRITYZONESELECTION_FLAG_KEY_SELECT         7:0   //  LW_BCRT2X_SELWRITYZONESELECTION_FLAG_KEYSET_INDEX
#define LW_BCRT30_SELWRITYZONESELECTION_FLAG_KEY_SELECT_LWD     LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWD    // 1  // LWD
#define LW_BCRT30_SELWRITYZONESELECTION_FLAG_KEY_SELECT_LWA     LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWA    // 2  // LWA
#define LW_BCRT30_SELWRITYZONESELECTION_FLAG_KEY_SELECT_PP      LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PP     // 3  // PP
#define LW_BCRT30_SELWRITYZONESELECTION_FLAG_KEY_SELECT_PD      LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PD     // 4  // PD
#define LW_BCRT30_SELWRITYZONESELECTION_FLAG_KEY_SELECT_XOC     LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_XOC    // 5  // XOC
#define LW_BCRT30_SELWRITYZONESELECTION_FLAG_KEY_SELECT_UEFI    LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_UEFI   // 6  // UEFI
#define LW_BCRT30_SELWRITYZONESELECTION_FLAG_KEY_SELECT_IST     LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_IST    // 7  // IST

// Assess this requirement for packing.
//   All structures following are either 32-bit quantities or
//     64-bit quantities following even multiples of 32-bit parameters.
//   These should naturally align under normal cirlwmstances.

// Cert2.0 cert link list support
#define LW_BCRT30_MAX_ALLOWED_CERT_COUNT MAX_ALLOWED_CERT_COUNT  // 50

#define LW_BCRT30_CERT_HEADER_VERSION_1        1      // Reserved for Cert1.0
#define LW_BCRT30_CERT_HEADER_VERSION_2        2

#define LW_BCRT30_CERT_HEADER_CERT_TYPE            LW_BCRT20_CERTBLK_CERT_TYPE               //  15:0
#define LW_BCRT30_CERT_HEADER_CERT_TYPE_ILWALID    LW_BCRT20_CERTBLK_CERT_TYPE_ILWALID       //     0
#define LW_BCRT30_CERT_HEADER_CERT_TYPE_ROOT       LW_BCRT30_TYPE_CERT_ROOT                  //     1
#define LW_BCRT30_CERT_HEADER_CERT_TYPE_MASTER     LW_BCRT30_TYPE_CERT_MASTER                //     2
#define LW_BCRT30_CERT_HEADER_CERT_TYPE_LWD        LW_BCRT30_TYPE_CERT_LWD                   //     3
#define LW_BCRT30_CERT_HEADER_CERT_TYPE_LWA        LW_BCRT30_TYPE_CERT_LWA                   //     5
#define LW_BCRT30_CERT_HEADER_CERT_TYPE_PP         LW_BCRT30_TYPE_CERT_PP                    //     6
#define LW_BCRT30_CERT_HEADER_CERT_TYPE_PD         LW_BCRT30_TYPE_CERT_PD                    //     7
#define LW_BCRT30_CERT_HEADER_CERT_TYPE_XOC        LW_BCRT30_TYPE_CERT_XOC                   //     8
#define LW_BCRT30_CERT_HEADER_CERT_TYPE_IST        LW_BCRT30_TYPE_CERT_FEATURE_IST           //    16

#define LW_BCRT30_CERT_HEADER_FLAG_FIRST_CERT              BCRT20_FLAG_FIRST_CERT          //  BIT(0)
#define LW_BCRT30_CERT_HEADER_FLAG_LAST_CERT               BCRT20_FLAG_LAST_CERT           //  BIT(1)
#define LW_BCRT30_CERT_HEADER_FLAG_ZBX_SIGNING_CERT        BCRT20_FLAG_ZBX_SIGNING_CERT    //  BIT(2)
#define LW_BCRT30_CERT_HEADER_FLAG_PROJ_ID_PRESENT         BCRT20_FLAG_PROJ_ID_PRESENT     //  BIT(3)
#define LW_BCRT30_CERT_HEADER_FLAG_SYSTEM_MASK             BCRT20_FLAG_SYSTEM_MASK         // ( BCRT20_FLAG_FIRST_CERT | BCRT20_FLAG_LAST_CERT | BCRT20_FLAG_ZBX_SIGNING_CERT )
#define LW_BCRT30_CERT_HEADER_FLAG_CERT_VINTAGE            BCRT20_FLAG_CERT_VINTAGE        //  BIT(31)

// typedef  struct {
//          LwU32    version;            // LW_BCRT30_CERT_HEADER_VERSION_2
//          LwU32    size;               // size of this structure
//          LwU32    prev_cert;          // offset of previous x509 cert. 0 if first one
//          LwU32    next_cert;          // offset of next x509 cert or beginning of next block (hash filed) for the last x509 cert
//          LwU32    cert_type;          // Verdor cert | Master Cert | Feature Cert
//          LwU32    cert_format;        //
//          LwU32    flag;               // Set BCRT20_FLAG_LAST_CERT flag if reached last Cert record.
// } BCRT20_CERT_HEADER;
#define LW_BCRT30_CERT_HEADER  BCRT20_CERT_HEADER

// end of Cert2.0 cert link list support

// signature header
#define LW_BCRT30_SIGNATURE_HEADER_VERSION          BCRT_SIGNATURE_HEADER_VERSION     // 0x1
#define LW_BCRT30_SIGNATURE_HEADER_VERSION_V2       BCRT_SIGNATURE_HEADER_VERSION_V2  // 0x2
#define LW_BCRT30_SIGNATURE_HEADER_VERSION_V3       3

#define LW_BCRT30_SIG_HEADER_ALGO_HASH              LW_BCRT20_SIG_HEADER_ALGO_HASH                //  7:0     // Hash used for signature of this block
#define LW_BCRT30_SIG_HEADER_ALGO_HASH_DMHASH       LW_BCRT20_SIG_HEADER_ALGO_HASH_DMHASH         //   1
#define LW_BCRT30_SIG_HEADER_ALGO_HASH_SHA256       LW_BCRT20_SIG_HEADER_ALGO_HASH_SHA256         //   2
#define LW_BCRT30_SIG_HEADER_ALGO_CRYPTO            LW_BCRT20_SIG_HEADER_ALGO_CRYPTO              // 15:8
#define LW_BCRT30_SIG_HEADER_ALGO_CRYPTO_AES        LW_BCRT20_SIG_HEADER_ALGO_CRYPTO_AES          //   1
#define LW_BCRT30_SIG_HEADER_ALGO_CRYPTO_RSA1024    LW_BCRT20_SIG_HEADER_ALGO_CRYPTO_RSA1024      //   2
#define LW_BCRT30_SIG_HEADER_ALGO_CRYPTO_RSA2048    LW_BCRT20_SIG_HEADER_ALGO_CRYPTO_RSA2048      //   3
#define LW_BCRT30_SIG_HEADER_ALGO_CRYPTO_RSA3072    LW_BCRT20_SIG_HEADER_ALGO_CRYPTO_RSA3072      //   4
#define LW_BCRT30_SIG_HEADER_ALGO_CRYPTO_ECC256     LW_BCRT20_SIG_HEADER_ALGO_CRYPTO_ECC256       //   5
#define LW_BCRT30_SIG_HEADER_ALGO_CRYPTO_RSAPSS2048 6
#define LW_BCRT30_SIG_HEADER_ALGO_CRYPTO_RSAPSS3072 7
#define LW_BCRT30_SIG_HEADER_ALGO_CRYPTO_RSAPSS4096 8
#define LW_BCRT30_SIG_HEADER_ALGO_CRYPTO_AES256     9

#define LW_BCRT30_SIG_HEADER_FLAG_RSVD0                      4:0     // Deprecated flags from previous BIOS Cert versions. Reserved for new uses
#define LW_BCRT30_SIG_HEADER_FLAG_NEXT_SIG_AVAILABLE         LW_BCRT20_SIG_HEADER_FLAG_NEXT_SIG_AVAILABLE        // 5:5     // Indicate there is another signature behind this signature
#define LW_BCRT30_SIG_HEADER_FLAG_NEXT_SIG_AVAILABLE_NO      LW_BCRT20_SIG_HEADER_FLAG_NEXT_SIG_AVAILABLE_NO     //  0      // No,   The signature is the last one
#define LW_BCRT30_SIG_HEADER_FLAG_NEXT_SIG_AVAILABLE_YES     LW_BCRT20_SIG_HEADER_FLAG_NEXT_SIG_AVAILABLE_YES    //  1      // Yes,  there is signature behind this signature
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE                   31:8    // Security Zone for this signature.
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE_0                 LW_BCRT30_VDPA_SEC_ZONE_0      //     0
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE_1                 LW_BCRT30_VDPA_SEC_ZONE_1      // BIT(0)
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE_2                 LW_BCRT30_VDPA_SEC_ZONE_2      // BIT(1)
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE_3                 LW_BCRT30_VDPA_SEC_ZONE_3      // BIT(2)
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE_4                 LW_BCRT30_VDPA_SEC_ZONE_4      // BIT(3)
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE_5                 LW_BCRT30_VDPA_SEC_ZONE_5      // BIT(4)
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE_6                 LW_BCRT30_VDPA_SEC_ZONE_6      // BIT(5)
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE_7                 LW_BCRT30_VDPA_SEC_ZONE_7      // BIT(6)
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE_8                 LW_BCRT30_VDPA_SEC_ZONE_8      // BIT(7)
#define LW_BCRT30_SIG_HEADER_FLAG_SEC_ZONE_9                 LW_BCRT30_VDPA_SEC_ZONE_9      // BIT(8)

#define LW_BCRT30_SIG_HEADER_FLAG1_KEY_SELECT                7:0     // Used to find correct public key for the PKC signature in the list of cert. Also can be used to enforce signing policy
#define LW_BCRT30_SIG_HEADER_FLAG1_KEY_SELECT_UNDEFINED      0
#define LW_BCRT30_SIG_HEADER_FLAG1_KEY_SELECT_LWD            LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWD              // 1  // LWD  Key signed security zone or data blob signature
#define LW_BCRT30_SIG_HEADER_FLAG1_KEY_SELECT_LWA            LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWA              // 2  // LWA  Key signed security zone or data blob signature
#define LW_BCRT30_SIG_HEADER_FLAG1_KEY_SELECT_PP             LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PP               // 3  // PP   Key signed security zone or data blob signature
#define LW_BCRT30_SIG_HEADER_FLAG1_KEY_SELECT_PD             LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PD               // 4  // PD   Key signed security zone or data blob signature
#define LW_BCRT30_SIG_HEADER_FLAG1_KEY_SELECT_XOC            LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_XOC              // 5  // XOC  Key signed security zone or data blob signature
#define LW_BCRT30_SIG_HEADER_FLAG1_KEY_SELECT_UEFI           LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_UEFI             // 6  // UEFI Key signed security zone or data blob signature
#define LW_BCRT30_SIG_HEADER_FLAG1_KEY_SELECT_IST            LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_IST              // 7  // IST  Key signed security zone or data blob signature
#define IS_VENDOR_KEY( x )                                   ( ( (x) == LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWD) || \
                                                               ( (x) == LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_LWA) || \
                                                               ( (x) == LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PP) || \
                                                               ( (x) == LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_PD) || \
                                                               ( (x) == LW_BCRT30_VDPA_SEC_ZONE_KEY_SELECT_XOC) \
                                                             )

#define LW_BCRT30_SIG_HEADER_FLAG1_SIG_LOCATION              11:8   // location of the signature presented by this signature header structure
#define LW_BCRT30_SIG_HEADER_FLAG1_SIG_LOCATION_LOCAL           0   // Signature located right after this header structure
#define LW_BCRT30_SIG_HEADER_FLAG1_SIG_LOCATION_DATA_BLOB       1   // Signature located within the data blob of this security zone. DIRT ID of VDPA entry should be used to
                                                                    // indicate the data blob type, and method for parsing and verification. i.e. UEFI
#define LW_BCRT30_SIG_HEADER_FLAG1_SIG_SOURCE               19:16   // Signature generating source
#define LW_BCRT30_SIG_HEADER_FLAG1_SIG_SOURCE_LOCAL         0       // Signature generated using private key stored locally
#define LW_BCRT30_SIG_HEADER_FLAG1_SIG_SOURCE_HSM           1       // Signature generated using private key stored in HSM
#define LW_BCRT30_SIG_HEADER_FLAG1_SIG_SOURCE_EXTERNAL      2       // Signature generated externally.  ( BIOSMOD is not used to sign the signature )

typedef  struct {
    LwU32    version;
    LwU32    size;               // size of this structure
    LwU32    algo;               // Hash/Crypto used for the signature that controlled by this header
    Lw64     timestamp;          // The timestamp of the cert block generating with seconds elapse since 1970 Jan 1.
    LwU32    flag;               // security zone, PKC key index if used, and etc.
    LwU32    flag1;
} BCRT30_SIGNATURE_HEADER_V3;        //BCRT20_SIGNATURE_HEADER_V2

typedef struct {
    BCRT30_SIGNATURE_HEADER_V3  sigheader;
    LwU8                        sig[BCRT30_RSA3K_SIG_SIZE];
} BCRT30_RSAPSS3K_SIG_STRUCT_S, *BCRT30_RSAPSS3K_SIG_STRUCT_SPTR;
// BCRT2X_LS_AES128_SIG_STRUCT_S, *BCRT2X_LS_AES128_SIG_STRUCT_SPTR;


#if defined(LW_FALCON) || defined(GCC_FALCON)
#pragma pack()
#else
#pragma pack(pop)
#endif

#endif    // __CERT30_H__INCLUDED_
