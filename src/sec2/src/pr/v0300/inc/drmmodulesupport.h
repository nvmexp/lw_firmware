/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMMODULESUPPORT_H
#define DRMMODULESUPPORT_H

#include <drmselwrecoretypes.h>

ENTER_PK_NAMESPACE;

/***************************************************************************
**
** Declare the DRM_MODULEX_IsModuleXSupported functions for all modules.
**
***************************************************************************/

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_ARCLK_IsAntirollbackClockSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_ARCLK_IsAntirollbackClockUnsupported( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_CERTCACHE_IsCertCacheSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_CERTCACHE_IsCertCacheUnsupported( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_CLEANSTORE_IsCleanStoreSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_CLEANSTORE_IsCleanStoreUnsupported( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_CONTRACT_IsContractSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_CONTRACT_IsContractUnsupported( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_CRT_IsCRTSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_CRT_IsCRTUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_DEVICEASSETS_IsDeviceAssetsSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_DEVICEASSETS_IsDeviceAssetsUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_DOMAIN_IsDomainSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_DOMAIN_IsDomainUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_EST_IsESTSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_EST_IsESTUnsupported( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_HDS_IsHDSSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_HDS_IsHDSUnsupported( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_HBHC_IsHdsBlockHeaderCacheSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_HBHC_IsHdsBlockHeaderCacheUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_LICGEN_IsLicGenSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_LICGEN_IsLicGenUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_M2TS_IsM2TSEncryptorSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_M2TS_IsM2TSEncryptorUnsupported( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_METERCERTREVOCATION_IsMeterCertRevocationSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_METERCERTREVOCATION_IsMeterCertRevocationUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_METERING_IsMeteringSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_METERING_IsMeteringUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_MODELREVOCATION_IsModelRevocationSupported( DRM_VOID ) ;
DRM_API DRM_BOOL DRM_CALL DRM_MODELREVOCATION_IsModelRevocationUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_MOVE_IsMoveSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_MOVE_IsMoveUnsupported( DRM_VOID );

#define DRM_NST_IsNSTSupported()   DRM_EST_IsESTSupported()
#define DRM_NST_IsNSTUnsupported() DRM_EST_IsESTUnsupported()

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PERSTREAMKEYS_IsPerStreamKeysSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PERSTREAMKEYS_IsPerStreamKeysUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PRND_IsPRNDSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PRND_IsPRNDUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PRND_IsPRNDTransmitterSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PRND_IsPRNDTransmitterUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PRND_IsPRNDReceiverSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_PRND_IsPRNDReceiverUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_REVOCATION_IsRevocationSupported( DRM_VOID );
#define DRM_REVOCATION_IsV1RIVSupported(x) DRM_WMDRMNET_IsWmdrmnetSupported(x)

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_CLK_IsSelwreClockSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_CLK_IsSelwreClockUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_CLK_IsSelwreClockExternSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_CLK_IsSelwreClockExternUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_SELWRESTOP_IsSelwreStopSupported( DRM_VOID );
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_SELWRESTOP_IsSelwreStopUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_THUMBNAIL_IsThumbnailSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_THUMBNAIL_IsThumbnailUnsupported( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_WMDRMNET_IsWmdrmnetSupported( __in DRM_SELWRECORE_CONTEXT *f_pSelwreCoreCtx );
DRM_API DRM_BOOL DRM_CALL DRM_WMDRMNET_IsWmdrmnetSupported_NoContext( DRM_VOID );

DRM_API DRM_BOOL DRM_CALL DRM_XMLHASH_IsXmlHashSupported( DRM_VOID );
DRM_API DRM_BOOL DRM_CALL DRM_XMLHASH_IsXmlHashUnsupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_H264_IsSupported( DRM_VOID );

DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL DRM_COCKTAIL_IsSupported( DRM_VOID );

EXIT_PK_NAMESPACE;

#endif  // DRMMODULESUPPORT_H

