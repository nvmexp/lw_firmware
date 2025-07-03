/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @copydoc vbios_frts.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "rmlsfm.h"
#include "ucode_interface.h"
#include "config/g_profile.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objvbios.h"
#include "vbios/vbios_frts.h"
#include "vbios/vbios_frts_access.h"
#include "vbios/vbios_frts_cert.h"
#include "pmu_objpmu.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/*!
 * @brief   Verifies whether the access path to FRTS is secure when expected to
 *          be
 *
 * @return  @ref FLCN_OK                        Success
 * @return  @ref FLCN_ERR_PRIV_SEC_VIOLATION    FRTS access path is not secure
 */
static FLCN_STATUS s_vbiosFrtsSelwreAccessVerify(const VBIOS_FRTS *pFrts)
    GCC_ATTRIB_SECTION("imem_init", "s_vbiosFrtsSelwreAccessVerify");

/*!
 * @brief   Verifies the initial metadata about the overall image descriptor
 *          itself.
 *
 * @param[in]   identifier  The "identifier" field of the
 *                          @ref FW_IMAGE_DESCRIPTOR
 * @param[in]   version     Version of the @ref FW_IMAGE_DESCRIPTOR
 * @param[in]   size        Size of the @ref FW_IMAGE_DESCRIPTOR
 *
 * @return  @ref FLCN_OK                Success.
 * @return  @ref FLCN_ERR_ILWALID_STATE The metadata is inconsistent with what
 *                                      is expected.
 */
static FLCN_STATUS s_vbiosFrtsImageDescriptorMetaVerify(
    LwU32 identifier, LwU32 version, LwU32 size)
    GCC_ATTRIB_SECTION("imem_init", "s_vbiosFrtsImageDescriptorMetaVerify");

/*!
 * @brief   Does initial processing for @ref FW_IMAGE_DESCRIPTOR
 *          @ref LW_FW_IMAGE_DESC_VERSION_VER_1, including:
 *              1.) Preparing it for access.
 *              2.) Verifying any version-specific data.
 *              3.) Extracting the further metadata about the location of the
 *                  other FRTS regions.
 *
 * @param[in,out]   pAccess     Pointer to structure to access FRTS
 * @param[out]      pMetadata   Pointer to structure into which to store FRTS
 *                              metadata
 *
 * @return  @ref FLCN_OK                Success.
 * @return  @ref FLCN_ERR_ILWALID_STATE FRTS state is misconfigured.
 * @return  Others                      Errors propagated from callees.
 */
static FLCN_STATUS s_vbiosFrtsImageDescriptorV1InitialProcess(
    VBIOS_FRTS_ACCESS *pAccess, VBIOS_FRTS_METADATA *pMetadata)
    GCC_ATTRIB_SECTION("imem_init", "s_vbiosFrtsImageDescriptorV1InitialProcess");

/*!
 * @brief   Verifies that the metadata about locations of data in FRTS is sane
 *          and valid.
 *
 * @param[in]   pMetadata       Pointer to metadata to verify
 * @param[in]   descriptorSize  Size of the image descriptor header
 *
 * @return  @ref FLCN_OK                Success.
 * @return  @ref FLCN_ERR_ILWALID_STATE Metadata is inconsistent.
 */
static FLCN_STATUS s_vbiosFrtsMetadataVerify(
        const VBIOS_FRTS_METADATA *pMetadata, LwU32 descriptorSize)
    GCC_ATTRIB_SECTION("imem_init", "s_vbiosFrtsMetadataVerify");

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS
vbiosFrtsConstructAndVerify
(
    VBIOS_FRTS *pFrts
)
{
    FLCN_STATUS status;
    LwU32 dmaIdx;
    LwU32 descriptorIdentifier;
    LwU32 descriptorSize;
    LwU32 descriptorVersionField;
    LwU32 descriptorVersion;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pFrts != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsConstructAndVerify_exit);

    //
    // If the version is set to INVALID, FRTS is not available, so exit early,
    // without an error.
    //
    if (pFrts->config.version == VBIOS_FRTS_CONFIG_VERSION_ILWALID)
    {
        goto vbiosFrtsConstructAndVerify_exit;
    }

    //
    // Get the DMA index to use to access FRTS based on the media type. Assume
    // that:
    //  1.) If FRTS is in FB, it can be accessed with the same settings as the
    //      ucode.
    //  2.) If FRTS is in sysmem, it can be accessed via the coherent sysmem DMA
    //      index.
    //
    switch (pFrts->config.mediaType)
    {
        case VBIOS_FRTS_CONFIG_MEDIA_TYPE_FB:
            dmaIdx = RM_PMU_DMAIDX_UCODE;
            break;
        case VBIOS_FRTS_CONFIG_MEDIA_TYPE_SYSMEM:
            dmaIdx = RM_PMU_DMAIDX_PHYS_SYS_COH;
            break;
        default:
            dmaIdx = RM_PMU_DMAIDX_END;
            break;
    }
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (dmaIdx != RM_PMU_DMAIDX_END),
        FLCN_ERR_ILWALID_STATE,
        vbiosFrtsConstructAndVerify_exit);

    // First verify that FRTS can be accessed via the necessary DMA index
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosFrtsConfigDmaIdxVerify_HAL(&Vbios, &pFrts->config, dmaIdx),
        vbiosFrtsConstructAndVerify_exit);

    //
    // Next, verify that if the PMU is running LS mode that we're actually going
    // to be accessing FRTS selwrely.
    //
    if (Pmu.bLSMode)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_vbiosFrtsSelwreAccessVerify(pFrts),
            vbiosFrtsConstructAndVerify_exit);
    }

    // Construct any state necessary for accessing FRTS next.
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosFrtsAccessConstruct(
            &pFrts->access, &pFrts->config, dmaIdx),
        vbiosFrtsConstructAndVerify_exit);

    //
    // Prepare the metadata in the image descriptor header for retrieval and
    // then retrieve it.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosFrtsAccessImageDescriptorMetaPrepare(&pFrts->access),
        vbiosFrtsConstructAndVerify_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosFrtsAccessImageDescriptorMetaGet(
            &pFrts->access,
            &descriptorIdentifier,
            &descriptorVersionField,
            &descriptorSize),
        vbiosFrtsConstructAndVerify_exit);

    //
    // The version is technically only a subset of the version field in the
    // structure, so extract that.
    //
    descriptorVersion =
        REF_VAL(LW_FW_IMAGE_DESC_VERSION_VER, descriptorVersionField);

    //
    // Verify that the initial metadata is sane before we try to retrieve the
    // rest of the image descriptor
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_vbiosFrtsImageDescriptorMetaVerify(
            descriptorIdentifier,
            descriptorVersion,
            descriptorSize),
        vbiosFrtsConstructAndVerify_exit);

    // Extract the data from the version-specific image descriptor.
    switch (descriptorVersion)
    {
        case LW_FW_IMAGE_DESC_VERSION_VER_1:
            status = s_vbiosFrtsImageDescriptorV1InitialProcess(
                &pFrts->access, &pFrts->metadata);
            break;
        default:
            status = FLCN_ERR_ILWALID_STATE;
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        vbiosFrtsConstructAndVerify_exit);

    // Verify the metadaa is sane.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_vbiosFrtsMetadataVerify(&pFrts->metadata, descriptorSize),
        vbiosFrtsConstructAndVerify_exit);

    // Verify CERT-version specific parameters.
    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_CERT30))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsCert30Verify(&pFrts->metadata),
            vbiosFrtsConstructAndVerify_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_NOT_SUPPORTED,
            vbiosFrtsConstructAndVerify_exit);
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosFrtsAccessVdpaEntriesPrepare(&pFrts->access, &pFrts->metadata),
        vbiosFrtsConstructAndVerify_exit);

vbiosFrtsConstructAndVerify_exit:
    return status;
}

FLCN_STATUS
vbiosFrtsDirtParse
(
    VBIOS_FRTS *pFrts,
    VBIOS_DIRT *pDirt
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pFrts != NULL) &&
        (pDirt != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        vbiosFrtsDirtParse_exit);

    //
    // If the version is set to INVALID, FRTS is not available, so exit early,
    // without an error, leaving the VBIOS_DIRT structure unpopulated.
    //
    if (pFrts->config.version == VBIOS_FRTS_CONFIG_VERSION_ILWALID)
    {
        goto vbiosFrtsDirtParse_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_VBIOS_FRTS_CERT30))
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            vbiosFrtsCert30DirtParse(pDirt, &pFrts->metadata, &pFrts->access),
            vbiosFrtsDirtParse_exit);
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            FLCN_ERR_NOT_SUPPORTED,
            vbiosFrtsDirtParse_exit);
    }

vbiosFrtsDirtParse_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
static FLCN_STATUS
s_vbiosFrtsSelwreAccessVerify
(
    const VBIOS_FRTS *pFrts
)
{
    FLCN_STATUS status;
    LwU64 frtsWprOffset;
    LwU64 frtsWprSize;

    // First, verify that we're actually going to be going through secure WPR
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pFrts->config.mediaType == VBIOS_FRTS_CONFIG_MEDIA_TYPE_FB) &&
        (pFrts->config.wprId != LSF_UNPROTECTED_REGION_ID) &&
        (pFrts->config.wprId != LSF_VPR_REGION_ID),
        FLCN_ERR_PRIV_SEC_VIOLATION,
        s_vbiosFrtsSelwreAccessVerify_exit);

    //
    // Next, verify that FRTS config is actually pointing inside of the FRTS WPR
    // region
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosFrtsSubWprRangeGet_HAL(
            &Vbios, pFrts->config.wprId, &frtsWprOffset, &frtsWprSize),
        s_vbiosFrtsSelwreAccessVerify_exit);
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pFrts->config.offset >= frtsWprOffset) &&
        (pFrts->config.size <= frtsWprSize),
        FLCN_ERR_PRIV_SEC_VIOLATION,
        s_vbiosFrtsSelwreAccessVerify_exit);

    // Verify that the access via WPR is actually secure
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosFrtsWprAccessSelwreVerify_HAL(
            &Vbios, pFrts->config.wprId, frtsWprOffset, frtsWprSize),
        s_vbiosFrtsSelwreAccessVerify_exit);

s_vbiosFrtsSelwreAccessVerify_exit:
    return status;
}

static FLCN_STATUS
s_vbiosFrtsImageDescriptorMetaVerify
(
    LwU32 identifier,
    LwU32 version,
    LwU32 size
)
{
    FLCN_STATUS status;

    //
    // First, verify that we're actually looking at FRTS by checking the initial
    // signature
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (identifier == LW_FW_IMAGE_DESC_IDENTIFIER_SIG),
        FLCN_ERR_ILWALID_STATE,
        s_vbiosFrtsImageDescriptorMetaVerify_exit);

    //
    // Find the version (erroring out if it's not recognized) and verify the
    // expected size for the given version.
    //
    switch (version)
    {
        case LW_FW_IMAGE_DESC_VERSION_VER_1:
            status = (size == sizeof(FW_IMAGE_DESCRIPTOR)) ?
                FLCN_OK : FLCN_ERR_ILWALID_STATE;
            break;
        default:
            status = FLCN_ERR_ILWALID_STATE;
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        s_vbiosFrtsImageDescriptorMetaVerify_exit);

s_vbiosFrtsImageDescriptorMetaVerify_exit:
    return status;
}

static FLCN_STATUS
s_vbiosFrtsImageDescriptorV1InitialProcess
(
    VBIOS_FRTS_ACCESS *pAccess,
    VBIOS_FRTS_METADATA *pMetadata
)
{
    FLCN_STATUS status;
    const FW_IMAGE_DESCRIPTOR *pDescriptor;

    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosFrtsAccessImageDescriptorV1Prepare(pAccess),
        s_vbiosFrtsImageDescriptorV1InitialProcess_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosFrtsAccessImageDescriptorV1Get(pAccess, &pDescriptor),
        s_vbiosFrtsImageDescriptorV1InitialProcess_exit);

    // Ensure that the copy of the data successfully completed
    PMU_ASSERT_TRUE_OR_GOTO(status,
         FLD_TEST_REF(
            LW_FW_IMAGE_DESC_FLAG_COPY_COMPLETE, _YES, pDescriptor->flag),
         FLCN_ERR_ILWALID_STATE,
         s_vbiosFrtsImageDescriptorV1InitialProcess_exit);

    // Extract the metadata from the descriptor
    pMetadata->vdpaEntryOffset = pDescriptor->vdpa_entry_offset;
    pMetadata->vdpaEntryCount = pDescriptor->vdpa_entry_count;
    pMetadata->vdpaEntrySize = pDescriptor->vdpa_entry_size;
    pMetadata->imageOffset = pDescriptor->fw_Image_offset;
    pMetadata->imageSize = pDescriptor->fw_Image_size;

s_vbiosFrtsImageDescriptorV1InitialProcess_exit:
    return status;
}

static FLCN_STATUS
s_vbiosFrtsMetadataVerify
(
    const VBIOS_FRTS_METADATA *pMetadata,
    LwU32 descriptorSize
)
{
    FLCN_STATUS status;

    //
    // Verify a few additional properites of the memory region:
    // 1.) The sub-regions are laid out as expected:
    //      Image descriptor    (aligned up)
    //      VDPA entries        (aligned up)
    //      VBIOS image 
    // 2.) That the FRTS memory region is smaller than the maximum expected
    //     size.
    // 3.) That the end of the image is aligned to the sub-region alignment.
    //
    const LwLength vdpaExpectedOffset = LW_ALIGN_UP(
        descriptorSize, VBIOS_FRTS_SUB_REGION_ALIGNMENT);
    const LwLength vdpaExpectedSize = LW_ALIGN_UP(
        pMetadata->vdpaEntrySize * pMetadata->vdpaEntryCount,
        VBIOS_FRTS_SUB_REGION_ALIGNMENT);
    const LwLength fwImageExpectedOffset =
        vdpaExpectedOffset + vdpaExpectedSize;
    const LwLength frtsRegionSize =
        pMetadata->imageOffset + pMetadata->imageSize;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pMetadata->vdpaEntryOffset == vdpaExpectedOffset) &&
        (pMetadata->imageOffset == fwImageExpectedOffset) &&
        (frtsRegionSize <= PROFILE_FRTS_MAX_SIZE) &&
        LW_IS_ALIGNED(frtsRegionSize, VBIOS_FRTS_SUB_REGION_ALIGNMENT),
        FLCN_ERR_ILWALID_STATE,
        s_vbiosFrtsMetadataVerify_exit);

s_vbiosFrtsMetadataVerify_exit:
    return status;
}
