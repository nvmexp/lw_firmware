/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_OEMTIME_C
#include <stdlib.h>
#include <drmtypes.h>
#include <drmbytemanip.h>
#include <oemcommon.h>
#include <drmerr.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;
#ifdef NONE
/*
** Note: We do not worry about math overflow in this file.
** Reasons:
** 1. We don't care about math overflow in test-only code (DRM_TEST_USE_OFFSET_CLOCK).
** 2. All functions are not supported past 2038 AD when overflow could occur.
*/

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "Changing parameters to const to satisfy warning would make them not match the standard OEM interface." )

#if DRM_TEST_USE_OFFSET_CLOCK
/*
** This global will allow an offset to be added to the main system's clock,
** and is used by the test code to adjust the clock
** If g_fOEMTimeOffsetIsAbsolute is TRUE, then g_lOEMTimeOffset is an
** offset from zero instead of the current time
*/
DRM_EXPORT_VAR DRM_LONG g_lOEMTimeOffset = 0;
DRM_EXPORT_VAR DRM_BOOL g_fOEMTimeOffsetIsAbsolute = FALSE;
#endif /* DRM_TEST_USE_OFFSET_CLOCK */

#define NANOSECONDS_PER_SECOND   1000000000

/*
** colwert a 32-bit time_t to a 64-bit file time
** only valid for 1970-2038 AD
*/
static void _time_t_To_UI64(
    __in  time_t      time_tA,
    __out DRM_UINT64 *pui64filetime )
{
    /* colwert a time_t (seconds since 1970) to seconds since 1601. */
    DRM_UINT64 tics = DRM_UI64LITERAL( 0, 0 );

    tics = DRM_UI64Add( DRM_UI64( time_tA ), DRM_C_SECONDS_FROM_1601_TO_1970 );

    /* colwert seconds to 100-nanosecond tics. */
    *pui64filetime = DRM_UI64Mul( tics, DRM_UI64( DRM_C_TICS_PER_SECOND ) );
}
static DRM_VOID _struct_tm_To_SYSTEMTIME(
    __in  const struct tm *ptm,
    __out DRMSYSTEMTIME   *psystime )
{
    psystime->wYear         = (DRM_WORD)( (DRM_WORD)ptm->tm_year + 1900 );
    psystime->wMonth        = (DRM_WORD)( (DRM_WORD)ptm->tm_mon + 1 );
    psystime->wDay          = (DRM_WORD)ptm->tm_mday;
    psystime->wHour         = (DRM_WORD)ptm->tm_hour;
    psystime->wMinute       = (DRM_WORD)ptm->tm_min;
    psystime->wSecond       = (DRM_WORD)ptm->tm_sec;
    psystime->wDayOfWeek    = (DRM_WORD)ptm->tm_wday;
    psystime->wMilliseconds = 0;
}

static DRM_VOID _SYSTEMTIME_To_struct_tm(
    __in  const DRMSYSTEMTIME  *psystime,
    __out struct tm            *ptm )
{
    ptm->tm_year = psystime->wYear  - 1900; /* current minus 1900 */
    ptm->tm_mon  = psystime->wMonth - 1;    /* (0-11) */
    ptm->tm_mday = psystime->wDay;          /* (1-31) */
    ptm->tm_hour = psystime->wHour;         /* (0-23) */
    ptm->tm_min  = psystime->wMinute;       /* (0-59) */
    ptm->tm_sec  = psystime->wSecond;       /* (0-59) */
    ptm->tm_wday = psystime->wDayOfWeek;    /* (0-6)  */
}

static DRM_BOOL _SystemTimeToTime_t(
    __in  const DRMSYSTEMTIME *psystime,
    __out time_t              *ptimet )
{
    struct tm  tmA   = { 0 };
    struct tm  tmB   = { 0 };
    time_t     timeA = 0;
    time_t     timeB = 0;

    /* internal function, no need to test ptimet == NULL */

    if( ( psystime->wYear  <  1601 ) ||
        ( psystime->wMonth == 0 || psystime->wMonth > 12 ) ||
        ( psystime->wDay == 0 || psystime->wMonth > 31 ) ||
        ( psystime->wHour > 23 ) ||
        ( psystime->wMinute > 59 ) ||
        ( psystime->wSecond > 59 ) )
    {
        *ptimet = 0;
        return FALSE;
    }

    _SYSTEMTIME_To_struct_tm( psystime, &tmA );

    /*
    ** The mktime function needs some care.  The tm struct we give it is in the TZ we want but maketime feels the need
    ** to adjust for the current machine time zone.  To work around this we have to figure out through more calls to
    ** gmtime and mktime what that TZ offset is so we can later remove it from the original result of mktime
    */
    timeA = mktime( &tmA );
    MEMCPY( &tmB, gmtime( &timeA ), sizeof( tmB ) );

    timeB = mktime( &tmB );

    if( timeB > timeA )
    {
        timeA -= ( timeB - timeA );
    }
    else
    {
        timeA += ( timeA - timeB );
    }

    *ptimet = timeA;
    return TRUE;
}

DRM_API DRM_BOOL DRM_CALL Oem_Clock_SystemTimeToFileTime(
    __in  const DRMSYSTEMTIME *psystime,
    __out DRMFILETIME         *pfiletime )
{
    DRM_UINT64 ui64  = DRM_UI64LITERAL( 0, 0 );
    time_t     timeA = 0;

    if( !_SystemTimeToTime_t( psystime, &timeA ) )
    {
        pfiletime->dwLowDateTime  = 0;
        pfiletime->dwHighDateTime = 0;
        return FALSE;
    }

    _time_t_To_UI64( timeA, &ui64 );

    ui64 = DRM_UI64Add( ui64, DRM_UI64( (DRM_DWORD)( psystime->wMilliseconds * 10000 ) ) );

    UI64_TO_FILETIME( ui64, *pfiletime );

    return TRUE;
}

DRM_API DRM_BOOL DRM_CALL Oem_Clock_FileTimeToSystemTime(
    __in  const DRMFILETIME *pfiletime,
    __out DRMSYSTEMTIME     *psystime )
{
    DRM_UINT64 tics          = DRM_UI64LITERAL( 0, 0 );
    DRM_UINT64 ui64          = DRM_UI64LITERAL( 0, 0 );
    const struct tm *ptm     = NULL;
    time_t     timeA         = 0;
    DRM_BOOL   fOK           = FALSE;
    DRM_DWORD  cMilliseconds = 0;

    if( psystime != NULL )
    {
        ZEROMEM( psystime, sizeof( *psystime ) );
    }

    if( pfiletime == NULL || psystime == NULL )
    {
        return FALSE;
    }

    FILETIME_TO_UI64( *pfiletime, ui64 );
    FILETIME_TO_UI64( *pfiletime, tics );

    cMilliseconds  = DRM_I64ToUI32( DRM_UI2I64( DRM_UI64Mod( ui64, DRM_UI64( DRM_C_TICS_PER_SECOND ) ) ) );
    cMilliseconds /= 10000;

    /* colwert to seconds */

    tics = DRM_UI64Div( tics, DRM_UI64( DRM_C_TICS_PER_SECOND ) );

    /* change base to ANSI time's */

    tics = DRM_UI64Sub( tics, DRM_C_SECONDS_FROM_1601_TO_1970 );

    timeA = (time_t)DRM_I64ToUI32( DRM_UI2I64( tics ) );

    if( ( ptm = gmtime( &timeA ) ) != NULL )
    {
        _struct_tm_To_SYSTEMTIME( ptm, psystime );
        psystime->wMilliseconds = (DRM_WORD)cMilliseconds;
        fOK = TRUE;
    }

    return fOK;
}

static DRM_FRE_INLINE DRM_VOID DRM_CALL _Clock_GetSystemTimeAsUINT64(
    __out       DRM_UINT64  *pui64time )
{
    time_t     timeNow  = 0;
    DRM_UINT64 ui64time = DRM_UI64LITERAL( 0, 0 );

#if DRM_TEST_USE_OFFSET_CLOCK
    if( !g_fOEMTimeOffsetIsAbsolute )
#endif /* DRM_TEST_USE_OFFSET_CLOCK */
    {
        time( &timeNow );
        _time_t_To_UI64( timeNow, &ui64time );
    }

#if DRM_TEST_USE_OFFSET_CLOCK
    if( g_lOEMTimeOffset >= 0 )
    {
        ui64time = DRM_UI64Add( ui64time, DRM_UI64Mul( DRM_UI64( DRM_C_TICS_PER_SECOND ), DRM_UI64( (DRM_DWORD)g_lOEMTimeOffset ) ) );
    }
    else
    {
        ui64time = DRM_UI64Sub( ui64time, DRM_UI64Mul( DRM_UI64( DRM_C_TICS_PER_SECOND ), DRM_UI64( (DRM_DWORD)-g_lOEMTimeOffset ) ) );
    }
#endif /* DRM_TEST_USE_OFFSET_CLOCK */

    *pui64time = ui64time;
}

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Clock_GetSystemTimeAsFileTime(
    __in_opt    DRM_VOID    *pOEMContext,
    __out       DRMFILETIME *pfiletime )
{
    DRM_RESULT dr       = DRM_SUCCESS;
    DRM_UINT64 ui64time = DRM_UI64LITERAL( 0, 0 );

    UNREFERENCED_PARAMETER( pOEMContext );

    ChkArg( pfiletime != NULL );

    pfiletime->dwLowDateTime  = 0;
    pfiletime->dwHighDateTime = 0;

    ChkVOID( _Clock_GetSystemTimeAsUINT64( &ui64time ) );

    if( g_fSelwreClockOffsetNegative )
    {
        ChkDR( DRM_UInt64Sub( ui64time, g_ui64SelwreClockOffsetValue, &ui64time ) );
    }
    else
    {
        ChkDR( DRM_UInt64Add( ui64time, g_ui64SelwreClockOffsetValue, &ui64time ) );
    }

    ChkVOID( UI64_TO_FILETIME( ui64time, *pfiletime ) );

ErrorExit:
    return;
}

DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Clock_GetSystemTime(
    __in_opt    DRM_VOID      *pOEMContext,
    __out       DRMSYSTEMTIME *psystime )
{
    DRMFILETIME filetime = { 0 };

    ChkVOID( Oem_Clock_GetSystemTimeAsFileTime( pOEMContext, &filetime ) );
    (DRM_VOID)Oem_Clock_FileTimeToSystemTime( &filetime, psystime );
}

DRM_API_VOID DRM_VOID DRM_CALL Oem_Clock_SetSystemTime(
    __in_opt       DRM_VOID      *f_pOEMContext,
    __in     const DRMSYSTEMTIME *f_lpSystemTime )
{
#if !defined( __arm ) && !defined(WINNT) && !defined(__MACINTOSH__)
#if DRM_TEST_USE_OFFSET_CLOCK
    DRMFILETIME ftLwrrent   = { 0 };
    DRMFILETIME ftNew       = { 0 };
    DRM_UINT64  ui64Lwrrent = DRM_UI64LITERAL( 0, 0 );
    DRM_UINT64  ui64New     = DRM_UI64LITERAL( 0, 0 );

    Oem_Clock_GetSystemTimeAsFileTime( f_pOEMContext, &ftLwrrent );

    if( DRM_SUCCEEDED( Oem_Clock_SystemTimeToFileTime( f_lpSystemTime, &ftNew ) ) )
    {
        ChkVOID( FILETIME_TO_UI64( ftLwrrent, ui64Lwrrent ) );
        ChkVOID( FILETIME_TO_UI64( ftNew, ui64New ) );

        DRMASSERT( !g_fOEMTimeOffsetIsAbsolute ); /* Not supported */
        g_lOEMTimeOffset += DRM_I64ToUI32( DRM_I64Div( DRM_I64Sub( DRM_UI2I64( ui64New ), DRM_UI2I64( ui64Lwrrent ) ), DRM_I64( DRM_C_TICS_PER_SECOND ) ) );
    }
#else /* DRM_TEST_USE_OFFSET_CLOCK */
    time_t     timeA      = 0;

    /* colwert it to time_t structure  */
    if( f_lpSystemTime != NULL
     && _SystemTimeToTime_t( f_lpSystemTime, &timeA ) )
    {
        /* colwert gmt time from time_t to local time in struct tm */
        localtime( &timeA );

        /*OEMNOTE: this function is not in ANSI C.  Setting the system time is now entirely OS-dependent and there is no
        **         standard ANSI C way to do it.  Porting kit users should replace this with a non-portable API call
        **         _setsystem() expects a local time
        */

        stime( &timeA );
    }
#endif /* DRM_TEST_USE_OFFSET_CLOCK */
#endif
}

DRM_API_VOID DRM_VOID DRM_CALL Oem_Clock_SetSelwreClockOffset(
    __in_opt       DRM_VOID   *f_pOEMContext,
    __in     const DRM_UINT64 *f_pui64SelwreTime )
{
    DRM_RESULT  dr             = DRM_SUCCESS;
    DRM_UINT64  ui64SystemTime = DRM_UI64LITERAL( 0, 0 );

    UNREFERENCED_PARAMETER( f_pOEMContext );

    ChkArg( f_pui64SelwreTime != NULL );

    ChkVOID( _Clock_GetSystemTimeAsUINT64( &ui64SystemTime ) );

    if( DRM_UI64Les( *f_pui64SelwreTime, ui64SystemTime ) )
    {
        ChkDR( DRM_UInt64Sub( ui64SystemTime, *f_pui64SelwreTime, &g_ui64SelwreClockOffsetValue ) );
        g_fSelwreClockOffsetNegative = TRUE;
    }
    else
    {
        ChkDR( DRM_UInt64Sub( *f_pui64SelwreTime, ui64SystemTime, &g_ui64SelwreClockOffsetValue ) );
        g_fSelwreClockOffsetNegative = FALSE;
    }

ErrorExit:
    return;
}

#define NANOSECONDS_PER_MILLISECOND      1000000
DRM_API DRM_DWORD DRM_CALL Oem_Clock_GetTickCount(
    __in_opt    DRM_VOID      *f_pOEMContext )
{
    /*
    ** The clock() function must NOT be used.
    ** Oem_Clock_GetTickCount must include time the thread is not active,
    ** e.g. while sleeping or another process is active, but clock()
    ** only tracks CPU time for this process.
    */
    DRM_DWORD  dwTickCount = 0;
    DRM_UINT64 ui64QPC     = Oem_Clock_QueryPerformanceCounter( f_pOEMContext );

    ui64QPC = DRM_UI64Div( ui64QPC, DRM_UI64( NANOSECONDS_PER_MILLISECOND ) );
    dwTickCount = DRM_UI64Low32( ui64QPC );

#if DRM_TEST_USE_OFFSET_CLOCK
    if( f_pOEMContext != NULL )
    {
        const DRM_TEST_OEM_CONTEXT *pTestOemContext = (const DRM_TEST_OEM_CONTEXT *)f_pOEMContext;

        if( ( pTestOemContext->fFreezeTickCount ) && ( pTestOemContext->dwTickCount > 0 ) )
        {
            dwTickCount = pTestOemContext->dwTickCount;
        }
    }
#endif /* DRM_TEST_USE_OFFSET_CLOCK */

    return dwTickCount;
}

DRM_API DRM_UINT64 DRM_CALL Oem_Clock_QueryPerformanceCounter(
    __in_opt    DRM_VOID      *f_pOEMContext )
{
    DRM_UINT64 ui64Counter = DRM_UI64LITERAL( 0, 0 );
    struct timespec ts;

    UNREFERENCED_PARAMETER( f_pOEMContext );
    if( clock_gettime( CLOCK_MONOTONIC, &ts ) == 0 )
    {
        DRM_UINT64 ui64nsec = DRM_UI64LITERAL( 0, 0 );
        DRM_UINT64 ui64sec  = DRM_UI64LITERAL( 0, 0 );

        if( sizeof( ts.tv_nsec ) == sizeof( DRM_DWORD ) )
        {
            ui64nsec = DRM_UI64HL( 0, (DRM_DWORD)ts.tv_nsec );
        }
        else
        {
            ui64nsec = (DRM_UINT64)ts.tv_nsec;
        }

        if( sizeof( ts.tv_sec ) == sizeof( DRM_DWORD ) )
        {
            ui64sec  = DRM_UI64HL( 0, (DRM_DWORD)ts.tv_sec );
        }
        else
        {
            ui64sec = (DRM_UINT64)ts.tv_sec;
        }

        ui64Counter = DRM_UI64Add( DRM_UI64Mul( ui64sec, DRM_UI64( NANOSECONDS_PER_SECOND ) ), ui64nsec );
    }

    return ui64Counter;
}

DRM_API DRM_UINT64 DRM_CALL Oem_Clock_QueryPerformanceFrequency(
    __in_opt    DRM_VOID      *f_pOEMContext )
{
    UNREFERENCED_PARAMETER( f_pOEMContext );
    return DRM_UI64( NANOSECONDS_PER_SECOND );
}

PREFAST_POP /* Changing parameters to const to satisfy warning would make them not match the standard OEM interface. */
#endif /* NONE */
EXIT_PK_NAMESPACE_CODE;
