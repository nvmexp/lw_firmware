/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMPROFILECONSTANTS_H__
#define __DRMPROFILECONSTANTS_H__

/* Note: Do not insert entries.  Always add at the end.  Missing entries were used in previous versions. */
#define PERF_MOD_BLACKBOX                       1
#define PERF_MOD_DRMCHAIN                       2
#define PERF_MOD_DRMHDS                         3
#define PERF_MOD_DRMHDSIMPL                     4
#define PERF_MOD_DRMLICACQREQ                   5
#define PERF_MOD_DRMLICACQRESP                  6
#define PERF_MOD_DRMLICEVAL                     7
#define PERF_MOD_DRMLICSTORE                    8
#define PERF_MOD_DRMSECSTORE                    9
#define PERF_MOD_DEVICEDEVCERT                  11
#define PERF_MOD_DRM_APP                        12
#define PERF_MOD_METERING                       13
#define PERF_MOD_OEMFILEIO                      14
#define PERF_MOD_DRMHDRPARSER                   18
#define PERF_MOD_DRMUTILITIES                   23
#define PERF_MOD_DRMXMLPARSER                   24
#define PERF_MOD_DRMBASE64                      25
#define PERF_MOD_OEMINFO                        26
#define PERF_MOD_DRMCIPHER                      27
#define PERF_MOD_DESKEY                         28
#define PERF_MOD_RC4                            29
#define PERF_MOD_DRMCBC                         30
#define PERF_MOD_OEMAES                         32
#define PERF_MOD_DRMAES                         33
#define PERF_MOD_DRMSHA256                      35
#define PERF_MOD_DRM_ECC_P256                   36
#define PERF_MOD_BCERT                          37      /* Binary Certificates */
#define PERF_MOD_DOMAINSTORE                    38      /* Domain store */
#define PERF_MOD_DOMAINAPI                      39      /* Domain APIs */
#define PERF_MOD_XMLSIG                         40      /* XML encryption/signature */
#define PERF_MOD_SOAPXML                        41      /* SOAP/XML utility */
#define PERF_MOD_LICACQV3                       42      /* V3 license acquisition */
#define PERF_MOD_UTF8                           43      /* UTF-8 */
#define PERF_MOD_DRMHMAC                        44
#define PERF_MOD_REVOCATION                     48
#define PERF_MOD_RSA                            49
#define PERF_MOD_DRMPRO                         55      /* PlayReady Object APIs */
#define PERF_MOD_DRMEMB                         56      /* License Embedding APIs */
#define PERF_MOD_DRMXMLBUILDER                  57      /* Both UTF-16 and UTF-8 XML builders */
#define PERF_MOD_NDCERT                         58      /* ND Certificates */
#define PERF_MOD_BLOBCACHE                      59      /* BLOB Cache */
#define PERF_MOD_ACTIVATION                     60

/* blackbox\blackbox.c */
#define PERF_FUNC_DRM_BBX_CipherKeySetup                    1
#define PERF_FUNC_DRM_BBX_StoreDomainPrivateKeys            2
#define PERF_FUNC_DRM_BBX_HashValue                         3
#define PERF_FUNC_DRM_BBX_SymmetricVerify                   4
#define PERF_FUNC_DRM_BBX_Initialize                        5
#define PERF_FUNC_DRM_BBX_CanBind                           6
#define PERF_FUNC__DecryptContentKeyXML                     7
#define PERF_FUNC_VerifyChecksum                            8
#define PERF_FUNC__GetDevicePrivkey                         9
#define PERF_FUNC_DRM_BBX_RebindSLK                         10
#define PERF_FUNC_DRM_BBX_DecryptLicense                    11
#define PERF_FUNC_DRM_BBX_SymmetricCipher                   12
#define PERF_FUNC_DRM_BBX_Legacy_SymmetricCipher            13

/* core\drmblobcache.c */
#define PERF_FUNC_DRM_BLOBCACHE_Verify          1
#define PERF_FUNC_DRM_BLOBCACHE_Update          2

/* core\drmchain.c */
#define PERF_FUNC__PerformActions                   1
#define PERF_FUNC__ProcessEndOfChain                2
#define PERF_FUNC_DRM_LIC_CompleteLicenseChain      3
#define PERF_FUNC_DRM_LIC_ReportActions             4
#define PERF_FUNC_DRM_LIC_CheckClockRollback        5
#define PERF_FUNC__GetLicenseInfoAndSetup           6
#define PERF_FUNC__ValidateRevocatiolwersions       7

/* core\drmhds.c */
#define PERF_FUNC_DRM_HDS_OpenStore             1
#define PERF_FUNC_DRM_HDS_CloseStore            2
#define PERF_FUNC_DRM_HDS_OpenNamespace         3
#define PERF_FUNC_DRM_HDS_OpenSlot              4
#define PERF_FUNC_DRM_HDS_InitSlotEnum          5
#define PERF_FUNC_DRM_HDS_SlotEnumNext          6

/* core\drmhds.c */
#define PERF_FUNC__HdsGetPutBlockHDR                    1
#define PERF_FUNC__HdsGetPutBlockPayload                2
#define PERF_FUNC__HdsSearchSlotInFile                  3
#define PERF_FUNC__HdsHashToChildBlock                  4
#define PERF_FUNC__HdsSearchSlotInBlock                 5
#define PERF_FUNC__HdsCopyBlockBuffer                   6
#define PERF_FUNC_DRM_HDSIMPL_AllocBlockBuffer          7
#define PERF_FUNC__HdsHashKeyToIndex                    8
#define PERF_FUNC__HdsLoadBlockHDR                      9
#define PERF_FUNC__HdsLoadSRN                           10
#define PERF_FUNC__HdsGenSRNHash                        11
#define PERF_FUNC__WriteSRN                             12
#define PERF_FUNC__ReadSRN                              13
#define PERF_FUNC__WriteCommonBlockHeader               14
#define PERF_FUNC__ReadCommonBlockHeader                15
#define PERF_FUNC__WriteChildBlockHeader                16
#define PERF_FUNC__ReadChildBlockHeader                 17
#define PERF_FUNC__WriteDataBlockHeader                 18
#define PERF_FUNC__ReadDataBlockHeader                  19
#define PERF_FUNC__HdsGenBlockHash                      20
#define PERF_FUNC__HdsGetPutChildBlockNum               21
#define PERF_FUNC__HdsExpandStore                       22
#define PERF_FUNC__HdsUpdateSRN                         23
#define PERF_FUNC__HdsAllocBlock                        24
#define PERF_FUNC__HdsFreeBlock                         25
#define PERF_FUNC__HdsLocateFreeBlockForSlot            26
#define PERF_FUNC__HdsCreateOversizedSlot               27
#define PERF_FUNC__HdsOpenSlotFromHint                  28
#define PERF_FUNC__HdsAllocSlotInFile                   29
#define PERF_FUNC__HdsTraverseBlocksInPostOrder         30
#define PERF_FUNC__HdsOpenSlot                          31
#define PERF_FUNC__HdsVerifySlotContext                 32
#define PERF_FUNC__HdsReadWriteSlot                     33
#define PERF_FUNC__HdsAdjustChildPayload                34
#define PERF_FUNC__HdsFindLeftmostLeafBlock             35
#define PERF_FUNC__HdsCopySlot_Child2Child              36
#define PERF_FUNC__HdsCopyChildPayload                  37
#define PERF_FUNC__HdsDefragmentFile                    38
#define PERF_FUNC__HdsDeleteSubTree                     39
#define PERF_FUNC__HdsTraverseNextRightSiblingBlock     40

/* core\drmliceval.c */
#define PERF_FUNC_DRM_LEVL_PerformOperationsXMR                             6

/* core\drmlicstore.c */
#define PERF_FUNC__InitEnum                     1
#define PERF_FUNC_DRM_LST_EnumNext              2
#define PERF_FUNC_DRM_LST_Open                  3
#define PERF_FUNC_DRM_LST_Close                 4
#define PERF_FUNC_DRM_LST_GetLicense            5

/* core\drmsecstore.c */
#define PERF_FUNC_DRM_SST_OpenKeyTokens                   1
#define PERF_FUNC_DRM_SST_CreateGlobalStorePassword       2
#define PERF_FUNC_DRM_SST_CreateLicenseStatePassword      3
#define PERF_FUNC_DRM_SST_CloseKey                        4
#define PERF_FUNC_DRM_SST_GetData                         5
#define PERF_FUNC_DRM_SST_SetData                         6
#define PERF_FUNC_DRM_SST_GetTokelwalue                   7
#define PERF_FUNC_DRM_SST_SetExplicitResolutionTokelwalue 8
#define PERF_FUNC__VerifySlotHash                         9

/* core\drmplayreadyobj.c */
#define PERF_FUNC_DRM_PRO_Create                                   1
#define PERF_FUNC_DRM_PRO_GetCipherTypeFromPlayReadyHeader         2
#define PERF_FUNC_DRM_PRO_GetCipherType                            3
#define PERF_FUNC_DRM_PRO_ColwertHeaderFromWmdrmToPlayReady        4
#define PERF_FUNC_Drm_PlayReadyObject_ColwertFromWmdrmHeader       5
#define PERF_FUNC_DRM_PRO_CreateRightsManagementHeader             6
#define PERF_FUNC_DRM_PRO_GetRMHeaderChecksum                      7
#define PERF_FUNC_DRM_PRO_GetDecryptorSetupTypeFromPlayReadyHeader 8
#define PERF_FUNC_DRM_PRO_GetDecryptorSetupType                    9


/* devcertparser\devcert.c */
#define PERF_FUNC_DRM_DCP_GetAttribute          1
#define PERF_FUNC_DRM_DCP_LoadPropertiesCache   2

/* devicedevcert\devicedevcert.c */
/* modules\symopt\real\symoptdevicedevcertreal.c */
#define PERF_FUNC__CompareMachineId                             1
#define PERF_FUNC_DRM_DDC_Certificates_Initialize               3

/* drmmanager\drmmanager.c */
/* modules\selwre_clock\real\selwreclockmanager.c */
#define PERF_FUNC_Drm_Initialize                                1
#define PERF_FUNC_Drm_Uninitialize                              2
#define PERF_FUNC_Drm_LicenseAcq_GenerateChallenge              3
#define PERF_FUNC_Drm_Device_GetProperty                        4
#define PERF_FUNC_Drm_LicenseAcq_ProcessResponse                5
#define PERF_FUNC_Drm_Reader_Bind                               6
#define PERF_FUNC_Drm_Reader_Commit                             7
#define PERF_FUNC_DRM_CLK_Drm_SelwreClock_ProcessResponseImpl   10
#define PERF_FUNC_DRM_CLK_Drm_SelwreClock_GenerateChallengeImpl 11
#define PERF_FUNC_DRM_CLK_Drm_SelwreClock_GetValueImpl          12
#define PERF_FUNC_Drm_StoreMgmt_CleanupStore                    13
#define PERF_FUNC_DRM_CLK_CheckSelwreClock                      15
#define PERF_FUNC__SetupLicEvalObjectToShare                    16
#define PERF_FUNC_Drm_Elwelope_Open                             20
#define PERF_FUNC_Drm_Elwelope_InitializeRead                   21
#define PERF_FUNC_Drm_Elwelope_Close                            22
#define PERF_FUNC_Drm_Elwelope_GetSize                          23
#define PERF_FUNC_Drm_Elwelope_Read                             24
#define PERF_FUNC_Drm_Elwelope_Seek                             25
#define PERF_FUNC_Drm_StoreMgmt_DeleteLicenses                  26
#define PERF_FUNC_Drm_License_GetProperty                       27
#define PERF_FUNC_Drm_Content_UpdateEmbeddedStore               28
#define PERF_FUNC_Drm_Content_GetProperty                       29
#define PERF_FUNC_Drm_Elwelope_WritePlayReadyObject             30
#define PERF_FUNC_Drm_Content_UpdateEmbeddedStore_Commit        31
#define PERF_FUNC_Drm_Revocation_SetBuffer                      32
#define PERF_FUNC_Drm_Activation_ProcessResponseGenerateChallenge 33

/* modules\metering\real\drmmeterimpv1.c */
/* modules\metering\real\drmmeterimp.c */
/* modules\metering\real\drmmeterapi.c */
#define PERF_FUNC_DRM_MTR_UpdateData                        1
#define PERF_FUNC_DRM_MTR_ProcessMeterResponse              2
#define PERF_FUNC__BuildMeterCertChallengeXML               3
#define PERF_FUNC__ProcessMeterCertInResponse               4
#define PERF_FUNC_DRM_MTR_GenerateMeterCertChallenge        5
#define PERF_FUNC_DRM_MTR_ProcessMeterCertResponse          6
#define PERF_FUNC_Drm_Metering_GenerateChallenge            7
#define PERF_FUNC_Drm_Metering_ProcessResponse              8
#define PERF_FUNC_Drm_MeterCert_Update                      9
#define PERF_FUNC_Drm_MeterCert_InitEnum                    10
#define PERF_FUNC_Drm_MeterCert_EnumNext                    11
#define PERF_FUNC__DecryptCipherData                        12
#define PERF_FUNC_Drm_MeterCert_GenerateChallenge           13
#define PERF_FUNC_Drm_MeterCert_ProcessResponse             14
#define PERF_FUNC_DRM_MTR_GenerateMeterDataChallenge        15
#define PERF_FUNC_DRM_MTR_ProcessMeterDataResponse          16
#define PERF_FUNC_DRM_MTR_ParseBinaryMeterCert              17
#define PERF_FUNC__ProcessMeterDataResponse                 18
#define PERF_FUNC__CalcMaximumChallengeCharCount            19
#define PERF_FUNC__CalcFixedUnencryptedChallengeCharCount   20
#define PERF_FUNC__CalcFixedEncryptedChallengeCharCount     21
#define PERF_FUNC__CalcVariableChallengeCharCount           22
#define PERF_FUNC__ProcessAllKIDs                           23
#define PERF_FUNC__CalcKIDDataCharCount                     24
#define PERF_FUNC__BuildKIDDataXML                          25
#define PERF_FUNC__BuildMeterDataChallengeXML               26
#define PERF_FUNC__BuildMeterDataChallengeDataXML           27
#define PERF_FUNC__PrepareMeterChallengeContext             28
#define PERF_FUNC_Drm_GetMeterCert_By_MID                   29

/* modules\activation\real\drmactivationinternal.c */
#define PERF_FUNC_DRM_ACT_GenerateChallengeInternal     1
#define PERF_FUNC_DRM_ACT_ProcessWSDLResponseInternal   2

/* oem\ansi\oemfileio.c */
#define PERF_FUNC_Oem_File_SetFilePointer       1
#define PERF_FUNC_Oem_File_Lock                 2
#define PERF_FUNC_Oem_File_Unlock               3
#define PERF_FUNC_Oem_File_Open                 4
#define PERF_FUNC_Oem_File_Read                 5
#define PERF_FUNC_Oem_File_Write                6
#define PERF_FUNC_Oem_File_GetSize              7
#define PERF_FUNC_Oem_File_FlushBuffers         8

/* modules\selwre_clock\real\selwreclockrequest.c */
#define PERF_FUNC_DRM_CLK_CreateChallenge       1

/* selwre_clock\selwreclockresponse.c */
#define PERF_FUNC_DRM_CLK_ProcessResponse       1

/* core\drmutilities.c */
/* core\drmutilitieslite.c */
/* modules\certcache\real\certcachereal.c */
#define PERF_FUNC__CheckCertificate                         1
#define PERF_FUNC_DRM_CERTCACHE_VerifyCachedCertificate     2

/* modules\xmlparser\common\drmxmlparserlite.c */
#define PERF_FUNC__GetXMLSubNodeW               1
#define PERF_FUNC_DRM_XML_GetSubNodeByPath      2
#define PERF_FUNC_DRM_XML_GetNodeAttribute      3
#define PERF_FUNC_DRM_XML_GetSubNodeA           4
#define PERF_FUNC__GetXMLSubNodeA               5

/* oem\ansi\oeminfo.c */
/* oem\wince\oeminfo.c */
#define PERF_FUNC_Oem_Device_GetModelInfo       1

/* core\drmcipher.c */
#define PERF_FUNC_DRM_CPHR_InitDecrypt          1
#define PERF_FUNC_DRM_CPHR_Decrypt              2
#define PERF_FUNC_DRM_CPHR_Encrypt              3
#define PERF_FUNC_DRM_CPHR_Init                 4

/* oem\common\aes\oemaes.c */
#define PERF_FUNC_Oem_Aes_SetKey                1
#define PERF_FUNC_Oem_Aes_EncryptOne            2
#define PERF_FUNC_Oem_Aes_DecryptOne            3

/* oem\common\AES\oemaesmulti.c */
#define PERF_FUNC_Oem_Aes_CtrProcessData        1
#define PERF_FUNC_Oem_Aes_EcbEncryptData        2
#define PERF_FUNC_Oem_Aes_EcbDecryptData        3
#define PERF_FUNC_Omac1_GenerateSignTag         4
#define PERF_FUNC_Omac1_GenerateSignInfo        5
#define PERF_FUNC_Oem_Omac1_Sign                6
#define PERF_FUNC_Oem_Omac1_Verify              7
#define PERF_FUNC_Oem_Aes_CbcEncryptData        8
#define PERF_FUNC_Oem_Aes_CbcDecryptData        9

/* certs\drmbcertbuilder.c */
/* certs\drmbcertparser.c */
#define PERF_FUNC_calcManufacturerInfoSize                      1
#define PERF_FUNC_calcKeyInfoSize                               2
#define PERF_FUNC_calcFeatureInfoSize                           3
#define PERF_FUNC_calcSignatureInfoSize                         4
#define PERF_FUNC_calcDomainInfoSize                            5
#define PERF_FUNC_calcCertSize                                  6
#define PERF_FUNC_checkBuffer                                   7
#define PERF_FUNC_addAlignedData                                8
#define PERF_FUNC_updateCertChainHeader                         9
#define PERF_FUNC_addCertChainHeader                            10
#define PERF_FUNC_addCertHeader                                 11
#define PERF_FUNC_addCertObjectHeader                           12
#define PERF_FUNC_addCertBasicInfo                              13
#define PERF_FUNC_addCertFeatureInfo                            14
#define PERF_FUNC_addCertKeyInfo                                15
#define PERF_FUNC_addCertManufacturerString                     16
#define PERF_FUNC_addCertManufacturerInfo                       17
#define PERF_FUNC_addCertSignatureInfo                          18
#define PERF_FUNC_addCertDomainInfo                             19
#define PERF_FUNC_addCertPCInfo                                 20
#define PERF_FUNC_addCertDeviceInfo                             21
#define PERF_FUNC_BCert_AddCert                                 22
#define PERF_FUNC_DRM_BCert_GetSelwrityVersion                  23
#define PERF_FUNC_addCertSelwrityVersion                        24
#define PERF_FUNC_verifyAdjacentCerts                           25
#define PERF_FUNC_getObjectHeader                               26
#define PERF_FUNC_parseCertHeader                               27
#define PERF_FUNC_parseCertBasicInfo                            28
#define PERF_FUNC_parseDomainInfo                               29
#define PERF_FUNC_parsePCInfo                                   30
#define PERF_FUNC_parseDeviceInfo                               31
#define PERF_FUNC_parseFeatureInfo                              32
#define PERF_FUNC_parseManufacturerString                       33
#define PERF_FUNC_parseSignatureInfo                            34
#define PERF_FUNC_parseKeyInfo                                  35
#define PERF_FUNC_parseCertificate                              36
#define PERF_FUNC_DRM_BCert_FindObjectInCertByType              37
#define PERF_FUNC_DRM_BCert_GetChainHeader                      38
#define PERF_FUNC_DRM_BCert_ParseCertificateChain               39
#define PERF_FUNC_DRM_BCert_GetCertificate                      40
#define PERF_FUNC_DRM_BCert_GetPublicKey                        41
#define PERF_FUNC_addCertSilverLightInfo                        42
#define PERF_FUNC_parseSilverLightInfo                          43
#define PERF_FUNC_DRM_BCert_VerifySignature                     44
#define PERF_FUNC_calcExtDataSignKeyInfoSize                    45
#define PERF_FUNC_calcExtDataContainerSize                      46
#define PERF_FUNC_addExtDataSignKeyInfo                         47
#define PERF_FUNC_addExtDataContainer                           48
#define PERF_FUNC_BCert_AddExtendedDataToCert                   49
#define PERF_FUNC_verifyExtDataSignature                        50
#define PERF_FUNC_parseExtDataSignKeyInfo                       51
#define PERF_FUNC_parseExtDataContainer                         52
#define PERF_FUNC_addCertMeteringInfo                           53
#define PERF_FUNC_calcMeteringInfoSize                          54
#define PERF_FUNC_parseSelwrityVersion                          55
#define PERF_FUNC_DRM_BCert_GetPublicKeyByUsage                 56
#define PERF_FUNC_parseServerInfo                               57
#define PERF_FUNC_addCertServerInfo                             58
#define PERF_FUNC_DRM_BCert_GetManufacturerStrings              59
#define PERF_FUNC_BCert_AddCert_BBX                             60

/* domainstore\drmdomainstore.c */
#define PERF_FUNC__DomainStore_AddData          1
#define PERF_FUNC__DomainStore_GetData          2
#define PERF_FUNC__DomainStore_DeleteData       3
#define PERF_FUNC__DomainStore_InitEnumData     4
#define PERF_FUNC__DomainStore_EnumNextData     5
#define PERF_FUNC_DRM_DOMST_OpenStore           6
#define PERF_FUNC_DRM_DOMST_CloseStore          7
#define PERF_FUNC_DRM_DOMST_DeleteKeys          8

/* domain\drmdomainimp.c */
/* domain\drmdomainkeyxmrparser.c */
#define PERF_FUNC__BuildJoinChallengeDataXML                1
#define PERF_FUNC__BuildLeaveChallengeDataXML               2
#define PERF_FUNC__BuildJoinChallengeXML                    3
#define PERF_FUNC__BuildLeaveChallengeXML                   4
#define PERF_FUNC_DRM_DOMKEYXMR_GetHeader                   5
#define PERF_FUNC_DRM_DOMKEYXMR_GetSessionKey               6
#define PERF_FUNC_DRM_DOMKEYXMR_GetPrivKeyContainer         7
#define PERF_FUNC_DRM_DOMKEYXMR_GetPrivKey                  8
#define PERF_FUNC__ProcessJoinDataFromResponse              9
#define PERF_FUNC__ParseLeaveDomainChallengeQueryData       10
#define PERF_FUNC_DRM_DOM_GenerateJoinChallenge             11
#define PERF_FUNC_DRM_DOM_ProcessJoinResponse               12
#define PERF_FUNC_DRM_DOM_GenerateLeaveChallenge            13
#define PERF_FUNC_DRM_DOM_ProcessLeaveResponse              14
#define PERF_FUNC_DRM_DOM_FindCert                          15
#define PERF_FUNC_DRM_DOM_InitCertEnum                      16
#define PERF_FUNC_DRM_DOM_EnumNextCert                      17
#define PERF_FUNC__ParseJoinDomainChallengeQueryData        18

/* core\drmxmlsig.c */
#define PERF_FUNC__CalcSHA256Hash                               1
#define PERF_FUNC__VerifySHA256Hash                             2
#define PERF_FUNC__BuildCipherDataNode                          3
#define PERF_FUNC_DRM_XMLSIG_ExtractCipherData                  4
#define PERF_FUNC__BuildSignedInfoNode                          5
#define PERF_FUNC__BuildSignatureValueNode                      6
#define PERF_FUNC__BuildPublicKeyInfoNodeWithName               7
#define PERF_FUNC__ExtractPublicKeyByName                       8
#define PERF_FUNC__BuildECC256PublicKeyInfoNode                 9
#define PERF_FUNC__ExtractECC256PublicKey                       10
#define PERF_FUNC_DRM_XMLSIG_BuildEncryptedKeyInfoNode          11
#define PERF_FUNC_DRM_XMLSIG_BuildEncryptedDataNode             12
#define PERF_FUNC_DRM_XMLSIG_BuildSignatureNode                 13
#define PERF_FUNC_DRM_XMLSIG_VerifySignature                    14

/* core\drmsoapxmlutility.c */
#define PERF_FUNC_DRM_SOAPXML_EncodeData                  1
#define PERF_FUNC_DRM_SOAPXML_DecodeData                  2
#define PERF_FUNC_DRM_SOAPXML_GetDeviceCert               3
#define PERF_FUNC_DRM_SOAPXML_InitXMLKey                  4
#define PERF_FUNC_DRM_SOAPXML_EncryptDataWithXMLKey       5
#define PERF_FUNC_DRM_SOAPXML_BuildSOAPHeaderXML          6
#define PERF_FUNC__ParseLwstomData                        7
#define PERF_FUNC__ParseLwstomDataByPath                  8
#define PERF_FUNC_DRM_SOAPXML_ParseStatusCode             9
#define PERF_FUNC_Drm_GetAdditionalResponseData           10
#define PERF_FUNC_DRM_SOAPXML_GetAdditionalResponseData   11
#define PERF_FUNC_DRM_SOAPXML_ParseLwstomDataForProtocol  12
#define PERF_FUNC_DRM_SOAPXML_ValidateProtocolSignature   13
#define PERF_FUNC_DRM_SOAPXML_BuildClientInfo             14
#define PERF_FUNC__GetAdditionalResponseExceptionData     15
#define PERF_FUNC__GetAdditionalResponseDomainIDData      16

/* core\drmlicacqv3.c */
/* drmmanager\drmmanager.c */
#define PERF_FUNC__CalcDeviceCertCharCount                1
#define PERF_FUNC__CalcDomainCertsCharCount               2
#define PERF_FUNC__BuildDomainCertsXML                    3
#define PERF_FUNC__BuildCertChainsXML                     4
#define PERF_FUNC__BuildRevListInfoXML                    5
#define PERF_FUNC__BuildLicenseChallengeDataXML           6
#define PERF_FUNC__BuildLicenseAcknowledgementDataXML     7
#define PERF_FUNC__BuildLicenseChallengeXML               8
#define PERF_FUNC__BuildLicenseAcknowledgementXML         9
#define PERF_FUNC__GetLicenseState                        11
#define PERF_FUNC__PrepareUplinks                         12
#define PERF_FUNC__PrepareLicenseChallenge                13
#define PERF_FUNC_DRM_LA_ParseLicenseAcquisitionURL       14
#define PERF_FUNC_DRM_LA_ProcessLicenseV3                 15
#define PERF_FUNC__ExtractLicensesFromLicenseResponse     16
#define PERF_FUNC_DRM_LA_ProcessRevocationPackage         17
#define PERF_FUNC__ExtractDataFromLicenseResponse         18
#define PERF_FUNC__GenerateLicenseChallengeV3             19
#define PERF_FUNC__ProcessLicenseResponseV3               20
#define PERF_FUNC_Drm_LicenseAcq_GenerateAck              21
#define PERF_FUNC_Drm_LicenseAcq_ProcessAckResponse       22

/* core\drmutf.c */
#define PERF_FUNC_DRM_UTF8_VerifyBytes          1

/* core\drmhmac.c */
#define PERF_FUNC_DRM_HMAC_CreateMAC            1
#define PERF_FUNC_DRM_HMAC_VerifyMAC            2

/* oem\common\ecc\base\oemeccp256.c */
/* oem\common\ecc\baseimpl\oemeccp256impl.c */
#define PERF_FUNC_OEM_ECDSA_Verify_P256                             1
#define PERF_FUNC_OEM_ECDSA_Sign_P256                               2
#define PERF_FUNC_OEM_ECC_GenKeyPair_P256                           3
#define PERF_FUNC_OEM_ECC_Encrypt_P256                              4
#define PERF_FUNC_OEM_ECC_Decrypt_P256                              5
#define PERF_FUNC_OEM_ECC_CanMapToPoint_P256                        6
#define PERF_FUNC_OEM_ECC_GenerateHMACKey_P256                      7
#define PERF_FUNC_Colwert_P256_PointToPlaintext                     8
#define PERF_FUNC_OEM_ECC_GenKeyPairRestrictedPriv_P256             9
#define PERF_FUNC_Colwert_P256_PointToBigEndianBytes                10
#define PERF_FUNC_Colwert_P256_PlaintextToPoint                     11
#define PERF_FUNC_Colwert_P256_ModularIntToDigitsModOrder           12
#define PERF_FUNC_Colwert_P256_ModularIntToBigEndianBytesModOrder   13
#define PERF_FUNC_Colwert_P256_DigitsToBigEndianBytes               14
#define PERF_FUNC_DRM_ECC_MapX2PointP256                            15
#define PERF_FUNC_Colwert_P256_BigEndianBytesToPoint                16
#define PERF_FUNC_Colwert_P256_BigEndianBytesToModular              17
#define PERF_FUNC_Colwert_P256_BigEndianBytesToDigitsModOrder       18
#define PERF_FUNC_Colwert_DigitsToBigEndianBytes                    19
#define PERF_FUNC_Colwert_P256_BigEndianBytesToDigits               20
#define PERF_FUNC_Colwert_BigEndianBytesToDigits                    21

/* crypto\drmsha256\drmsha256.c */
#define PERF_FUNC_OEM_SHA256_Init               1
#define PERF_FUNC_OEM_SHA256_UpdateOffset       2
#define PERF_FUNC_OEM_SHA256_Finalize           3
#define PERF_FUNC_SHA256_Transform              4

/* Revocation */
/* core\drmrevocation.c */
/* modules\devicerevocation\real\devicerevocationimplreal.c */
/* modules\legacyxmlcert\real\drmlegacyxmlcertrevocationreal.c */
/* core\drmrevocationstore.c */
/* core\drmbcrlparser.c*/
#define PERF_FUNC__CheckCertInRevocationList                        3
#define PERF_FUNC_DRM_RVK_UpdateRevocatiolwersionsCache             7
#define PERF_FUNC_DRM_RVK_VerifyRevocationList                      8
#define PERF_FUNC_DRM_RVK_UpdateRevocationList                      9
#define PERF_FUNC_DRM_RVK_GetLegacyXMLCertList                      10
#define PERF_FUNC_DRM_RVK_GetSSTRevocationList                      11
#define PERF_FUNC_DRM_RVK_VerifyLegacyXMLCertRevocationList         12
#define PERF_FUNC_DRM_RVK_UpdateLegacyXMLCertRevocationList         13
#define PERF_FUNC_DRM_RVK_UpdateLegacyXMLCertRevocationListDecoded  14
#define PERF_FUNC_DRM_RVK_VerifyLegacyXMLCertCRLSignature           15
#define PERF_FUNC_DRM_RVK_GetLegacyXMLCertRevocationEntries         16
#define PERF_FUNC_DRM_RVK_VerifyBinaryLegacyXMLCertSignature        17
#define PERF_FUNC_DRM_RVK_VerifyRevocationInfo                      18
#define PERF_FUNC_DRM_RVK_GetLwrrentRevocationInfo                  19
#define PERF_FUNC_DRM_RVK_StoreRevocationLists                      20
#define PERF_FUNC_DRM_RVK_StoreRevInfo                              21
#define PERF_FUNC__ExtractRevocationList                            23
#define PERF_FUNC_DRM_RVK_GetCRL                                    24
#define PERF_FUNC_DRM_RVK_SetCRL                                    25
#define PERF_FUNC_DRM_RVS_InitRevocationStore                       26
#define PERF_FUNC_DRM_RVS_StoreRevocationData                       27
#define PERF_FUNC__CreateRevocationStorePassword                    28
#define PERF_FUNC__LoopkupRevocationLIDFromGUID                     29
#define PERF_FUNC_DRM_RVS_GetRevocationData                         30
#define PERF_FUNC_DRM_BCrl_VerifySignature                          31
#define PERF_FUNC_DRM_RVK_GetDeviceRevocationList                   33
#define PERF_FUNC__UpdateRevocationList                             34
/* RSA */
/* oem\common\RSA\oaeppssimpl\oemrsaoaeppssimpl.c */
/* oem\common\RSA\base\oemrsa.c */
#define PERF_FUNC__OAEPDecode                                       1
#define PERF_FUNC__GenerateMGF1Mask                                 2
#define PERF_FUNC__OAEPEncode                                       3
#define PERF_FUNC__PSSEncode                                        4
#define PERF_FUNC__PSSVerify                                        5
#define PERF_FUNC__BigEndianBytesToDigits                           6
#define PERF_FUNC__DigitsToBigEndianBytes                           7
#define PERF_FUNC_OEM_RSA_SetPublicKey_2048BIT                      8
#define PERF_FUNC_OEM_RSA_SetPrivateKey_2048BIT                     9
#define PERF_FUNC_OEM_RSA_ParsePublicKey_2048BIT                    10
#define PERF_FUNC_OEM_RSA_ZeroPublicKey_2048BIT                     11
#define PERF_FUNC_OEM_RSA_ZeroPrivateKey_2048BIT                    12
#define PERF_FUNC__ModularExponentiate                              13
#define PERF_FUNC_OEM_RSA_Decrypt_2048BIT                           14
#define PERF_FUNC_OEM_RSA_OaepDecrypt_2048BIT                       15
#define PERF_FUNC_OEM_RSA_OaepEncrypt_2048BIT                       16
#define PERF_FUNC_OEM_RSA_OaepEncrypt_4096BIT                       17
#define PERF_FUNC_OEM_RSA_PssSign_2048BIT                           18
#define PERF_FUNC_OEM_RSA_PssVerify_2048BIT                         19
#define PERF_FUNC_OEM_RSA_PssVerify_4096BIT                         20
#define PERF_FUNC_OEM_RSA_PssSign_4096BIT                           21
#define PERF_FUNC_OEM_RSA_OaepDecrypt_4096BIT                       22



/* modules\legacyxmlcert\real\drmlegacyxmlcertrsakeysreal.c */
#define PERF_FUNC_DRM_XML_RSA_WritePublicKeyNodeA                  12
#define PERF_FUNC_DRM_XML_RSA_WritePublicKeyNode                   13
#define PERF_FUNC_DRM_XML_RSA_ParseBase64PublicKey                 15
#define PERF_FUNC_DRM_XML_RSA_ParseBase64PublicKeyA                16
#define PERF_FUNC_DRM_XML_RSA_WritePrivateKeyNode                  17
#define PERF_FUNC_DRM_XML_RSA_ParseBase64PrivateKey                18

/* core\drmembedding.c */
#define PERF_FUNC_DRM_EMB_UpdateEmbeddedStore                       1

/* DRM XML builder, UTF-8 and UTF-16 */
/* modules\xmlbuilder\common\drmxmlbuilderalite.c */
/* modules\xmlbuilder\common\drmxmlbuilderulite.c */
/* modules\xmlbuilder\real\drmxmlbuilder.c */
#define PERF_FUNC_DRM_XMB_CreateDolwmentA                   1
#define PERF_FUNC_DRM_XMB_CloseDolwmentA                    2
#define PERF_FUNC_DRM_XMB_OpenNodeA                         3
#define PERF_FUNC_DRM_XMB_CloseLwrrNodeA                    4
#define PERF_FUNC_DRM_XMB_AddAttributeA                     5
#define PERF_FUNC_DRM_XMB_AddDataA                          6
#define PERF_FUNC_DRM_XMB_WriteTagA                         7
#define PERF_FUNC_DRM_XMB_AddCDataA                         8
#define PERF_FUNC_DRM_XMB_WriteCDATATagA                    9
#define PERF_FUNC_DRM_XMB_ReserveSpaceA                     10
#define PERF_FUNC_DRM_XMB_ShiftDataFromLwrrentPositionA     11
#define PERF_FUNC_DRM_XMB_ShiftLwrrentPointerA              12
#define PERF_FUNC_DRM_XMB_GetLwrrentBufferPointerA          13
#define PERF_FUNC_DRM_XMB_AppendNodeA                       14
#define PERF_FUNC_DRM_XMB_AESEncryptAndCloseLwrrNodeA       15
#define PERF_FUNC_DRM_XMB_RSASignAndCloseLwrrNodeA          16
#define PERF_FUNC_DRM_XMB_HashAndRSASignAndCloseLwrrNodeA   17
#define PERF_FUNC_DRM_XMB_HashAndCloseLwrrNodeA             18

/* Start wide-char function with some gap in values */
#define PERF_FUNC_DRM_XMB_CreateDolwment                    22
#define PERF_FUNC_DRM_XMB_ReallocDolwment                   23
#define PERF_FUNC_DRM_XMB_CloseDolwment                     24
#define PERF_FUNC_DRM_XMB_OpenNode                          25
#define PERF_FUNC_DRM_XMB_CloseLwrrNode                     26
#define PERF_FUNC_DRM_XMB_GetLwrrNodeName                   27
#define PERF_FUNC_DRM_XMB_AddAttribute                      28
#define PERF_FUNC_DRM_XMB_AddData                           29
#define PERF_FUNC_DRM_XMB_ReserveSpace                      30
#define PERF_FUNC_DRM_XMB_AddCData                          31
#define PERF_FUNC_DRM_XMB_AppendNode                        32
#define PERF_FUNC_DRM_XMB_AddXMLNode                        33
#define PERF_FUNC_DRM_XMB_WriteTag                          34
#define PERF_FUNC_DRM_XMB_WriteCDATATag                     35
#define PERF_FUNC_DRM_XMB_EncryptAndCloseLwrrNode           36
#define PERF_FUNC_DRM_XMB_SignAndCloseLwrrNode              37
#define PERF_FUNC_DRM_XMB_KeyedHashAndCloseLwrrNode         38

#endif   /* #ifndef __DRMPROFILECONSTANTS_H__ */

