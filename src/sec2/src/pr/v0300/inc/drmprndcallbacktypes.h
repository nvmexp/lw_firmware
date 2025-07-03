/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMPRNDCALLBACKTYPES_H
#define DRMPRNDCALLBACKTYPES_H

#include <drmpragmas.h>
#include <drmtypes.h>
#include <drmbcertconstants.h>

ENTER_PK_NAMESPACE;

typedef enum
{
    DRM_PRND_CONTENT_IDENTIFIER_TYPE_KID                            = 0x1,
    DRM_PRND_CONTENT_IDENTIFIER_TYPE_LWSTOM                         = 0x2,
} DRM_PRND_CONTENT_IDENTIFIER_TYPE;

#define DRM_PRND_CERTIFICATE_DATA_DIGEST_SIZE                       32
#define DRM_PRND_CERTIFICATE_DATA_CLIENT_ID_SIZE                    16
#define DRM_PRND_CERTIFICATE_DATA_MAX_MANUFACTURER_STRING_LENGTH    129 /* Null-terminated */
typedef struct __tagDRM_PRND_CERTIFICATE_DATA
{
    DRM_DWORD                       dwType;                 /* Should be: DRM_BCERT_CERTTYPE_PC (0x1) or DRM_BCERT_CERTTYPE_DEVICE (0x2).  Refer to drmbcertconstants.h. */
    DRM_DWORD                       dwPlatformIdentifier;   /* One of many DRM_BCERT_SELWRITY_VERSION_PLATFORM_* values.  Refer to drmbcertconstants.h. */
    DRM_DWORD                       dwSelwrityLevel;        /* Typically will be one of the DRM_BCERT_SELWRITYLEVEL_* constants, e.g. DRM_BCERT_SELWRITYLEVEL_150.  Refer to drmbcertconstants.h. */
    DRM_DWORD                       dwSelwrityVersion;
    DRM_DWORD                       dwExpirationDate;

    DRM_DWORD                       cSupportedFeatures;
    DRM_DWORD                       rgdwSupportedFeatureSet[ DRM_BCERT_MAX_FEATURES ];

    DRM_BYTE                        rgbClientID[ DRM_PRND_CERTIFICATE_DATA_CLIENT_ID_SIZE ];

    DRM_BYTE                        rgbModelDigest[ DRM_PRND_CERTIFICATE_DATA_DIGEST_SIZE ];
    DRM_CHAR                        szModelManufacturerName[ DRM_PRND_CERTIFICATE_DATA_MAX_MANUFACTURER_STRING_LENGTH ];
    DRM_CHAR                        szModelName[ DRM_PRND_CERTIFICATE_DATA_MAX_MANUFACTURER_STRING_LENGTH ];
    DRM_CHAR                        szModelNumber[ DRM_PRND_CERTIFICATE_DATA_MAX_MANUFACTURER_STRING_LENGTH ];
} DRM_PRND_CERTIFICATE_DATA;

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: Callback contexts should never be const")
typedef DRM_RESULT ( DRM_CALL *Drm_Prnd_Data_Callback )(
    __inout_ecount_opt( 1 )                        DRM_VOID                                    *f_pvDataCallbackContext,
    __in_opt                                       DRM_PRND_CERTIFICATE_DATA                   *f_pCertificateData,
    __in_opt                                 const DRM_ID                                      *f_pLwstomDataTypeID,
    __in_bcount_opt( f_cbLwstomData )        const DRM_BYTE                                    *f_pbLwstomData,
    __in                                           DRM_DWORD                                    f_cbLwstomData );
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: Callback contexts should never be const")
typedef DRM_RESULT ( DRM_CALL *Drm_Prnd_Content_Identifier_Callback )(
    __inout_ecount_opt( 1 )                        DRM_VOID                                    *f_pvContentIdentifierCallbackContext,
    __in                                           DRM_PRND_CONTENT_IDENTIFIER_TYPE             f_eContentIdentifierType,
    __in_bcount_opt( f_cbContentIdentifier ) const DRM_BYTE                                    *f_pbContentIdentifier,
    __in                                           DRM_DWORD                                    f_cbContentIdentifier,
    __in_ecount_opt( 1 )                           DRM_KID                                     *f_pkidContent );
PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */

EXIT_PK_NAMESPACE;

#endif // DRMPRNDCALLBACKTYPES_H

