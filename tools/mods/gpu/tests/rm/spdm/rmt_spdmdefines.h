/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RMT_SPDMDEFINES_H
#define RMT_SPDMDEFINES_H

//
// Defines and macros for SPDM unit tests.
//

// SPDM value enums.

enum SpdmRequestId
{
    SpdmRequestIdGetDigests                  = 0x81,
    SpdmRequestIdGetCertificate              = 0x82,
    SpdmRequestIdChallenge                   = 0x83,
    SpdmRequestIdGetVersion                  = 0x84,
    SpdmRequestIdGetMeasurements             = 0xE0,
    SpdmRequestIdGetCapabilities             = 0xE1,
    SpdmRequestIdNegotiateAlgorithms         = 0xE3,
    SpdmRequestIdKeyExchange                 = 0xE4,
    SpdmRequestIdFinish                      = 0xE5,
    SpdmRequestIdPskExchange                 = 0xE6,
    SpdmRequestIdPskFinish                   = 0xE7,
    SpdmRequestIdHeartbeat                   = 0xE8,
    SpdmRequestIdKeyUpdate                   = 0xE9,
    SpdmRequestIdGetEncapsulatedRequest      = 0xEA,
    SpdmRequestIdDeliverEncapsulatedResponse = 0xEB,
    SpdmRequestIdEndSession                  = 0xEC,
    SpdmRequestIdRespondIfReady              = 0xFF,
    SpdmRequestIdVendorDefinedRequest        = 0xFE,
};

enum SpdmResponseId
{
    SpdmResponseIdDigests                    = 0x01,
    SpdmResponseIdCertificate                = 0x02,
    SpdmResponseIdChallengeAuth              = 0x03,
    SpdmResponseIdVersion                    = 0x04,
    SpdmResponseIdMeasurements               = 0x60,
    SpdmResponseIdCapabilities               = 0x61,
    SpdmResponseIdAlgorithms                 = 0x63,
    SpdmResponseIdKeyExchangeRsp             = 0x64,
    SpdmResponseIdFinishRsp                  = 0x65,
    SpdmResponseIdPskExchangeRsp             = 0x66,
    SpdmResponseIdPskFinishRsp               = 0x67,
    SpdmResponseIdHeartbeatAck               = 0x68,
    SpdmResponseIdKeyUpdateAck               = 0x69,
    SpdmResponseIdEncapsulatedRequest        = 0x6A,
    SpdmResponseIdEncapsulatedResponseAck    = 0x6B,
    SpdmResponseIdEndSessionAck              = 0x6C,
    SpdmResponseIdVendorDefinedResponse      = 0x7E,
    SpdmResponseIdError                      = 0x7F,
};

enum SpdmCapability
{
    SpdmCapabilityCacheCap                   = 0x00000001,
    SpdmCapabilityCertCap                    = 0x00000002,
    SpdmCapabilityChalCap                    = 0x00000004,
    SpdmCapabilityMeasNoSigCap               = 0x00000008,
    SpdmCapabilityMeasSigCap                 = 0x00000010,
    SpdmCapabilityMeasFreshCap               = 0x00000020,
    SpdmCapabilityEncryptCap                 = 0x00000040,
    SpdmCapabilityMacCap                     = 0x00000080,
    SpdmCapabilityMutAuthCap                 = 0x00000100,
    SpdmCapabilityKeyExCap                   = 0x00000200,
    SpdmCapabilityPskCap                     = 0x00000400,
    SpdmCapabilityPskCapReserved             = 0x00000800,
    SpdmCapabilityEncapCap                   = 0x00001000,
    SpdmCapabilityHbeatCap                   = 0x00002000,
    SpdmCapabilityKeyUpdCap                  = 0x00004000,
    SpdmCapabilityHandshakeInTheClearCap     = 0x00008000,
    SpdmCapabilityPubKeyIdCap                = 0x00010000,
};

enum SpdmAlgTableType
{
    SpdmAlgTableTypeDhe                      = 0x02,
    SpdmAlgTableTypeAeadCipherSuite          = 0x03,
    SpdmAlgTableTypeReqBaseAsymAlg           = 0x04,
    SpdmAlgTableTypeKeySchedule              = 0x05,
};

enum SpdmMeasurementSpecification
{
    SpdmMeasurementSpecificationDmtf         = 0x01,
};

enum SpdmBaseAsymAlgo
{
    SpdmBaseAsymAlgoTpmAlgRsaSsa2048         = 0x0001,
    SpdmBaseAsymAlgoTpmAlgRsaPss2048         = 0x0002,
    SpdmBaseAsymAlgoTpmAlgRsaSsa3072         = 0x0004,
    SpdmBaseAsymAlgoTpmAlgRsaPss3072         = 0x0008,
    SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP256   = 0x0010,
    SpdmBaseAsymAlgoTpmAlgRsaSsa4096         = 0x0020,
    SpdmBaseAsymAlgoTpmAlgRsaPss4096         = 0x0040,
    SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP384   = 0x0080,
    SpdmBaseAsymAlgoTpmAlgEcdsaEccNistP521   = 0x0100,
};

enum SpdmBaseHashAlgo
{
    SpdmBaseHashAlgoTpmAlgSha256             = 0x0001,
    SpdmBaseHashAlgoTpmAlgSha384             = 0x0002,
    SpdmBaseHashAlgoTpmAlgSha512             = 0x0004,
    SpdmBaseHashAlgoTpmAlgSha3_256           = 0x0008,
    SpdmBaseHashAlgoTpmAlgSha3_384           = 0x0010,
    SpdmBaseHashAlgoTpmAlgSha3_512           = 0x0020,
};

enum SpdmMeasurementHashAlgo
{
    SpdmMeasurementHashAlgoRawBitStream      = 0x0001,
    SpdmMeasurementHashAlgoTpmAlgSha256      = 0x0002,
    SpdmMeasurementHashAlgoTpmAlgSha384      = 0x0004,
    SpdmMeasurementHashAlgoTpmAlgSha512      = 0x0008,
    SpdmMeasurementHashAlgoTpmAlgSha3_256    = 0x0010,
    SpdmMeasurementHashAlgoTpmAlgSha3_384    = 0x0020,
    SpdmMeasurementHashAlgoTpmAlgSha3_512    = 0x0040,
};

enum SpdmChallengeRequestHashAlgo
{
  SpdmChallengeRequestHashAlgoNoMeasurementSummaryHash     = 0,
  SpdmChallengeRequestHashAlgoTcbComponentMeasurementHash  = 1,
  SpdmChallengeRequestHashAlgoAllMeasurementHash           = 0xFF,
};

enum SpdmDheAlgo
{
    SpdmDheAlgoTypeFfdhe2048                 = 0x0001,
    SpdmDheAlgoTypeFfdhe3072                 = 0x0002,
    SpdmDheAlgoTypeFfdhe4096                 = 0x0004,
    SpdmDheAlgoTypeSecp256r1                 = 0x0008,
    SpdmDheAlgoTypeSecp384r1                 = 0x0010,
    SpdmDheAlgoTypeSecp521r1                 = 0x0020,
};

enum SpdmAeadAlgo
{
    SpdmAeadAlgoAes128Gcm                    = 0x0001,
    SpdmAeadAlgoAes256Gcm                    = 0x0002,
    SpdmAeadAlgoChaCha20Poly1305             = 0x0004,

    // Note: the below is LW-defined, not an official part of the SPDM spec.
    SpdmAeadAlgoAes128CtrHmacSha256          = 0x8000,
};

enum SpdmKeyScheduleAlgo
{
    SpdmKeyScheduleAlgoSpdm                  = 0x01,
};

enum SpdmErrorCode
{
    SpdmErrorCodeIlwalidRequest              = 0x01,
    SpdmErrorCodeIlwalidSession              = 0x02,
    SpdmErrorCodeBusy                        = 0x03,
    SpdmErrorCodeUnexpectedRequest           = 0x04,
    SpdmErrorCodeUnspecified                 = 0x05,
    SpdmErrorCodeDecryptError                = 0x06,
    SpdmErrorCodeUnsupportedRequest          = 0x07,
    SpdmErrorCodeRequestInFlight             = 0x08,
    SpdmErrorCodeIlwalidResponseCode         = 0x09,
    SpdmErrorCodeSessionLimitExceeded        = 0x0A,
    SpdmErrorCodeMajorVersionMismatch        = 0x41,
    SpdmErrorCodeResponseNotReady            = 0x42,
    SpdmErrorCodeRequestResynch              = 0x43,
    SpdmErrorCodeVendorDefined               = 0xFF,
};

enum SpdmVersionId
{
    SpdmVersionId_1_0                        = 0x10,
    SpdmVersionId_1_1                        = 0x11,
};

// SPDM message structs.

typedef struct
{
    LwU8 SpdmVersionId;
    LwU8 RequestResponseCode;
    LwU8 Param1;
    LwU8 Param2;
} SPDM_MESSAGE_HEADER, *PSPDM_MESSAGE_HEADER;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8 SpdmVersionId;
        LwU8 RequestResponseCode;
        LwU8 Reserved1;
        LwU8 Reserved2;
    } GetVersion;
} SPDM_MESSAGE_GET_VERSION, *PSPDM_MESSAGE_GET_VERSION;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8 SpdmVersionId;
        LwU8 RequestResponseCode;
        LwU8 Reserved1;
        LwU8 Reserved2;
        LwU8 Reserved3;
        LwU8 VersionNumberEntryCount;
        LwU8 Version1Low;
        LwU8 Version1High;
        LwU8 Version2Low;
        LwU8 Version2High;
    } Version;
} SPDM_MESSAGE_VERSION, *PSPDM_MESSAGE_VERSION;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8  SpdmVersionId;
        LwU8  RequestResponseCode;
        LwU8  Reserved1;
        LwU8  Reserved2;
        LwU8  Reserved3;
        LwU8  CTExponent;
        LwU8  Reserved4;
        LwU8  Reserved5;
        LwU32 Flags;
    } GetCapabilities;
} SPDM_MESSAGE_GET_CAPABILITIES, *PSPDM_MESSAGE_GET_CAPABILITIES;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8  SpdmVersionId;
        LwU8  RequestResponseCode;
        LwU8  Reserved1;
        LwU8  Reserved2;
        LwU8  Reserved3;
        LwU8  CTExponent;
        LwU8  Reserved4;
        LwU8  Reserved5;
        LwU32 Flags;
    } Capabilities;
} SPDM_MESSAGE_CAPABILITIES, *PSPDM_MESSAGE_CAPABILITIES;

typedef struct
{
    LwU8  AlgType;
    LwU8  AlgCount;
    LwU16 AlgSupported;
//  TODO - Determine if we need to use below.
//  LwU8  AlgExternal[4 * 0xF & AlgCount];
} SPDM_ALG_TABLE;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8  SpdmVersionId;
        LwU8  RequestResponseCode;
        LwU8  NumAlgTables;
        LwU8  Reserved1;
        LwU16 MessageLength;
        LwU8  MeasurementSpecification;
        LwU8  Reserved2;
        LwU32 BaseAsymAlgo;
        LwU32 BaseHashAlgo;
        LwU8  Reserved3[12];
        LwU8  ExtAsymCount;
        LwU8  ExtHashCount;
        LwU8  Reserved4[2];
//      TODO - Determine if we need to use below.
//      LwU8  ExtAsym[4*ExtAsymCount]
//      LwU8  ExtHash[4*ExtHashCount]
        SPDM_ALG_TABLE AlgTables[4];
    } NegotiateAlgorithms;
} SPDM_MESSAGE_NEGOTIATE_ALGORITHMS, *PSPDM_MESSAGE_NEGOTIATE_ALGORITHMS;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8  SpdmVersionId;
        LwU8  RequestResponseCode;
        LwU8  NumAlgTables;
        LwU8  Reserved1;
        LwU16 MessageLength;
        LwU8  MeasurementSpecificationSel;
        LwU8  Reserved2;
        LwU32 MeasurementHashAlgo;
        LwU32 BaseAsymSel;
        LwU32 BaseHashSel;
        LwU8  Reserved3[12];
        LwU8  ExtAsymSelCount;
        LwU8  ExtHashSelCount;
        LwU8  Reserved4[2];
//      TODO - Determine if we need to use below.
//      LwU8  ExtAsymSel[4*ExtAsymCount]
//      LwU8  ExtHashSel[4*ExtHashCount]
        SPDM_ALG_TABLE AlgTables[4];
    } Algorithms;
} SPDM_MESSAGE_ALGORITHMS, *PSPDM_MESSAGE_ALGORITHMS;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8 SpdmVersionId;
        LwU8 RequestResponseCode;
        LwU8 Reserved1;
        LwU8 Reserved2;
    } GetDigests;
} SPDM_MESSAGE_GET_DIGESTS, *PSPDM_MESSAGE_GET_DIGESTS;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8 SpdmVersionId;
        LwU8 RequestResponseCode;
        LwU8 Reserved1;
        LwU8 SlotMask;
        LwU8 Digests[];
    } Digests;
} SPDM_MESSAGE_DIGESTS, *PSPDM_MESSAGE_DIGESTS;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8  SpdmVersionId;
        LwU8  RequestResponseCode;
        LwU8  SlotNumber;
        LwU8  Reserved1;
        LwU16 Offset;
        LwU16 Length;
    } GetCertificate;
} SPDM_MESSAGE_GET_CERTIFICATE, *PSPDM_MESSAGE_GET_CERTIFICATE;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8  SpdmVersionId;
        LwU8  RequestResponseCode;
        LwU8  SlotNumber;
        LwU8  Reserved1;
        LwU16 PortionLength;
        LwU16 RemainderLength;
        LwU8  CertChain[];
    } Certificate;
} SPDM_MESSAGE_CERTIFICATE, *PSPDM_MESSAGE_CERTIFICATE;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8  SpdmVersionId;
        LwU8  RequestResponseCode;
        LwU8  Attributes;
        LwU8  Reserved1;
    } EndSession;
} SPDM_MESSAGE_END_SESSION, *PSPDM_MESSAGE_END_SESSION;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8  SpdmVersionId;
        LwU8  RequestResponseCode;
        LwU8  Reserved1;
        LwU8  Reserved2;
    } EndSessionAck;
} SPDM_MESSAGE_END_SESSION_ACK, *PSPDM_MESSAGE_END_SESSION_ACK;

#define SPDM_END_SESSION_REQUEST_ATTR_NEGOTIATED_STATE_CACHE             0:0
#define SPDM_END_SESSION_REQUEST_ATTR_NEGOTIATED_STATE_CACHE_PRESERVED   0
#define SPDM_END_SESSION_REQUEST_ATTR_NEGOTIATED_STATE_CACHE_CLEAR       1

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8  SpdmVersionId;
        LwU8  RequestResponseCode;
        LwU8  KeyOperation;
        LwU8  Tag;
    } KeyUpdate;
} SPDM_MESSAGE_KEY_UPDATE, *PSPDM_MESSAGE_KEY_UPDATE;

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8  SpdmVersionId;
        LwU8  RequestResponseCode;
        LwU8  KeyOperation;
        LwU8  Tag;
    } KeyUpdateAck;
} SPDM_MESSAGE_KEY_UPDATE_ACK, *PSPDM_MESSAGE_KEY_UPDATE_ACK;

#define SPDM_KEY_UPDATE_KEY_OP_UPDATE_KEY         1
#define SPDM_KEY_UPDATE_KEY_OP_UPDATE_ALL_KEYS    2  
#define SPDM_KEY_UPDATE_KEY_OP_VERIFY_NEW_KEY     3 

typedef union
{
    SPDM_MESSAGE_HEADER Header;

    struct
    {
        LwU8 SpdmVersionId;
        LwU8 RequestResponseCode;
        LwU8 ErrorCode;
        LwU8 ErrorData;
        LwU8 ExtendedErrorData[];
    } Error;
} SPDM_MESSAGE_ERROR, *PSPDM_MESSAGE_ERROR;

#pragma pack(1)

typedef struct {
    LwU16  Alpha               : 4;
    LwU16  UpdateVersionNumber : 4;
    LwU16  MinorVersion        : 4;
    LwU16  MajorVersion        : 4;
} SPDM_VERSION_NUMBER, *PSPDM_VERSION_NUMBER;

typedef struct {
    LwU32 SpecId;            // SELWRED_MESSAGE_OPAQUE_DATA_SPEC_ID
    LwU8  OpaqueVersion;     // SELWRED_MESSAGE_OPAQUE_VERSION
    LwU8  TotalElements;
    LwU16 Reserved;
    //opaque_element_table_t  opaque_list[];
} SPDM_SELWRED_MESSAGE_GENERAL_OPAQUE_DATA_TABLE_HEADER, *PSPDM_SELWRED_MESSAGE_GENERAL_OPAQUE_DATA_TABLE_HEADER;

typedef struct {
    LwU8  Id;
    LwU8  VendorLen;
    LwU16 OpaqueElementDataLen;
    //LwU8    SmDataVersion;
    //LwU8    SmDataId;
    //LwU8    SmData[];
} SPDM_OPAQUE_ELEMENT_TABLE_HEADER, *PSPDM_OPAQUE_ELEMENT_TABLE_HEADER;

typedef struct {
    LwU8 SmDataVersion; // SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_DATA_VERSION
    LwU8 SmDataId;      // SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_ID_SUPPORTED_VERSION
    LwU8 VersionCount;
    //SPDM_VERSION_NUMBER   VersionsList[VersionCount];
} SPDM_SELWRED_MESSAGE_OPAQUE_ELEMENT_SUPPORTED_VERSION, *PSPDM_SELWRED_MESSAGE_OPAQUE_ELEMENT_SUPPORTED_VERSION;

typedef struct {
    LwU8 SmDataVersion; // SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_DATA_VERSION
    LwU8 SmDataId;      // SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_ID_VERSION_SELECTION
    SPDM_VERSION_NUMBER  SelectedVersion;
} SPDM_SELWRED_MESSAGE_OPAQUE_ELEMENT_VERSION_SELECTION, *PSPDM_SELWRED_MESSAGE_OPAQUE_ELEMENT_VERSION_SELECTION;
#pragma pack()


#define SELWRED_MESSAGE_OPAQUE_DATA_SPEC_ID (0x444D5446)
#define SELWRED_MESSAGE_OPAQUE_VERSION      (0x1)

///
/// SPDM registry_id
///
#define SPDM_REGISTRY_ID_DMTF    0
#define SPDM_REGISTRY_ID_TCG     1
#define SPDM_REGISTRY_ID_USB     2
#define SPDM_REGISTRY_ID_PCISIG  3
#define SPDM_REGISTRY_ID_IANA    4
#define SPDM_REGISTRY_ID_HDBASET 5
#define SPDM_REGISTRY_ID_MIPI    6
#define SPDM_REGISTRY_ID_CXL     7
#define SPDM_REGISTRY_ID_JEDEC   8

#define SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_DATA_VERSION         (0x1)
#define SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_ID_VERSION_SELECTION (0x0)
#define SELWRED_MESSAGE_OPAQUE_ELEMENT_SMDATA_ID_SUPPORTED_VERSION (0x1)

#endif // RMT_SPDMDEFINES_H

