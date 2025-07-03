/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __DRM_CALLBACKTYPES_H__
#define __DRM_CALLBACKTYPES_H__

#include <drmerr.h>
#include <drmoutputleveltypes.h>
#include <drmxmrformattypes.h>

ENTER_PK_NAMESPACE;

typedef enum __tag_DRM_POLICY_CALLBACK_TYPE
{
    DRM_PLAY_OPL_CALLBACK                       = 0x1,  /* DRM_PLAY_OPL_3_0                                 */
    DRM_POLICY_CALLBACK_RESERVED_2              = 0x2,  /* Never called, enum value maintained for compat   */
    DRM_POLICY_CALLBACK_RESERVED_3              = 0x3,  /* Never called, enum value maintained for compat   */
    DRM_EXTENDED_RESTRICTION_CONDITION_CALLBACK = 0x4,  /* DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT         */
    DRM_EXTENDED_RESTRICTION_ACTION_CALLBACK    = 0x5,  /* DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT         */
    DRM_EXTENDED_RESTRICTION_QUERY_CALLBACK     = 0x6,  /* DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT         */
    DRM_SELWRE_STATE_TOKEN_RESOLVE_CALLBACK     = 0x7,  /* DRM_SELWRE_STATE_TOKEN_RESOLVE_DATA              */
    DRM_RESTRICTED_SOURCEID_CALLBACK            = 0x8,  /* DRM_RESTRICTED_SOURCEID_CALLBACK_STRUCT          */
    DRM_OEM_KEY_INFO_CALLBACK                   = 0x9,  /* DRM_OEM_KEY_INFO_CALLBACK_STRUCT                 */
    DRM_ENABLE_LICENSE_REJECTION_CALLBACK       = 0xA,  /* DRM_ENABLE_LICENSE_REJECTION_CALLBACK_STRUCT     */
} DRM_POLICY_CALLBACK_TYPE;

typedef DRM_RESULT (DRM_CALL* DRMPFNPOLICYCALLBACK)(
    __in     const DRM_VOID                 *f_pvCallbackData,
    __in           DRM_POLICY_CALLBACK_TYPE  f_dwCallbackType,
    __in_opt const DRM_KID                  *f_pKID,            /* KID that is being enumerated, i.e. the KID of the leaf-most license in a chain.  Will be NULL for callbacks not dealing with a license. */
    __in_opt const DRM_LID                  *f_pLID,            /* LID of the actual license being called upon, i.e. may be leaf or root license in a chain.  Will be NULL for callbacks not dealing with a license. */
    __in_opt const DRM_VOID                 *f_pv );            /* Void pointer to opaque data passed in alongside the DRMPFNPOLICYCALLBACK parameter which is then passed to the callback, e.g. in Drm_Reader_Bind */

typedef struct
{
    DRM_WORD                             wRightID;
    DRM_XB_UNKNOWN_OBJECT               *pRestriction;
    DRM_XMRFORMAT                       *pXMRLicense;
    DRM_VOID                            *pContextSST;   /* DRM_SECSTORE_CONTEXT */
} DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT;

typedef struct
{
    DRM_DWORD dwSourceID; /* global requirement, no need to pass right ID */
} DRM_RESTRICTED_SOURCEID_CALLBACK_STRUCT;

typedef struct
{
          DRM_DWORD  cb;
    const DRM_BYTE  *pb;
} DRM_OEM_KEY_INFO_CALLBACK_STRUCT;

/*
** Each time a candidate simple license or license chain is located during Drm_Reader_Bind,
** the policy callback is called for that candidate with f_dwCallbackType equal to
** DRM_ENABLE_LICENSE_REJECTION_CALLBACK and f_pvCallbackData set to the following structure.
**
** The idSession member will match the m_idSession member of the DRM_LICENSE_RESPONSE structure.
** Cannot-persist licenses will always populate this value.
** Persistent licenses stored via DRM_CDMI_Update function will always populate this value.
** Persistent licenses stored via Drm_LicenseAcq_ProcessResponse will only populate this value
** if the source code from path source\modules\hdssession\real is linked into the PK;
** persistent session information is otherwise not stored on disk and is unavailable during Bind.
** When dealing with a license chain, idSession always represents the leaf license's session.
**
** Because multiple licenses may be passed into this function (in the case of license chains),
** both the f_pKID and f_pLID params will be NULL, but you can obtain the KID(s) using the
** following sample code.  The if block in this sample code will be entered iff the candidate
** is a license chain (in which case oKID will be set to the leaf KID).
**
** BEGIN_SAMPLE_CODE
**
** DRM_ENABLE_LICENSE_REJECTION_CALLBACK_STRUCT *pIn = ( (DRM_ENABLE_LICENSE_REJECTION_CALLBACK_STRUCT*)f_pvCallbackData );
** DRM_KID oKID = DRM_ID_EMPTY;
** DRMCRT_memcpy( oKID.rgb, pIn->rgXMRLicenses[ 0 ].OuterContainer.KeyMaterialContainer.ContentKey.guidKeyID.rgb, sizeof( oKID.rgb ) );
** 
** if( pIn->cXMRLicenses > 1 )
** {
**     DRM_KID oKIDRoot = DRM_ID_EMPTY;
**     DRMCRT_memcpy( oKIDRoot.rgb, pIn->rgXMRLicenses[ 1 ].OuterContainer.KeyMaterialContainer.ContentKey.guidKeyID.rgb, sizeof( oKIDRoot.rgb ) );
** }
**
** END_SAMPLE_CODE
**
** If you return an error from a call to this callback, the candidate will be rejected
** and the search for a viable candidate will continue as if that candidate was unusable
** for any other reason.
**
** You have total control over the conditions under which you reject a candidate.
**
** For example, if you have a license with an expiration date in the near future
** (especially if it has real-time expiration set), but you also know that you've acquired
** a license with a longer expiration (or no expiration at all), you may wish to reject
** the shorter duration license if you see it via this callback by returning an error.
**
** This callback is called *before* setting up a decryptor in order to avoid that
** (potentially expensive) operation for any license(s) which you are rejecting.
**
** Once you return success from this function for a given candidate, you will
** not receive callbacks for any remaining candidates unless there is an error setting
** up the decryptor (in which case the search for a viable candidate will continue).
**
** Since the sequence of licenses found during license enumeration is non-deterministic,
** you are only guaranteed to get a callback for every available candidate if you return
** an error every time this function is called on a given Drm_Reader_Bind call.
**
** If there are multiple leaf and/or root licenses for any given KID, it is possible
** that any given license will be passed via this callback multiple times.
** This is because the callback is called for the entire candidate as a unit rather
** than being called for each license individually so that you can evaluate the
** unit as a whole rather than having to keep track of the leaf/root separately.
*/
typedef struct
{
          DRM_DWORD      cXMRLicenses;
    const DRM_XMRFORMAT *rgXMRLicenses;
          DRM_ID         idSession;
} DRM_ENABLE_LICENSE_REJECTION_CALLBACK_STRUCT;

EXIT_PK_NAMESPACE;

#endif /* __DRMCALLBACKTYPES_H__ */

