/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   boot_utils.c
 * @brief  Utilities routines for SOE Boot Binary
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"

/* ------------------------- Application Includes --------------------------- */

void memClear(void *pStart, LwU32 sizeInBytes)
{
  LwU8 *pLoc = (LwU8 *) pStart;
  while (sizeInBytes--) {
    *pLoc = 0;
    pLoc++;
  }
}

void* memCopy(void *pDst, const void *pSrc, LwU32 sizeInBytes)
{
  LwU8       *pDstC = (LwU8 *) pDst;
  const LwU8 *pSrcC = (LwU8 *) pSrc;
  while (sizeInBytes--) {
    *pDstC++ = *pSrcC++;
  }
  return pDst;
}
