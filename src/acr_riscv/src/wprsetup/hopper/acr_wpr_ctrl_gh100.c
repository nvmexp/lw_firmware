/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_wpr_ctrl_gh100.c
 */
//
// Includes
//
#include "acr.h"
#include "acr_objacr.h"
#include "acr_signature.h"

static inline ACR_STATUS
_acrLsfLsbHeaderHandler_v2(LwU32, PLSF_LSB_HEADER_V2, void *)  ATTR_OVLY(OVL_NAME);

static inline ACR_STATUS
_acrLsfWprHeaderHandler_v2(LwU32, PLSF_WPR_HEADER_V2, void *)  ATTR_OVLY(OVL_NAME);

static inline ACR_STATUS
_acrLsfSubWprHeaderHandler_v2(LwU32, PLSF_SHARED_SUB_WPR_HEADER_V2, void *) ATTR_OVLY(OVL_NAME);

static inline ACR_STATUS
_acrLsfUcodeDescHandler_v2(LwU32, PLSF_UCODE_DESC_V2, void *) ATTR_OVLY(OVL_NAME);

static inline ACR_STATUS
_acrCheckDependency_v2(PLSF_UCODE_DESC_V2 pUcodeDescV2, void *pIoBuffer) ATTR_OVLY(OVL_NAME);

static inline ACR_STATUS _acrLsfLsbHeaderWrapperCmdPrologue(LwU32 command,  PLSF_LSB_HEADER_WRAPPER pWrapper,
                                                               void *pIoBuffer, LwBool *pMoreProcess) ATTR_OVLY(OVL_NAME);

static inline ACR_STATUS _acrGetUcodeDescWrapper(PLSF_LSB_HEADER_WRAPPER pWrapper, void *pIoBuffer) ATTR_OVLY(OVL_NAME);

GCC_ATTRIB_ALWAYSINLINE()
static inline ACR_STATUS
_acrGetUcodeDescWrapper
(
    PLSF_LSB_HEADER_WRAPPER pLsfLsbHeaderWrapper,
    void *pIoBuffer
)
{
    PLSF_LSB_HEADER_V2       pLsbHeaderV2         = NULL;
    PLSF_UCODE_DESC_WRAPPER  pLsfUcodeDescWrapper = NULL;
    PWPR_GENERIC_HEADER      pGenHeader           = NULL;

    pGenHeader = &pLsfLsbHeaderWrapper->genericHdr;

    switch(pGenHeader->version)
    {
        case LSF_LSB_HEADER_VERSION_2:
        {
            pLsbHeaderV2 = (PLSF_LSB_HEADER_V2)&pLsfLsbHeaderWrapper->u;
            pLsfUcodeDescWrapper = &pLsbHeaderV2->signature;
        }
        break;

        default:
            return ACR_ERROR_ILWALID_OPERATION;
    }

    if (pLsfUcodeDescWrapper == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    pGenHeader = &pLsfUcodeDescWrapper->genericHdr;

    switch (pGenHeader->version)
    {
        case LSF_UCODE_DESC_VERSION_2 :
        {
           acrMemcpy_HAL(pAcr, (void *)pIoBuffer, (void *)pLsfUcodeDescWrapper, LSF_UCODE_DESC_WRAPPER_V2_SIZE_BYTE);
        }
        break;

        default:
            return ACR_ERROR_ILWALID_OPERATION;
    }

    return ACR_OK;
}

/*!
 * @brief LSF_LSB_HEADER_WRAPPER version 2 handler function.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline ACR_STATUS
_acrLsfLsbHeaderHandler_v2
(
    LwU32 command,
    PLSF_LSB_HEADER_V2 pLsbHeaderV2,
    void *pIoBuffer
)
{
    if (pIoBuffer == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (command)
    {
        case LSF_LSB_HEADER_COMMAND_SET_UCODE_OFFSET:
        {
            pLsbHeaderV2->ucodeOffset = *(LwU32 *)pIoBuffer;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_UCODE_OFFSET:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->ucodeOffset;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_UCODE_SIZE:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->ucodeSize;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_DATA_SIZE:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->dataSize;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_SET_BL_DATA_OFFSET:
        {
            pLsbHeaderV2->blDataOffset = *(LwU32 *)pIoBuffer;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_BL_DATA_OFFSET:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->blDataOffset;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_SET_APP_CODE_OFFSET:
        {
            pLsbHeaderV2->appCodeOffset = *(LwU32 *)pIoBuffer;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_SET_APP_CODE_SIZE:
        {
            pLsbHeaderV2->appCodeSize = *(LwU32 *)pIoBuffer;
        }
        break;
#ifdef BOOT_FROM_HS_BUILD
        case LSF_LSB_HEADER_COMMAND_SET_APP_IMEM_OFFSET:
        {
            pLsbHeaderV2->appImemOffset = *(LwU32 *)pIoBuffer;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_SET_APP_DMEM_OFFSET:
        {
            pLsbHeaderV2->appDmemOffset = *(LwU32 *)pIoBuffer;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_APP_IMEM_OFFSET:
        {
            *(LwU32 *)pIoBuffer =  pLsbHeaderV2->appImemOffset;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_APP_DMEM_OFFSET:
        {
            *(LwU32 *)pIoBuffer =  pLsbHeaderV2->appDmemOffset;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_HS_FMC_PARAMS:
        {
           acrMemcpy_HAL(pAcr, (void *)pIoBuffer, (void *)&pLsbHeaderV2->hsFmcParams, sizeof(HS_FMC_PARAMS));
        }
        break;        
#endif
        case LSF_LSB_HEADER_COMMAND_SET_APP_DATA_OFFSET:
        {
            pLsbHeaderV2->appDataOffset = *(LwU32 *)pIoBuffer;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_SET_APP_DATA_SIZE:
        {
            pLsbHeaderV2->appDataSize = *(LwU32 *)pIoBuffer;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_SET_BL_DATA_SIZE:
        {
            pLsbHeaderV2->blDataSize = *(LwU32 *)pIoBuffer;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_UCODE_DESC_WRAPPER:
        {
           acrMemcpy_HAL(pAcr, (void *)pIoBuffer, (void *)&pLsbHeaderV2->signature, LSF_UCODE_DESC_WRAPPER_V2_SIZE_BYTE);
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_APP_CODE_OFFSET:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->appCodeOffset;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_APP_CODE_SIZE:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->appCodeSize;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_APP_DATA_OFFSET:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->appDataOffset;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_APP_DATA_SIZE:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->appDataSize;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_BL_CODE_SIZE:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->blCodeSize;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_SET_BL_CODE_SIZE:
        {
            pLsbHeaderV2->blCodeSize = *(LwU32 *)pIoBuffer;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_BL_DATA_SIZE:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->blDataSize;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_BL_IMEM_OFFSET:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->blImemOffset;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_FLAGS:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->flags;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_MONITOR_CODE_OFFSET:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->monitorCodeOffset;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_MONITOR_DATA_OFFSET:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->monitorDataOffset;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_MANIFEST_OFFSET:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->manifestOffset;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_PRESENT:
        {
            *(LwBool *)pIoBuffer = pLsbHeaderV2->hsOvlSigBlobParams.bHsOvlSigBlobPresent;
        }
        break;
 
        case LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_OFFSET:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->hsOvlSigBlobParams.hsOvlSigBlobOffset;
        }
        break;
 
        case LSF_LSB_HEADER_COMMAND_GET_HS_OVL_SIG_BLOB_SIZE:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->hsOvlSigBlobParams.hsOvlSigBlobSize;
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_MANIFEST_SIZE:
        {
            *(LwU32 *)pIoBuffer = pLsbHeaderV2->manifestSize;
        }
        break;

        default:
            return ACR_ERROR_ILWALID_ARGUMENT;
    }

    return ACR_OK;
}

GCC_ATTRIB_ALWAYSINLINE()
static inline ACR_STATUS
_acrLsfLsbHeaderWrapperCmdPrologue
(
    LwU32 command,
    PLSF_LSB_HEADER_WRAPPER pWrapper,
    void *pIoBuffer,
    LwBool *pMoreProcess
)
{
    PWPR_GENERIC_HEADER pGenericHdr;
    ACR_STATUS status = ACR_ERROR_ILWALID_ARGUMENT;

    if (pIoBuffer == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (command)
    {
        case LSF_LSB_HEADER_COMMAND_COPY_WRAPPER:
        {
            pGenericHdr = &pWrapper->genericHdr;

            if (pGenericHdr->version == LSF_LSB_HEADER_VERSION_2)
            {
                acrMemcpy_HAL(pAcr, (void *)pIoBuffer, (void *)pWrapper, LSF_LSB_HEADER_WRAPPER_V2_SIZE_BYTE);
                *pMoreProcess = LW_FALSE;
            }
            else
            {
                return ACR_ERROR_ILWALID_OPERATION;
            }
        }
        break;

        case LSF_LSB_HEADER_COMMAND_GET_UCODE_DESC_WRAPPER:
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrGetUcodeDescWrapper(pWrapper, pIoBuffer));
            *pMoreProcess = LW_FALSE;
        }
        break;
    }

    return ACR_OK;
}

/*!
 * @brief LSF_LSB_HEADER_WRAPPER control function. Dispatch command to different version handler per header version.
 */
ACR_STATUS
acrLsfLsbHeaderWrapperCtrl_GH100
(
    LwU32 command,
    PLSF_LSB_HEADER_WRAPPER pWrapper,
    void *pIoBuffer
)
{
    PWPR_GENERIC_HEADER pGenericHdr;
    PLSF_LSB_HEADER_V2  pLsbHeaderV2;
    ACR_STATUS          status;
    LwBool              bMoreProcess = LW_TRUE;

    if (pWrapper == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrLsfLsbHeaderWrapperCmdPrologue(command, pWrapper, pIoBuffer, &bMoreProcess));

    if (bMoreProcess)
    {
        pGenericHdr = &pWrapper->genericHdr;

        switch (pGenericHdr->version)
        {
            case LSF_LSB_HEADER_VERSION_2:
            {
                pLsbHeaderV2 = &pWrapper->u.lsfLsbHdrV2;
                CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrLsfLsbHeaderHandler_v2(command, pLsbHeaderV2, pIoBuffer));
            }
            break;

            default:
                return ACR_ERROR_ILWALID_OPERATION;
        }
    }

    return ACR_OK;
}

/*!
 * @brief LSF_WPR_HEADER_WRAPPER version 2 handler function.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline ACR_STATUS
_acrLsfWprHeaderHandler_v2
(
    LwU32 command,
    PLSF_WPR_HEADER_V2 pWprHeaderV2,
    void *pIoBuffer
)
{
    if (pIoBuffer == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (command)
    {
        case LSF_WPR_HEADER_COMMAND_GET_FALCON_ID:
        {
          *(LwU32 * )pIoBuffer = pWprHeaderV2->falconId;
        }
        break;

        case LSF_WPR_HEADER_COMMAND_SET_LSB_OFFSET:
        {
            pWprHeaderV2->lsbOffset = *(LwU32 * )pIoBuffer;
        }
        break;

        case LSF_WPR_HEADER_COMMAND_SET_BIN_VERSION:
        {
            pWprHeaderV2->bilwersion = *(LwU32 * )pIoBuffer;
        }
        break;

        case LSF_WPR_HEADER_COMMAND_GET_BIN_VERSION:
        {
            *(LwU32 * )pIoBuffer = pWprHeaderV2->bilwersion;
        }
        break;

        case LSF_WPR_HEADER_COMMAND_GET_LSB_OFFSET:
        {
            *(LwU32 * )pIoBuffer = pWprHeaderV2->lsbOffset;
        }
        break;

        case LSF_WPR_HEADER_COMMAND_SET_FALCON_ID:
        {
             pWprHeaderV2->falconId = *(LwU32 * )pIoBuffer;
        }
        break;

        default:
            return ACR_ERROR_ILWALID_OPERATION;
    }

    return ACR_OK;
}

/*!
 * @brief LSF_WPR_HEADER_WRAPPER control function. Dispatch command to different version handler per header version.
 */
ACR_STATUS
acrLsfWprHeaderWrapperCtrl_GH100
(
    LwU32 command,
    PLSF_WPR_HEADER_WRAPPER pWrapper,
    void *pIoBuffer
)
{
    PWPR_GENERIC_HEADER pGenericHdr;
    PLSF_WPR_HEADER_V2  pWprHeaderV2;
    ACR_STATUS          status;

    if (pWrapper == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    pGenericHdr = &pWrapper->genericHdr;

    switch (pGenericHdr->version)
    {
        case LSF_WPR_HEADER_VERSION_2:
        {
            pWprHeaderV2 = &pWrapper->u.lsfWprHdrV2;
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrLsfWprHeaderHandler_v2(command, pWprHeaderV2, pIoBuffer));
        }
        break;

        default:
            return ACR_ERROR_ILWALID_OPERATION;
    }

    return ACR_OK;
}

/*!
 * @brief LSF_SHARED_SUB_WPR_HEADER_WRAPPER version 2 handler function.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline ACR_STATUS
_acrLsfSubWprHeaderHandler_v2
(
    LwU32 command,
    PLSF_SHARED_SUB_WPR_HEADER_V2 pSubWprV2,
    void *pIoBuffer
)
{
    if (pIoBuffer == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    switch (command)
    {
        case LSF_SUB_WPR_HEADER_COMMAND_GET_START_ADDR:
        {
            *(LwU32 *)pIoBuffer = pSubWprV2->startAddr;
        }
        break;

        case LSF_SUB_WPR_HEADER_COMMAND_GET_SIZE4K:
        {
            *(LwU32 *)pIoBuffer = pSubWprV2->size4K;
        }
        break;

        case LSF_SUB_WPR_HEADER_COMMAND_GET_USECASEID:
        {
            *(LwU32 *)pIoBuffer = pSubWprV2->useCaseId;
        }
        break;

        default:
            return ACR_ERROR_ILWALID_OPERATION;
    }

    return ACR_OK;
}

/*!
 * @brief LSF_SHARED_SUB_WPR_HEADER_WRAPPER control function. Dispatch command to different version handler per header version.
 */
ACR_STATUS
acrLsfSubWprHeaderWrapperCtrl_GH100
(
    LwU32 command,
    PLSF_SHARED_SUB_WPR_HEADER_WRAPPER pWrapper,
    void *pIoBuffer
)
{
    PWPR_GENERIC_HEADER pGenericHdr;
    PLSF_SHARED_SUB_WPR_HEADER_V2 pSubWprV2;
    ACR_STATUS          status;

    if (pWrapper == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    pGenericHdr = &pWrapper->genericHdr;

    switch (pGenericHdr->version)
    {
        case LSF_WPR_HEADER_VERSION_2:
        {
            pSubWprV2 = &pWrapper->u.lsfSubWprHdrV2;
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrLsfSubWprHeaderHandler_v2(command, pSubWprV2, pIoBuffer));
        }
        break;

        default:
            return ACR_ERROR_ILWALID_OPERATION;
    }

    return ACR_OK;
}

/*!
 * @brief To check dependency mapping table in ucode descriptor algn with version 2 defines.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline ACR_STATUS
_acrCheckDependency_v2
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
            return ACR_ERROR_REVOCATION_CHECK_FAIL;
        }
    }

    return ACR_OK;
}

/*!
 * @brief LSF_UCODE_DESC_WRAPPER version 2 handler function.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline ACR_STATUS
_acrLsfUcodeDescHandler_v2
(
    LwU32 command,
    PLSF_UCODE_DESC_V2 pUcodeDescV2,
    void *pIoBuffer
)
{
    ACR_STATUS status;
    LwU32      size = 0;

    if (pIoBuffer == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
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
                pUcodeDescV2->sigSize <= ACR_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE)
            {
                acrMemcpy_HAL(pAcr, (void *)pIoBuffer, (void *)pUcodeDescV2->debugSig[LSF_UCODE_COMPONENT_INDEX_CODE],
                                pUcodeDescV2->sigSize);
            }
            else
            {
                return ACR_ERROR_ILWALID_OPERATION;
            }
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_DEBUG_SIGNATURE_DATA:
        {
            if (pUcodeDescV2->sigSize <= LSF_SIGNATURE_SIZE_MAX_BYTE &&
                pUcodeDescV2->sigSize <= ACR_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE)
            {
                acrMemcpy_HAL(pAcr, (void *)pIoBuffer, (void *)pUcodeDescV2->debugSig[LSF_UCODE_COMPONENT_INDEX_DATA],
                                pUcodeDescV2->sigSize);
            }
            else
            {
                return ACR_ERROR_ILWALID_OPERATION;
            }
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_PROD_SIGNATURE_CODE:
        {
            if (pUcodeDescV2->sigSize <= LSF_SIGNATURE_SIZE_MAX_BYTE &&
                pUcodeDescV2->sigSize <= ACR_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE)
            {
                acrMemcpy_HAL(pAcr, (void *)pIoBuffer, (void *)pUcodeDescV2->prodSig[LSF_UCODE_COMPONENT_INDEX_CODE],
                                pUcodeDescV2->sigSize);
            }
            else
            {
                return ACR_ERROR_ILWALID_OPERATION;
            }
        }
        break;

        case LSF_UCODE_DESC_COMMAND_GET_PROD_SIGNATURE_DATA:
        {
            if (pUcodeDescV2->sigSize <= LSF_SIGNATURE_SIZE_MAX_BYTE &&
                pUcodeDescV2->sigSize <= ACR_LS_SIGNATURE_BUFFER_SIZE_MAX_BYTE)
            {
                acrMemcpy_HAL(pAcr, (void *)pIoBuffer, (void *)pUcodeDescV2->prodSig[LSF_UCODE_COMPONENT_INDEX_DATA],
                               pUcodeDescV2->sigSize);
            }
            else
            {
                return ACR_ERROR_ILWALID_OPERATION;
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
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrCheckDependency_v2(pUcodeDescV2, pIoBuffer));
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
            acrMemcpy_HAL(pAcr, (void *)pIoBuffer, (void *)pUcodeDescV2->lsEncIV,
                               LS_ENCRYPTION_AES_CBC_IV_SIZE_BYTE);
        }
        break;

        case LSF_UCODE_DESC_COMMAND_COPY_DEPMAP_CTX:
        {
            if (pUcodeDescV2->depMapCount)
            {
                size = pUcodeDescV2->depMapCount * 8;
                acrMemcpy_HAL(pAcr, (void *)pIoBuffer, (void *)pUcodeDescV2->depMap, size);
            }
        }
        break;

        default:
            return ACR_ERROR_ILWALID_OPERATION;
    }

    return ACR_OK;
}

/*!
 * @brief LSF_UCODE_DESC_WRAPPER control function. Dispatch command to different version handler per header version.
 */
ACR_STATUS
acrLsfUcodeDescWrapperCtrl_GH100
(
    LwU32 command,
    PLSF_UCODE_DESC_WRAPPER pWrapper,
    void *pIoBuffer
)
{
    PWPR_GENERIC_HEADER pGenericHdr;
    PLSF_UCODE_DESC_V2 pUcodeDescV2;
    ACR_STATUS          status;

    if (pWrapper == NULL)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    pGenericHdr = &pWrapper->genericHdr;

    switch (pGenericHdr->version)
    {
        case LSF_WPR_HEADER_VERSION_2:
        {
            pUcodeDescV2 = &pWrapper->u.lsfUcodeDescV2;
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(_acrLsfUcodeDescHandler_v2(command, pUcodeDescV2, pIoBuffer));
        }
        break;

        default:
            return ACR_ERROR_ILWALID_OPERATION;
    }

    return ACR_OK;
}
