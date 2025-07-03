/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/*
**  drmerrsay.h
**
**  Defines functions used for looking up DRM error values.
**
*/

#ifndef __DRMERRSAY_H__
#define __DRMERRSAY_H__

#include <drmtypes.h>

ENTER_PK_NAMESPACE;

typedef struct _tagDRM_ERROR_MESSAGE
{
    DRM_RESULT      m_drValue;
    DRM_ANSI_CONST_STRING m_dastrName;
    DRM_ANSI_CONST_STRING m_dastrDescription;
} DRM_ERROR_MESSAGE;

extern const DRM_ERROR_MESSAGE g_rgoPKErrorMap[];

DRM_API DRM_BOOL   DRM_CALL DRM_ERR_IsErrorCodeKnown( DRM_RESULT f_drErrorCode );

DRM_API const DRM_CHAR*  DRM_CALL DRM_ERR_GetErrorNameFromCode(
    __in                      DRM_RESULT      f_drErrorCode,
    __deref_opt_out_opt const DRM_CHAR      **f_ppszDescription );

DRM_API DRM_RESULT DRM_CALL DRM_ERR_GetErrorCodeFromStringA(
    __in_ecount( f_cchString )  const DRM_CHAR   *f_pszString,
    __in                              DRM_DWORD   f_cchString,
    __out                             DRM_RESULT *f_pdrErrorCode );

EXIT_PK_NAMESPACE;

#endif /* __DRMERRSAY_H__ */
