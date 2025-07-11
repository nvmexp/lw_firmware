/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/* This source was autogenerated by xbgen.
** DO NOT EDIT THIS SOURCE MANUALLY.
** If changes need to be applied update the XML and regenerate this source.
*/
/*
** This file defines the following generated formats
** DRM_XMRFORMAT
*/
#ifndef XMRFORMAT_H
#define XMRFORMAT_H 1

ENTER_PK_NAMESPACE;

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_POOR_DATA_ALIGNMENT_25021, "Ignore poor alignment of XBinary data structures" );

#define DRM_XMRFORMAT_LWRRENT_VERSION 3
#define DRM_XMRFORMAT_STACK_SIZE 2048
#define DRM_XMRFORMAT_MIN_UNPACK_DIVISOR 4
#define DRM_XMRFORMAT_MAX_UNPACK_FACTOR 4
#define OUTER_CONTAINER_ID 0x0001
#define GLOBAL_POLICY_CONTAINER_ID 0x0002
#define PLAYBACK_POLICY_CONTAINER_ID 0x0004
#define OUTPUT_PROTECTION_OBJECT_ID 0x0005
#define EXPLICIT_ANALOG_VIDEO_OUTPUT_PROTECTION_CONTAINER_ID 0x0007
#define ANALOG_VIDEO_OUTPUT_CONFIGURATION_OBJECT_ID 0x0008
#define KEY_MATERIAL_CONTAINER_ID 0x0009
#define CONTENT_KEY_OBJECT_ID 0x000A
#define SIGNATURE_OBJECT_ID 0x000B
#define SERIAL_NUMBER_OBJECT_ID 0x000C
#define RIGHTS_OBJECT_ID 0x000D
#define EXPIRATION_OBJECT_ID 0x0012
#define ISSUEDATE_OBJECT_ID 0x0013
#define METERING_OBJECT_ID 0x0016
#define GRACE_PERIOD_OBJECT_ID 0x001A
#define SOURCEID_OBJECT_ID 0x0022
#define RESTRICTED_SOURCEID_OBJECT_ID 0x0028
#define DOMAIN_ID_OBJECT_ID 0x0029
#define ECC_DEVICE_KEY_OBJECT_ID 0x002A
#define POLICY_METADATA_OBJECT_ID 0x002C
#define OPTIMIZED_CONTENT_KEY_OBJECT_ID 0x002D
#define EXPLICIT_DIGITAL_AUDIO_OUTPUT_PROTECTION_CONTAINER_ID 0x002E
#define RINGTONE_POLICY_CONTAINER_ID 0x002F
#define EXPIRATION_AFTER_FIRSTPLAY_OBJECT_ID 0x0030
#define DIGITAL_AUDIO_OUTPUT_CONFIGURATION_OBJECT_ID 0x0031
#define REVOCATION_INFORMATION_VERSION_2_OBJECT_ID 0x0032
#define EMBEDDING_BEHAVIOR_OBJECT_ID 0x0033
#define SELWRITY_LEVEL_ID 0x0034
#define PLAY_ENABLER_CONTAINER_ID 0x0036
#define MOVE_ENABLER_OBJECT_ID 0x0037
#define COPY_ENABLER_CONTAINER_ID 0x0038
#define PLAY_ENABLER_OBJECT_ID 0x0039
#define COPY_ENABLER_OBJECT_ID 0x003A
#define UPLINK_KID_2_OBJECT_ID 0x003B
#define COPY_POLICY_2_CONTAINER_ID 0x003C
#define COPYCOUNT_2_OBJECT_ID 0x003D
#define EXELWTE_POLICY_CONTAINER_ID 0x003F
#define EXELWTE_POLICY_OBJECT_ID 0x0040
#define READ_POLICY_CONTAINER_ID 0x0041
#define REMOVAL_DATE_OBJECT_ID 0x0050
#define AUX_KEY_OBJECT_ID 0x0051
#define UPLINKX_OBJECT_ID 0x0052
#define ILWALID_RESERVED_53_ID 0x0053
#define APPLICATION_ID_LIST_ID 0x0054
#define REAL_TIME_EXPIRATION_ID 0x0055
#define EXPLICIT_DIGITAL_VIDEO_OUTPUT_PROTECTION_CONTAINER_ID 0x0058
#define DIGITAL_VIDEO_OUTPUT_CONFIGURATION_OBJECT_ID 0x0059
#define SELWRESTOP_OBJECT_ID 0x005A
#define UNKNOWN_OBJECT_ID 0xFFFD
#define UNKNOWN_CONTAINER_ID 0xFFFE

typedef struct __tagDRM_XMRFORMAT_EXTENDED_HEADER
{
    DRM_BOOL      fValid;
    DRM_DWORD     dwVersion;
    DRM_ID        LID;
} DRM_XMRFORMAT_EXTENDED_HEADER;

typedef struct __tagDRM_XMRFORMAT_DWORD
{
    DRM_BOOL      fValid;
    DRM_DWORD     dwValue;
} DRM_XMRFORMAT_DWORD;

typedef struct __tagDRM_XMRFORMAT_WORD
{
    DRM_BOOL     fValid;
    DRM_WORD     wValue;
} DRM_XMRFORMAT_WORD;

typedef struct __tagDRM_XMRFORMAT_GUID
{
    DRM_BOOL     fValid;
    DRM_ID       idValue;
} DRM_XMRFORMAT_GUID;

typedef struct __tagDRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION
{
    DRM_BOOL                                              fValid;
    struct __tagDRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION *pNext;

    DRM_ID                                                idOPL;
    DRM_XB_BYTEARRAY                                      xbbaConfig;
} DRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION;

typedef enum __tagDRM_XMRFORMAT_TYPES 
{
    DRM_XMRFORMAT_OUTER_CONTAINER_ENTRY_TYPE                    = 0x1,
    DRM_XMRFORMAT_GLOBAL_POLICY_CONTAINER_ENTRY_TYPE            = 0x2,
    DRM_XMRFORMAT_PLAYBACK_POLICY_CONTAINER_ENTRY_TYPE          = 0x4,
    DRM_XMRFORMAT_MINIMUM_OUTPUT_PROTECTION_LEVELS_ENTRY_TYPE   = 0x5,
    DRM_XMRFORMAT_EXPLICIT_ANALOG_VIDEO_PROTECTION_ENTRY_TYPE   = 0x7,
    DRM_XMRFORMAT_ANALOG_VIDEO_OPL_ENTRY_TYPE                   = 0x8,
    DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER_ENTRY_TYPE             = 0x9,
    DRM_XMRFORMAT_CONTENT_KEY_ENTRY_TYPE                        = 0xA,
    DRM_XMRFORMAT_SIGNATURE_ENTRY_TYPE                          = 0xB,
    DRM_XMRFORMAT_SERIAL_NUMBER_ENTRY_TYPE                      = 0xC,
    DRM_XMRFORMAT_RIGHTS_ENTRY_TYPE                             = 0xD,
    DRM_XMRFORMAT_EXPIRATION_ENTRY_TYPE                         = 0x12,
    DRM_XMRFORMAT_ISSUEDATE_ENTRY_TYPE                          = 0x13,
    DRM_XMRFORMAT_METERING_ENTRY_TYPE                           = 0x16,
    DRM_XMRFORMAT_GRACEPERIOD_ENTRY_TYPE                        = 0x1A,
    DRM_XMRFORMAT_SOURCEID_ENTRY_TYPE                           = 0x22,
    DRM_XMRFORMAT_RESTRICTED_SOURCEID_ENTRY_TYPE                = 0x28,
    DRM_XMRFORMAT_DOMAIN_ID_ENTRY_TYPE                          = 0x29,
    DRM_XMRFORMAT_DEVICE_KEY_ENTRY_TYPE                         = 0x2A,
    DRM_XMRFORMAT_POLICY_METADATA_ENTRY_TYPE                    = 0x2C,
    DRM_XMRFORMAT_OPTIMIZED_CONTENT_KEY_ENTRY_TYPE              = 0x2D,
    DRM_XMRFORMAT_EXPLICIT_DIGITAL_AUDIO_PROTECTION_ENTRY_TYPE  = 0x2E,
    DRM_XMRFORMAT_EXPIRE_AFTER_FIRST_USE_ENTRY_TYPE             = 0x30,
    DRM_XMRFORMAT_DIGITAL_AUDIO_OPL_ENTRY_TYPE                  = 0x31,
    DRM_XMRFORMAT_REVOCATION_INFO_VERSION_ENTRY_TYPE            = 0x32,
    DRM_XMRFORMAT_EMBEDDING_BEHAVIOR_ENTRY_TYPE                 = 0x33,
    DRM_XMRFORMAT_SELWRITY_LEVEL_ENTRY_TYPE                     = 0x34,
    DRM_XMRFORMAT_MOVE_ENABLER_ENTRY_TYPE                       = 0x37,
    DRM_XMRFORMAT_UPLINK_KID_ENTRY_TYPE                         = 0x3B,
    DRM_XMRFORMAT_COPY_POLICIES_CONTAINER_ENTRY_TYPE            = 0x3C,
    DRM_XMRFORMAT_COPY_COUNT_ENTRY_TYPE                         = 0x3D,
    DRM_XMRFORMAT_REMOVAL_DATE_ENTRY_TYPE                       = 0x50,
    DRM_XMRFORMAT_AUX_KEY_ENTRY_TYPE                            = 0x51,
    DRM_XMRFORMAT_UPLINKX_ENTRY_TYPE                            = 0x52,
    DRM_XMRFORMAT_REAL_TIME_EXPIRATION_ENTRY_TYPE               = 0x55,
    DRM_XMRFORMAT_EXPLICIT_DIGITAL_VIDEO_PROTECTION_ENTRY_TYPE  = 0x58,
    DRM_XMRFORMAT_DIGITAL_VIDEO_OPL_ENTRY_TYPE                  = 0x59,
    DRM_XMRFORMAT_SELWRESTOP_ENTRY_TYPE                         = 0x5A,
    DRM_XMRFORMAT_COPY_UNKNOWN_OBJECT_ENTRY_TYPE                = 0xFFFD,
    DRM_XMRFORMAT_GLOBAL_POLICY_UNKNOWN_OBJECT_ENTRY_TYPE       = 0xFFFD,
    DRM_XMRFORMAT_PLAYBACK_UNKNOWN_OBJECT_ENTRY_TYPE            = 0xFFFD,
    DRM_XMRFORMAT_COPY_UNKNOWN_CONTAINER_ENTRY_TYPE             = 0xFFFE,
    DRM_XMRFORMAT_UNKNOWN_CONTAINERS_ENTRY_TYPE                 = 0xFFFE,
    DRM_XMRFORMAT_PLAYBACK_UNKNOWN_CONTAINER_ENTRY_TYPE         = 0xFFFE,
} DRM_XMRFORMAT_TYPES;
/* Count Includes XB_OBJECT_GLOBAL_HEADER */
#define DRM_XMRFORMAT_TYPE_COUNT       45
#define DRM_XMRFORMAT_FORMAT_ID        XB_DEFINE_DWORD_FORMAT_ID( 'X', 'M', 'R', 0 )

typedef struct __tagDRM_XMRFORMAT_SERIAL_NUMBER
{
    DRM_BOOL             fValid;
    DRM_XB_BYTEARRAY     xbbaSerialNumber;
} DRM_XMRFORMAT_SERIAL_NUMBER;

typedef struct __tagDRM_XMRFORMAT_RESTRICTED_SOURCEID
{
    DRM_BOOL         fValid;
    DRM_XB_EMPTY     EMPTY;
} DRM_XMRFORMAT_RESTRICTED_SOURCEID;

typedef struct __tagDRM_XMRFORMAT_EXPIRATION
{
    DRM_BOOL      fValid;
    DRM_DWORD     dwBeginDate;
    DRM_DWORD     dwEndDate;
} DRM_XMRFORMAT_EXPIRATION;

typedef struct __tagDRM_XMRFORMAT_REAL_TIME_EXPIRATION
{
    DRM_BOOL         fValid;
    DRM_XB_EMPTY     EMPTY;
} DRM_XMRFORMAT_REAL_TIME_EXPIRATION;

typedef struct __tagDRM_XMRFORMAT_DOMAIN_ID
{
    DRM_BOOL      fValid;
    DRM_ID        idAccount;
    DRM_DWORD     dwRevision;
} DRM_XMRFORMAT_DOMAIN_ID;

typedef struct __tagDRM_XMRFORMAT_POLICY_METADATA
{
    DRM_BOOL                                   fValid;
    struct __tagDRM_XMRFORMAT_POLICY_METADATA *pNext;

    DRM_ID                                     idMetadataType;
    DRM_XB_BYTEARRAY                           xbbaMetadata;
} DRM_XMRFORMAT_POLICY_METADATA;

typedef struct __tagDRM_XMRFORMAT_SELWRITY_LEVEL
{
    DRM_BOOL     fValid;
    DRM_WORD     wMinimumSelwrityLevel;
} DRM_XMRFORMAT_SELWRITY_LEVEL;

typedef struct __tagDRM_XMRFORMAT_GLOBAL_POLICY_CONTAINER
{
    DRM_BOOL                               fValid;
    DRM_XMRFORMAT_SERIAL_NUMBER            SerialNumber;
    DRM_XMRFORMAT_WORD                     Rights;
    DRM_XMRFORMAT_DWORD                    SourceID;
    DRM_XMRFORMAT_RESTRICTED_SOURCEID      RestrictedSourceID;
    DRM_XMRFORMAT_EXPIRATION               Expiration;
    DRM_XMRFORMAT_REAL_TIME_EXPIRATION     RealTimeExpiration;
    DRM_XMRFORMAT_DWORD                    ExpirationAfterUse;
    DRM_XMRFORMAT_DWORD                    IssueDate;
    DRM_XMRFORMAT_DWORD                    GracePeriod;
    DRM_XMRFORMAT_GUID                     Metering;
    DRM_XMRFORMAT_GUID                     SelwreStop;
    DRM_XMRFORMAT_DOMAIN_ID                DomainID;
    DRM_XMRFORMAT_WORD                     EmbeddingBehavior;
    DRM_XB_UNKNOWN_OBJECT                 *pUnknownObjects;
    DRM_XMRFORMAT_POLICY_METADATA         *pPolicyMetadata;
    DRM_XMRFORMAT_DWORD                    RevocationInfoVersion;
    DRM_XMRFORMAT_SELWRITY_LEVEL           SelwrityLevel;
    DRM_XMRFORMAT_DWORD                    RemovalDate;
} DRM_XMRFORMAT_GLOBAL_POLICY_CONTAINER;

typedef struct __tagDRM_XMRFORMAT_MINIMUM_OUTPUT_PROTECTION_LEVELS
{
    DRM_BOOL     fValid;
    DRM_WORD     wCompressedDigitalVideo;
    DRM_WORD     wUncompressedDigitalVideo;
    DRM_WORD     wAnalogVideo;
    DRM_WORD     wCompressedDigitalAudio;
    DRM_WORD     wUncompressedDigitalAudio;
    DRM_XB_BYTEARRAY  xbbaRawData;
} DRM_XMRFORMAT_MINIMUM_OUTPUT_PROTECTION_LEVELS;

typedef struct __tagDRM_XMRFORMAT_EXPLICIT_ANALOG_VIDEO_PROTECTION
{
    DRM_BOOL                                     fValid;
    DRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION    *pOPL;
    DRM_DWORD                                    cEntries;
} DRM_XMRFORMAT_EXPLICIT_ANALOG_VIDEO_PROTECTION;

typedef struct __tagDRM_XMRFORMAT_EXPLICIT_DIGITAL_AUDIO_PROTECTION
{
    DRM_BOOL                                     fValid;
    DRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION    *pOPL;
    DRM_DWORD                                    cEntries;
} DRM_XMRFORMAT_EXPLICIT_DIGITAL_AUDIO_PROTECTION;

typedef struct __tagDRM_XMRFORMAT_EXPLICIT_DIGITAL_VIDEO_PROTECTION
{
    DRM_BOOL                                     fValid;
    DRM_XMRFORMAT_EXPLICIT_OUTPUT_PROTECTION    *pOPL;
    DRM_DWORD                                    cEntries;
} DRM_XMRFORMAT_EXPLICIT_DIGITAL_VIDEO_PROTECTION;

typedef struct __tagDRM_XMRFORMAT_PLAYBACK_POLICY_CONTAINER
{
    DRM_BOOL                                            fValid;
    DRM_XMRFORMAT_MINIMUM_OUTPUT_PROTECTION_LEVELS      MinimumOutputProtectionLevel;
    DRM_XMRFORMAT_EXPLICIT_ANALOG_VIDEO_PROTECTION      ExplicitAnalogVideoProtection;
    DRM_XMRFORMAT_EXPLICIT_DIGITAL_AUDIO_PROTECTION     ExplicitDigitalAudioProtection;
    DRM_XMRFORMAT_EXPLICIT_DIGITAL_VIDEO_PROTECTION     ExplicitDigitalVideoProtection;
    DRM_XB_UNKNOWN_OBJECT                              *pUnknownObjects;
    DRM_XB_UNKNOWN_CONTAINER                            UnknownContainer;
} DRM_XMRFORMAT_PLAYBACK_POLICY_CONTAINER;

typedef struct __tagDRM_XMRFORMAT_COPY_POLICIES_CONTAINER
{
    DRM_BOOL                     fValid;
    DRM_XMRFORMAT_DWORD          CopyCount;
    DRM_XMRFORMAT_DWORD          MoveEnabler;
    DRM_XB_UNKNOWN_OBJECT       *pUnknownObjects;
    DRM_XB_UNKNOWN_CONTAINER     UnknownContainer;
} DRM_XMRFORMAT_COPY_POLICIES_CONTAINER;

typedef struct __tagDRM_XMRFORMAT_CONTENT_KEY
{
    DRM_BOOL             fValid;
    DRM_ID               guidKeyID;
    DRM_WORD             wSymmetricCipherType;
    DRM_WORD             wKeyEncryptionCipherType;
    DRM_XB_BYTEARRAY     xbbaEncryptedKey;
} DRM_XMRFORMAT_CONTENT_KEY;

typedef struct __tagDRM_XMRFORMAT_OPTIMIZED_CONTENT_KEY
{
    DRM_BOOL             fValid;
    DRM_WORD             wKeyEncryptionCipherType;
    DRM_XB_BYTEARRAY     xbbaKeyData;
} DRM_XMRFORMAT_OPTIMIZED_CONTENT_KEY;

typedef struct __tagDRM_XMRFORMAT_DEVICE_KEY
{
    DRM_BOOL             fValid;
    DRM_WORD             wEccLwrveType;
    DRM_XB_BYTEARRAY     xbbaKeyData;
} DRM_XMRFORMAT_DEVICE_KEY;

typedef struct __tagDRM_XMRFORMAT_UPLINK_KID
{
    DRM_BOOL             fValid;
    DRM_ID               idUplinkKID;
    DRM_WORD             wChecksumType;
    DRM_XB_BYTEARRAY     xbbaChainedChecksum;
} DRM_XMRFORMAT_UPLINK_KID;

typedef struct __tagDRM_XMRFORMAT_AUX_KEY
{
    DRM_BOOL             fValid;
    DRM_WORD             cAuxKeys;
    DRM_XB_BYTEARRAY     xbbaAuxKeys;
} DRM_XMRFORMAT_AUX_KEY;

typedef struct __tagDRM_XMRFORMAT_UPLINKX
{
    DRM_BOOL             fValid;
    DRM_ID               idUplinkKID;
    DRM_XB_BYTEARRAY     xbbaChecksum;
    DRM_WORD             cEntries;
    DRM_XB_DWORDLIST     dwlLocations;
    DRM_XB_BYTEARRAY     xbbaKey;
} DRM_XMRFORMAT_UPLINKX;

typedef struct __tagDRM_XMRFORMAT_KEY_MATERIAL_CONTAINER
{
    DRM_BOOL                                fValid;
    DRM_XMRFORMAT_CONTENT_KEY               ContentKey;
    DRM_XMRFORMAT_OPTIMIZED_CONTENT_KEY     OptimizedContentKey;
    DRM_XMRFORMAT_DEVICE_KEY                DeviceKey;
    DRM_XMRFORMAT_UPLINK_KID                UplinkKID;
    DRM_XMRFORMAT_AUX_KEY                   AuxKeys;
    DRM_XMRFORMAT_UPLINKX                   UplinkX;
} DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER;

typedef struct __tagDRM_XMRFORMAT_SIGNATURE
{
    DRM_BOOL             fValid;
    DRM_WORD             wType;
    DRM_XB_BYTEARRAY     xbbaSignature;
} DRM_XMRFORMAT_SIGNATURE;

typedef struct __tagDRM_XMRFORMAT_OUTER_CONTAINER
{
    DRM_BOOL                                    fValid;
    DRM_XMRFORMAT_GLOBAL_POLICY_CONTAINER       GlobalPolicyContainer;
    DRM_XMRFORMAT_PLAYBACK_POLICY_CONTAINER     PlaybackPolicyContainer;
    DRM_XMRFORMAT_COPY_POLICIES_CONTAINER       CopyPolicyContainer;
    DRM_XB_UNKNOWN_CONTAINER                    UnknownContainer;
    DRM_XMRFORMAT_KEY_MATERIAL_CONTAINER        KeyMaterialContainer;
    DRM_XMRFORMAT_SIGNATURE                     Signature;
} DRM_XMRFORMAT_OUTER_CONTAINER;

typedef struct __tagDRM_XMRFORMAT
{
    DRM_BOOL                          fValid;
    DRM_XMRFORMAT_EXTENDED_HEADER     HeaderData;
    DRM_XMRFORMAT_OUTER_CONTAINER     OuterContainer;
    DRM_XB_BYTEARRAY                  xbbaRawData;
} DRM_XMRFORMAT;

DRM_EXPORT_VAR extern DRM_GLOBAL_CONST DRM_XB_FORMAT_DESCRIPTION s_DRM_XMRFORMAT_FormatDescription[1];

PREFAST_POP;

EXIT_PK_NAMESPACE;

#endif // XMRFORMAT_H
