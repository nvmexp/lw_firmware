/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef DRMCONTEXTSIZES_H
#define DRMCONTEXTSIZES_H

#include <drmtypes.h>
#include <oemcryptoctx.h>
#include <drmstkalloctypes.h>
#include <drmdatastoretypes.h>
#include <drmhds.h>
#include <drmselwrestore.h>
#include <drmliceval.h>
#include <drmutilitieslite.h>
#include <drmmetercertstore.h>
#include <drmblackboxtypes.h>
#include <drmlicstore.h>
#include <drmoutputleveltypes.h>
#include <drmchaintypes.h>
#include <drmplayreadyobj.h>
#include <drmdomainstoretypes.h>
#include <drmnoncestore.h>
#include <drmprndprotocoltypes.h>
#include <drmlicgentypes.h>
#include <drmselwrecoretypes.h>

ENTER_PK_NAMESPACE;

/* Recommended activation context size. */
#define ACTIVATION_CONTEXT_SIZE                  1024

/* The buffer should be big enough to store a single XMR license. */
#define MAX_NST_BUFFER_SIZE                      1024

typedef enum _DRM_CONTENT_SET_PROPERTY
{
    DRM_CSP_HEADER_NOT_SET          = 0,
    DRM_CSP_V1_HEADER               = 1,
    DRM_CSP_V2_HEADER               = 2,
    DRM_CSP_KID                     = 3,
    DRM_CSP_V2_4_HEADER             = 5,
    DRM_CSP_V4_HEADER               = 6,
    DRM_CSP_AUTODETECT_HEADER       = 7,
    DRM_CSP_PLAYREADY_OBJ           = 8,
    DRM_CSP_V4_1_HEADER             = 9,
    DRM_CSP_PLAYREADY_OBJ_WITH_KID  = 10,
    DRM_CSP_HEADER_COMPONENTS       = 11,
    DRM_CSP_V4_2_HEADER             = 12,
    DRM_CSP_DECRYPTION_OUTPUT_MODE  = 13,
} DRM_CONTENT_SET_PROPERTY;

typedef enum _DRM_CONTENT_GET_PROPERTY
{
    DRM_CGP_HEADER_KID              = 0,
    DRM_CGP_HEADER_TYPE             = 1,
    DRM_CGP_HEADER                  = 2,
    DRM_CGP_PLAYREADY_OBJ           = 3,
    DRM_CGP_CIPHER_TYPE             = 4,
    DRM_CGP_DECRYPTORSETUP          = 5,
    DRM_CGP_HEADER_KIDS             = 6,
    DRM_CGP_DECRYPTION_OUTPUT_MODE  = 7,
} DRM_CONTENT_GET_PROPERTY;

typedef struct __tagDRM_APP_CONTEXT_INTERNAL
{
    DRM_BOOL                    fLicStoreOpenXMR;
    DRM_BOOL                    fSecStoreGlobalContextOpen;
    DRM_CONTENT_SET_PROPERTY    eHeaderInContext;
    DRM_BOOL                    fCanBind;

    DRM_BOOL                    fResponseSignatureChecked;
    DRM_DWORD                   dwLastResponseProtocol;

    DRM_VOID                   *pOEMContext;

    DRM_KID                     KID;

    DRM_BINDING_INFO            oBindInfo[DRM_MAX_LICENSE_CHAIN_DEPTH];
    DRM_XMRFORMAT               licenseXMR[DRM_MAX_LICENSE_CHAIN_DEPTH];

    DRM_LICSTORE_CONTEXT        oLicStoreContextXMR;
    DRM_LICSTORE_CONTEXT        oLicStoreContextEmbedded;
    DRM_LICSTOREENUM_CONTEXT    oLicEnum[DRM_MAX_LICENSE_CHAIN_DEPTH];
    DRM_LICSTOREENUM_MULTIPLE_CONTEXT oLicEnumMultiple[DRM_MAX_LICENSE_CHAIN_DEPTH];

    DRM_LICEVAL_CONTEXT         oLicEvalContext;
    DRM_SECSTORE_CONTEXT        oSecStoreGlobalContext;

    DRM_DWORD                   dwChainDepth;
    DRM_SECSTORE_CONTEXT        rgcontextSST[DRM_MAX_LICENSE_CHAIN_DEPTH];

    DRM_SECSTORE_CONTEXT        contextSSTCertCache;

    DRM_REVLIST_ID_VALUES        idCRLsLwrrent;
    DRM_REVOCATIONSTORE_CONTEXT  oRevContext;

    DRM_BYTE                    *pbRevocationBuffer;
    DRM_DWORD                    cbRevocationBuffer;

    DRM_FFLICENSE                oFFLicense;
    DRM_METERCERT_CONTEXT        oMeterCertContext;
    DRM_DST                      oDatastoreHDS;
    DRM_HDS_CONTEXT              oContextHDS;
    DRM_DST                      oDatastoreEST;
    DRM_EST_CONTEXT              oContextEST;
    DRM_BOOL                     fStoreOpened;

    DRM_BYTE                    *pbOpaqueBuffer;
    DRM_DWORD                    cbOpaqueBuffer;
    DRM_BYTE                    *pbDRMHeaderData;  /* Should always point to the start of pbOpaqueBuffer */
    DRM_DWORD                    cbDRMHeaderData;  /* Should always be cbOpaqueBuffer - DRM_DWORD_ALIGNED_SIZE( DRM_MAX_LICENSESIZE ) */
    DRM_BYTE                    *pbDRMLicense;    /* Should always point to the last DRM_DWORD_ALIGNED_SIZE( DRM_MAX_LICENSESIZE ) bytes of pbOpaqueBuffer */
    DRM_DWORD                    cbDRMLicense;    /* Should always be DRM_DWORD_ALIGNED_SIZE( DRM_MAX_LICENSESIZE ) */

    DRM_WCHAR                   *pwszHeaderData;  /* Used with pbDRMHeaderData: Points into the PRO at the V4 header */
    DRM_DWORD                    cbHeaderData;
    DRM_BYTE                    *pbEmbeddedStore; /* Used with pbDRMHeaderData: Points into the PRO at the embedded store */
    DRM_DWORD                    cbEmbeddedStore;
    DRM_DWORD                    cbPROData;       /* Used with pbDRMHeaderData: Specifies the length of the entire PRO if it is set  */
    DRM_DWORD                    cbTotalHeaderData; /* Used with pbDRMHeaderData: Specifies the total length of the buffer filled with header data or PRO data */

    DRM_BOOL                     fClockSet;
    DRM_BOOL                     fBindThenCommit;
    DRM_DWORD                    dwSourceid;

    DRM_DWORD                    dwLicRevInfoVer;

    DRM_DOMAINSTORE_CONTEXT      oDomainStoreContext;
    DRM_LICSTORE_CONTEXT         oLicStoreContextNST;
    DRM_NONCESTORE_CONTEXT       oNonceStoreContext;

    DRM_BYTE                     rgbNSTBuffer[ MAX_NST_BUFFER_SIZE ];

    DRM_CHAR                     szClientVersion[DRM_VER_STRING_MAX_LEN];
    DRM_BYTE                    *pbActivationCtx;

    DRM_PRND_TRANSMITTER_CONTEXT    *poPrndTransmitterContext;
    DRM_PRND_RECEIVER_CONTEXT       *poPrndReceiverContext;
    DRM_BOOL                         fPRNDRxLSTOpened;
    DRM_LICSTORE_CONTEXT             oLSTContextPRNDRxStore;
    DRM_DST                          oDSTContextPRNDRxStore;
    DRM_EST_CONTEXT                  oESTContextPRNDRxStore;
    DRM_BYTE                         rgbPRNDRxStore[SIZE_OF_PLAYREADY_EMBEDDED_LICENSE_SPACE];

    DRM_DST                          oDSTContextMemSST;
    DRM_EST_CONTEXT                  oESTContextMemSST;
    DRM_BYTE                         rgbMemSSTStore[SIZE_OF_PLAYREADY_EMBEDDED_LICENSE_SPACE];

    DRM_LOCAL_LICENSE_CONTEXT       *poLocalLicenseContext;

    DRM_SELWRECORE_CONTEXT           oSelwreCoreCtx;
    DRM_DWORD                        dwDecryptionOutputMode;   /* Refer to the member with the same name in DRM_FFLICENSE */

} DRM_APP_CONTEXT_INTERNAL;

#define DRM_APP_CONTEXT_BUFFER_SIZE        sizeof( DRM_APP_CONTEXT_INTERNAL )

EXIT_PK_NAMESPACE;

#endif // DRMCONTEXTSIZES_H

