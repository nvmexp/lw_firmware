/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRMMODULESUPPORT_H__
#define __DRMMODULESUPPORT_H__

#include <drmtypes.h>
#include <drmactivation.h>
#include <drmselwrecoretypes.h>

ENTER_PK_NAMESPACE;

/***************************************************************************
**
** Declare the DRM_MODULEX_IsModuleXSupported functions for all modules.
**
***************************************************************************/

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_ARCLK_IsAntirollbackClockSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_ARCLK_IsAntirollbackClockUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_ARCLK_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_CDMI_IsCdmiSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_CDMI_IsCdmiUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_CDMI_GetReeFeatureXml( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_CLEANSTORE_IsCleanStoreSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_CLEANSTORE_IsCleanStoreUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_CLEANSTORE_GetReeFeatureXml( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_CONTRACT_IsContractSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_CONTRACT_IsContractUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_CONTRACT_GetReeFeatureXml( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_CRT_IsCRTSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_CRT_IsCRTUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_CRT_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_DEVICEASSETS_IsDeviceAssetsSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_DEVICEASSETS_IsDeviceAssetsUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_DEVICEASSETS_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_DOMAIN_IsDomainSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_DOMAIN_IsDomainUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_DOMAIN_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_EST_IsESTSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_EST_IsESTUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_EST_GetReeFeatureXml( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_HDS_IsHDSSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_HDS_IsHDSUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_HDS_GetReeFeatureXml( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_HBHC_IsHdsBlockHeaderCacheSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_HBHC_IsHdsBlockHeaderCacheUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_HBHC_GetReeFeatureXml( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_HDSSESSION_IsHDSSessionSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_HDSSESSION_IsHDSSessionUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_HDSSESSION_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_LICGEN_IsLicGenSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_LICGEN_IsLicGenUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_LICGEN_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_KG_IsKeyGenSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_KG_IsKeyGenUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_KG_GetReeFeatureXml( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_METERCERTREVOCATION_IsMeterCertRevocationSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_METERCERTREVOCATION_IsMeterCertRevocationUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_METERCERTREVOCATION_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_METERING_IsMeteringSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_METERING_IsMeteringUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_METERING_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_MODELREVOCATION_IsModelRevocationSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_MODELREVOCATION_IsModelRevocationUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_MODELREVOCATION_GetReeFeatureXml( DRM_VOID );

#define DRM_NST_IsNSTSupported()   DRM_EST_IsESTSupported()
#define DRM_NST_IsNSTUnsupported() DRM_EST_IsESTUnsupported()
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_NST_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PERF_IsPerformanceSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PERF_IsPerformanceUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_PERF_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PROFILING_IsProfilingSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PROFILING_IsProfilingUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_PROFILING_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_REACT_IsReactivationSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_REACT_IsReactivationUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_REACT_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_REVOCATION_IsRevocationSupported( DRM_VOID );
#define DRM_REVOCATION_IsV1RIVSupported(x) DRM_LEGACYXMLCERT_IsLegacyXMLCertSupported(x)
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_REVOCATION_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_SELWREDELETE_IsSelwreDeleteSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_SELWREDELETE_IsSelwreDeleteUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_SELWREDELETE_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_SELWRESTOP_IsSelwreStopSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_SELWRESTOP_IsSelwreStopUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_SELWRESTOP_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_SELWRETIME_IsSelwreTimeSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_SELWRETIME_IsSelwreTimeUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_SELWRETIME_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_STRUCTURED_IsStructuredSerializationSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_STRUCTURED_IsStructuredSerializationUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_STRUCTURED_GetReeFeatureXml( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_LEGACYXMLCERT_IsLegacyXMLCertSupported( _In_ DRM_SELWRECORE_CONTEXT *f_pSelwreCoreCtx );
DRM_API DRM_BOOL DRM_CALL DRM_LEGACYXMLCERT_IsLegacyXMLCertSupported_NoContext( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_LEGACYXMLCERT_GetReeFeatureXml( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_XMLHASH_IsXmlHashSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_XMLHASH_IsXmlHashUnsupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_XMLHASH_GetReeFeatureXml( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_H264_IsSupported( DRM_VOID );
/*
** The H.264 slice header parsing code is only linked into the TEE and only when the DRM_TEE_H264_PreProcessEncryptedData API is supported.
** Because there is no public API associated with this functionality, and we may or may not add one in the future, we do not include this in
** the FeatureXml information we send to the server since it's effectively FALSE for the REE and TEE support is specified by relevant TEE API
** information.  If we add a public API for this later, that is when we will add associated FeatureXml API.
*/

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_COCKTAIL_IsSupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_LICACQ_IsLicacqSupported( DRM_VOID );
DRM_NO_INLINE DRM_API const DRM_ANSI_CONST_STRING *DRM_CALL DRM_LICACQ_GetReeFeatureXml( DRM_VOID );

//typedef const DRM_ANSI_CONST_STRING *( DRM_CALL *pfnReeFeatureXmlFunc )();

#define DRM_ALL_REE_FEATURE_XML_FUNCS                               \
            DRM_ACTIVATION_GetReeFeatureXml,                        \
            DRM_ARCLK_GetReeFeatureXml,                             \
            DRM_CDMI_GetReeFeatureXml,                              \
            DRM_CLEANSTORE_GetReeFeatureXml,                        \
            DRM_CONTRACT_GetReeFeatureXml,                          \
            DRM_CRT_GetReeFeatureXml,                               \
            DRM_DEVICEASSETS_GetReeFeatureXml,                      \
            DRM_DOMAIN_GetReeFeatureXml,                            \
            DRM_EST_GetReeFeatureXml,                               \
            DRM_HDS_GetReeFeatureXml,                               \
            DRM_HBHC_GetReeFeatureXml,                              \
            DRM_HDSSESSION_GetReeFeatureXml,                        \
            DRM_KG_GetReeFeatureXml,                                \
            DRM_LICGEN_GetReeFeatureXml,                            \
            DRM_METERCERTREVOCATION_GetReeFeatureXml,               \
            DRM_METERING_GetReeFeatureXml,                          \
            DRM_MODELREVOCATION_GetReeFeatureXml,                   \
            DRM_NST_GetReeFeatureXml,                               \
            DRM_PERF_GetReeFeatureXml,                              \
            DRM_REACT_GetReeFeatureXml,                             \
            DRM_REVOCATION_GetReeFeatureXml,                        \
            DRM_SELWREDELETE_GetReeFeatureXml,                      \
            DRM_SELWRESTOP_GetReeFeatureXml,                        \
            DRM_SELWRETIME_GetReeFeatureXml,                        \
            DRM_STRUCTURED_GetReeFeatureXml,                        \
            DRM_LEGACYXMLCERT_GetReeFeatureXml,                     \
            DRM_XMLHASH_GetReeFeatureXml,                           \
            DRM_LICACQ_GetReeFeatureXml,                            \

//typedef DRM_BOOL ( DRM_CALL *pfnReeFeatureIsSupported )();

/* 
** DRM_CRT_IsCRTUnsupported is included because thir will be XML in the REE Features
** segment when CRT is not supported. EST is included twice because one is for
** DRM_NST_IsNSTSupported, but because that is just a macro that runs DRM_EST_IsESTSupported,
** DRM_EST_IsESTSupported is used in its place. 
*/
#define DRM_ALL_REE_FEATURE_IS_SUPPORTED_FUNCS                      \
        DRM_ACTIVATION_IsActivationSupported,                       \
        DRM_ARCLK_IsAntirollbackClockSupported,                     \
        DRM_CDMI_IsCdmiSupported,                                   \
        DRM_CLEANSTORE_IsCleanStoreSupported,                       \
        DRM_CONTRACT_IsContractSupported,                           \
        DRM_CRT_IsCRTSupported,                                     \
        DRM_DEVICEASSETS_IsDeviceAssetsSupported,                   \
        DRM_DOMAIN_IsDomainSupported,                               \
        DRM_EST_IsESTSupported,                                     \
        DRM_HDS_IsHDSSupported,                                     \
        DRM_HBHC_IsHdsBlockHeaderCacheSupported,                    \
        DRM_HDSSESSION_IsHDSSessionSupported,                       \
        DRM_KG_IsKeyGenSupported,                                   \
        DRM_LICGEN_IsLicGenSupported,                               \
        DRM_METERCERTREVOCATION_IsMeterCertRevocationSupported,     \
        DRM_METERING_IsMeteringSupported,                           \
        DRM_MODELREVOCATION_IsModelRevocationSupported,             \
        DRM_EST_IsESTSupported,                                     \
        DRM_PERF_IsPerformanceSupported,                            \
        DRM_REACT_IsReactivationSupported,                          \
        DRM_REVOCATION_IsRevocationSupported,                       \
        DRM_SELWREDELETE_IsSelwreDeleteSupported,                   \
        DRM_SELWRESTOP_IsSelwreStopSupported,                       \
        DRM_SELWRETIME_IsSelwreTimeSupported,                       \
        DRM_STRUCTURED_IsStructuredSerializationSupported,          \
        DRM_XMLHASH_IsXmlHashSupported,                             \
        DRM_LICACQ_IsLicacqSupported,                               \

typedef DRM_BOOL ( DRM_CALL *pfnReeFeatureIsSupportedWithSelwreCore )( _In_ DRM_SELWRECORE_CONTEXT *f_pSelwreCoreCtx );

#define DRM_ALL_REE_FEATURE_IS_SUPPORTED_FUNCS_WITH_SELWRE_CORE     \
        DRM_LEGACYXMLCERT_IsLegacyXMLCertSupported,                 \


EXIT_PK_NAMESPACE;

#endif  /* __DRMMODULESUPPORT_H__ */

