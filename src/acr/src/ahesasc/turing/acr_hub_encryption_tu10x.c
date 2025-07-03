/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file: acr_hub_encryption_tu10x.c 
 */
#include "acr.h"
#include "dev_gc6_island.h"
#include "sec2mutexreservation.h"
#include "scp_secret_usage.h"
#include "dev_gc6_island_addendum.h"
// For HUB Encryption
#include "dev_fbhub_sec_bus.h"

#include "config/g_acr_private.h"
#include "config/g_acrlib_private.h"


typedef struct _acr_hub_bsi_info
{
    LwU32 rsvdSize;
    LwU32 rsvdSizeDw;
    LwU32 bsiHubOffset;
} ACR_HUB_BSI_INFO, *PACR_HUB_BSI_INFO;

#define SFBHUB_KEY_SIZE_DWORD           LW_SFBHUB_ENCRYPTION_REGION_SUBKEY__REGION_SIZE

#if defined(AHESASC) || defined(BSI_LOCK)
static ACR_STATUS acrGetHubBsiInfo(PACR_HUB_BSI_INFO pInfo)   ATTR_OVLY(OVL_NAME);
#endif

// Using global here to easily obtain 16 byte alignment
static LwU8 _tmpBuf[16]                                 ATTR_ALIGNED(16) ATTR_OVLY(".data");
//
// SALT_1/2/3/4 are a part of the same 128 bit salt, it is used to
// derive the key for encrypting the hub encryption key.
// DerivedKey = E(SCP_KEY_AT_INDEX_<N>, 128_BIT_SALT), where
// 128_BIT_SALT = HUB_ENCRYPTION_SALT_1 || HUB_ENCRYPTION_SALT_2 << 32 || HUB_ENCRYPTION_SALT_3 << 64 || HUB_ENCRYPTION_SALT_4 << 96
//
#define HUB_ENCRYPTION_SALT_1 0xAF1728E1
#define HUB_ENCRYPTION_SALT_2 0x1CB93271
#define HUB_ENCRYPTION_SALT_3 0xDF82BFAC
#define HUB_ENCRYPTION_SALT_4 0xEDAF7461

// Mask key to use for encrypting the HUB Encryption key
#define SCP_HUB_KEY_INDEX SCP_SECRET_KEY_INDEX_HUB_ENC_KEY

#define BSI_RAM_SIZE_BYTES_SHIFT        10
/*!
 * @brief ACR routine called during init to program keys
 *        and nonce for encrypted paging support.
 *
 * ASSUMPTION #1: FALCON_ENGINE_RESET is selwred to level 3 such that ACR code can not be reset midway and entire sequence can be considered atomic (or we go back to hw reset state).
 *                Just FYI for James: This is getting done by Sanket. So, no action here due to assumption.
 * ASSUMPTION #2: No falcon is running with LSMODE set at the time of this call. TODO: WE ARE NOT ENFORCING THIS ASSUMPTION AT THE MOMENT.
 */
// TODO for jamesx by GP107 FS: Consider passing in the region ids requiring programming, see https://lwcr.lwpu.com/sw/20591029/diffs/vs_base/653244/
#if defined(AHESASC) || defined(BSI_LOCK)
ACR_STATUS
acrProgramHubEncryption_TU10X(void)
{
    ACR_STATUS status = ACR_OK;
    LwU32 i           = 0;
    LwU32 clrRdValid  = 0;
    LwU32 clrWrValid  = 0;
    LwU32 setRdValid  = 0;
    LwU32 setWrValid  = 0;
    LwU32 acrRegionId;
    ACR_BSI_HUB_DESC_ARRAY hubDataArr;
    ACR_HUB_BSI_INFO info = {0};

    //
    // Initializing to HW init value for GP10X
    // Should be revisited for Turing, please check below two WARNINGs for details
    //
    LwU32 enableEnc = DRF_DEF(_SFBHUB, _ENCRYPTION_ACR_CFG, _REG1, _ENCRYPT_DIS) |
                      DRF_DEF(_SFBHUB, _ENCRYPTION_ACR_CFG, _REG2, _ENCRYPT_DIS) |
                      DRF_DEF(_SFBHUB, _ENCRYPTION_ACR_CFG, _REG3, _ENCRYPT_DIS);

    /****************************************************************************************/
    /* WARNING : Skipping this check for BSI_LOCK binary because of BSI-RAM size constraint */
    /*           This needs to be revisited for Turing, Bug 200346512                       */
    /****************************************************************************************/
#ifndef UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE
    //
    // Zero the hubDataArr data structure to make sure un-used fields are 0
    // otherwise we may leak random data into the BSI
    //
    for (acrRegionId = 0; acrRegionId < MAX_HUB_ENCRYPTION_REGION_COUNT; acrRegionId++)
    {
        for (i = 0; i < SFBHUB_KEY_SIZE_DWORD; i++)
        {
            hubDataArr.entries[acrRegionId].key[i]   = 0x0;
            hubDataArr.entries[acrRegionId].nonce[i] = 0x0;
        }
    }
#endif

    // Get information about the BSI
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetHubBsiInfo(&info));

    /***************************************************************************************************************************/
    /* WARNING : Skipping this check for BSI_LOCK binary because of BSI-RAM size constraint                                    */
    /*           Coming out of GC6, assuming HW init as the expected value for LW_SFBHUB_ENCRYPTION_ACR_CFG is safe given that */
    /*           we did not use IFF to overwrite this register. Reading this register would cost us BSI RAM space but          */
    /*           using #define to init enableEnc variable above would yield the same result and be more space efficient        */
    /*           This needs to be revisited for Turing, Bug 200346512                                                          */
    /***************************************************************************************************************************/
#ifndef UNDESIRABLE_OPTIMIZATION_FOR_BSI_RAM_SIZE
    // Pre-populate per region config values
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        acrSelwreBusReadRegister_HAL(pAcr,
                SEC2_SELWREBUS_TARGET_HUB, LW_SFBHUB_ENCRYPTION_ACR_CFG, &enableEnc));
#endif

    for (acrRegionId = ACR_REGION_START_IDX; acrRegionId <= ACR_REGION_END_IDX; acrRegionId++)
    {
        // Used to clear VIDMEM_READ_VALID
        clrRdValid |= DRF_IDX_DEF(_SFBHUB, _ENCRYPTION_ACR_VIDMEM_READ_VALID_CLR,
                           _REG, acrRegionId, _CLR);

        // Used to clear VIDMEM_WRITE_VALID
        clrWrValid |= DRF_IDX_DEF(_SFBHUB, _ENCRYPTION_ACR_VIDMEM_WRITE_VALID_CLR,
                           _REG, acrRegionId, _CLR);

        // Use to set ACR_CFG_REGION(i)_ENCRYPTION_EN
        enableEnc = FLD_IDX_SET_DRF(_SFBHUB, _ENCRYPTION_ACR_CFG, _REG, acrRegionId, _ENCRYPT_EN, enableEnc);

        // Used to set VIDMEM_READ VALID
        setRdValid |= DRF_IDX_DEF(_SFBHUB, _ENCRYPTION_ACR_VIDMEM_READ_VALID_SET,
                           _REG, acrRegionId, _SET);

        // Used to set VIDMEM_WRITE VALID
        setWrValid |= DRF_IDX_DEF(_SFBHUB, _ENCRYPTION_ACR_VIDMEM_WRITE_VALID_SET,
                           _REG, acrRegionId, _SET);
    }

    // 1. Clear the ACR_VIDMEM_READ/WRITE bits, this function can be called while encryption
    //    is active, so we need to disable access first before updating the keys/nonces
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        acrSelwreBusWriteRegister_HAL(pAcr, SEC2_SELWREBUS_TARGET_HUB,
                                     LW_SFBHUB_ENCRYPTION_ACR_VIDMEM_READ_VALID_CLR, clrRdValid));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        acrSelwreBusWriteRegister_HAL(pAcr, SEC2_SELWREBUS_TARGET_HUB,
                                     LW_SFBHUB_ENCRYPTION_ACR_VIDMEM_WRITE_VALID_CLR, clrWrValid));

#ifdef AHESASC
    // 2. Generate the HUB Encryption keys and nonces
    for (acrRegionId = ACR_REGION_START_IDX; acrRegionId <= ACR_REGION_END_IDX; acrRegionId++)
    {
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetTRand_HAL(pAcr, hubDataArr.entries[acrRegionId].key, SFBHUB_KEY_SIZE_DWORD));
        CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrGetTRand_HAL(pAcr, hubDataArr.entries[acrRegionId].nonce, SFBHUB_KEY_SIZE_DWORD));
    }

    // 3. Encrypt and save the HUB Encryption keys
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrEncryptAndSaveHubEncryptionKeys_HAL(pAcr, &hubDataArr));

    // 4. Program the keys and clear the key data from HUB data struct
    for (acrRegionId = ACR_REGION_START_IDX; acrRegionId <= ACR_REGION_END_IDX; acrRegionId++)
    {
        for (i = 0; i < SFBHUB_KEY_SIZE_DWORD; i++)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(
                acrSelwreBusWriteRegister_HAL(pAcr, SEC2_SELWREBUS_TARGET_HUB,
                                            LW_SFBHUB_ENCRYPTION_REGION_SUBKEY(acrRegionId*4 + i),
                                            hubDataArr.entries[acrRegionId].key[i]));

            hubDataArr.entries[acrRegionId].key[i] = 0x0;

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(
                acrSelwreBusWriteRegister_HAL(pAcr, SEC2_SELWREBUS_TARGET_HUB,
                                            LW_SFBHUB_ENCRYPTION_REGION_SUBNONCE(acrRegionId*4 + i),
                                            hubDataArr.entries[acrRegionId].nonce[i]));
        }
    }

    // 5. Save rest of the required params to BSI
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        acrBsiRamReadWrite_TU10X((LwU32*)&hubDataArr, info.rsvdSizeDw, info.bsiHubOffset, LW_FALSE));
#endif // AHESASC

#ifdef BSI_LOCK
    // 2. Load data structure from BSI
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        acrBsiRamReadWrite_TU10X((LwU32*)&hubDataArr, info.rsvdSizeDw, info.bsiHubOffset, LW_TRUE));

    // 3. Decrypt the HUB Encryption keys
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrLoadAndDecryptHubEncryptionKeys_HAL(pAcr, &hubDataArr));

    // 4. Program the keys and clear the key data from HUB data struct
    for (acrRegionId = ACR_REGION_START_IDX; acrRegionId <= ACR_REGION_END_IDX; acrRegionId++)
    {
        for (i = 0; i < SFBHUB_KEY_SIZE_DWORD; i++)
        {
            CHECK_STATUS_AND_RETURN_IF_NOT_OK(
                acrSelwreBusWriteRegister_HAL(pAcr, SEC2_SELWREBUS_TARGET_HUB,
                                            LW_SFBHUB_ENCRYPTION_REGION_SUBKEY(acrRegionId*4 + i),
                                            hubDataArr.entries[acrRegionId].key[i]));

            hubDataArr.entries[acrRegionId].key[i] = 0x0;

            CHECK_STATUS_AND_RETURN_IF_NOT_OK(
                acrSelwreBusWriteRegister_HAL(pAcr, SEC2_SELWREBUS_TARGET_HUB,
                                            LW_SFBHUB_ENCRYPTION_REGION_SUBNONCE(acrRegionId*4 + i),
                                            hubDataArr.entries[acrRegionId].nonce[i]));
        }
    }
#endif // BSI_LOCK

    // 6 (for Load)/5 (for Lock). Enable/Re-enable HUB Encryption

    // 6/5.2 Enable encryption
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        acrSelwreBusWriteRegister_HAL(pAcr, SEC2_SELWREBUS_TARGET_HUB, LW_SFBHUB_ENCRYPTION_ACR_CFG, enableEnc));

    // 6/5.3 Set ACR_VIDMEM Read/Write valid bits
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        acrSelwreBusWriteRegister_HAL(pAcr, SEC2_SELWREBUS_TARGET_HUB,
                                     LW_SFBHUB_ENCRYPTION_ACR_VIDMEM_READ_VALID_SET, setRdValid));
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        acrSelwreBusWriteRegister_HAL(pAcr, SEC2_SELWREBUS_TARGET_HUB,
                                     LW_SFBHUB_ENCRYPTION_ACR_VIDMEM_WRITE_VALID_SET, setWrValid));

    return ACR_OK;
}
#endif

/*!
 * @brief This function does the following:
 *
 * @return    ACR_STATUS
 */
ACR_STATUS acrEncryptAndSaveHubEncryptionKeys_TU10X(PACR_BSI_HUB_DESC_ARRAY pHubDataArr)
{
    ACR_STATUS status = ACR_OK;
    LwU32 *pBuf;
    LwU32 i = 0;

    pBuf = (LwU32*)_tmpBuf;
    pBuf[0] = HUB_ENCRYPTION_SALT_1;
    pBuf[1] = HUB_ENCRYPTION_SALT_2;
    pBuf[2] = HUB_ENCRYPTION_SALT_3;
    pBuf[3] = HUB_ENCRYPTION_SALT_4;

    // Setup SCP
    falc_scp_trap(TC_INFINITY);

    // 1. Derive the encryption key for encryption HUB Encryption keys
    // Derive the encryption key (to R4), this key will be used to
    // encrypt the HUB Encryption key (before saving it)
    // Load salt
    // TODO for jamesx by GP107 FS: Add a macro that sets up the stuff so its easier to read
    falc_trapped_dmwrite(falc_sethi_i((LwU32)(_tmpBuf), SCP_R3));
    falc_dmwait();
    // Load key
    falc_scp_secret(SCP_HUB_KEY_INDEX, SCP_R2);
    falc_scp_key(SCP_R2);
    // Derived key in R4
    falc_scp_encrypt(SCP_R3, SCP_R4);
    falc_scp_chmod(0x1, SCP_R4);                                            // Only allow secure keyable access i.e. only bit0 set

    //2. Encrypt the HUB Encryption keys
    // Move data to aligned buffer
    // TODO for jamesx by GP107 FS: purpose: Handle more than just START_IDX
    pBuf = (LwU32*)_tmpBuf;
    for (i = 0; i < SFBHUB_KEY_SIZE_DWORD; i++)
    {
        pBuf[i] = pHubDataArr->entries[ACR_REGION_START_IDX].key[i];
    }

    // Load data (hub encryption key)
    falc_trapped_dmwrite(falc_sethi_i((LwU32)_tmpBuf, SCP_R3));
    falc_dmwait();
    // Encrypt to R2
    falc_scp_key(SCP_R4);
    falc_scp_encrypt(SCP_R3, SCP_R2);
    // Read back results
    falc_trapped_dmread(falc_sethi_i((LwU32)(_tmpBuf), SCP_R2));
    falc_dmwait();

    // 3. Save the result to BSI secure scratch
    pBuf = (LwU32*)_tmpBuf;
    for (i = 0; i < SFBHUB_KEY_SIZE_DWORD; i++)
    {
        ACR_REG_WR32(BAR0, LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH(i), pBuf[i]);

        pBuf[i] = 0x0;
    }

    // Teardown the SCP
    falc_scp_trap(TC_DISABLE_CCR);

    return status;
}

ACR_STATUS acrLoadAndDecryptHubEncryptionKeys_TU10X(PACR_BSI_HUB_DESC_ARRAY pHubDataArr)
{
    ACR_STATUS status = ACR_OK;
    LwU32 *pBuf;
    LwU32 i = 0;

    pBuf = (LwU32*)_tmpBuf;
    pBuf[0] = HUB_ENCRYPTION_SALT_1;
    pBuf[1] = HUB_ENCRYPTION_SALT_2;
    pBuf[2] = HUB_ENCRYPTION_SALT_3;
    pBuf[3] = HUB_ENCRYPTION_SALT_4;

    // Setup SCP
    falc_scp_trap(TC_INFINITY);

    // Derive the encryption key (to R4), this key will be used to
    // encrypt the HUB Encryption key (before saving it)
    // Load salt
    falc_trapped_dmwrite(falc_sethi_i((LwU32)(_tmpBuf), SCP_R3));
    falc_dmwait();
    // Load key
    falc_scp_secret(SCP_HUB_KEY_INDEX, SCP_R2);
    falc_scp_key(SCP_R2);
    // Derived key in R4
    falc_scp_encrypt(SCP_R3, SCP_R4);
    // Setup the decryption key into R3 (rotate key for decryption)
    falc_scp_rkey10(SCP_R4, SCP_R3);
    falc_scp_chmod(0x1, SCP_R4);                                            // Only allow secure keyable access i.e. only bit0 set
    falc_scp_chmod(0x1, SCP_R3);                                            // Only allow secure keyable access i.e. only bit0 set

    // Load encrypted key
    pBuf = (LwU32*)_tmpBuf;
    for (i = 0; i < SFBHUB_KEY_SIZE_DWORD; i++)
    {
        pBuf[i] = ACR_REG_RD32(BAR0, LW_PGC6_BSI_HUB_ENCRYPTION_SELWRE_SCRATCH(i));
    }

    // Load the data
    falc_trapped_dmwrite(falc_sethi_i((LwU32)_tmpBuf, SCP_R2));
    falc_dmwait();
    // Decrypt to R4
    falc_scp_key(SCP_R3);
    falc_scp_decrypt(SCP_R2, SCP_R4);
    // Read results back
    falc_trapped_dmread(falc_sethi_i((LwU32)(_tmpBuf), SCP_R4));
    falc_dmwait();

    pBuf = (LwU32*)_tmpBuf;
    // TODO for jamesx by GP107 FS: purpose: Handle more than just START_IDX
    for (i = 0; i < SFBHUB_KEY_SIZE_DWORD; i++)
    {
        pHubDataArr->entries[ACR_REGION_START_IDX].key[i] = pBuf[i];
        pBuf[i] = 0x0;
    }

    // Teardown the SCP
    falc_scp_trap(TC_DISABLE_CCR);

    return status;
}

/*!
 * @brief Read/write content of BSI RAM from offset
 *
 * @param[in/out] pBuf          Depends on read or write
 * @param[in]     sizeDwords    size to read from BSI ram
 * @param[in]     offset        offset from we want to read
 * @param[in]     bRead         direction of the transfer
 *
 * @return        ACR_OK:                                           If read/write is successful
 * @return        ACR_ERROR_HUB_ENCRYPTION_REQUEST_FAILED:          If supplied offset is not in valid range
 * @return        Error codes from acrAcquire/ReleaseSelwreMutex_HAL: If mutex acquire/release fails
 */
ACR_STATUS
acrBsiRamReadWrite_TU10X
(
    LwU32 *pBuf,
    LwU32  sizeDwords,
    LwU32  offset,
    LwBool bRead
)
{
    ACR_STATUS status = ACR_OK;
    LwU32 i;
    LwU32 ramCtrl = 0;
    LwU32 bsiRamSize;
    LwU8  acrMutexBsiRamAccess = 0;

    bsiRamSize =(DRF_VAL( _PGC6, _BSI_RAM, _SIZE, ACR_REG_RD32(BAR0, LW_PGC6_BSI_RAM))) << BSI_RAM_SIZE_BYTES_SHIFT;

    if ((offset + (sizeDwords * sizeof(LwU32)) - 1) >=
                       bsiRamSize)
    {
        return ACR_ERROR_HUB_ENCRYPTION_REQUEST_FAILED;
    }

    //
    // TODO for jamesx by GP107 FS: purpose: Restore the AUTOINC values
    //
    // Acquire mutex for accessing the BSI
    //
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrAcquireSelwreMutex_HAL(pAcr, SEC2_MUTEX_BSI_WRITE, &acrMutexBsiRamAccess));

    ramCtrl = FLD_SET_DRF_NUM(_PGC6, _BSI_RAMCTRL, _ADDR, offset, ramCtrl);
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _RAUTOINCR, _ENABLE, ramCtrl);
    ramCtrl = FLD_SET_DRF(_PGC6, _BSI_RAMCTRL, _WAUTOINCR, _ENABLE, ramCtrl);
    ACR_REG_WR32(BAR0, LW_PGC6_BSI_RAMCTRL, ramCtrl);

    for (i = 0; i < sizeDwords; i++)
    {
        if (bRead)
        {
            pBuf[i] = ACR_REG_RD32(BAR0, LW_PGC6_BSI_RAMDATA);
        }
        else
        {
            ACR_REG_WR32(BAR0, LW_PGC6_BSI_RAMDATA, pBuf[i]);
        }
    }

    // Release the mutex for accessing the BSI
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(acrReleaseSelwreMutex_HAL(pAcr, SEC2_MUTEX_BSI_WRITE, acrMutexBsiRamAccess));

    return ACR_OK;
}

#if defined(AHESASC) || defined(BSI_LOCK)
static ACR_STATUS
acrGetHubBsiInfo(PACR_HUB_BSI_INFO pInfo)
{
    ACR_STATUS status = ACR_OK;
    LwU32 bsiRamSize;
    LwU32 dwSize = sizeof(LwU32);

    BSI_RAM_SELWRE_SCRATCH_HDR_V2 brssHeader;
    LwU32 brssHeaderSz = sizeof(BSI_RAM_SELWRE_SCRATCH_HDR_V2);
    LwU32 brssHeaderSzDw = brssHeaderSz / dwSize;

    if (!pInfo)
    {
        return ACR_ERROR_ILWALID_ARGUMENT;
    }

    pInfo->rsvdSize     = LW_ALIGN_UP(sizeof(ACR_BSI_HUB_DESC_ARRAY), dwSize);
    pInfo->rsvdSizeDw   = pInfo->rsvdSize / dwSize;

    bsiRamSize = (DRF_VAL( _PGC6, _BSI_RAM, _SIZE, ACR_REG_RD32(BAR0, LW_PGC6_BSI_RAM))) << BSI_RAM_SIZE_BYTES_SHIFT;

    if (bsiRamSize < brssHeaderSz)
    {
        return ACR_ERROR_BSI_ILWALID_SIZE;
    }

    // Read the BRSS Header structure (no need to read the whole struct)
    CHECK_STATUS_AND_RETURN_IF_NOT_OK(
        acrBsiRamReadWrite_TU10X((LwU32*)&brssHeader, brssHeaderSzDw, bsiRamSize - brssHeaderSz, LW_TRUE));

    // BRSS is required and must be at the supported version for HUB Encryption
    if ((brssHeader.identifier != LW_BRSS_IDENTIFIER) ||
        (brssHeader.version != LW_BRSS_VERSION_V2))
    {
#ifndef ACR_FMODEL_BUILD
        return ACR_ERROR_BRSS_NOT_FOUND_OR_UNSUPPORTED_VERSION;
#else
        //
        // Relaxing the requirement of v2 of BRSS for fmodel due to the following reasons
        // 1. Sim UDE has not been updated to BRSS v2 (bug 200194032)
        // 2. Most fmodel/RTL runs use RDE instead of UDE due to long run times of UDE. Bug 1438345 tracks the request
        // for fixes so that we can switch to UDE
        //
        // When BRSS_IDENTIFIER is not present, we just allocate space at the top of BSI_RAM for HUB encryption DS
        // When BRSS_IDENTIFIER is present but not v2, we leave the space as indicated by the "size" field of BRSS struct
        // at the top of BSI RAM and put the Hub encryption DS just below it
        //
        if (brssHeader.identifier != LW_BRSS_IDENTIFIER)
        {
            pInfo->bsiHubOffset = bsiRamSize - pInfo->rsvdSize;
        }
        else
        {
            pInfo->bsiHubOffset = bsiRamSize - brssHeader.size - pInfo->rsvdSize;
        }
        return ACR_OK;
#endif
    }

    // We must have enough space reserved for HUB Encryption
    if (pInfo->rsvdSize > brssHeader.hubEncryptionStructSize)
    {
        return ACR_ERROR_BRSS_INSUFFICIENT_HUB_ENCRYPTION_RSVD;
    }
    else
    {
        // Note that brssHeader.size is the size of the entire BRSS struct not just the header
        pInfo->bsiHubOffset = bsiRamSize - brssHeader.size - pInfo->rsvdSize;

        return ACR_OK;
    }

    // We should not reach here
    return ACR_ERROR_UNREACHABLE_CODE;
}
#endif
