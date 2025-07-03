/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/*!
* @file    hdcp22wired_srm.c
* @brief   Library to implement revocation check functionality for HDCP22.
*/

// TODO: Change the file name to hdcp22wired_certrx_srm.c

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_ake.h"
#include "hdcp22wired_srm.h"
#include "lib_hdcpauth.h"

/* ------------------------ Static Function Prototypes  --------------------- */
static FLCN_STATUS _hdcp22ConsumeAndReadFromFb(PRM_FLCN_MEM_DESC pFbDmaDesc, LwU32 *pFbOff, LwU8 *pTmpBuf, LwU32 *pDoneB, LwU8 *pDest, LwU32 destSize)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredCertrxSrm", "_hdcp22ConsumeAndReadFromFb");
static FLCN_STATUS _hdcp22FbDmaRead(LwU8 *pDest, PRM_FLCN_MEM_DESC pMemDesc, LwU32 srcOffset, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredCertrxSrm", "_hdcp22FbDmaRead");
static FLCN_STATUS _hdcp22Srm1ReadVRLs(LwU8 *pSrmBuf, PRM_FLCN_MEM_DESC pSrmDmaDesc, LwU32 *pFbOff, LwU32 *pLength, LwU32 *pDoneB)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredCertrxSrm", "_hdcp22Srm1ReadVRLs");

/* ------------------------ External definations----------------------------- */
/* ------------------------ Private Functions ------------------------------ */
/*
 * _hdcp22ConsumeAndReadFromFb
 *
 * @brief Used in SRM to Copy from Src to Dest. Both are in DMEM, but if the
 *        required number of LwU8s is not available in Src, this copies 256
 *        LwU8s into Src.
 *
 * @param[in]     pFbDmaDesc    Pointer to FB DMA
 * @param[in/out] pFbOff        Pointer to next fb offset to be used for accessing data in FB
 * @param[in/out] pTmpBuf       Source that holds the data. 256 LwU8 aligned
 * @param[in/out] pDoneB        Offset into Source until which LwU8s are consumed.
 *                              Incase 256-pDoneB is less than required LwU8s, data
 *                              is pulled from FB and fbOff is updated.
 * @param[in/out] pDest         Destination
 * @param[in]     destSize      Number of required LwU8s
 */
static FLCN_STATUS
_hdcp22ConsumeAndReadFromFb
(
    PRM_FLCN_MEM_DESC   pFbDmaDesc,
    LwU32              *pFbOff,
    LwU8               *pTmpBuf,
    LwU32              *pDoneB,
    LwU8               *pDest,
    LwU32               destSize
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 leftB = HDCP22_MAX_DMA_SIZE - ((*pDoneB) % HDCP22_MAX_DMA_SIZE);

    if (leftB < destSize)
    {
        if (pDest)
        {
            hdcpmemcpy(pDest, &(pTmpBuf[*pDoneB]), leftB);
        }

        destSize -= leftB;
        (*pDoneB) += leftB;

        //
        // Current max SRM DMA transfer size is 384Bytes(signature) which is 2
        // _hdcp22FbDmaReads worst case.
        //
        do
        {
            CHECK_STATUS(_hdcp22FbDmaRead(&pTmpBuf[*pFbOff], pFbDmaDesc, *pFbOff, HDCP22_MAX_DMA_SIZE));
            *pFbOff += HDCP22_MAX_DMA_SIZE;

            if (destSize <= HDCP22_MAX_DMA_SIZE)
            {
                if (pDest)
                {
                    hdcpmemcpy(&(pDest[leftB]), pTmpBuf, destSize);
                }

                (*pDoneB) += destSize;
                break;
            }
            else
            {
                if (pDest)
                {
                    hdcpmemcpy(&(pDest[leftB]), pTmpBuf, HDCP22_MAX_DMA_SIZE);
                }

                destSize -= HDCP22_MAX_DMA_SIZE;
                leftB += HDCP22_MAX_DMA_SIZE;
                (*pDoneB) += HDCP22_MAX_DMA_SIZE;
            }
        } while (destSize);
    }
    else
    {
        if (pDest)
        {
            hdcpmemcpy(pDest, &(pTmpBuf[*pDoneB]), destSize);
        }

        (*pDoneB) += destSize;
    }

label_return:
    return status;
}

/*
 * @brief Handles the Data transfer between FB and DMEM.
 * @param[in]     pDest      Destination in DMEM.
 * @param[in]     pMemDesc   Pointer to transfer memory descriptor
 * @param[in/out] srcOffset  Current fb offset to be used for accessing data in
 *                           FB
 * @param[in]     size       Number of required LwU8s.
 */
static FLCN_STATUS
_hdcp22FbDmaRead
(
    LwU8               *pDest,
    PRM_FLCN_MEM_DESC   pMemDesc,
    LwU32               srcOffset,
    LwU32               size
)
{
    if (!LW_IS_ALIGNED((LwUPtr)pDest, DMA_MIN_READ_ALIGNMENT) ||
        !LW_IS_ALIGNED(srcOffset, DMA_MIN_READ_ALIGNMENT) ||
        !LW_IS_ALIGNED(size, DMA_MIN_READ_ALIGNMENT))
    {
        return FLCN_ERR_DMA_ALIGN;
    }

#if UPROC_RISCV
    //
    // GSP all global datas located at FB, and no need to use DMA transfer.
    // TODO (Bug 200417270)
    // - syslib needs to provide API that can get VA of data at FB.
    // - Revisit that put all hdcp22 global datas on DMEM instead FB considering security.
    //
    LwU64 srcVAddr = engineFBGPA_FULL_VA_BASE +
        (((LwU64)pMemDesc->address.hi << 32) | pMemDesc->address.lo) +
        srcOffset;

    hdcpmemcpy((void *)pDest, (void *)srcVAddr, size);

    return FLCN_OK;
#else
    LwU32 offset = 0;
    LwU32 dmaSize;

    while (size > 0)
    {
        dmaSize = LW_MIN(size, HDCP22_DMA_BUFFER_SIZE);

        if (FLCN_OK != hdcpDmaRead(pDest + offset, pMemDesc, srcOffset + offset, 
                                   dmaSize))
        {
            break;
        }

        size -= dmaSize;
        offset += dmaSize;
    }

    return (size != 0) ? FLCN_ERROR : FLCN_OK;
#endif // !UPROC_RISCV
}

/*
 * @brief Handles Read SRM1 VRLs to buffer
 * @param[in]   pSrmBuf     Destination in DMEM.
 * @param[in]   pSrmDmaDesc Pointer to transfer memory descriptor
 * @param[in]   pFbOff      Pointer to FB offset
 * @param[in]   pLength     Pointer to length var.
 * @param[in]   pDoneB      Pointer to doneB var.
 *
 * @return     FLCN_STATUS  FLCN_OK when succeed else error.
 */
static FLCN_STATUS
_hdcp22Srm1ReadVRLs
(
    LwU8               *pSrmBuf,
    PRM_FLCN_MEM_DESC   pSrmDmaDesc,
    LwU32              *pFbOff,
    LwU32              *pLength,
    LwU32              *pDoneB
)
{
    FLCN_STATUS status      = FLCN_OK;
    LwU32       noOfDevices = 0;
    LwU8        tmp     = 0;

    // Process VRLs.
    while (*pLength)
    {
        // noOfDevices.
        CHECK_STATUS(_hdcp22ConsumeAndReadFromFb(pSrmDmaDesc, pFbOff, pSrmBuf,
                                                 pDoneB, &tmp, 1));
        noOfDevices = DRF_VAL(_HDCP22, _SRM_1X_BITS, _NO_OF_DEVICES, tmp);
        (*pLength) -= 1;

        if ((*pLength) < (noOfDevices * HDCP22_SIZE_RECV_ID_8))
        {
            status = FLCN_ERR_HDCP_ILWALID_SRM;
            goto label_return;
        }

        // DeviceIds.
        CHECK_STATUS(_hdcp22ConsumeAndReadFromFb(pSrmDmaDesc, pFbOff, pSrmBuf,
                                                 pDoneB,  NULL,
                                                 noOfDevices * HDCP22_SIZE_RECV_ID_8));
        (*pLength) -= noOfDevices * HDCP22_SIZE_RECV_ID_8;
    }

    if ((*pLength) != 0)
    {
        status = FLCN_ERR_HDCP_ILWALID_SRM;
    }

label_return:
    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*
 * hdcp22wiredSrmRead
 *
 * @brief Read HDCP2X SRM from FB to buffer.
 * @param[in]  pSrmDmaDesc      Pointer to SRM DMA descriptor.
 * @param[in]  pSrmBuf          Pointer to SRM buffer.
 * @param[in]  pTotalSrmSize    Pointer to whole SRM size.
 *
 * @return     FLCN_STATUS      FLCN_OK when succeed else error.
 */
FLCN_STATUS
hdcp22wiredSrmRead
(
    PRM_FLCN_MEM_DESC   pSrmDmaDesc,
    LwU8               *pSrmBuf,
    LwU32              *pTotalSrmSize
)
{
    FLCN_STATUS status  = FLCN_OK;
    LwU32       fbOff   = 0;
    LwU32       doneB   = 0;
    LwU32       doneB2  = 0;
    LwU32       srmIndx = 0;
    LwU32       length  = 0;
    LwU8        tmpBuf[4];
    LwU32       tmp     = 0;
    LwU8        srmGen  = 0;
    LwU32       noOfDevices = 0;
    LwU8        srmHdcp2Indicator = 0;

    if (DMA_ADDR_IS_ZERO(pSrmDmaDesc) || !pSrmBuf || !pTotalSrmSize)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto label_return;
    }

    *pTotalSrmSize = 0;

    do
    {
        // The genreation number starts from 1 and increase sequentially.
        srmIndx += 1;

        if (srmIndx == 1)
        {
            // The 1st generation SRM.

            // The 1st word for SRM version, indicator.
            CHECK_STATUS(_hdcp22FbDmaRead(pSrmBuf, pSrmDmaDesc, 0, HDCP22_MAX_DMA_SIZE));
            fbOff = HDCP22_MAX_DMA_SIZE;
            doneB += 4;

            tmp = *((LwU32 *)pSrmBuf);
            swapEndianness(&tmp, &tmp, sizeof(LwU32));
            srmHdcp2Indicator = DRF_VAL(_HDCP22,_SRM_HEADER_BITS,_HDCP2_IND, tmp);
            *pTotalSrmSize = 4;

            // Read VRL length and generations
            tmp = (pSrmBuf[doneB] << 24) | (pSrmBuf[doneB+1] << 16) | (pSrmBuf[doneB+2] << 8) | (pSrmBuf[doneB+3]);
            length = DRF_VAL(_HDCP22, _SRM_1X_BITS, _LENGTH, tmp);
            srmGen = DRF_VAL(_HDCP22, _SRM_1X_BITS, _GENERATION, tmp);
            doneB += 4;
            *pTotalSrmSize += 4;

            // Check if exceed maximum size can support before copying from FB.
            if ((length + 4) > HDCP22_SIZE_SRM_MAX)
            {
                status = FLCN_ERR_NOT_SUPPORTED;
                goto label_return;
            }

            if (srmHdcp2Indicator)
            {
                // Check for minimim length needed
                if (length < HDCP22_SRM_2X_FIRST_GEN_MIN_LEN)
                {
                    status = FLCN_ERR_HDCP_ILWALID_SRM;
                    goto label_return;
                }

                // Read number of devices.
                noOfDevices = ((pSrmBuf[9] & 0xC0) >> 6) | (pSrmBuf[8] << 8);
                doneB += 4;
                length -= 4;
                *pTotalSrmSize += 4;

                // Subtract signature and size of length field itself.
                length -= (3 + HDCP22_SRM_SIGNATURE_SIZE);
                if (length != (noOfDevices * HDCP22_SIZE_RECV_ID_8))
                {
                    status = FLCN_ERR_HDCP_ILWALID_SRM;
                    goto label_return;
                }

                *pTotalSrmSize += noOfDevices * HDCP22_SIZE_RECV_ID_8;

                // DeviceIds.
                CHECK_STATUS(_hdcp22ConsumeAndReadFromFb(pSrmDmaDesc, &fbOff, pSrmBuf,
                                                         &doneB,  NULL,
                                                         noOfDevices * HDCP22_SIZE_RECV_ID_8));
            }
            else
            {
                if (length < HDCP22_SRM_1X_FIRST_GEN_MIN_LEN)
                {
                    status = FLCN_ERR_HDCP_ILWALID_SRM;
                    goto label_return;
                }

                // Subtract signature and size of length field itself.
                length -= (3 + HDCP22_SRM_DSA_SIGNATURE_SIZE);

                // Update total length first.
                *pTotalSrmSize += length;

                CHECK_STATUS(_hdcp22Srm1ReadVRLs(pSrmBuf, pSrmDmaDesc, &fbOff, &length, &doneB));
            }
        }
        else
        {
            // Following generations.

            if (srmHdcp2Indicator)
            {
                // Read Length for next gen
                doneB2 = doneB;
                CHECK_STATUS(_hdcp22ConsumeAndReadFromFb(pSrmDmaDesc, &fbOff, pSrmBuf, &doneB2, (LwU8*)tmpBuf, 4));
                tmp = (tmpBuf[doneB] << 24) | (tmpBuf[doneB+1] << 16) | (tmpBuf[doneB+2] << 8) | (tmpBuf[doneB+3]);
                doneB += 4;

                length      = DRF_VAL(_HDCP22, _SRM_2X_NEXT_GEN, _LENGTH, tmp);
                noOfDevices = DRF_VAL(_HDCP22, _SRM_2X_NEXT_GEN, _NO_OF_DEVICES, tmp);
                *pTotalSrmSize += 4;

                if (length < HDCP22_SRM_2X_NEXT_GEN_MIN_LEN)
                {
                    status = FLCN_ERR_HDCP_ILWALID_SRM;
                    goto label_return;
                }

                // Check if exceed maximum size can support before copying from FB.
                if ((*pTotalSrmSize + length) > HDCP22_SIZE_SRM_MAX)
                {
                    status = FLCN_ERR_NOT_SUPPORTED;
                    goto label_return;
                }

                length -= (4 + HDCP22_SRM_SIGNATURE_SIZE);
                if (length != (noOfDevices * HDCP22_SIZE_RECV_ID_8))
                {
                    status = FLCN_ERR_HDCP_ILWALID_SRM;
                    goto label_return;
                }

                *pTotalSrmSize += noOfDevices * HDCP22_SIZE_RECV_ID_8;

                // DeviceIds.
                CHECK_STATUS(_hdcp22ConsumeAndReadFromFb(pSrmDmaDesc, &fbOff, pSrmBuf,
                                                         &doneB,  NULL,
                                                         noOfDevices * HDCP22_SIZE_RECV_ID_8));
            }
            else
            {
                doneB2 = doneB;
                CHECK_STATUS(_hdcp22ConsumeAndReadFromFb(pSrmDmaDesc, &fbOff, pSrmBuf, &doneB2, (LwU8*)tmpBuf, 2));

                // Read Length for next gen, number of devices.
                tmp = (pSrmBuf[doneB] << 8) | pSrmBuf[doneB+1];
                doneB += 2;

                length = DRF_VAL(_HDCP22, _SRM_1X_NEXT_GEN, _LENGTH, tmp);
                *pTotalSrmSize += (length - HDCP22_SRM_DSA_SIGNATURE_SIZE);

                // Check if exceed maximum size can support before copying from FB.
                if ((*pTotalSrmSize + HDCP22_SRM_DSA_SIGNATURE_SIZE) > HDCP22_SIZE_SRM_MAX)
                {
                    status = FLCN_ERR_NOT_SUPPORTED;
                    goto label_return;
                }

                if (length < 2 + HDCP22_SRM_DSA_SIGNATURE_SIZE)
                {
                    status = FLCN_ERR_HDCP_ILWALID_SRM;
                    goto label_return;
                }

                length -= (2 + HDCP22_SRM_DSA_SIGNATURE_SIZE);

                CHECK_STATUS(_hdcp22Srm1ReadVRLs(pSrmBuf, pSrmDmaDesc, &fbOff, &length, &doneB));
            }
        }

        // SRM signature.
        if (srmHdcp2Indicator)
        {
            CHECK_STATUS(_hdcp22ConsumeAndReadFromFb(pSrmDmaDesc, &fbOff, pSrmBuf,
                                                     &doneB, NULL,
                                                     HDCP22_SRM_SIGNATURE_SIZE));
            *pTotalSrmSize += HDCP22_SRM_SIGNATURE_SIZE;
        }
        else
        {
            CHECK_STATUS(_hdcp22ConsumeAndReadFromFb(pSrmDmaDesc, &fbOff, pSrmBuf,
                                                     &doneB, NULL,
                                                     HDCP22_SRM_DSA_SIGNATURE_SIZE));
            *pTotalSrmSize += HDCP22_SRM_DSA_SIGNATURE_SIZE;
        }
    } while (srmIndx < srmGen);

label_return:

    return status;
}

/*
 * hdcp22RevocationCheck
 *
 * @brief Does SRM validation and receiver ID revocation check for both HDCP1.x
 *        2.x devices. Also supports older and newer version of SRM
 *
 * @param[in]  pKsvList     List of KSVs to be verified.
 * @param[in]  noOfKsvs     Number of Ksvs in above list
 * @param[in]  pSrmDmaDesc  Pointer to SRM DMA descriptor
 */
FLCN_STATUS
hdcp22RevocationCheck
(
    LwU8                *pKsvList,
    LwU32                noOfKsvs,
    PRM_FLCN_MEM_DESC    pSrmDmaDesc
)
{
    LwU32 ctlOptions = 0;

    ctlOptions = FLD_SET_DRF(_FLCN, _HDCP_CTL_OPTIONS, _KEY_TYPE, _PROD, ctlOptions);

    return hdcp22wiredHandleSrmRevocate(pKsvList, noOfKsvs, pSrmDmaDesc, ctlOptions);
}

/*
 *  @brief:This function does below
 *      > Reads certificate, rrx, receiver caps
 *      > Verifies certificate
 *      > SRM revocation.
 *
 *  @param[in]  pSession         Pointer to HDCP22 active session.
 *  @param[in]  pCertRx          Pointer to HDCP22_CERTIFICATE
 *
 *  @returns    FLCN_STATUS      FLCN_OK on successfull exelwtion
 *                               Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22HandleVerifyCertRx
(
    HDCP22_SESSION     *pSession,
    HDCP22_CERTIFICATE *pCertRx
)
{
    FLCN_STATUS          status = FLCN_OK;
    LwBool               bIsAuxI2cDettached = LW_FALSE;

    //
    // TODO: To move this function to a appropriate file or we can rename this 
    // file to hdcp22wired_rxcert_srm.c.
    //

    if (pSession == NULL)
    {
        return FLCN_ERROR;
    }

    hdcpmemset(pCertRx, 0 , sizeof(HDCP22_CERTIFICATE));

    hdcpAttachAndLoadOverlay(HDCP22WIRED_AKE);
    // Read the Certificate form SINK
    status = hdcp22RecvAkeSendCert(pSession, pCertRx);
    hdcpDetachOverlay(HDCP22WIRED_AKE);
    CHECK_STATUS(status);

    hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_FALSE);
    bIsAuxI2cDettached = LW_TRUE;

    // Verify Certificate
    status = hdcp22wiredVerifyCertificate(pCertRx);
    CHECK_STATUS(status);

    if (!DMA_ADDR_IS_ZERO(&pSession->srmDma))
    {
        CHECK_STATUS(hdcp22RevocationCheck(&pCertRx->id[0],
                                           1, 
                                           &pSession->srmDma));
    }

    hdcpmemcpy(pSession->sesVariablesAke.receiverId, &pCertRx->id[0],
               HDCP22_SIZE_RECV_ID_8);

    hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_TRUE);
    bIsAuxI2cDettached = LW_FALSE;

label_return:

    if (bIsAuxI2cDettached)
    {
        hdcp22ConfigAuxI2cOverlays(pSession->sorProtocol, LW_TRUE);
    }
    return status;
};

