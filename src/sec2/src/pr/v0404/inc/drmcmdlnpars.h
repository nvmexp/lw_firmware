/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __CMDLNPARS_H__
#define __CMDLNPARS_H__

#include <drmtypes.h>

#define DRM_CMD_MAX_ARGUMENTS 15

#ifdef __cplusplus
extern "C" {
#endif

DRM_LONG DRM_CALL DRM_Main( _In_ DRM_LONG argc, _At_buffer_( argv, _Iter_, argc, _In_opt_z_ ) DRM_WCHAR **argv );

#ifdef __cplusplus
}
#endif

ENTER_PK_NAMESPACE;

DRM_LONG DRM_CALL DRM_CMD_ColwertToArgvArgc(
    __in __nullterminated                                     const DRM_WCHAR *lpCmdLine,
    _Out_opt_cap_( DRM_CMD_MAX_ARGUMENTS ) _Deref_post_opt_z_       DRM_WCHAR *argv[ DRM_CMD_MAX_ARGUMENTS ] );

DRM_BOOL DRM_CALL DRM_CMD_ParseCmdLine(
    __in_z_opt                                                                                                                                                          const DRM_WCHAR        *pwszArgument,
    __inout_ecount_opt( 1 )                                                                                                                                                   DRM_WCHAR        *pwchOptionChar,
    __out_opt _Post_satisfies_( f_pdstrParam == NULL || f_pdstrParam->pwszString == NULL || _String_length_( f_pdstrParam->pwszString ) == f_pdstrParam->cchString )          DRM_CONST_STRING *f_pdstrParam,
    __out_opt _Post_satisfies_( f_pdstrExtra == NULL || f_pdstrExtra->pwszString == NULL || _String_length_( f_pdstrExtra->pwszString ) == f_pdstrExtra->cchString )          DRM_CONST_STRING *f_pdstrExtra );

DRM_BOOL DRM_CALL DRM_CMD_ParseCmdLineMultiChar(
    __in_z    const DRM_WCHAR        *pwszArgument,
    __inout         DRM_CONST_STRING *pwszOptionStr,
    __out_opt       DRM_CONST_STRING *f_pdstrParam,
    __out_opt       DRM_CONST_STRING *f_pdstrExtra );

DRM_RESULT DRM_CALL DRM_CMD_TryProcessDefaultOption(
    __in_z const DRM_WCHAR  *f_wszArgument );

EXIT_PK_NAMESPACE;

#endif /* __CMDLNPARS_H__ */
