/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __OEMCOMMON_H__
#define __OEMCOMMON_H__

#include <drmcompiler.h>
#include <oemsha1.h>
#include <oemsha256.h>
#include <oemcommonmem.h>
#include <drmbase64.h>
#include <drmbytemanip.h>

#if defined(__cplusplus) && USE_CLAW

#include <clawrt/rt.h>

#else /* defined(__cplusplus) && USE_CLAW */

#define CLAW_AUTO_RANDOM_CIPHER
#define CLAW_ATTRIBUTE_NOINLINE

#endif /* defined(__cplusplus) && USE_CLAW */

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_NONCONST_PARAM_25004, "OEM Context should never be const." )

ENTER_PK_NAMESPACE;

extern DRM_EXPORT_VAR DRM_CONST_STRING g_dstrDrmPath;
extern const DRM_EXPORT_VAR DRM_CHAR   g_chPathSeparator;
extern const DRM_EXPORT_VAR DRM_WCHAR  g_wchPathSeparator;

extern const DRM_CONST_STRING g_dstrPrivKey;

/* Session Expiration Timeout Config Values */
#define Oem_Config_SETimeout_Key     "SessionExpirationTimeout"
/*
** sizeof(static string) includes null terminating byte
** which we don't want since we are going to append it with session id
*/
#define Oem_Config_SETimeout_Key_LEN ( sizeof( Oem_Config_SETimeout_Key ) - 1 )

/* Skew the clock but a fixed offset */
#define Oem_Config_Timeoffset_Key     "ClockTimeOffset"

/*
** sizeof(static string) includes null terminating byte
** which we don't want since we are going to append it with session id
*/
#define Oem_Config_Timeoffset_Key_LEN ( sizeof( Oem_Config_Timeoffset_Key ) - 1 )

/* Revocation List Timeout Config Values */
#define Oem_Config_RLTimeout_Key     "RevocationListTimeout"
/*
** sizeof(static string) includes null terminating byte
** which we don't want since we are going to append it with session id
*/
#define Oem_Config_RLTimeout_Key_LEN ( sizeof( Oem_Config_RLTimeout_Key ) - 1 )

#define Oem_Config_NoTxAuth_Key      "NoTxAuth"
/*
** sizeof(static string) includes null terminating byte
** which we don't want since we are going to append it with session id
*/
#define Oem_Config_NoTxAuth_Key_LEN ( sizeof( Oem_Config_NoTxAuth_Key ) - 1 )

/* Proximity Detection RTT Threshold */
#define Oem_Config_ProximityRTTThreshold_Key "ProximityRTTThreshold"
/*
 ** sizeof(static string) includes null terminating byte
 ** which we don't want since we are going to append it with the DWORD value
 */
#define Oem_Config_ProximityRTTThreshold_Key_LEN ( sizeof( Oem_Config_ProximityRTTThreshold_Key ) - 1 )

typedef enum DRMFILESPATH
{
    CERTPATH = 0,
    CERTTEMPLATEPATH,
    HDSPATH,
    KEYFILEPATH
} DRMFILESPATH;

typedef enum _DRM_DEVICE_CERT_TYPE
{
    DRM_DCT_PLAYREADY_TEMPLATE = 1,
    DRM_DCT_PLAYREADY_DEVICE,
    DRM_DCT_PLAYREADY_LPROV,
} DRM_DEVICE_CERT_TYPE;

/*
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
*/
DRM_API DRM_RESULT DRM_CALL Oem_GetDRMFullPathName(
    __deref_out_z DRM_WCHAR **f_ppwszFullPath,
    __in_opt const DRM_CONST_STRING *f_pdstrFilename,
    __in DRMFILESPATH f_eFilesPath );

/*
** OEM_MANDATORY:
** If the device supports Device Assets (DRM_SUPPORT_DEVICEASSETS=1), this function MUST be implemented by the OEM.
*/
DRM_API DRM_RESULT DRM_CALL Oem_Device_GetCert(
    __in_opt                                  DRM_VOID              *f_pOEMContext,
    __in                                      DRM_DEVICE_CERT_TYPE   f_eCertType,
    __out_opt                                 DRM_DEVICE_CERT_TYPE  *f_peCertTypeReturned,
    __out_bcount_opt( *f_pcbDevCert )         DRM_BYTE              *f_pbDevCert,
    __inout                                   DRM_DWORD             *f_pcbDevCert );

/*
** OEM_MANDATORY:
** If the device supports Device Assets (DRM_SUPPORT_DEVICEASSETS=1), this function MUST be implemented by the OEM.
*/
DRM_API DRM_RESULT DRM_CALL Oem_Device_GetOEMProtectedPrivateKeys(
    __in_opt                              DRM_VOID              *f_pOEMContext,
    __in                                  DRM_DEVICE_CERT_TYPE   f_eCertType,
    __out_opt                             DRM_DEVICE_CERT_TYPE  *f_peCertTypeReturned,
    __out_bcount_opt( *f_pcbKey )         DRM_BYTE              *f_pbKey,
    __inout                               DRM_DWORD             *f_pcbKey );

/*
** OEM_MANDATORY:
** If the device supports the concept of applications then the application id
** is used at provisioning time to differentiate between different apps.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Device_GetApplicationID(
    __out DRM_ID* f_pidApplicationID );


DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Device_GetModelInfo(
    __in_opt DRM_VOID   *f_pOEMContext,
    __out_ecount_opt(*f_pcchModelInfo) DRM_WCHAR *f_pwchModelInfo,
    __inout DRM_DWORD *f_pcchModelInfo );

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_BUFFER_PARAM_25033, "Out params can't be const.")
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Config_Read(
    __in_opt DRM_VOID *f_pOEMContext,
    __in_z const DRM_CHAR *f_szKey,
    __out_bcount_opt( *f_pcbValue ) DRM_BYTE *f_pbValue,
    __inout DRM_DWORD *f_pcbValue );
PREFAST_POP /* __WARNING_NONCONST_BUFFER_PARAM_25033 "Out params can't be const." */

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Config_Write(
    __in_opt                       DRM_VOID  *f_pOEMContext,
    __in_z                   const DRM_CHAR  *f_szKey,
    __in_bcount( f_cbValue ) const DRM_BYTE  *f_pbValue,
    __in                           DRM_DWORD  f_cbValue );

/*
** OEM_MANDATORY_CONDITIONALLY:
** If the device supports Secure Time (DRM_SUPPORT_SELWRETIME=1), this function MUST be implemented by the OEM.
*/
DRM_API DRM_RESULT DRM_CALL Oem_Clock_GetResetState(
    __in_opt DRM_VOID *f_pOEMContext,
    __out DRM_BOOL *f_pfReset );

/*
** OEM_MANDATORY_CONDITIONALLY:
** If the device supports Secure Time (DRM_SUPPORT_SELWRETIME=1), this function MUST be implemented by the OEM.
*/
DRM_API DRM_RESULT DRM_CALL Oem_Clock_SetResetState(
    __in_opt DRM_VOID *f_pOEMContext,
    __in DRM_BOOL f_fReset );

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "OEM contexts can't be const." )

/*
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
** Note: The OEM implementation of this function must:
**  1) Have sufficient resolution, specifically <= 20 ms for this function.
**  2) Continue to increase in value while the process is not running (e.g. while sleeping)
*/
DRM_API DRM_DWORD DRM_CALL Oem_Clock_GetTickCount(
    __in_opt    DRM_VOID      *f_pOEMContext );
PREFAST_POP /* __WARNING_NONCONST_BUFFER_PARAM_25033 "OEM contexts can't be const." */

/*
** OEM_MANDATORY_CONDITIONALLY:
** If the device supports H.264 slice header parsing (DRM_SUPPORT_H264) or profiling (DRM_SUPPORT_PROFILING), this function MUST be implemented by the OEM.
** Some test functions and tools will also give useful performance output if this function is implemented.
** Note: This function (when paired with Oem_Clock_QueryPerformanceFrequency) MUST support an overall resolution of 1 millisecond (ms) or better.
*/
DRM_API DRM_UINT64 DRM_CALL Oem_Clock_QueryPerformanceCounter(
    __in_opt    DRM_VOID      *f_pOEMContext );

/*
** OEM_MANDATORY_CONDITIONALLY:
** If the device supports H.264 slice header parsing (DRM_SUPPORT_H264) or profiling (DRM_SUPPORT_PROFILING), this function MUST be implemented by the OEM.
** Some test functions and tools will also give useful performance output if this function is implemented.
** Note: This function (when paired with Oem_Clock_QueryPerformanceCounter) MUST support an overall resolution of 1 millisecond (ms) or better.
*/
DRM_API DRM_UINT64 DRM_CALL Oem_Clock_QueryPerformanceFrequency(
    __in_opt    DRM_VOID      *f_pOEMContext );


/* OEM file IO functions (oemfileio.c). */

/* File Handle */
typedef DRM_VOID *OEM_FILEHDL;
#define OEM_ILWALID_HANDLE_VALUE ((OEM_FILEHDL)-1)

/* Oem_File_Open Access modes */
#define OEM_GENERIC_READ        0x80000000UL
#define OEM_GENERIC_WRITE       0x40000000UL
#define OEM_GENERIC_EXELWTE     0x20000000UL
#define OEM_GENERIC_ALL         0x10000000UL

/* Oem_File_Open Share modes */
#define OEM_FILE_SHARE_NONE     0x00000000UL
#define OEM_FILE_SHARE_READ     0x00000001UL
#define OEM_FILE_SHARE_WRITE    0x00000002UL

/* Oem_File_Open Creation dispositions */
#define OEM_CREATE_NEW          0x00000001UL
#define OEM_CREATE_ALWAYS       0x00000002UL
#define OEM_OPEN_EXISTING       0x00000003UL
#define OEM_OPEN_ALWAYS         0x00000004UL
#define OEM_TRUNCATE_EXISTING   0x00000005UL

#define OEM_ATTRIBUTE_HIDDEN    0x00000002UL
#define OEM_ATTRIBUTE_SYSTEM    0x00000004UL
#define OEM_ATTRIBUTE_NORMAL    0x00000080UL

#define OEM_FILE_FLAG_WRITE_THROUGH 0x80000000

/* SetFilePointer move methods */
#define OEM_FILE_BEGIN          0
#define OEM_FILE_LWRRENT        1
#define OEM_FILE_END            2

/*
** For information on these functions and parameters see MSDN
** For Oem_File_Open see CreateFile in MSDN -- not all flags are supported.
*/

/**********************************************************************
** Function:    Oem_File_Open
** Synopsis:    Creates, opens, reopens a file
** Arguments:   [f_pOEMContext]--Optional pointer to OEM specific context data
**              [f_pwszFileName]--Pointer to DRM_WCHAR buffer holding File
**              name.
**              [f_dwAccessMode]--Type of access to the object.
**              OEM_GENERIC_READ, OEM_GENERIC_WRITE,
**              OEM_GENERIC_EXELWTE and OEM_GENERIC_ALL
**              [f_dwShareMode]--Sharing mode of the object
**              OEM_FILE_SHARE_NONE, OEM_FILE_SHARE_READ
**              and OEM_FILE_SHARE_WRITE
**              [f_dwCreationDisposition]--Action to take on files
**              that exist, and on files that do not exist.
**              OEM_CREATE_NEW, OEM_CREATE_ALWAYS, OEM_OPEN_EXISTING
**              OEM_OPEN_ALWAYS and OEM_TRUNCATE_EXISTING
**              [f_dwAttributes]--File attributes and flags.
**              OEM_ATTRIBUTE_HIDDEN, OEM_ATTRIBUTE_SYSTEM and
**              OEM_ATTRIBUTE_NORMAL
** Returns:     Valid OEM FILE HANDLE. If fails,
**              returns OEM_ILWALID_HANDLE_VALUE
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API OEM_FILEHDL DRM_CALL Oem_File_Open(
    __in_opt DRM_VOID *f_pOEMContext,
    __in_z const DRM_WCHAR *f_pwszFileName,
    __in DRM_DWORD f_dwAccessMode,
    __in DRM_DWORD f_dwShareMode,
    __in DRM_DWORD f_dwCreationDisposition,
    __in DRM_DWORD f_dwAttributes );

/**********************************************************************
** Function:    Oem_File_Close
** Synopsis:    Closes an open handle opened by Oem_File_Open.
** Arguments:   [f_hFile]--File Handle
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_File_Close(
    __in OEM_FILEHDL f_hFile );

/**********************************************************************
** Function:    Oem_File_Delete
** Synopsis:    Deletes a file with the given name
** Arguments:   [f_pwszFileName]--Path to file to be deleted
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_File_Delete(
    __in_z const DRM_WCHAR *f_pwszFileName );

/**********************************************************************
** Function:    Oem_File_Read
** Synopsis:    Reads data from a file.
** Arguments:   [f_hFile]--File Handle
**              [f_pvBuffer]--Pointer to the buffer that receives the
**              data read from the file.
**              [f_nNumberOfBytesToRead]--Number of bytes to read.
**              [f_pNumberOfBytesRead]--Total number of bytes read.
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_File_Read(
    __in OEM_FILEHDL f_hFile,
    __out_ecount_part( f_nNumberOfBytesToRead, *f_pNumberOfBytesRead ) DRM_VOID *f_pvBuffer,
    __in DRM_DWORD f_nNumberOfBytesToRead,
    __out DRM_DWORD *f_pNumberOfBytesRead );

/**********************************************************************
** Function:    Oem_File_Write
** Synopsis:    Writes data to a file.
** Arguments:   [f_hFile]--File Handle
**              [f_pvBuffer]--Pointer to the buffer holding the
**              data read to the file.
**              [f_nNumberOfBytesToWrite]--Number of bytes to write.
**              [f_pNumberOfBytesWritten]--Total number of bytes written.
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_File_Write(
    __in OEM_FILEHDL f_hFile,
    __in_ecount( f_nNumberOfBytesToWrite ) DRM_VOID *f_pvBuffer,
    __in DRM_DWORD f_nNumberOfBytesToWrite,
    __out DRM_DWORD *f_pNumberOfBytesWritten );

/**********************************************************************
** Function:    Oem_File_SetFilePointer
** Synopsis:    Sets File pointer.
** Arguments:   [f_lDistanceToMove]--Number of bytes to move
**              [f_dwMoveMethod]--Starting point for the file pointer move
**              OEM_FILE_BEGIN, OEM_FILE_LWRRENT and  OEM_FILE_END
**              [f_pdwNewFilePointer]--New File pointer.
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_File_SetFilePointer(
    __in OEM_FILEHDL f_hFile,
    __in DRM_LONG f_lDistanceToMove,
    __in DRM_DWORD f_dwMoveMethod,
    __out_opt DRM_DWORD *f_pdwNewFilePointer );

/**********************************************************************
** Function:    Oem_File_Lock
** Synopsis:    Immidiately locks the portion of specified file.
** Arguments:   [f_hFile]--File Handle
**              [f_fExclusive]-- If TRUE, locks file for exclusive access
**              by the calling process.
**              [f_dwFileOffset]--Offset from begining of file.
**              [f_nNumberOfBytesToLock]--Number of bytes to lock.
**              [f_fWait]--Whether to wait for the lock to complete.
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** If the device supports locking (DRM_SUPPORT_FILE_LOCKING=1), this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_File_Lock(
    __in OEM_FILEHDL f_hFile,
    __in DRM_BOOL f_fExclusive,
    __in DRM_DWORD f_dwFileOffset,
    __in DRM_DWORD f_nNumberOfBytesToLock,
    __in DRM_BOOL f_fWait );

/**********************************************************************
** Function:    Oem_File_Unlock
** Synopsis:    Unlocks the locked portion of specified file.
** Arguments:   [f_hFile]--File Handle
**              [f_dwFileOffset]--Offset from begining of file.
**              [f_nNumberOfBytesToLock]--Number of bytes to lock.
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** If the device supports locking (DRM_SUPPORT_FILE_LOCKING=1), this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_File_Unlock(
    __in OEM_FILEHDL f_hFile,
    __in DRM_DWORD f_dwFileOffset,
    __in DRM_DWORD f_nNumberOfBytesToUnlock );

/**********************************************************************
** Function:    Oem_File_SetEndOfFile
** Synopsis:    Moves the end-of-file (EOF) position for the
**              specified file to the current position of the file
**              pointer.
** Arguments:   [f_hFile]--File Handle
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL Oem_File_SetEndOfFile(
    __in OEM_FILEHDL f_hFile);

/**********************************************************************
** Function:    Oem_File_GetSize
** Synopsis:    Gets size of the file.
** Arguments:   [f_hFile]--File Handle
**              [f_pdwFileSize]--Pointer to DRM_DWORD to get the size.
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_File_GetSize(
    __in OEM_FILEHDL f_hFile,
    __out DRM_DWORD  *f_pdwFileSize );

/**********************************************************************
** Function:    Oem_File_FlushBuffers
** Synopsis:    Flushes the buffers of the specified file and causes
**              all buffered data to be written to the file.
** Arguments:   [f_hFile]--File Handle
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_File_FlushBuffers(
    __in OEM_FILEHDL f_hFile );

/* OEM time functions (oemtime.c). */

/**********************************************************************
** Function:    Oem_Clock_SystemTimeToFileTime
** Synopsis:    Colwerts System Time format to File time format.
** Arguments:   [f_lpSystemTime]--Pointer to DRMSYSTEMTIME structure
**              containing system time
**              [f_lpFileTime]--Pointer to DRMFILETIME structure to get
**              the time.
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_Clock_SystemTimeToFileTime(
    __in const DRMSYSTEMTIME *f_lpSystemTime,
    __out DRMFILETIME *f_lpFileTime );

/**********************************************************************
** Function:    Oem_Clock_FileTimeToSystemTime
** Synopsis:    Colwerts File time format to System Time format.
** Arguments:   [f_lpFileTime]--Pointer to DRMFILETIME structure
**              containing file time
**              [f_lpSystemTime]--Pointer to DRMSYSTEMTIME structure to
**              get the time.
** Returns:     Non zero value if succeeds, zero if failed.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API DRM_BOOL DRM_CALL Oem_Clock_FileTimeToSystemTime(
    __in const DRMFILETIME *f_lpFileTime,
    __out DRMSYSTEMTIME *f_lpSystemTime );

/**********************************************************************
** Function:    Oem_Clock_GetSystemTime
** Synopsis:    Gets current System time. It is expressed in UTC.
** Arguments:   [f_pOEMContext]--OEM specific data
**              [f_lpSystemTime]--Pointer to DRMSYSTEMTIME structure
**              to get the time.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API_VOID DRM_VOID DRM_CALL Oem_Clock_GetSystemTime(
    __in_opt    DRM_VOID      *f_pOEMContext,
    __out       DRMSYSTEMTIME *f_psystime );

/**********************************************************************
** Function:    Oem_Clock_GetSystemTimeOffsetAsInt64
** Synopsis:    Gets the offset of the SelwreClock time from the SystemTime. 
**              It is computed from UTC time and returned as a signed int64.
** Arguments:   [f_pOEMContext]--OEM specific data
**              [f_pfiletime]--Pointer to DRM_INT64 to store the offset.
**
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port.
**
***********************************************************************/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_Clock_GetSystemTimeOffsetAsInt64(
    __in_opt    DRM_VOID    *f_pOEMContext,
    __out       DRM_INT64   *f_pi64SelwreClockOffset );

/**********************************************************************
** Function:    Oem_Clock_GetSystemTimeAsFileTime
** Synopsis:    Gets current System time. It is expressed in UTC.
** Arguments:   [f_pOEMContext]--OEM specific data
**              [f_pfiletime]--Pointer to DRMFILETIME structure
**              to get the time.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API_VOID DRM_VOID DRM_CALL Oem_Clock_GetSystemTimeAsFileTime(
    __in_opt    DRM_VOID    *f_pOEMContext,
    __out       DRMFILETIME *f_pfiletime );

/**********************************************************************
** Function:    Oem_Clock_SetSystemTime
** Synopsis:    Sets the System time. It is expressed in UTC.
** Arguments:   [f_pOEMContext]--OEM specific data
**              [f_lpSystemTime]--Pointer to DRMSYSTEMTIME structure
**              holding the time.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API_VOID DRM_VOID DRM_CALL Oem_Clock_SetSystemTime(
    __in_opt       DRM_VOID      *f_pOEMContext,
    __in     const DRMSYSTEMTIME *f_lpSystemTime );

/**********************************************************************
** Function:    Oem_Clock_SetSelwreClockOffsetValue
** Synopsis:    Sets the offset between the secure core clock and the
**              system clock to the provided value.
** Arguments:   [f_pOEMContext]--OEM specific data
**              [f_pui64SelwreTime]--DRM_UINT64 value to set the offset
**              equal to.
**              [f_IsSelwreClockOffsetNegative]--DRM_BOOL stating if the
**              the offset should push the secure time 'forward' or 
**              'back' in time.
**
** OEM_OPTIONAL:
** You do not need to replace this function implementation. However, this
** function MUST have a valid implementation for your PlayReady port.
**
***********************************************************************/
DRM_API_VOID DRM_VOID DRM_CALL Oem_Clock_SetSelwreClockOffsetValue(
    __in_opt       DRM_VOID   *f_pOEMContext,
    __in     const DRM_UINT64  f_ui64SelwreTime,
    __in     const DRM_BOOL    f_IsSelwreClockOffsetNegative );

/**********************************************************************
** Function:    Oem_Clock_SetSelwreClockOffset
** Synopsis:    Sets the offset between the secure core clock and the
**              system clock to the difference between the System Time
**              and the provided SelwreTime
** Arguments:   [f_pOEMContext]--OEM specific data
**              [f_pui64SelwreTime]--Pointer to DRM_UINT64 structure
**              holding the current secure time.
**
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
**
***********************************************************************/
DRM_API_VOID DRM_VOID DRM_CALL Oem_Clock_SetSelwreClockOffset(
    __in_opt       DRM_VOID   *f_pOEMContext,
    __in     const DRM_UINT64 *f_pui64SelwreTime );


PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "Out params can't be const")

/* OEM platform initialization functions (oemimpl.c). */
/*
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
*/
DRM_API DRM_RESULT DRM_CALL Oem_Platform_Init( __inout_opt DRM_VOID *f_pvUserCtx );

/*
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
*/
DRM_API DRM_RESULT DRM_CALL Oem_Platform_UnInit( __inout_opt DRM_VOID *f_pvUserCtx );

PREFAST_POP /* __WARNING_NONCONST_PARAM_25004 */


/* OEM memory allocation functions (oemimpl.c). */

/*
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
** All memory allocations must be 8-byte aligned.
*/
DRM_API DRM_VOID* DRM_CALL Oem_MemAlloc(
    __in DRM_DWORD f_cbSize );

/*
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
*/
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Oem_MemFree(
    __drv_freesMem( Oem ) __in DRM_VOID *f_pv );

#define SAFE_OEM_FREE( p ) DRM_DO {                                                                                 \
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_REDUNDANT_POINTER_TEST_28922, "Safe macros always check for null" )       \
    if ( (p) != NULL )                                                                                              \
    {                                                                                                               \
        vPortFreeFreeable( ( DRM_VOID * )(p) );                                                                     \
        (p) = NULL;                                                                                                 \
    }                                                                                                               \
PREFAST_POP  /* __WARNING_REDUNDANT_POINTER_TEST_28922 */                                                           \
} DRM_WHILE_FALSE

#define SAFE_SELWRE_OEM_FREE( p, c ) DRM_DO {                                                                       \
PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_REDUNDANT_POINTER_TEST_28922, "Safe macros always check for null" )       \
    if ((p) != NULL)                                                                                                \
    {                                                                                                               \
        OEM_SELWRE_ZERO_MEMORY( (DRM_VOID*)(p), (c) );                                                              \
        Oem_MemFree( (DRM_VOID*)(p) );                                                                              \
        (p) = NULL;                                                                                                 \
    }                                                                                                               \
PREFAST_POP  /* __WARNING_REDUNDANT_POINTER_TEST_28922 */                                                           \
} DRM_WHILE_FALSE

#define OEM_MEM_REALLOC( __ptrType, __pvToRealloc, __cbNew, __cbOld ) DRM_DO {                                      \
    __ptrType __pvNew = NULL;                                                                                       \
    if( ( __cbNew ) > 0 )                                                                                           \
    {                                                                                                               \
        ChkMem( __pvNew = (__ptrType)Oem_MemAlloc( __cbNew ) );                                                     \
        if( ( __pvToRealloc ) != NULL && ( __cbOld ) > 0 )                                                          \
        {                                                                                                           \
            MEMCPY( __pvNew, __pvToRealloc, DRM_MIN( ( (DRM_DWORD)( __cbNew ) ), ( (DRM_DWORD)( __cbOld ) ) ) );    \
        }                                                                                                           \
    }                                                                                                               \
    SAFE_OEM_FREE( __pvToRealloc );                                                                                 \
    ( __pvToRealloc ) = __pvNew;                                                                                    \
} DRM_WHILE_FALSE

/*
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
*/
DRM_API_VOID DRM_VOID DRM_CALL Oem_Random_SetSeed(
    __in_opt DRM_VOID *f_pOEMContext,
    __in const DRM_UINT64 f_qwNewSeed );

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_NONCONST_PARAM_25004, "OEM contexts can't be const" );
/*
** OEM_MANDATORY:
** For all devices, this function MUST be implemented by the OEM.
*/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Random_GetBytes(
    __in_opt               DRM_VOID  *f_pOEMContext,
    __out_bcount(f_cbData) DRM_BYTE  *f_pbData,
    __in                   DRM_DWORD  f_cbData );

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Random_GetEntropy(
    __in_opt                 DRM_VOID  *f_pOEMContext,
    __out_bcount( f_cbData ) DRM_BYTE  *f_pbData,
    __in                     DRM_DWORD  f_cbData );
PREFAST_POP

/*
** OEM_MANDATORY:
** If the device supports XML hashing (DRM_SUPPORT_XMLHASH=1), this function MUST be implemented by the OEM.
*/
DRM_API DRM_DWORD DRM_CALL Oem_GetLwrrentThreadId( DRM_VOID );

DRM_API_VOID DRM_VOID DRM_CALL Oem_Hds_ClearBlockHeaderCache( DRM_VOID );

DRM_API DRM_RESULT DRM_CALL Oem_Hds_CheckBlockHeaderCache(
    __in      DRM_DWORD   f_dwBlockNumber,
    __in_opt  OEM_FILEHDL f_hHDSFileHandle,
    __out_opt DRM_DWORD  *f_pdwBlockHeaderMetadata,
    __out_bcount_opt( f_cbBytesToRead ) DRM_BYTE   *f_pbBlock,
    __in      DRM_DWORD   f_cbBytesToRead );

DRM_API DRM_RESULT DRM_CALL Oem_Hds_UpdateBlockHeaderCache(
    __in                                      DRM_DWORD   f_dwBlockNumber,
    __in_opt                                  OEM_FILEHDL f_hHDSFileHandle,
    __in                                      DRM_DWORD   f_dwBlockHeaderMetadata,
    __in_bcount_opt( f_cbBytesToWrite ) const DRM_BYTE   *f_pbBlock,
    __in                                      DRM_DWORD   f_cbBytesToWrite );

DRM_API_VOID DRM_VOID DRM_CALL Oem_Hds_ClearBlockHeaderCacheForFile(
    __in    OEM_FILEHDL f_hHDSFileHandle );

DRM_API DRM_RESULT DRM_CALL Oem_Hds_GetParams(
    __in_opt        DRM_VOID  *f_pOEMContext,
    __in      const DRM_WCHAR *f_pwszDeviceStoreName,
    __out_opt       DRM_DWORD *f_pdwDataStoreFileSizeInitKB,
    __out_opt       DRM_DWORD *f_pdwDataStoreFileSizeGrowKB,
    __out_opt       DRM_DWORD *f_pdwDataStoreBlockSize );

typedef struct
{
    DRM_BOOL    fFreezeTickCount;
    DRM_DWORD   dwTickCount;
} DRM_TEST_OEM_CONTEXT;


/* Code Coverage functions (oemimpl.c) */
DRM_API_VOID DRM_VOID DRM_CALL Oem_CodeCoverage_FlushData(
    __in_opt const DRM_VOID  *f_pOEMContext );

typedef       DRM_VOID * OEM_CRYPTO_HANDLE;
typedef const DRM_VOID * OEM_CRYPTO_CONST_HANDLE;
#define OEM_CRYPTO_HANDLE_ILWALID ((OEM_CRYPTO_HANDLE)0)

typedef enum __tagDRM_PKCRYPTO_SUPPORTED_ALGORITHMS
{
    eDRM_ECC_Reserved_P160  = 1,     /* Do not reuse: Compatibility */
    eDRM_ECC_P256           = 2,
    eDRM_RSA                = 3
} DRM_PKCRYPTO_SUPPORTED_ALGORITHMS;

EXIT_PK_NAMESPACE;

PREFAST_POP     /* __WARNING_NONCONST_PARAM_25004 */

#endif /* __OEMCOMMON_H__ */

