/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acr_verify_ls_signature_tu10x.c
 */

//
// Includes
//
#include "acr.h"
#include "dev_gc6_island.h"
#include "dev_fb.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"

// Buffer to load ucode while callwlating DMHASH
#define ACR_UCODE_LOAD_BUF_SIZE        ACR_FALCON_BLOCK_SIZE_IN_BYTES
LwU8 g_UcodeLoadBuf[2][ACR_UCODE_LOAD_BUF_SIZE]                                ATTR_ALIGNED(ACR_FALCON_BLOCK_SIZE_IN_BYTES) ATTR_OVLY(".data");
LwU8 g_LsSigGrpLoadBuf[ACR_LS_SIG_GRP_LOAD_BUF_SIZE]                           ATTR_ALIGNED(ACR_FALCON_BLOCK_SIZE_IN_BYTES) ATTR_OVLY(".data");
LwU8 g_dmHash[ACR_AES128_DMH_SIZE_IN_BYTES]                                    ATTR_ALIGNED(ACR_AES128_DMH_SIZE_IN_BYTES)   ATTR_OVLY(".data");
LwU8 g_dmHashForGrp[ENUM_LS_SIG_GRP_ID_TOTAL][ACR_AES128_DMH_SIZE_IN_BYTES]    ATTR_ALIGNED(ACR_AES128_DMH_SIZE_IN_BYTES)   ATTR_OVLY(".data");
LwU8 g_kdfSalt[ACR_AES128_SIG_SIZE_IN_BYTES]                                   ATTR_ALIGNED(ACR_AES128_SIG_SIZE_IN_BYTES)   ATTR_OVLY(".data") =
{
    0xB6,0xC2,0x31,0xE9,0x03,0xB2,0x77,0xD7,0x0E,0x32,0xA0,0x69,0x8F,0x4E,0x80,0x62
};

#ifdef AHESASC
ACR_FLCN_BL_CMDLINE_ARGS  g_blCmdLineArgs       ATTR_OVLY(".data") ATTR_ALIGNED(ACR_FLCN_BL_CMDLINE_ALIGN);
ACR_SCRUB_STATE           g_scrubState          ATTR_OVLY(".data") ATTR_ALIGNED(ACR_SCRUB_ZERO_BUF_ALIGN);
#endif

//
// Extern Variables
//
extern LwU8             g_pWprHeader[LSF_WPR_HEADERS_TOTAL_SIZE_MAX];
extern RM_FLCN_ACR_DESC g_desc;
extern ACR_DMA_PROP     g_dmaProp;
extern LwBool           g_bIsDebug;

ACR_STATUS _acrCallwlateLsGrpSigHash(LwU8 *pUcode, LwU32 startPc, LwU32  sizeOfUcode,
                                     LwU32 totalLsGrpEntries, ACR_LS_SIG_GRP_ENTRY *pLsSigGrpEntries) ATTR_OVLY(OVL_NAME);

/*!
 * Callwlate LS signature for overlay groups. This function takes an ucode block and its PC as inputs.
 * It then walks through list of overlays per group and check if any part of ucode block falls within any range
 * and if yes, uses the code to callwlate the DMHASH for that group.
 *
 * @param[in] pUcode            Block of ucode which will be checked for LS Grp
 * @param[in] startPc           Starting PC of the block provided
 * @param[in] sizeOfUcode       Size of the given block
 * @param[in] totalLsGrpEntries Total LS grp entries found for this falcon's ucode
 * @param[in] pLsSigGrpEntries  Pointer to start of LS Grp entries
 */
ACR_STATUS _acrCallwlateLsGrpSigHash
(
    LwU8                 *pUcode,
    LwU32                startPc,
    LwU32                sizeOfUcode,
    LwU32                totalLsGrpEntries,
    ACR_LS_SIG_GRP_ENTRY *pLsSigGrpEntries
)
{
    ACR_STATUS    status        = ACR_OK;
    LwU32         i             = 0;
    LwU32         j             = 0;
    LwBool        bFound        = LW_FALSE;
    LwU32         nStart        = 0;
    LwU32         nEnd          = 0;
    LwU32         endPc         = startPc + sizeOfUcode;
    LwU8          *pU           = 0;

    // Sanity checks
    if ((pUcode == 0) || (!sizeOfUcode) || (pLsSigGrpEntries == 0))
    {
        status = ACR_ERROR_UNEXPECTED_ARGS;
        goto label_return;
    }

    // Check if this block needs to be accounted for
    for (i = 0; i < totalLsGrpEntries; i++)
    {
        bFound = LW_FALSE;

        // StartPc is within this range
        if ((startPc >= pLsSigGrpEntries[i].startPc) &&
            (startPc < pLsSigGrpEntries[i].endPc))
        {
            bFound = LW_TRUE;
            nStart = startPc;

            // Get the end
            if (endPc < pLsSigGrpEntries[i].endPc)
                nEnd = endPc;
            else
                nEnd = pLsSigGrpEntries[i].endPc;

        }
        // End PC is within the range
        else if ((endPc > pLsSigGrpEntries[i].startPc) &&
                 (endPc <= pLsSigGrpEntries[i].endPc))
        {
            bFound = LW_TRUE;
            nEnd = endPc;

            // Get the start
            nStart = pLsSigGrpEntries[i].startPc;

        }
        // Both start and end are outside of range
        else if ((startPc <= pLsSigGrpEntries[i].startPc) &&
                  (endPc >= pLsSigGrpEntries[i].endPc))
        {
            bFound = LW_TRUE;
            nStart = pLsSigGrpEntries[i].startPc;
            nEnd   = pLsSigGrpEntries[i].endPc;

        }

        if (bFound == LW_TRUE)
        {
            // Ensure GRPID is within expected range
            if ((pLsSigGrpEntries[i].grpId == ENUM_LS_SIG_GRP_ID_ILWALID) ||
                ((pLsSigGrpEntries[i].grpId > ENUM_LS_SIG_GRP_ID_TOTAL) && (pLsSigGrpEntries[i].grpId != ENUM_LS_SIG_GRP_ID_ALL)))
            {
                status = ACR_ERROR_ILWALID_LS_SIG_GRP_ID;
                goto label_return;
            }

            pU = &(pUcode[nStart - startPc]);
            if (pLsSigGrpEntries[i].grpId == ENUM_LS_SIG_GRP_ID_ALL)
            {
                for (j = ENUM_LS_SIG_GRP_ID_1; j <= ENUM_LS_SIG_GRP_ID_TOTAL; j++)
                {
                    // g_dmHashForGrp starts from 0th index
                    acrCallwlateDmhash_HAL(pAcr, g_dmHashForGrp[(j - 1)], pU, (nEnd - nStart));
                }
            }
            else
            {
                // g_dmHashForGrp starts from 0th index
                acrCallwlateDmhash_HAL(pAcr, g_dmHashForGrp[(pLsSigGrpEntries[i].grpId - 1)], pU, (nEnd - nStart));
            }
        }
    }

label_return:
    return status;
}


/*!
 * Callwlate DMhash with the given buffer
 */
void acrCallwlateDmhash_TU10X
(
    LwU8           *pHash,
    LwU8           *pData,
    LwU32          size
)
{
    LwU32  doneSize = 0;
    LwU8   *pKey;

    while (doneSize < size)
    {
        pKey = (LwU8 *)&(pData[doneSize]);

        // H_i = H_{i-1} XOR encrypt(key = m_i, pMessage = H_{i-1})
        falc_scp_trap(TC_INFINITY);
        falc_trapped_dmwrite(falc_sethi_i((LwU32)pKey, SCP_R2));
        falc_scp_chmod(0x1, SCP_R2);                                            // Only allow secure keyable access i.e. only bit0 set
        falc_dmwait();
        falc_trapped_dmwrite(falc_sethi_i((LwU32)pHash, SCP_R3));
        falc_dmwait();
        falc_scp_key(SCP_R2);
        falc_scp_encrypt(SCP_R3, SCP_R4);
        falc_scp_xor(SCP_R4, SCP_R3);
        falc_trapped_dmread(falc_sethi_i((LwU32)pHash, SCP_R3));
        falc_dmwait();
        falc_scp_trap(TC_DISABLE_CCR);

        doneSize += 16;
    }
}

/*!
 * Verify LS ucode signature
 */
ACR_STATUS
acrVerifySignature_TU10X
(
    LwU8            *pSignature,
    LwU32           falconId,
    LwU32           binarysize,
    LwU32           binOffset,
    PLSF_LSB_HEADER pLsfHeader,
    LwBool          bIsUcode
)
{
    ACR_STATUS      status                                = ACR_OK;
    LwU32           doneSize                              = 0;
    LwU32           actSize                               = 0;
    LwU32           doneBufInd                            = 0;
    LwU32           pendBufInd                            = 1;
    LwU32           tmp                                   = 0;
    LwU32           maxLsSigGrpEntriesPossibleInLwrrUcode = 0;
    LwU32           *pTmp                                 = 0;
    LwU32           i                                     = 0;
    LwU32           numLsSigGrpEntriesInLwrrUcode         = 0;
    LwBool          bLsSigGrp                             = LW_FALSE;
    ACR_LS_SIG_GRP_ENTRY  *pLsSigGrpEntry                 = 0;
    ACR_LS_SIG_GRP_HEADER *pLsSigGrpHeader                = 0;

    // Reset the global hash
    acrlibMemset_HAL(pAcrlib, g_dmHash, 0x0, 16);

    // Reset the hash for LS sig groups
    for (i = 0; i < ENUM_LS_SIG_GRP_ID_TOTAL; i++)
    {
        acrlibMemset_HAL(pAcrlib, g_dmHashForGrp[i], 0x0, 16);
    }

    if (binarysize > ACR_UCODE_LOAD_BUF_SIZE)
        actSize = ACR_UCODE_LOAD_BUF_SIZE;
    else
        actSize = binarysize;

    if (bIsUcode == LW_TRUE)
    {
        // Read LS_SIG_GROUP details. It is last block of ucode

        //
        // Individual falcons will place the LS group details at the end of their ucode in the following format.
        //
        //  ------ Ucode
        //  ------ LS Grp block start (1 block at present)
        //  ------ 3 MAGIC DWORDS to identify the presence of LS group for this ucode
        //  ------ 4th DWORD gives the total number of entries
        //  ------ 5th DWORD is where the LS GRP entry starts
        //  ------ --- LS GRP Entry has 3 DWORDS.
        //  ------ --- --- Start, end and grp ID
        //
        // |-----------------------------------------------|
        // | |
        // | Ucode |
        // | |
        // | |
        // |-- |-----------------------------------------------| --|
        // | | Magic Pattern #1 | |
        // | | Magic Pattern #2 | |
        // | | Magic Pattern #3 | |
        // | | Number of sig grp entries | |
        // | | Start PC | |
        // LS Sig Grp Entry #1 --| | End PC | |
        // | | Grp ID | |-- LS Sig Grp Block
        // |-- |-----------------------------------------------| |
        // | | Start PC | |
        // LS Sig Grp Entry #2 --| | End PC | |
        // | | Grp ID | |
        // |-- |-----------------------------------------------| |
        // ... |
        // |
        // |-- |-----------------------------------------------| |
        // | | Start PC | |
        // LS Sig Grp Entry #N --| | End PC | |
        // | | Grp ID | |
        // |-- |-----------------------------------------------| --|
        //

        if (binarysize > ACR_LS_SIG_GRP_LOAD_BUF_SIZE)
        {
            //
            // The size of LS grp depends on the size of g_LsSigGrpLoadBuf buffer. At present, code doesn't expect more than 20
            // entries. (3 MAGIC + 1 NoofEntries + (20 * 3)) -> 64 DWORDS -> 256 bytes
            //
            acrIssueDma_HAL(pAcr, (void *)g_LsSigGrpLoadBuf, LW_FALSE,  (binOffset + binarysize - ACR_LS_SIG_GRP_LOAD_BUF_SIZE),
                ACR_LS_SIG_GRP_LOAD_BUF_SIZE, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp);

            pLsSigGrpHeader = (ACR_LS_SIG_GRP_HEADER *) g_LsSigGrpLoadBuf;

            //
            // Compare the MAGIC pattern and number of elements.
            // MAGIC pattern is temporary solution. In future, we would like to pass the presence of LS group details
            // via LSB header.
            //
            if ((pLsSigGrpHeader->magicPattern1 == ACR_LS_SIG_GROUP_MAGIC_PATTERN_1) &&
                (pLsSigGrpHeader->magicPattern2 == ACR_LS_SIG_GROUP_MAGIC_PATTERN_2) &&
                (pLsSigGrpHeader->magicPattern3 == ACR_LS_SIG_GROUP_MAGIC_PATTERN_3) &&
                (pLsSigGrpHeader->numLsSigGrps > 0))
            {

                bLsSigGrp                     = LW_TRUE;
                pLsSigGrpEntry                = (ACR_LS_SIG_GRP_ENTRY *) ((LwU32) g_LsSigGrpLoadBuf + sizeof(ACR_LS_SIG_GRP_HEADER));
                numLsSigGrpEntriesInLwrrUcode = pLsSigGrpHeader->numLsSigGrps;

                // Check whether numLsSigGrpEntriesInLwrrUcode  provided is a sane value
                maxLsSigGrpEntriesPossibleInLwrrUcode  = ((ACR_LS_SIG_GRP_LOAD_BUF_SIZE - (sizeof(ACR_LS_SIG_GRP_HEADER))) / sizeof(ACR_LS_SIG_GRP_ENTRY));
                if (numLsSigGrpEntriesInLwrrUcode > maxLsSigGrpEntriesPossibleInLwrrUcode )
                {
                    //Invalid number of entries, error
                    status = ACR_ERROR_ILWALID_LS_SIG_GRP_ENTRY_COUNT;
                    goto label_return;
                }

            }
        }
    }

    acrIssueDma_HAL(pAcr, (void *)g_UcodeLoadBuf[pendBufInd], LW_FALSE,  binOffset,
        actSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &g_dmaProp);

    do
    {
        falc_dmwait();
        doneSize += actSize;

        // Swap the pend and done buffer indexes
        tmp = doneBufInd;
        doneBufInd = pendBufInd;
        pendBufInd = tmp;

        // Save the old actualSize
        tmp = actSize;

        if (doneSize < binarysize)
        {
            actSize = binarysize - doneSize;
            if (actSize > ACR_UCODE_LOAD_BUF_SIZE)
                actSize = ACR_UCODE_LOAD_BUF_SIZE;

            // Schedule one more for pendBufInd
            acrIssueDma_HAL(pAcr, (void *)g_UcodeLoadBuf[pendBufInd], LW_FALSE,  binOffset + doneSize,
                actSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_NO, &g_dmaProp);
        }

        // Handle the done buffer now
        acrCallwlateDmhash_HAL(pAcr, g_dmHash, g_UcodeLoadBuf[doneBufInd], tmp);
        if (bLsSigGrp == LW_TRUE)
        {
            //
            // Bootloader resides first and not accounted in LS sig grp callwlation
            // This check will be false only for first block
            //
            if ((doneSize - tmp) >= pLsfHeader->blCodeSize)
            {
                // Use the previously completed buffer to callwlate LS signature for overlay groups
                status = _acrCallwlateLsGrpSigHash(g_UcodeLoadBuf[doneBufInd], (doneSize - tmp - pLsfHeader->blCodeSize), tmp, numLsSigGrpEntriesInLwrrUcode , pLsSigGrpEntry);
                if (status != ACR_OK)
                {
                    goto label_return;
                }
            }
        }
    } while (doneSize < binarysize);

    // Check if versioning is supported and include it in signature callwlation
    if (pLsfHeader->signature.bSupportsVersioning)
    {
        // Using UcodeLoadBuf[0] for version info packing
        acrlibMemset_HAL(pAcrlib, g_UcodeLoadBuf[0], 0x0, ACR_UCODE_LOAD_BUF_SIZE);

        // Check to make sure size is as expected
        ct_assert(ACR_UCODE_LOAD_BUF_SIZE >= (LW_ALIGN_UP((0x4 + (LSF_FALCON_DEPMAP_SIZE * 4 * 2)), 16)));
        if (pLsfHeader->signature.depMapCount > LSF_FALCON_DEPMAP_SIZE)
        {
            status = ACR_ERROR_TOO_MANY_UCODE_DEPENDENCIES;
            // Add the failing falconId in mailbox to ease debug during bringup
            acrWriteFailingFalconIdToMailbox_HAL(pAcr, falconId);
            goto label_return;
        }

        pTmp = (LwU32 *)g_UcodeLoadBuf[0];

        // Version number goes first
        pTmp[0] = pLsfHeader->signature.version;
        ++pTmp;

        // Now the map
        for (i=0; i< pLsfHeader->signature.depMapCount; i++)
        {
           pTmp[0] = ((LwU32*)pLsfHeader->signature.depMap)[i*2];
           pTmp[1] = ((LwU32*)pLsfHeader->signature.depMap)[(i*2)+1];
           pTmp += 2;
        }

        // Total size of version + map
        tmp = 0x4 + (pLsfHeader->signature.depMapCount * 4 * 2);
        tmp = LW_ALIGN_UP(tmp, 16);

        // Callwlate the hash now
        acrCallwlateDmhash_HAL(pAcr, g_dmHash, g_UcodeLoadBuf[0], tmp);
    }

#ifdef AMPERE_PROTECTED_MEMORY

    // Add the LS hash of ucode or data to the measured state
    if ((status = acrMeasureDmhash_HAL(pAcr, g_dmHash, falconId, bIsUcode)) != ACR_OK)
    {
        goto label_return;
    }
#endif

    if ((status = acrDeriveLsVerifKeyAndEncryptDmHash_HAL(pAcr, g_UcodeLoadBuf[0], g_dmHash, falconId, LW_TRUE)) != ACR_OK)
    {
        goto label_return;
    }

    //
    // g_dmHash has the signature now
    // compare with given signature
    //

    if(acrMemcmp(g_dmHash, pSignature, 16))
    {
        // If acrMemcmp returns non-zero value, then it means that sigantures mismatch
        status = ACR_ERROR_LS_SIG_VERIF_FAIL;
        // Add the failing falconId in mailbox to ease debug during bringup
        acrWriteFailingFalconIdToMailbox_HAL(pAcr, falconId);
        goto label_return;
    }

    if (bLsSigGrp == LW_TRUE)
    {
        //
        // Copy LS Grp signatures. Grp ID 0 is invalid
        //
        for (i = ENUM_LS_SIG_GRP_ID_1; i <= ENUM_LS_SIG_GRP_ID_TOTAL; i++)
        {
            // g_dmHashForGrp starts from 0th index
            if (falconId == LSF_FALCON_ID_SEC2)
            {
                status = acrCopyLsGrpSigToRegsForSec2_HAL(pAcr, (LwU32 *)(g_dmHashForGrp[i - 1]), i);
                if (status != ACR_OK)
                {
                    goto label_return;
                }
            }
        }
    }

label_return:
    return status;
}

/*!
 * Encrypts the DMHASH
 * Expects the inputs to be atleast 16B aligned
 */
ACR_STATUS acrDeriveLsVerifKeyAndEncryptDmHash_TU10X
(
    LwU8   *pSaltBuf,  //16B aligned
    LwU8   *pDmHash,   //16B aligned
    LwU32  falconId,
    LwBool bUseFalconId
)
{
    // Check for alignment
    if ( (((LwU32)pSaltBuf) % ACR_AES128_SIG_SIZE_IN_BYTES) ||
         (((LwU32)pDmHash) % ACR_AES128_SIG_SIZE_IN_BYTES))
    {
        return ACR_ERROR_UNEXPECTED_ALIGNMENT;
    }

    //
    // Derive key
    // derived key = AES-ECB((g_kdfSalt XOR falconID), key)
    // Signature = AES-ECB(dmhash, derived key)
    //

    // Use pSaltBuf as tmp Buf
    acrlibMemcpy_HAL(pAcrlib, pSaltBuf, g_kdfSalt, 16);

    if (bUseFalconId == LW_TRUE)
    {
        ((LwU32*)pSaltBuf)[0] ^= (falconId);
    }

    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)(pSaltBuf), SCP_R3)); // Load XORed salt
    falc_dmwait();
    if (g_bIsDebug)
        falc_scp_secret(ACR_LS_VERIF_KEY_INDEX_DEBUG, SCP_R2);                  // Load the keys accordingly
    else
        falc_scp_secret(ACR_LS_VERIF_KEY_INDEX_PROD_TU10X_AND_LATER, SCP_R2);   // Load the keys accordingly
    falc_scp_key(SCP_R2);
    falc_scp_encrypt(SCP_R3, SCP_R4);                                       // Derived key is in R4
    falc_scp_chmod(0x1, SCP_R4);                                            // Only allow secure keyable access i.e. only bit0 set
    falc_trapped_dmwrite(falc_sethi_i((LwU32)pDmHash, SCP_R3));             // Load the hash
    falc_dmwait();
    falc_scp_key(SCP_R4);
    falc_scp_encrypt(SCP_R3, SCP_R2);                                       // Signature is in R2
    falc_trapped_dmread(falc_sethi_i((LwU32)pDmHash, SCP_R2));             // Read back the signature
    falc_dmwait();
    falc_scp_trap(TC_DISABLE_CCR);

    return ACR_OK;
}


/*!
 * At present we have only one set of registers reserved enough to hold signature for one group.
 * Only 2 DWORDS are saved and compared.
 */
ACR_STATUS
acrCopyLsGrpSigToRegsForSec2_TU10X(LwU32 *pSig, LwU32 grpId)
{
    ACR_STATUS status = ACR_OK;
    LwU32      regAddrD0 = 0;
    LwU32      regAddrD1 = 0;

    switch (grpId)
    {
        case ENUM_LS_SIG_GRP_ID_1:
            regAddrD0 = LW_PGC6_BSI_SELWRE_SCRATCH_0;
            regAddrD1 = LW_PGC6_BSI_SELWRE_SCRATCH_1;
            break;
        default:
            status = ACR_ERROR_ILWALID_LS_SIG_GRP_ID;
            goto label_return;
    }

    ACR_REG_WR32(BAR0, regAddrD0, pSig[0]);
    ACR_REG_WR32(BAR0, regAddrD1, pSig[1]);

label_return:
    return status;
}

#ifdef AHESASC
/*!
 * @brief Do sanity check on the bootloader arguments provided
 */
ACR_STATUS
acrSanityCheckBlData_TU10X
(
    LwU32              falconId,
    LwU32              blDataOffset,
    LwU32              blDataSize
)
{
    //
    // We dont expect bootloader data for falcons other than PMU, DPU, SEC2 and
    // GR falcons
    //
    if ( (blDataOffset >= g_scrubState.scrubTrack) && (blDataSize > 0))
    {
        // Bootloader data size is not expected to be more than 256 bytes
        if (blDataSize > LSF_LS_BLDATA_EXPECTED_SIZE)
        {
            return ACR_ERROR_BLDATA_SANITY;
        }

        // Read loader config
        if((acrIssueDma_HAL(pAcr, (void *)&g_blCmdLineArgs, LW_FALSE,  blDataOffset,
            blDataSize, ACR_DMA_FROM_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp))
            != blDataSize)
        {
            return ACR_ERROR_DMA_FAILURE;
        }

        // Write KNOWN HALT ASM code into RESERVED location..

        switch (falconId)
        {
            case LSF_FALCON_ID_PMU:
#ifdef DPU_PRESENT
            case LSF_FALCON_ID_DPU:
#elif GSP_PRESENT
            case LSF_FALCON_ID_GSPLITE:
#endif
            case LSF_FALCON_ID_SEC2:
            case LSF_FALCON_ID_FECS:
            case LSF_FALCON_ID_GPCCS:
                g_blCmdLineArgs.genericBlArgs.reserved[0] = ACR_FLCN_BL_RESERVED_ASM;
                break;
#ifdef FBFALCON_PRESENT
            case LSF_FALCON_ID_FBFALCON:
                // TODO: Move FbFlcn to generic bootloader. Filed bug 200406898
                g_blCmdLineArgs.legacyBlArgs.reserved = ACR_FLCN_BL_RESERVED_ASM;
                break;
#endif
            default:
                return ACR_ERROR_BLDATA_SANITY;
                break;
        }

        // Write back loader config
        if((acrIssueDma_HAL(pAcr, (void *)&g_blCmdLineArgs, LW_FALSE,  blDataOffset,
            blDataSize, ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp))
            != blDataSize)
        {
            return ACR_ERROR_DMA_FAILURE;
        }

    }

    return ACR_OK;
}

//
// Include below functions only if signature-verification is enabled.
// When RISC-V LS support is enabled, we always verify RISC-V signatures
// and so always need the below functions.
//
#if defined(ACR_SIG_VERIF) || defined(ACR_RISCV_LS)
/*
 * @brief Scrub the code and data content on error scenarios. Also write the WPR header back to FB.
 */
static
ACR_STATUS _acrScrubContentAndWriteWpr(PLSF_LSB_HEADER pLsfHeader)
{
    ACR_STATUS status = ACR_OK;

    // Scrub code
    if ((acrIssueDma_HAL(pAcr, g_scrubState.zeroBuf, LW_FALSE, pLsfHeader->ucodeOffset, pLsfHeader->ucodeSize,
       ACR_DMA_TO_FB_SCRUB, ACR_DMA_SYNC_NO, &g_dmaProp)) != pLsfHeader->ucodeSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    // Scrub data
    if ((acrIssueDma_HAL(pAcr, g_scrubState.zeroBuf, LW_FALSE, pLsfHeader->ucodeOffset + pLsfHeader->ucodeSize,
        pLsfHeader->dataSize, ACR_DMA_TO_FB_SCRUB, ACR_DMA_SYNC_NO, &g_dmaProp)) != pLsfHeader->dataSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    // Write WPR header back to FB and return...
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrWriteWprHeader_HAL(pAcr));
    return status;
}

/*!
 * @brief Fetches bin version from all WPR entries
 */
static
void _acrGetAllBilwersions(LwU32 *pFalconBilwersions)
{

    LwU32           index = 0;
    PLSF_WPR_HEADER pWprHeader;

    for (index=0; index < LSF_FALCON_ID_END; index++)
    {
        // Assign invalid Bin version to all falcons first
        pFalconBilwersions[index] = LSF_FALCON_BIN_VERSION_ILWALID;
    }

    for (index=0; index < LSF_FALCON_ID_END; index++)
    {
        pWprHeader = GET_WPR_HEADER(index);

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader->falconId, pWprHeader->lsbOffset))
        {
            break;
        }

        // Overiding bin version for falcons present in WPR header along with value
        pFalconBilwersions[pWprHeader->falconId] = pWprHeader->bilwersion;

    }
}

/*!
 * @brief Function which checks for revocation info on LS falcon and corresponding dependencies
 */
ACR_STATUS
acrCheckForLSRevocation_TU10X(LwU32 falconId, PLSF_LSB_HEADER pLsfHeader, LwU32 *pBilwersions)
{
    ACR_STATUS      status      = ACR_OK;
    LwU32           depfalconId = 0;
    LwU32           expVer      = 0;
    LwU32           i           = 0;

    // Make sure the version that we used for sig callwlation is what is present in WPRHEADER
    if (pLsfHeader->signature.version != pBilwersions[falconId])
    {
        status = ACR_ERROR_REVOCATION_CHECK_FAIL;
        goto label_return;
    }

    // Check if dependencies are passing
    for (i=0; i< pLsfHeader->signature.depMapCount; i++)
    {
        depfalconId = ((LwU32*)pLsfHeader->signature.depMap)[i*2];
        expVer      = ((LwU32*)pLsfHeader->signature.depMap)[(i*2)+1];

        //
        // Checks for revocation, invalid bin version in pBilwersions implies that
        // particular falcon is not present in WPR header.
        //
        if ((pBilwersions[depfalconId] < expVer) &&
               (pBilwersions[depfalconId] != LSF_FALCON_BIN_VERSION_ILWALID))
        {
            status = ACR_ERROR_REVOCATION_CHECK_FAIL;
            goto label_return;
        }
    }


label_return:
    return status;
}
#endif // ACR_SIG_VERIF || ACR_RISCV_LS
#endif // AHESASC

/*!
 * @brief Scrub FB with zeroes
 *
 * @param[in] wprIndex  Index into g_desc region properties holding WPR region
 */
ACR_STATUS
acrScrubUnusedWprWithZeroes_TU10X
(
    LwU32              nextKnownOffset,
    LwU32              size
)
{
#if defined (AHESASC)
    LwS32         gap     = 0;
    LwU32         i       = 0;

    if (!g_scrubState.scrubTrack)
    {
        // Zero out the buf
        for (i = 0; i < (ACR_SCRUB_ZERO_BUF_SIZE_BYTES/4); i++)
        {
            ((LwU32*)(g_scrubState.zeroBuf))[i] = 0;
        }
    }

    {
        gap = (nextKnownOffset) - (g_scrubState.scrubTrack);

        if (gap > 0)
        {
#ifndef ACR_SKIP_SCRUB
            if ((acrIssueDma_HAL(pAcr, g_scrubState.zeroBuf, LW_FALSE, g_scrubState.scrubTrack, gap,
                ACR_DMA_TO_FB_SCRUB, ACR_DMA_SYNC_NO, &g_dmaProp)) != gap)
            {
                return ACR_ERROR_DMA_FAILURE;
            }
#endif

        }

        if (gap >=  0)
        {
            (g_scrubState.scrubTrack) += gap;
            (g_scrubState.scrubTrack) += size;
        }
    }

#endif


    return ACR_OK;
}

#if defined(AHESASC)
ACR_STATUS
acrValidateSignatureAndScrubUnusedWpr_TU10X(void)
{
    ACR_STATUS status = ACR_OK;
    LwU32 index = 0;
    LwU32 wprRegionSize = 0;
    LSF_LSB_HEADER lsbHeaders[LSF_FALCON_ID_END + 1];
    PLSF_LSB_HEADER pLsfHeader = NULL;
    PLSF_WPR_HEADER pWprHeader;
    RM_FLCN_ACR_REGION_PROP *pRegionProp;

    // Read the WPR header into heap first
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadWprHeader_HAL(pAcr));

//
// Signature-verification is always enabled for RISC-V engines when
// RISC-V LS support is enabled.
//
#if defined(ACR_SIG_VERIF) || defined(ACR_RISCV_LS)
    LwU32 falconBilwersions[LSF_FALCON_ID_END];
    _acrGetAllBilwersions(falconBilwersions);
#endif
    // Sets up scrubbing
    g_scrubState.scrubTrack = 0;

    acrScrubUnusedWprWithZeroes_HAL(pAcr, 0, LW_ALIGN_UP((sizeof(LSF_WPR_HEADER) * LSF_FALCON_ID_END),
                                                         LSF_WPR_HEADER_ALIGNMENT));

    //
    // Loop through each WPR header.
    // Break the loop when we see invalid falcon ID
    //
    for (index = 0; index <= LSF_FALCON_ID_END; index++)
    {
        pWprHeader = GET_WPR_HEADER(index);

        // Check if this is the end of WPR header by checking falconID
        if (IS_FALCONID_ILWALID(pWprHeader->falconId, pWprHeader->lsbOffset))
        {
            // Scrub till the end of LS ucodes content
            if (NULL == pLsfHeader)
            {
                // ACR should get enabled if we have atleast one LSFM falcon
                return ACR_ERROR_UNREACHABLE_CODE;
            }

            //
            // Get the end address of LS ucodes within WPR region i.e. region before shared data subWPRs
            // We do not need to scrub the shared data subWprs here
            //
            pRegionProp = &(g_desc.regions.regionProps[ACR_WPR1_REGION_IDX]);
            LwU64 lsfEndAddr = LW_ALIGN_UP((((LwU64)pRegionProp->startAddress << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT)
                                            + pLsfHeader->blDataOffset + pLsfHeader->blDataSize), SUB_WPR_SIZE_ALIGNMENT);

            wprRegionSize = (LwU32)(lsfEndAddr - ((LwU64)pRegionProp->startAddress << LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT));

            // Scrub the empty WPR after last LS ucode but till Data subWPRs
            acrScrubUnusedWprWithZeroes_HAL(pAcr, wprRegionSize, 0);
            break;
        }

        // Scrub the gap between last known scrub point and this LSB header
        acrScrubUnusedWprWithZeroes_HAL(pAcr, pWprHeader->lsbOffset, LW_ALIGN_UP(sizeof(LSF_LSB_HEADER),
                                                                                 LSF_LSB_HEADER_ALIGNMENT));

        // Lets reset the status
        pLsfHeader = &(lsbHeaders[index]);

        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReadLsbHeader_HAL(pAcr, pWprHeader, pLsfHeader));

        // Scrub the gap between last known scrub point and ucode/data
        acrScrubUnusedWprWithZeroes_HAL(pAcr, pLsfHeader->ucodeOffset, pLsfHeader->ucodeSize);
        acrScrubUnusedWprWithZeroes_HAL(pAcr, (pLsfHeader->ucodeOffset + pLsfHeader->ucodeSize), pLsfHeader->dataSize);

        // We dont expect bootloader data for falcons other than PMU and DPU.
        if ((status = acrSanityCheckBlData_HAL(pAcr, pWprHeader->falconId, pLsfHeader->blDataOffset, pLsfHeader->blDataSize))!= ACR_OK)
        {
            return status;
        }

        acrScrubUnusedWprWithZeroes_HAL(pAcr, pLsfHeader->blDataOffset, pLsfHeader->blDataSize);

        if (!acrIsFalconSupported(pWprHeader->falconId))
        {
            return ACR_ERROR_FLCN_ID_NOT_FOUND;
        }

//
// Signature-verification is always enabled for RISC-V engines when
// RISC-V LS support is enabled.
//
#if defined(ACR_SIG_VERIF) || defined(ACR_RISCV_LS)
        ACR_STATUS oldstatus = ACR_OK;
        LwU32 falconId = pWprHeader->falconId;
        LwU8 *pSignature;

#ifndef ACR_SIG_VERIF
        ACR_FLCN_CONFIG flcnCfg;
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrlibGetFalconConfig_HAL(pAcrlib, falconId, LSF_FALCON_INSTANCE_DEFAULT_0, &flcnCfg));

        // If full signature-verification isn't enabled, only verify RISC-V signatures.
        if (flcnCfg.uprocType != ACR_TARGET_ENGINE_CORE_RISCV)
        {
            goto VERIF_FAIL;
        }
#endif

        // Get the ucode signature
        if (g_bIsDebug)
            pSignature = pLsfHeader->signature.dbgKeys[ACR_LS_VERIF_SIGNATURE_CODE_INDEX];
        else
            pSignature = pLsfHeader->signature.prdKeys[ACR_LS_VERIF_SIGNATURE_CODE_INDEX];

        // LWENC makefile uses LWENC0 as falcon Id for all LWENCs.
        if (falconId == LSF_FALCON_ID_LWENC1 || falconId == LSF_FALCON_ID_LWENC2)
        {
            falconId = LSF_FALCON_ID_LWENC0;
        }

        // Verify code signature
        if ((status = acrVerifySignature_HAL(pAcr, pSignature, falconId, pLsfHeader->ucodeSize,
             pLsfHeader->ucodeOffset, pLsfHeader, LW_TRUE)) != ACR_OK)
        {
            // Code sig verif failed
            goto VERIF_FAIL;
        }

        // Get the data signature
        if (g_bIsDebug)
            pSignature = pLsfHeader->signature.dbgKeys[ACR_LS_VERIF_SIGNATURE_DATA_INDEX];
        else
            pSignature = pLsfHeader->signature.prdKeys[ACR_LS_VERIF_SIGNATURE_DATA_INDEX];

        // Verify data signature
        if ((status = acrVerifySignature_HAL(pAcr, pSignature, falconId, pLsfHeader->dataSize,
             (pLsfHeader->ucodeSize + pLsfHeader->ucodeOffset), pLsfHeader, LW_FALSE)) != ACR_OK) //UcodeSize will be the dataOffset
        {
            // Data sig verif failed
            goto VERIF_FAIL;
        }

        // LS dependency map check
        if ((status = acrCheckForLSRevocation_HAL(pAcr, falconId, pLsfHeader, falconBilwersions)) != ACR_OK)
        {
            goto VERIF_FAIL;
        }

VERIF_FAIL:
        if (status != ACR_OK)
        {
            oldstatus = status;

#ifdef ACR_FMODEL_BUILD
            if ((falconId == LSF_FALCON_ID_FECS) || (falconId == LSF_FALCON_ID_GPCCS))
            {
                //
                // TODO: Resolve this
                // Some falcons such as FECS and GPCCS doesnt have signatures for binaries in debug elwiroment (fmodel)
                // Filed HW bug 200179871
                //
                oldstatus = ACR_OK;
            }
#endif
            status = _acrScrubContentAndWriteWpr(pLsfHeader);

            if (oldstatus != ACR_OK)
                return oldstatus;
            else
                return status;
        }
#endif // ACR_SIG_VERIF || ACR_RISCV_LS
        // Setup SubWprs for code and data part of falcon ucodes in FB-WPR
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrSetupFalconCodeAndDataSubWprs_HAL(pAcr, pWprHeader->falconId, pLsfHeader))
    }

    // Compare the size now to make sure we accounted for all of WPR region
    if (g_scrubState.scrubTrack != wprRegionSize)
    {
        return ACR_ERROR_SCRUB_SIZE;
    }

    return status;
}
#endif

/*!
 * @brief Writes back WPR header
 */
ACR_STATUS
acrWriteWprHeader_TU10X(void)
{
    LwU32  wprSize  = LW_ALIGN_UP((sizeof(LSF_WPR_HEADER) * LSF_FALCON_ID_END),
                                LSF_WPR_HEADER_ALIGNMENT);

    // Read the WPR header
    if ((acrIssueDma_HAL(pAcr, g_pWprHeader, LW_FALSE, 0, wprSize,
            ACR_DMA_TO_FB, ACR_DMA_SYNC_AT_END, &g_dmaProp)) != wprSize)
    {
        return ACR_ERROR_DMA_FAILURE;
    }

    return ACR_OK;
}

