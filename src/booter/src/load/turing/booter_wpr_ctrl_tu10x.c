/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_wpr_ctrl_tu10x.c
 */
//
// Includes
//
#include "booter.h"

/*!
 * @brief To check dependency mapping table in ucode descriptor algn with version 2 defines.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline BOOTER_STATUS
_booterCheckDependency_v2
(
    PLSF_UCODE_DESC_V2 pUcodeDescV2,
    void *pIoBuffer
)
{
    LwU32   i;
    LwU32   depfalconId = 0;
    LwU32   expVer      = 0;
    LwU32   *pBilwersions = (LwU32 *)pIoBuffer;


    for (i=0; i< pUcodeDescV2->depMapCount; i++)
    {
        depfalconId = ((LwU32*)pUcodeDescV2->depMap)[i*2];
        expVer      = ((LwU32*)pUcodeDescV2->depMap)[(i*2)+1];

        //
        // Checks for revocation, invalid bin version in pBilwersions implies that
        // particular falcon is not present in WPR header.
        //
        if ((pBilwersions[depfalconId] < expVer) &&
            (pBilwersions[depfalconId] != LSF_FALCON_BIN_VERSION_ILWALID))
        {
            return BOOTER_ERROR_REVOCATION_CHECK_FAIL;
        }
    }

    return BOOTER_OK;
}

/*!
 * @brief LSF_UCODE_DESC_WRAPPER version 2 handler function.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline BOOTER_STATUS
_booterLsfUcodeDescHandler_v2
(
    LwU32 command,
    PLSF_UCODE_DESC_V2 pUcodeDescV2,
    void *pIoBuffer
)
{
    BOOTER_STATUS status;
    LwU32      size = 0;

    if (pIoBuffer == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    switch (command)
    {
        case LSF_UCODE_DESC_COMMAND_GET_BIN_VERION:
        {
           *(LwU32 *)pIoBuffer = pUcodeDescV2->lsUcodeVersion;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_FALCON_ID:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->falconId;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_DEBUG_SIGNATURE_CODE:
        {
            if (pUcodeDescV2->sigSize <= LSF_SIGNATURE_SIZE_MAX_BYTE &&
                pUcodeDescV2->sigSize <= BOOTER_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE)
            {
                booterMemcpy_HAL(pbooter, (void *)pIoBuffer, (void *)pUcodeDescV2->debugSig[LSF_UCODE_COMPONENT_INDEX_CODE],
                                pUcodeDescV2->sigSize);
            }
            else
            {
                return BOOTER_ERROR_ILWALID_OPERATION;
            }
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_DEBUG_SIGNATURE_DATA:
        {
            if (pUcodeDescV2->sigSize <= LSF_SIGNATURE_SIZE_MAX_BYTE &&
                pUcodeDescV2->sigSize <= BOOTER_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE)
            {
                booterMemcpy_HAL(pbooter, (void *)pIoBuffer, (void *)pUcodeDescV2->debugSig[LSF_UCODE_COMPONENT_INDEX_DATA],
                                pUcodeDescV2->sigSize);
            }
            else
            {
                return BOOTER_ERROR_ILWALID_OPERATION;
            }
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_PROD_SIGNATURE_CODE:
        {
            if (pUcodeDescV2->sigSize <= LSF_SIGNATURE_SIZE_MAX_BYTE &&
                pUcodeDescV2->sigSize <= BOOTER_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE)
            {
                booterMemcpy_HAL(pbooter, (void *)pIoBuffer, (void *)pUcodeDescV2->prodSig[LSF_UCODE_COMPONENT_INDEX_CODE],
                                pUcodeDescV2->sigSize);
            }
            else
            {
                return BOOTER_ERROR_ILWALID_OPERATION;
            }
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_PROD_SIGNATURE_DATA:
        {
            if (pUcodeDescV2->sigSize <= LSF_SIGNATURE_SIZE_MAX_BYTE &&
                pUcodeDescV2->sigSize <= BOOTER_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE)
            {
                booterMemcpy_HAL(pbooter, (void *)pIoBuffer, (void *)pUcodeDescV2->prodSig[LSF_UCODE_COMPONENT_INDEX_DATA],
                               pUcodeDescV2->sigSize);
            }
            else
            {
                return BOOTER_ERROR_ILWALID_OPERATION;
            }
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_HASH_ALGO_VER:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->hashAlgoVer;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_HASH_ALGO:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->hashAlgo;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO_VER:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->sigAlgoVer;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->sigAlgo;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_SIG_ALGO_PADDING_TYPE:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->sigAlgoPaddingType;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_CHECK_DEPENDENCY:
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_booterCheckDependency_v2(pUcodeDescV2, pIoBuffer));
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_DEPMAP_SIZE:
        {
            if (pUcodeDescV2->depMapCount)
            {
                size = pUcodeDescV2->depMapCount * 8;
            }

            *(LwU32 *)pIoBuffer = size;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_VER:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->lsUcodeVersion;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ID:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->lsUcodeId;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_LS_UCODE_ENCRYPTED_FLAG:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->bUcodeLsEncrypted;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_LS_ENCRYPTION_ALGO_TYPE:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->lsEncAlgoType;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_LS_ENCRYPTION_ALGO_VER:
        {
            *(LwU32 *)pIoBuffer = pUcodeDescV2->lsEncAlgoVer;
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_AES_CBC_IV:
        {
            booterMemcpy_HAL(pbooter, (void *)pIoBuffer, (void *)pUcodeDescV2->lsEncIV,
                               LS_ENCRYPTION_AES_CBC_IV_SIZE_BYTE);
        }
        break;

        case LSF_UCODE_DESC_COMMAND_COPY_DEPMAP_CTX:
        {
            if (pUcodeDescV2->depMapCount)
            {
                size = pUcodeDescV2->depMapCount * 8;
                booterMemcpy_HAL(pbooter, (void *)pIoBuffer, (void *)pUcodeDescV2->depMap, size);
            }
        }
        break;

        default:
            return BOOTER_ERROR_ILWALID_OPERATION;
    }

    return BOOTER_OK;
}

/*!
 * @brief LSF_UCODE_DESC_WRAPPER control function. Dispatch command to different version handler per header version.
 */
BOOTER_STATUS
booterLsfUcodeDescWrapperCtrl_TU10X
(
    LwU32 command,
    PLSF_UCODE_DESC_WRAPPER pWrapper,
    void *pIoBuffer
)
{
    PWPR_GENERIC_HEADER pGenericHdr;
    PLSF_UCODE_DESC_V2 pUcodeDescV2;
    BOOTER_STATUS          status;

    if (pWrapper == NULL)
    {
        return BOOTER_ERROR_ILWALID_ARGUMENT;
    }

    pGenericHdr = &pWrapper->genericHdr;

    switch (pGenericHdr->version)
    {
        case LSF_WPR_HEADER_VERSION_2:
        {
            pUcodeDescV2 = &pWrapper->u.lsfUcodeDescV2;
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_booterLsfUcodeDescHandler_v2(command, pUcodeDescV2, pIoBuffer));
        }
        break;

        default:
            return BOOTER_ERROR_ILWALID_OPERATION;
    }

    return BOOTER_OK;
}
