/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#include <oemtee.h>
#include <oemteeinternal.h>
#include <oemcommon.h>
#include <drmmathsafe.h>
#include <drmlastinclude.h>

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Prefast Noise: OEM_TEE_CONTEXT* should not be const.");

ENTER_PK_NAMESPACE_CODE;

//
// LWE (kwilson) Our port does not set the secure clock property DRM_TEE_PROPERTY_SUPPORTS_SELWRE_CLOCK 
// since we have no HW support for a secure clock. The DRM_TEE_PROPERTY_SUPPORTS_SELWRE_CLOCK property
// is only used by the normal world side to indicate which features are supported.  However, since the
// clock functions are common they are used inside the TEE code without checking this property.  In all
// cases, we should handle the DRM_E_NOTIMPL and FALSE secure clock return values without producing errors. 
//

#if defined(SEC_COMPILE)
/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port if your PlayReady port supports a
** clock inside the TEE which is not user settable and is guaranteed to be
** accurate whenever this API is called.  Under those conditions, this
** function MUST return the current date/time based on that clock.
**
** If the device does not support such a clock OR you do not wish to use that
** clock, this function MUST return DRM_E_NOTIMPL.
**
** If the TEE supports such a clock, this function MUST return
** DRM_E_TEE_CLOCK_NOT_SET if the clock has not yet been initialized
** by OEM_TEE_CLOCK_SetSelwreSystemTimeAsFileTime.
**
** This function MUST check that the secure clock does not need to be resynced.
** If resync is needed, this function MUST return DRM_E_TEE_CLOCK_DRIFTED.
**
** This function is used when the TEE attempts to verify the current
** date/time against a selwred clock.  The TEE does not require a
** selwred clock to function but provides better robustness
** guarantees if a selwred clock is provided to it.
**
** Synopsis:
**
** This function returns the current date/time from a clock inside the TEE
** which is not user settable and MUST be accurate when this API returns.
** This function MUST return DRM_E_NOTIMPL if such a clock is unavailable.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pftSystemTime:      (out)    The accurate current date/time.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CLOCK_GetSelwrelySetSystemTime(
    __inout                                              OEM_TEE_CONTEXT            *f_pContext,
    __out                                                DRMFILETIME                *f_pftSystemTime)
{
    UNREFERENCED_PARAMETER(f_pContext);
    UNREFERENCED_PARAMETER(f_pftSystemTime);

    //
    // LWE (kwilson) No support for clock. See comment at top of file.
    //
    return DRM_E_NOTIMPL;
}

/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port if your PlayReady port's TEE supports
** Remote Provisioning, Secure Time, and/or the H264 Slice Header Parsing Library
** (under /source/trustedexec/h264).
**
** This function is used by the TEE to determine the amount of time
** an operation takes.  This function is called twice by the caller for
** operations that require it - once at the start of the operation and once
** at the end of the operation.  The difference between the values returned
** is then used to determine how long the operation takes.
**
** This function requires access to some kind of timer inside the TEE.
** The timer does not necessarily need to return the current time,
** but it MUST be reasonably accurate accross calls and the user must NOT
** be able to manipulate it.  If such a timer is available, this function
** MUST return its value.  Otherwise, this function MUST return DRM_E_NOTIMPL.
**
** This function is required by the aforementioned features.
** If any of those features are supported by the TEE,
** the aforementioned timer MUST be available inside the TEE.
**
** If none of the aforementioned features are supported by the TEE,
** this function may return DRM_E_NOTIMPL.
**
** Synopsis:
**
** This function returns the a date/time which may be imprecise
** but cannot be user manipulated and MUST be relatively accurate
** across calls.
**
** Arguments:
**
** f_pContextAllowNULL: (in/out) The TEE context returned from
**                               OEM_TEE_BASE_AllocTEEContext.
**                               This function may receive NULL.
**                               This function may receive an
**                               OEM_TEE_CONTEXT where
**                               cbUnprotectedOEMData == 0 and
**                               pbUnprotectedOEMData == NULL.
** f_pftTimestamp:      (out)    The value of the timer as a date/time.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CLOCK_GetTimestamp(
    __inout_opt                                     OEM_TEE_CONTEXT            *f_pContextAllowNULL,
    __out                                           DRMFILETIME                *f_pftTimestamp)
{
    UNREFERENCED_PARAMETER(f_pContextAllowNULL);
    UNREFERENCED_PARAMETER(f_pftSystemTime);

    //
    // LWE (kwilson) No support for clock. See comment at top of file.
    //
    return DRM_E_NOTIMPL;
}
#endif

#ifdef NONE
/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port if your PlayReady TEE supports
** Remote Provisioning and/or Secure Time.
**
** This function requires the ability to set the system time of a clock inside
** the TEE that cannot be manipulated outside of the TEE. If such a clock is
** available, this function MUST set the current time to the provided system
** time. Otherwise, this function MUST return DRM_E_NOTIMPL.

** This function MUST set the clock in a way that the time stays relatively
** accurate accross power cycles.  This value will be later retrieved by
** calling the OEM_TEE_CLOCK_GetSelwrelySetSystemTime function.
**
** If your PlayReady TEE supports neither Remote Provisioning nor Secure Time,
** this function may return DRM_E_NOTIMPL.
**
** Synopsis:
**
** This function sets the current date/time as it is delivered from a remote
** provisioning response or Secure Time response.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
** f_pftSystemTime:      (in)     The accurate current date/time.
**
** Returns:
**
** On failure, this function should return an explicitly-defined error from
** drmresults.h OR an OEM-defined error code in the range 0x8004dd00 to 0x8004ddff.
**
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL OEM_TEE_CLOCK_SetSelwreSystemTime(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext,
    __in                                      const DRMFILETIME                *f_pftSystemTime )
{
    DRM_RESULT      dr          = DRM_SUCCESS;
    DRMSYSTEMTIME   oSystemTime = { 0 };

    ASSERT_TEE_CONTEXT_REQUIRED( f_pContext );

    ChkArg( f_pftSystemTime != NULL );
    ChkDR( Oem_Clock_FileTimeToSystemTime( f_pftSystemTime, &oSystemTime ) );
    ChkVOID( Oem_Clock_SetSystemTime( NULL, &oSystemTime ) );

    /* OEM MUST write oSystemTime to Persistent Storage */

ErrorExit:
    return dr;
}
#endif

#if defined(SEC_COMPILE)
/*
** OEM_MANDATORY_CONDITIONALLY:
** You MUST replace this function implementation with your own implementation
** that is specific to your PlayReady port if your PlayReady port supports a
** clock inside the TEE which is not user settable and is guaranteed to be
** accurate whenever this API is called.  Under those conditions, this
** function MUST return TRUE if the clock has been running long enough
** that a resync is required to ensure that it has not drifted and FALSE otherwise.
**
** This API should be used for your implementations where secure clock
** is subject to aclwmulating error overtime or subject to going completely
** out of sync due to external factors.
** This API is called during TEE initialization to ensure the sanity of the
** secure clock. This API also addresses aclwmulated loss of percision
** by forcing periodic resync defined by the MAX_ALLOWED_TIME_BEFORE_CLOCK_RESYNC.
**
** If the device supports a clock inside the TEE which cannot be user
** manipulated this function MUST:
** 1. If secure clock was lost, return TRUE.
** 2. Retrieve the secure clock value that was stored in the persistent storage
**    during the last call to OEM_TEE_CLOCK_SetSelwreSystemTimeAsFileTime.
** 3. If MAX_ALLOWED_TIME_BEFORE_CLOCK_RESYNC has passed since the last time
**    OEM_TEE_CLOCK_SetSelwreSystemTimeAsFileTime was called then trigger
**    a resync by returning TRUE.
** 4. Otherwise, return FALSE.
**
** If your PlayReady TEE supports neither Secure Time nor Remote Provisioning,
** this function may return FALSE.
**
** Synopsis:
**
** Returns TRUE if the clock needs to be ReSynced and FALSE otherwise.
**
** Arguments:
**
** f_pContext:           (in/out) The TEE context returned from
**                                OEM_TEE_BASE_AllocTEEContext.
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL OEM_TEE_CLOCK_SelwreClockNeedsReSync(
    __inout                                         OEM_TEE_CONTEXT            *f_pContext)
{

    ASSERT_TEE_CONTEXT_REQUIRED(f_pContext);
    UNREFERENCED_PARAMETER(f_pContext);

    //
    // LWE (kwilson) No support for clock. See comment at top of file.
    //
    return FALSE;
}
#endif

EXIT_PK_NAMESPACE_CODE;

PREFAST_POP; /* __WARNING_NONCONST_PARAM_25004 */
