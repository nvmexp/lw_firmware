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
 * @file   hdcp22wired_hdcp22wired0400.c
 * @brief  Hdcp22 wired 04.00 Hal Functions
 *
 * HDCP2.2 Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes --------------------------------- */
#include "dev_disp.h"
/* ------------------------ OpenRTOS includes ------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"
#include "dev_disp_addendum.h"
#ifdef HDCP22_KMEM_ENABLED
#include "dev_gsp_csb_addendum.h"
#endif

/* ------------------------ Types definitions ------------------------------- */
// To avoid HS mulitplication, use shift with bits.
#define KMEM_KEYSLOT_LENGTH_BITS    (4)

/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
#ifdef HDCP22_KMEM_ENABLED
/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Prototypes  ------------------------------------- */
static FLCN_STATUS _hdcp22wiredKmemAllocateFreeKSlts_v04_00(LwU32 kmemKSltLength, LwU32 requestSize)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22wiredKmemAllocateFreeKSlts_v04_00");
static LwBool _hdcp22wiredKmemIsKSltUsed_v04_00(LwU32 kSltId)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22wiredKmemIsKSltUsed_v04_00");
static FLCN_STATUS _hdcp22wiredKmemSetKSltsProtInfo_v04_00(LwUPtr kmemProtInfoBase, LwU32 startKSlt, LwU32 requestKSlts)
    GCC_ATTRIB_SECTION("imem_libHdcp22wired", "_hdcp22wiredKmemSetKSltsProtInfo_v04_00");
static FLCN_STATUS _hdcp22wiredGetSecretOffsetSize_v04_00(HDCP22_SECRET hdcp22Secret, LwU32 *pOffset, LwU32 *pSize)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "_hdcp22wiredGetSecretOffsetSize_v04_00");
#else
extern FLCN_STATUS hdcp22wiredWriteToSelwreMemory_v03_00(HDCP22_SECRET hdcp22Secret, LwU8 *pSecret, LwBool bIsReset);
extern FLCN_STATUS hdcp22wiredReadFromSelwreMemory_v03_00(HDCP22_SECRET hdcp22Secret, LwU8 *pSecret);
extern FLCN_STATUS hdcp22wiredSaveSessionSecrets_v03_00(LwU8 sorNum);
extern FLCN_STATUS hdcp22wiredRetrieveSessionSecrets_v03_00(LwU8 sorNum);
#endif // HDCP22_KMEM_ENABLED

FLCN_STATUS hdcp22wiredWriteToSelwreMemory_v04_00(HDCP22_SECRET hdcp22Secret, LwU8 *pSecret, LwBool bIsReset)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredWriteToSelwreMemory_v04_00");
FLCN_STATUS hdcp22wiredReadFromSelwreMemory_v04_00(HDCP22_SECRET hdcp22Secret, LwU8 *pSecret)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredReadFromSelwreMemory_v04_00");
FLCN_STATUS hdcp22wiredSaveSessionSecrets_v04_00(LwU8 sorNum)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredSaveSessionSecrets_v04_00");
FLCN_STATUS hdcp22wiredRetrieveSessionSecrets_v04_00(LwU8 sorNum)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredRetrieveSessionSecrets_v04_00");
FLCN_STATUS hdcp22wiredKmemWriteToKslt_v04_00(LwU32 offset, LwU32 size, LwU8 *pData)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredKmemWriteToKslt_v04_00");
FLCN_STATUS hdcp22wiredKmemReadFromKslt_v04_00(LwU32 offset, LwU32 size, LwU8 *pData)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredKmemReadFromKslt_v04_00");
FLCN_STATUS hdcp22wiredKmemCheckProtinfoHs_v04_00(void)
    GCC_ATTRIB_SECTION("imem_selwreActionHs", "hdcp22wiredKmemCheckProtinfoHs_v04_00");

/* ------------------------ Private Functions ------------------------------- */
#ifdef HDCP22_KMEM_ENABLED
/*!
 * @brief       Allocate free KMEM keyslots
 *
 * @param[in]   kmemKSltLength      KMeM keyslot length
 * @param[in]   requestSize         requesting secure memory storage size in bytes
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                                  Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22wiredKmemAllocateFreeKSlts_v04_00
(
    LwU32       kmemKSltLength,
    LwU32       requestSize
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       index  = LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START;
    LwU32       requestKSlts = (requestSize + kmemKSltLength - 1) / kmemKSltLength;

    if ((requestKSlts == 0) || (requestKSlts > LW_CGSP_FALCON_KMEM_HDCP_SLOTS_NUMBER))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto label_return;
    }

    for (index = LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START;
         index<(LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START + LW_CGSP_FALCON_KMEM_HDCP_SLOTS_NUMBER);
         index++)
    {
        // Make sure the slot is not used.
        if(_hdcp22wiredKmemIsKSltUsed_v04_00(index))
        {
            status = FLCN_ERR_ILLEGAL_OPERATION;
            goto label_return;
        }
    }

label_return:
    return status;
}

/*!
 * @brief       Check if KSlt valid or not.
 * @param[in]   kSltId          kSlt index
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static LwBool
_hdcp22wiredKmemIsKSltUsed_v04_00
(
    LwU32 kSltId
)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // The valid bits for each of the keyslot gets stored in a register array,
    // each regiter in this array holds valid bits for "next" 32 conselwtive
    // key slots.
    //
    // KMEM_VALID(N)[i:i] is the valid bit for key slot (N)*32 + (i)
    //

    //
    // Get index of the register and position within the register where valid
    // bit of given key slot is stored.
    //
    LwU32 idx = kSltId / 32;
    LwU32 pos = kSltId % 32;
    LwU32 data32 = 0x0;

    status = libInterfaceCsbRegRd32ErrChk(LW_CGSP_FALCON_KMEM_VALID(idx),
                                          &data32);

    return (status == FLCN_OK) ? ((data32>>pos) & 1 ) : LW_FALSE;
}

/*!
 * @brief       Setup KMEM protection setting.
 *
 * @param[in]   kmemProtInfoBase    kSlt protection information base
 * @param[in]   startKSlt           Start keyslot index
 * @param[in]   requestKSlts        number of requesting keyslots
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                                  Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22wiredKmemSetKSltsProtInfo_v04_00
(
    LwUPtr      kmemProtInfoBase,
    LwU32       startKSlt,
    LwU32       requestKSlts
)
{
    FLCN_STATUS status = FLCN_ERR_SELWREBUS_REGISTER_WRITE_ERROR;
    LwU32       index = 0;
    LwU32       protInfo = 0;
    LwU64      *pProtInfo = (LwU64*)kmemProtInfoBase;

    // Set protection as only HS can read/write.
    protInfo = LW_CGSP_FALCON_KMEM_PROTECTINFO_RESET;
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL0_READ, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL0_WRITE, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL1_READ, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL1_WRITE, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL2_READ, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL2_WRITE, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL3_READ, _ENABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL3_WRITE, _ENABLE, protInfo);

    // Disable Priv interface permission.
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _PRIV_EN, _DISABLE, protInfo);

    for (index=startKSlt; index<(startKSlt+requestKSlts); index++)
    {
        if (_hdcp22wiredKmemIsKSltUsed_v04_00(index))
        {
            goto label_return;
        }

        // protectInfo.high is reserved for futuer extension and only set protectInofo.low now.
        pProtInfo[index] = protInfo;

        if (!_hdcp22wiredKmemIsKSltUsed_v04_00(index))
        {
            goto label_return;
        }
    }

    status = FLCN_OK;

label_return:
    return status;
}

/*!
 * @brief       Helper func to get offset, size of secerets in selwreMemory.
 * @param[in]   hdcp22Secret    Secret type
 * @param[in]   pOffset         Pointer offset
 * @param[in]   pSize           Pointer to size
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
static FLCN_STATUS
_hdcp22wiredGetSecretOffsetSize_v04_00
(
    HDCP22_SECRET   hdcp22Secret,
    LwU32          *pOffset,
    LwU32          *pSize
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (hdcp22Secret)
    {
    case HDCP22_SECRET_DKEY:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsConfidential) +
                LW_OFFSETOF(SECRET_ASSETS_CONFIDENTIAL, encDkey);
            *pSize = HDCP22_SIZE_DKEY;
        }
        break;

    case HDCP22_SECRET_KD:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsConfidential) +
                LW_OFFSETOF(SECRET_ASSETS_CONFIDENTIAL, encKd);
            *pSize = HDCP22_SIZE_KD;
        }
        break;

    // Below fields are integrity check required.
    case HDCP22_SECRET_SOR_NUM:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, sorNum);
            *pSize = sizeof(LwU8);
        }
        break;

    case HDCP22_SECRET_LINK_IDX:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, linkIndex);
            *pSize = sizeof(LwU8);
        }
        break;

    case HDCP22_SECRET_IS_MST:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, bIsMst);
            *pSize = sizeof(LwBool);
        }
        break;

    case HDCP22_SECRET_IS_REPEATER:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, bIsRepeater);
            *pSize = sizeof(LwBool);
        }
        break;

    case HDCP22_SECRET_NUM_STREAMS:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, numStreams);
            *pSize = sizeof(LwU8);
        }
        break;

    case HDCP22_SECRET_STREAM_ID_TYPE:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, streamIdType);
            *pSize = sizeof(HDCP22_STREAM) * HDCP22_NUM_STREAMS_MAX;
        }
        break;

    case HDCP22_SECRET_DP_TYPE_MASK:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, dpTypeMask);
            *pSize = sizeof(LwU32) * HDCP22_NUM_DP_TYPE_MASK;
        }
        break;

    case HDCP22_SECRET_RX_ID:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, rxId);
            *pSize = HDCP22_SIZE_RECV_ID_8;
        }
        break;

    case HDCP22_SECRET_RTX:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, Rtx);
            *pSize = HDCP22_SIZE_R_TX;
        }
        break;

    case HDCP22_SECRET_RN:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, Rn);
            *pSize = HDCP22_SIZE_R_N;
        }
        break;

    case HDCP22_SECRET_MAX_NO_SORS:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, maxNoOfSors);
            *pSize = sizeof(LwU8);
        }
        break;

    case HDCP22_SECRET_MAX_NO_HEADS:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, maxNoOfHeads);
            *pSize = sizeof(LwU8);
        }
        break;

    case HDCP22_SECRET_PREV_STATE:
        {
            *pOffset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsIntegrity) +
                LW_OFFSETOF(SECRET_ASSETS_INTEGRITY, previousState);
            *pSize = sizeof(SELWRE_ACTION_TYPE);
        }
        break;

    default:
        status = FLCN_ERR_NOT_SUPPORTED;
    }

    return status;
}
#endif // HDCP22_KMEM_ENABLED

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief       Write confidential or integrity secrets
 * @param[in]   hdcp22Secret    Secret type
 * @param[in]   pSecret         Pointer to secret array.
 * @param[in]   bIsReset        flag to tell if reset without check integrity.
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredWriteToSelwreMemory_v04_00
(
    HDCP22_SECRET   hdcp22Secret,
    LwU8           *pSecret,
    LwBool          bIsReset
)
{
#ifdef HDCP22_KMEM_ENABLED
    FLCN_STATUS status  = FLCN_OK;
    LwU32       size    = 0;
    LwU32       offset  = 0;

    CHECK_STATUS(_hdcp22wiredGetSecretOffsetSize_v04_00(hdcp22Secret, &offset, &size));

    if (bIsReset)
    {
        LwU8 resetBuf[SELWREMEMORY_SECRET_MAX_SIZE];

        // To reset case, clear secret value.
        memset_hs(resetBuf, 0, SELWREMEMORY_SECRET_MAX_SIZE);
        status = hdcp22wiredKmemWriteToKslt_HAL(&Hdcp22wired, offset, size, resetBuf);
    }
    else
    {
        status = hdcp22wiredKmemWriteToKslt_HAL(&Hdcp22wired, offset, size, pSecret);
    }

label_return:
    return status;
#else
    //
    // If KMEM not supported or enabled, we use legacy scp crypt/xor to protect
    // for confidential secrets, and also hash check for intergity secrets.
    //
    return hdcp22wiredWriteToSelwreMemory_v03_00(hdcp22Secret, pSecret, bIsReset);
#endif
}

/*!
 * @brief       Read confidential or integrity secrets
 * @param[in]   hdcp22Secret    Secret type
 * @param[in]   pSecret         Pointer to secret array.
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredReadFromSelwreMemory_v04_00
(
    HDCP22_SECRET   hdcp22Secret,
    LwU8           *pSecret
)
{
#ifdef HDCP22_KMEM_ENABLED
    FLCN_STATUS status  = FLCN_OK;
    LwU32       size    = 0;
    LwU32       offset  = 0;

    CHECK_STATUS(_hdcp22wiredGetSecretOffsetSize_v04_00(hdcp22Secret, &offset, &size));

    status = hdcp22wiredKmemReadFromKslt_HAL(&Hdcp22wired, offset, size, pSecret);

label_return:
    return status;
#else
    //
    // If KMEM not supported or enabled, we use legacy scp crypt/xor to protect
    // for confidential secrets, and also hash check for intergity secrets.
    //
    return hdcp22wiredReadFromSelwreMemory_v03_00(hdcp22Secret, pSecret);
#endif
}

/*!
 * @brief       Save active session secrets per SOR.
 * @param[in]   sorNum      SOR number
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredSaveSessionSecrets_v04_00
(
    LwU8 sorNum
)
{
#ifdef HDCP22_KMEM_ENABLED
    FLCN_STATUS status = FLCN_OK;
    LwU32       encKd[HDCP22_SIZE_KD/sizeof(LwU32)];
    LwU32       offset = 0;
    LwU32       size = 0;

    if (sorNum >= HDCP22_ACTIVE_SESSIONS_MAX)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto label_return;
    }

    offset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsConfidential) +
                LW_OFFSETOF(SECRET_ASSETS_CONFIDENTIAL, encKd);
    size = HDCP22_SIZE_KD;

    CHECK_STATUS(hdcp22wiredKmemReadFromKslt_HAL(&Hdcp22wired, offset, size, (LwU8*)encKd));

    offset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsActiveSession) +
             sorNum * sizeof(SECRET_ASSETS_ACTIVE_SESSION) +
             LW_OFFSETOF(SECRET_ASSETS_ACTIVE_SESSION, encKd);

    CHECK_STATUS(hdcp22wiredKmemWriteToKslt_HAL(&Hdcp22wired, offset, size, (LwU8*)encKd));

label_return:
    return status;
#else
    return hdcp22wiredSaveSessionSecrets_v03_00(sorNum);
#endif
}

/*!
 * @brief       Retrieve active session secrets per SOR.
 * @param[in]   sorNum                  SOR number
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                          Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredRetrieveSessionSecrets_v04_00
(
    LwU8                            sorNum
)
{
#ifdef HDCP22_KMEM_ENABLED
    FLCN_STATUS status = FLCN_OK;
    LwU32       encKd[HDCP22_SIZE_KD/sizeof(LwU32)];
    LwU32       offset = 0;
    LwU32       size = 0;

    if (sorNum >= HDCP22_ACTIVE_SESSIONS_MAX)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto label_return;
    }

    size = HDCP22_SIZE_KD;
    offset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsActiveSession) +
             sorNum * sizeof(SECRET_ASSETS_ACTIVE_SESSION) +
             LW_OFFSETOF(SECRET_ASSETS_ACTIVE_SESSION, encKd);

    CHECK_STATUS(hdcp22wiredKmemReadFromKslt_HAL(&Hdcp22wired, offset, size, (LwU8*)encKd));

    offset = LW_OFFSETOF(HDCP22_SELWRE_MEMORY, secAssetsConfidential) +
                LW_OFFSETOF(SECRET_ASSETS_CONFIDENTIAL, encKd);

    CHECK_STATUS(hdcp22wiredKmemWriteToKslt_HAL(&Hdcp22wired, offset, size, (LwU8*)encKd));

label_return:
    return status;
#else
    return hdcp22wiredRetrieveSessionSecrets_v03_00(sorNum);
#endif
}

/*!
 * @brief       Initialize KMEM storage information.
 * @param[in]   requestSize     request storage size in bytes.
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredKmemInit_v04_00
(
    LwU32 requestSize
)
{
    FLCN_STATUS status = FLCN_OK;

#ifdef HDCP22_KMEM_ENABLED
    LwU32       data32;
    LwUPtr      kmemProtInfoBase;
    LwU32       kmemProtInfoLength;
    LwUPtr      kmemBaseAddr;
    LwU32       kmemNumKSlt;
    LwU32       kmemKSltLength;
    LwU32       kmemBromUsedSlt;
    LwU32       requestKSlts;
    LwU32       startKSlt;

    //
    // In HS, Start session we will cross check initialization of KMEM is done properly.
    // TODO: move initialize to HS after adding hdcp22wiredInitialize selwreAction.
    //
    CHECK_STATUS(libInterfaceCsbRegRd32ErrChk(LW_CGSP_FALCON_KMEMBASE, &kmemBaseAddr));

    CHECK_STATUS(libInterfaceCsbRegRd32ErrChk(LW_CGSP_FALCON_KMEMCFG, &data32));
    kmemNumKSlt =  DRF_VAL(_CGSP, _FALCON_KMEMCFG, _NUMOFKSLT, data32);
    kmemKSltLength = DRF_VAL(_CGSP, _FALCON_KMEMCFG, _KSLT_LENGTH, data32);
    kmemProtInfoLength =  DRF_VAL(_CGSP, _FALCON_KMEMCFG, _PROTECTINFO_LENGTH, data32);
    kmemBromUsedSlt = DRF_VAL(_CGSP, _FALCON_KMEMCFG, _BROM_USED_SLOTS, data32);

    if ((kmemBromUsedSlt > LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START) ||
        (kmemNumKSlt < (LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START + LW_CGSP_FALCON_KMEM_HDCP_SLOTS_NUMBER)) ||
        (kmemKSltLength != (1 << KMEM_KEYSLOT_LENGTH_BITS)))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto label_return;
    }

    kmemProtInfoBase = kmemBaseAddr + kmemNumKSlt * kmemKSltLength;

    CHECK_STATUS(_hdcp22wiredKmemAllocateFreeKSlts_v04_00(kmemKSltLength, requestSize));

    startKSlt = LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START;
    requestKSlts = LW_CGSP_FALCON_KMEM_HDCP_SLOTS_NUMBER;

    CHECK_STATUS(_hdcp22wiredKmemSetKSltsProtInfo_v04_00(kmemProtInfoBase, startKSlt, requestKSlts));

label_return:
#endif // HDCP22_KMEM_ENABLED

    return status;
}

/*!
 * @brief       Write to KMEM keyslot storage
 * @param[in]   offset          Offset to write
 * @param[in]   size            Write size in bytes
 * @param[in]   pData           Pointer to write data
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredKmemWriteToKslt_v04_00
(
    LwU32   offset,
    LwU32   size,
    LwU8   *pData
)
{
    FLCN_STATUS status = FLCN_OK;
#ifdef HDCP22_KMEM_ENABLED
    LwUPtr      kmemBaseAddr;
    LwU8       *pSelwreMemoryBaseAddr;

    // Check if exceed reserved keyslots.
    if ((offset + size) >= (LW_CGSP_FALCON_KMEM_HDCP_SLOTS_NUMBER << KMEM_KEYSLOT_LENGTH_BITS))
    {
        status = FLCN_ERR_ILLEGAL_OPERATION;
        goto label_return;
    }

    CHECK_STATUS(libInterfaceCsbRegRd32ErrChkHs(LW_CGSP_FALCON_KMEMBASE, &kmemBaseAddr));

    pSelwreMemoryBaseAddr = (LwU8*)(kmemBaseAddr +
                                    (LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START << KMEM_KEYSLOT_LENGTH_BITS));

    memcpy_hs(&pSelwreMemoryBaseAddr[offset], pData, size);

label_return:
#endif // HDCP22_KMEM_ENABLED
    return status;
}

/*!
 * @brief       Read from KMEM keyslot storage
 * @param[in]   offset          Offset to write
 * @param[in]   size            Write size in bytes
 * @param[in]   pData           Pointer to read data
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                              Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredKmemReadFromKslt_v04_00
(
    LwU32   offset,
    LwU32   size,
    LwU8   *pData
)
{
    FLCN_STATUS status = FLCN_OK;
#ifdef HDCP22_KMEM_ENABLED
    LwUPtr      kmemBaseAddr;
    LwU8       *pSelwreMemoryBaseAddr;

    // Check if exceed reserved keyslots.
    if ((offset + size) >= (LW_CGSP_FALCON_KMEM_HDCP_SLOTS_NUMBER << KMEM_KEYSLOT_LENGTH_BITS))
    {
        status = FLCN_ERR_ILLEGAL_OPERATION;
        goto label_return;
    }

    CHECK_STATUS(libInterfaceCsbRegRd32ErrChkHs(LW_CGSP_FALCON_KMEMBASE, &kmemBaseAddr));

    pSelwreMemoryBaseAddr = (LwU8*)(kmemBaseAddr +
                                    (LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START << KMEM_KEYSLOT_LENGTH_BITS));

    memcpy_hs(pData, &pSelwreMemoryBaseAddr[offset], size);
label_return:
#endif // HDCP22_KMEM_ENABLED
    return status;
}

/*!
 * @brief       Check KMEM protection setting.
 *
 * @returns     FLCN_STATUS FLCN_OK on successfull exelwtion
 *                                  Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredKmemCheckProtinfoHs_v04_00(void)
{
    FLCN_STATUS status = FLCN_OK;
#ifdef HDCP22_KMEM_ENABLED
    LwU32       data32;
    LwUPtr      kmemProtInfoBase;
    LwU32       kmemProtInfoLength;
    LwUPtr      kmemBaseAddr;
    LwU32       kmemNumKSlt;
    LwU32       kmemKSltLength;
    LwU32       kmemBromUsedSlt;
    LwU32       protInfo;
    LwU32       i;
    LwU64      *pProtInfoBase;

    //
    // TODO: remove the check HAL after adding Initialize and removing regAccess
    // selwreActions.
    //

    CHECK_STATUS(libInterfaceCsbRegRd32ErrChkHs(LW_CGSP_FALCON_KMEMBASE, &kmemBaseAddr));
    CHECK_STATUS(libInterfaceCsbRegRd32ErrChkHs(LW_CGSP_FALCON_KMEMCFG, &data32));

    kmemNumKSlt =  DRF_VAL(_CGSP, _FALCON_KMEMCFG, _NUMOFKSLT, data32);
    kmemKSltLength = DRF_VAL(_CGSP, _FALCON_KMEMCFG, _KSLT_LENGTH, data32);
    kmemProtInfoLength =  DRF_VAL(_CGSP, _FALCON_KMEMCFG, _PROTECTINFO_LENGTH, data32);
    kmemBromUsedSlt = DRF_VAL(_CGSP, _FALCON_KMEMCFG, _BROM_USED_SLOTS, data32);

    // To avoid HS multiplication, we use known length bits with shift operation.
    if ((kmemBromUsedSlt > LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START) ||
        (kmemNumKSlt < (LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START + LW_CGSP_FALCON_KMEM_HDCP_SLOTS_NUMBER)) ||
        (kmemKSltLength != (1 << KMEM_KEYSLOT_LENGTH_BITS)))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto label_return;
    }

    kmemProtInfoBase = kmemBaseAddr + (kmemNumKSlt << KMEM_KEYSLOT_LENGTH_BITS);
    pProtInfoBase = (LwU64*)kmemProtInfoBase;

    // Expecting protection setting.
    protInfo = LW_CGSP_FALCON_KMEM_PROTECTINFO_RESET;
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL0_READ, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL0_WRITE, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL1_READ, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL1_WRITE, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL2_READ, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL2_WRITE, _DISABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL3_READ, _ENABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _LEVEL3_WRITE, _ENABLE, protInfo);
    protInfo = FLD_SET_DRF(_CGSP_FALCON, _KMEM_PROTECTINFO, _PRIV_EN, _DISABLE, protInfo);

    for (i=0; i<LW_CGSP_FALCON_KMEM_HDCP_SLOTS_NUMBER; i++)
    {
        // Check if priv setting (protectInfo.low) modified after initialization.
        if ((LwU32)(pProtInfoBase[LW_CGSP_FALCON_KMEM_HDCP_SLOTS_START + i]) != protInfo)
        {
            status = FLCN_ERR_ILLEGAL_OPERATION;
            goto label_return;
        }
    }

label_return:
#endif
    return status;
}

