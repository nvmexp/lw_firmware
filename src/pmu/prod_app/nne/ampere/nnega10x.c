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
 * @file    nnega10x.c
 * @brief   Neural-net engine HAL's
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_oslayer.h"
#include "objnne.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_csb_addendum.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_nne_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Arguments needed to trigger an inference in LW_CPWR_THERM_DLPPE_NNET
 */
typedef struct
{
    /*!
     * @brief   Trigger params shared by all inference triggers
     */
    NNE_SYNC_TRIGGER_PARAMS super;

    /*!
     * @brief   Value to be written to LW_CPWR_THERM_DLPPE_NNET
     */
    LwU32                   nnet;
} NNE_SYNC_TRIGGER_PARAMS_AMPERE;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
//
// This is defined only as a stopgap solution to unblock dev while
// it is dislwssed if this macro would be useful to add into the manuals
//
#define LW_CPWR_THERM_DLPPE_DSCR_ADDR__SIZE_TMP  (LW_CPWR_THERM_DLPPE_DSCR_ADDR_RAM_ADDR_MAX + 4)
#define LW_CPWR_THERM_DLPPE_DSCR_ADDR_HEADER__SIZE_TMP                       (4)
#define LW_CPWR_THERM_DLPPE_DSCR_DATA_NETWORK_HEADER__SIZE_TMP               (4)
#define LW_CPWR_THERM_DLPPE_DSCR_DATA_LAYER_ENTRY__SIZE_TMP                 (16)

#define LW_CPWR_THERM_DLPPE_SWZL_ADDR__SIZE_TMP   (LW_CPWR_THERM_DLPPE_SWZL_ADDR_RAM_ADDR_MAX + 4)

#define LW_CPWR_THERM_DLPPE_PARM_ADDR__SIZE_TMP   (LW_CPWR_THERM_DLPPE_PARM_ADDR_RAM_ADDR_MAX + 4)

#define LW_CPWR_THERM_DLPPE_WEIGHT_ADDR__SIZE_TMP  (LW_CPWR_THERM_DLPPE_WEIGHT_ADDR_RAM_ADDR_MAX + 4)

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_nneRamAutoIncrAccess_GA10X(LwU32 dataReg, LwU32 *pData,
                    LwU32 copySize, LwBool bWrite)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneRamAutoIncrAccess_GA10X");

/*!
 * @brief   Ensures that the weight RAM allocated for this layer is sufficient
 *          for the provided parameters for the layer.
 *
 * @param[in]   numNeurons          Number of neurons for the layer
 * @param[in]   numWeightsPerNeuron Number of weights for each neuron in the
 *                                  layer
 * @param[in]   weightType          Weight type for the layer
 * @param[in]   weightRamSize       Size of weight RAM allocation
 */
static FLCN_STATUS s_nneDescLayerEntryWeightRamSizeValidate_GA10X(LwU32 numNeurons, LwU32 numWeightsPerNeuron, LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_ENUM weightType, LwU32 weightRamSize)
    GCC_ATTRIB_SECTION("imem_nne", "s_nneDescLayerEntryWeightRamSizeValidate_GA10X");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Get the size of the descriptor RAM, in bytes.
 *
 * @return Size of the descriptor RAM, in bytes.
 */
LwU32
nneDescRamSizeGet_GA10X(void)
{
    return LW_CPWR_THERM_DLPPE_DSCR_ADDR__SIZE_TMP;
}

/*!
 * @brief Get the size of the descriptor RAM header, in bytes.
 *
 * The descriptor RAM header contains information about the DLC, such as
 * version. This should not be confused with @ref nneDescRamNetworkHeaderSizeGet_GA10X
 * that holds per neural-network metadata.
 *
 * @return Size of the descriptor RAM header.
 */
LwU8
nneDescRamHeaderSizeGet_GA10X(void)
{
    return LW_CPWR_THERM_DLPPE_DSCR_ADDR_HEADER__SIZE_TMP;
}

/*!
 * @brief Get the size of the network header, in bytes.
 *
 * The network header is per neural-network metadata such as the number of layers
 * in the neural-net. This should not be confused with @ref nneDescRamHeaderSizeGet_GA10x
 * which returns information about the DLC itself.
 *
 * @return Size of a neural-network descriptor's header size, in bytes.
 */
LwU8
nneDescRamNetworkHeaderSizeGet_GA10X(void)
{
    return LW_CPWR_THERM_DLPPE_DSCR_DATA_NETWORK_HEADER__SIZE_TMP;
}

/*!
 * @brief Gets the size of a single layer entry in the descriptor RAM, in bytes.
 *
 * @return Gets the number of bytes for a single layer's entry in the descriptor RAM, in bytes.
 */
LwU8
nneDescRamLayerEntrySizeGet_GA10X(void)
{
    return LW_CPWR_THERM_DLPPE_DSCR_DATA_LAYER_ENTRY__SIZE_TMP;
}

/*!
 * @brief Get the DLC-compatible binary representation of an NNE_DESC's metadata.
 *
 * @param[IN]  pDesc                   NNE_DESC to get the binary representation for
 * @param[OUT] pData                   Buffer to write the binary data out to.
 * @param[IN]  size                    Size, in bytes, of the buffer passed in via @pData
 * @param[IN]  paramRamStatusOffset    Offset into the parameter RAM that @pdDesc should
 *                                     write its per inference loop status to.
 *
 * @return FLCN_ERR_ILWALID ARGUMENT   If @ref pDesc or @ref pBuf are NULL.
 * @return FLCN_ERR_NO_FREE_MEM        If the layer header size is smaller than @ref size.
 */
FLCN_STATUS
nneDescNetworkHeaderBinaryFormatGet_GA10X
(
    NNE_DESC   *pDesc,
    LwU32      *pBuf,
    LwU32       size,
    LwU8        loopMax,
    LwU32       paramRamStatusOffset
)
{
    LwU32       numLayers;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pDesc == NULL) ||
        (pBuf == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescNetworkHeaderBinaryFormatGet_GA10X_exit;
    }

    if (nneDescRamNetworkHeaderSizeGet_HAL() > size)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto nneDescNetworkHeaderBinaryFormatGet_GA10X_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescNumLayersGet(pDesc, &numLayers),
        nneDescNetworkHeaderBinaryFormatGet_GA10X_exit);

    // Populate the buffer.
    pBuf[0] = DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_DATA, _NETWORK_HEADER_NUM_LAYERS,    numLayers)     |
              DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_DATA, _NETWORK_HEADER_LOOP_MAX,      (loopMax - 1)) |
              DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_DATA, _NETWORK_HEADER_STATUS_OFFSET, paramRamStatusOffset);

nneDescNetworkHeaderBinaryFormatGet_GA10X_exit:
    return status;
}

/*!
 * @brief Get the DLC-compatible binary representation of a layer's metadata.
 *
 * @param[IN]  pLayer                 Layer that we wish to get the binary representation of.
 * @param[OUT] pBuf                   Buffer to write the binary representation to.
 * @param[IN]  size                   Size, in bytes, of the buffer passed in via @ref pBuf.
 * @param[IN]  swzlRamOffset          Offset into the swizzle RAM that the layer's swizzle resides.
 * @param[IN]  paraRamOutputOffset    Offset into the parameter RAM that this layer should output to.
 * @param[IN]  bOutputStride          Enable/disable output stride.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pLayer or @ref pBuf are NULL.
 * @return FLCN_ERR_NO_FREE_MEM        If the passed in buffer is insufficiently large.
 * @return FLCN_ERR_ILWALID_STATE      If @ref pLayer has invalid state.
 * @return FLCN_OK                     If the binary representation of @pLayer was
 *                                     sucessfully written to @ref pBuf.
 */
FLCN_STATUS
nneDescLayerEntryBinaryFormatGet_GA10X
(
    NNE_LAYER   *pLayer,
    LwU32       *pBuf,
    LwU32        size,
    LwU32        swzlRamOffset,
    LwU32        parmRamOutputOffset,
    LwBool       bOutputStride
)
{
    LwU32       numNeurons;
    LwU32       numWeightsPerNeuron;
    LwU32       weightRamOffset;
    LwU32       weightRamSize;
    LwU32       activationFunctionOffset;
    LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_ENUM
                weightType;
    LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_ENUM
                activationFunction;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pLayer == NULL) ||
        (pBuf   == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescLayerEntryBinaryFormatGet_GA10X_exit;
    }

    if (nneDescRamLayerEntrySizeGet_HAL() > size)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto nneDescLayerEntryBinaryFormatGet_GA10X_exit;
    }

    // Get all the layer parameters.
    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerNumNeuronsGet(pLayer, &numNeurons),
        nneDescLayerEntryBinaryFormatGet_GA10X_exit);

    //
    // The number of inputs to a layer is the same as the number of weights for
    // each neuron, so retrieve that number here.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerNumWeightsPerNeuronGet(pLayer, &numWeightsPerNeuron),
        nneDescLayerEntryBinaryFormatGet_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerWeightRamOffsetGet(pLayer, &weightRamOffset),
        nneDescLayerEntryBinaryFormatGet_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerWeightRamSizeGet(pLayer, &weightRamSize),
        nneDescLayerEntryBinaryFormatGet_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerWeightTypeGet(pLayer, &weightType),
        nneDescLayerEntryBinaryFormatGet_GA10X_exit);

    //
    // Validate that the size that the to-be-programmed values imply for the
    // weight RAM match the size actually allocated in the weight RAM.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        s_nneDescLayerEntryWeightRamSizeValidate_GA10X(
            numNeurons, numWeightsPerNeuron, weightType, weightRamSize),
        nneDescLayerEntryBinaryFormatGet_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerActivationFunctionOffsetGet(pLayer, &activationFunctionOffset),
        nneDescLayerEntryBinaryFormatGet_GA10X_exit);

    // Prepare the buffer.
    pBuf[0] = DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D0_NUM_NEURONS,       numNeurons)          |
              DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D0_NUM_INPUTS,        numWeightsPerNeuron);
    pBuf[1] = DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D1_SWIZZLE_OFFSET,    swzlRamOffset)       |
              DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D1_WEIGHT_OFFSET,     weightRamOffset);
    pBuf[2] = DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D2_OUTPUT_OFFSET,     parmRamOutputOffset) |
              DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D2_ACTIVATION_OFFSET, activationFunctionOffset);
    pBuf[3] = DRF_DEF(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D3_WEIGHT_STRIDE,     _DISABLE);

    if (bOutputStride)
    {
        pBuf[3] = FLD_SET_DRF(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D3_OUTPUT_STRIDE, _ENABLE, pBuf[3]);
    }
    else
    {
        pBuf[3] = FLD_SET_DRF(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D3_OUTPUT_STRIDE, _DISABLE, pBuf[3]);
    }

    switch (weightType)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_FP16:
            pBuf[3] = FLD_SET_DRF(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D3_WEIGHT_FORMAT, _FP16, pBuf[3]);
            break;
        case LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_FP32:
            pBuf[3] = FLD_SET_DRF(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D3_WEIGHT_FORMAT, _FP32, pBuf[3]);
            break;
        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneDescLayerEntryBinaryFormatGet_GA10X_exit;
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        nneLayerActivationFunctionGet(pLayer, &activationFunction),
        nneDescLayerEntryBinaryFormatGet_GA10X_exit);

    switch (activationFunction)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_IDENTITY:
            pBuf[3] = FLD_SET_DRF(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D3_ACTIVATION_SEL, _IDENTITY,   pBuf[3]);
            break;
        case LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_RELU:
            pBuf[3] = FLD_SET_DRF(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D3_ACTIVATION_SEL, _RELU,       pBuf[3]);
            break;
        case LW2080_CTRL_NNE_NNE_LAYER_FC_10_ACTIVATION_FUNCTION_TYPE_LEAKY_RELU:
            pBuf[3] = FLD_SET_DRF(_CPWR_THERM, _DLPPE_DSCR_DATA, _LAYER_D3_ACTIVATION_SEL, _LEAKY_RELU, pBuf[3]);
            break;
        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_ILWALID_STATE;
            goto nneDescLayerEntryBinaryFormatGet_GA10X_exit;
            break;
    }

nneDescLayerEntryBinaryFormatGet_GA10X_exit:
    return status;
}

/*!
 * @brief Configure the DLC with the number of descriptors loaded into its RAMs.
 *
 * @param[IN] numDesc   Number of descriptors to prorgam into the DLC.
 *
 * @return FLCN_ERR_OUT_OF_RANGE   If the number of descriptors is beyond what is
 *                                 supported by the DLC.
 * @return FLCN_OK                 If the DLC has been configured with the new number
 *                                 of descriptors.
 */
FLCN_STATUS
nneDescRamNumDescLoadedSet_GA10X
(
    LwU8 numDesc
)
{
    LwU32       data;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if (numDesc > LW_CPWR_THERM_DLPPE_DSCR_DATA_HEADER_NUM_NETWORKS_MAX)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto nneDescRamNumDescLoadedSet_GA10X_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescRamAccess_HAL(&Nne,
                             &data,
                             LW_CPWR_THERM_DLPPE_DSCR_ADDR_RAM_ADDR_HEADER,
                             sizeof(data),
                             LW_FALSE),
        nneDescRamNumDescLoadedSet_GA10X_exit);

    data = FLD_SET_DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_DATA, _HEADER_NUM_NETWORKS, numDesc, data);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneDescRamAccess_HAL(&Nne,
                             &data,
                             LW_CPWR_THERM_DLPPE_DSCR_ADDR_RAM_ADDR_HEADER,
                             sizeof(data),
                             LW_TRUE),
        nneDescRamNumDescLoadedSet_GA10X_exit);

nneDescRamNumDescLoadedSet_GA10X_exit:
    return status;
}

/*!
 * @brief   Burst read or write a portion of the descriptor RAM.
 *
 * @param[IN]   pData       @ref NNE_DESC to load.
 * @param[IN]   ramOffset   Offset into the descriptor RAM to load the descriptor.
 * @param[IN]   copySize    Size, in bytes, of the data to be written to be descriptor RAM.
 * @param[IN]   bWrite      Whether to write the RAM or read it
 *
 * @return  FLCN_ERR_OUT_OF_RANGE   If the access range extends over the end of
 *                                  the parameter RAM.
 * @return other errors             If internal sanity checking failed.
 * @return FLCN_OK                  If the descriptor RAM was successfully written to.
 */
FLCN_STATUS
nneDescRamAccess_GA10X
(
    LwU32   *pData,
    LwU32    ramOffset,
    LwU32    copySize,
    LwBool   bWrite
)
{
    LwU32       config;
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    if (!LW_IS_ALIGNED(ramOffset, sizeof(LwU32)))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescRamAccess_GA10X_exit;
    }

    if (nneDescRamSizeGet_HAL() < (ramOffset + copySize))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto nneDescRamAccess_GA10X_exit;
    }

    // Setup auto-incrementing read/write.
    config = DRF_NUM(_CPWR_THERM, _DLPPE_DSCR_ADDR, _RAM_ADDR,      ramOffset) |
             DRF_DEF(_CPWR_THERM, _DLPPE_DSCR_ADDR, _RAM_RAUTOINCR, _DISABLE)  |
             DRF_DEF(_CPWR_THERM, _DLPPE_DSCR_ADDR, _RAM_WAUTOINCR, _ENABLE);
    REG_WR32(CSB, LW_CPWR_THERM_DLPPE_DSCR_ADDR, config);

    // Burst read or write the data.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_nneRamAutoIncrAccess_GA10X(LW_CPWR_THERM_DLPPE_DSCR_DATA, pData,
                                     copySize, bWrite),
        nneDescRamAccess_GA10X_exit);

nneDescRamAccess_GA10X_exit:
    return status;
}

/*!
 * @brief Get the size of the swizzle RAM, in bytes.
 *
 * @return Size, in bytes, of the swizzle RAM.
 */
LwU32
nneSwzlRamSizeGet_GA10X(void)
{
    return LW_CPWR_THERM_DLPPE_SWZL_ADDR__SIZE_TMP;
}

/*!
 * @brief Get the binary format of a set of sizzles.
 *
 * @param[IN]  pSwzl     Array of swizzles to get the DLC-compatible binary representation of.
 * @param[OUT] pBuf      Buffer to store the binary to.
 * @param[IN]  numSwzl   Number of swizzles to get the binary representation of.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pSwzl or @ref pBuf are NULL
 * @return FLCN_OK                     If all swizzles were successfully translated to binary representation.
 */
FLCN_STATUS
nneSwzlBinaryFormatGet_GA10X
(
    NNE_SWZL   *pSwzl,
    LwU32      *pBuf,
    LwU32       numSwzl
)
{
    LwU32       swzlIdx;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if ((pSwzl == NULL) ||
        (pBuf == NULL))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneSwzlBinaryFormatGet_GA10X_exit;
    }

    for (swzlIdx = 0; swzlIdx < numSwzl; swzlIdx++)
    {
        pBuf[swzlIdx] = DRF_NUM(_CPWR_THERM, _DLPPE_SWZL_DATA, _INPUT_POINTER, pSwzl[swzlIdx].parmRamOffset) |
                        DRF_NUM(_CPWR_THERM, _DLPPE_SWZL_DATA, _INPUT_STRIDE,  pSwzl[swzlIdx].stride);
    }

nneSwzlBinaryFormatGet_GA10X_exit:
    return status;
}

/*!
 * @brief Burst write a portion of the swizzle RAM.
 *
 * @param[IN]   pData       Array of dwords hodling the DLC compatible binary representation of
 *                          swizzles to write to swizzle RAM.
 * @param[IN]   ramOffset   Offset into the swizzle RAM to write the swizzles to.
 * @parma[IN]   copySize    Size, in bytes, of the data to be written to swizzle RAM.
 *
 * @return  FLCN_ERR_OUT_OF_RANGE   If the access range extends over the end of
 *                                  the swizzle RAM.
 * @return other errors             If internal sanity checking failed.
 * @return FLCN_OK                  If the swizzle RAM was successfully written to.
 */
FLCN_STATUS
nneSwzlRamWrite_GA10X
(
    LwU32   *pData,
    LwU32    ramOffset,
    LwU32    copySize
)
{
    LwU32       config;
    FLCN_STATUS status = FLCN_OK;

    // Sanity checking.
    if (pData == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneSwzlRamWrite_GA10X_exit;
    }

    if ((nneSwzlRamSizeGet_HAL() < copySize) ||
        (nneSwzlRamSizeGet_HAL() - copySize < ramOffset))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_NO_FREE_MEM;
        goto nneSwzlRamWrite_GA10X_exit;
    }

    // Setup auto-incrementing read/write.
    config = DRF_NUM(_CPWR_THERM, _DLPPE_SWZL_ADDR, _RAM_ADDR,      ramOffset) |
             DRF_DEF(_CPWR_THERM, _DLPPE_SWZL_ADDR, _RAM_RAUTOINCR, _DISABLE)  |
             DRF_DEF(_CPWR_THERM, _DLPPE_SWZL_ADDR, _RAM_WAUTOINCR, _ENABLE);
    REG_WR32(CSB, LW_CPWR_THERM_DLPPE_SWZL_ADDR, config);

    // Burst read or write the data.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_nneRamAutoIncrAccess_GA10X(LW_CPWR_THERM_DLPPE_SWZL_DATA, pData,
                                     copySize, LW_TRUE),
        nneSwzlRamWrite_GA10X_exit);

nneSwzlRamWrite_GA10X_exit:
    return status;
}

/*!
 * @brief Get the size of the parameter RAM, in bytes.
 *
 * @return Size, in bytes, of the parameter RAM.
 */
LwU32
nneParmRamSizeGet_GA10X(void)
{
    return LW_CPWR_THERM_DLPPE_PARM_ADDR__SIZE_TMP;
}

/*!
 * @brief Burst write a portion of the descriptor RAM.
 *
 * @param[IN]   pData       @ref NNE_DESC to load.
 * @param[IN]   ramOffset   Offset into the descriptor RAM to load the descriptor.
 * @parma[IN]   copySize    Size, in bytes, of the data to be written to be descriptor RAM.
 *
 * @return  FLCN_ERR_OUT_OF_RANGE   If the access range extends over the end of
 *                                  the parameter RAM.
 * @return other errors             If internal sanity checking failed.
 * @return FLCN_OK                  If the descriptor RAM was successfully written to.
 */
FLCN_STATUS
nneParmRamAccess_GA10X
(
    LwU32   *pData,
    LwU32    ramOffset,
    LwU32    copySize,
    LwBool   bWrite
)
{
    LwU32         config;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity check.
    if (LW_ALIGN_DOWN(ramOffset, sizeof(LwU32)) != ramOffset)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneParmRamAccess_GA10X_exit;
    }

    if ((nneParmRamSizeGet_HAL() < copySize) ||
        ((nneParmRamSizeGet_HAL() - copySize) < ramOffset))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto nneParmRamAccess_GA10X_exit;
    }

    // Setup auto-incrementing read/write.
    config = DRF_NUM(_CPWR_THERM, _DLPPE_PARM_ADDR, _RAM_ADDR, ramOffset);
    if (bWrite)
    {
        config = FLD_SET_DRF(_CPWR_THERM, _DLPPE_PARM_ADDR, _RAM_RAUTOINCR, _DISABLE, config);
        config = FLD_SET_DRF(_CPWR_THERM, _DLPPE_PARM_ADDR, _RAM_WAUTOINCR, _ENABLE, config);
    }
    else
    {
        config = FLD_SET_DRF(_CPWR_THERM, _DLPPE_PARM_ADDR, _RAM_RAUTOINCR, _ENABLE, config);
        config = FLD_SET_DRF(_CPWR_THERM, _DLPPE_PARM_ADDR, _RAM_WAUTOINCR, _DISABLE, config);
    }
    REG_WR32(CSB, LW_CPWR_THERM_DLPPE_PARM_ADDR, config);

    // Burst read or write the data.
    PMU_ASSERT_OK_OR_GOTO(status,
        s_nneRamAutoIncrAccess_GA10X(LW_CPWR_THERM_DLPPE_PARM_DATA, pData,
                                     copySize, bWrite),
        nneParmRamAccess_GA10X_exit);

nneParmRamAccess_GA10X_exit:
    return status;

}

/*!
 * @brief Get the size of the weight RAM, in bytes.
 *
 * @return Size of the weight RAM, in bytes.
 */
LwU32
nneWeightRamSizeGet_GA10X(void)
{
    return LW_CPWR_THERM_DLPPE_WEIGHT_ADDR__SIZE_TMP;
}

/*!
 * @brief Burst write a portion of the weight RAM.
 *
 * @param[IN]   pData       @ref NNE_DESC to load.
 * @param[IN]   ramOffset   Offset into the weight RAM to load the weights.
 * @parma[IN]   copySize    Size, in bytes, of the data to be written to be weight RAM.
 *
 * @return  FLCN_ERR_OUT_OF_RANGE   If the access range extends over the end of
 *                                  the parameter RAM.
 * @return other errors             If internal sanity checking failed.
 * @return FLCN_OK                  If the descriptor RAM was successfully written to.
 */
FLCN_STATUS
nneWeightRamWrite_GA10X
(
    LwU8    *pData,
    LwU32    ramOffset,
    LwU32    copySize
)
{
    LwU32         config;
    LwU32         byteInDword;
    LwU32         dword  = 0;
    LwU32         idx    = 0;
    FLCN_STATUS   status = FLCN_OK;

    // Sanity checking.
    if (pData == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneWeightRamWrite_GA10X_exit;
    }

    if ((copySize  > nneWeightRamSizeGet_HAL()) ||
        (ramOffset > (nneWeightRamSizeGet_HAL() - copySize)))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto nneWeightRamWrite_GA10X_exit;
    }

    // Setup auto-incrementing read/write.
    config = DRF_NUM(_CPWR_THERM, _DLPPE_WEIGHT_ADDR, _RAM_ADDR,      LW_ALIGN_DOWN(ramOffset, sizeof(LwU32))) |
             DRF_DEF(_CPWR_THERM, _DLPPE_WEIGHT_ADDR, _RAM_RAUTOINCR, _DISABLE)  |
             DRF_DEF(_CPWR_THERM, _DLPPE_WEIGHT_ADDR, _RAM_WAUTOINCR, _ENABLE);
    REG_WR32(CSB, LW_CPWR_THERM_DLPPE_WEIGHT_ADDR, config);

    //
    // If the starting RAM offset is not DWORD aligned, we need to pre-load
    // the dword we're going to build up with the dword that is lwrrently at the
    // RAM offset location, with non-relevant bytes zeroed out.
    //
    byteInDword = ramOffset % sizeof(LwU32);
    if (byteInDword != 0)
    {
        dword  = REG_RD32(CSB, LW_CPWR_THERM_DLPPE_WEIGHT_DATA);
        dword &= (LwU32)(LWBIT32(byteInDword * 8) - 1);
    }

    // Build up dwords to write,and write them out every time a dword has been built up.
    for (idx = 0; idx < copySize; idx++)
    {
        byteInDword = (ramOffset + idx) % sizeof(LwU32);
        dword      |= pData[idx] << (byteInDword * 8);

        if (byteInDword == (sizeof(LwU32) - 1))
        {
            REG_WR32(CSB, LW_CPWR_THERM_DLPPE_WEIGHT_DATA, dword);
            dword = 0;
        }
    }

    // Check if we need to write out the last dword.
    if (byteInDword != (sizeof(LwU32) - 1))
    {
        dword |= REG_RD32(CSB, LW_CPWR_THERM_DLPPE_WEIGHT_DATA) & ~(LWBIT32((byteInDword + 1) * 8) - 1);
        REG_WR32(CSB, LW_CPWR_THERM_DLPPE_WEIGHT_DATA, dword);
    }

nneWeightRamWrite_GA10X_exit:
    return status;
}

/*!
 * @brief Trigger inference on the DLC and wait for completion.
 *
 * @param[IN]  nnetId           Neural-net ID that the NNE_DESC has been loaded on in
 *                              the DLC.
 * @param[IN]  loopCnt          Number of inference loops to run.
 * @param[IN]  pInferenceCfg    Inference-specific configuration.
 * @param[OUT] pHwExelwteTime   Time taken to run inference.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT   If @ref pInferenceCfg or @ref pHwExelwtionTime is NULL.
 * @return FLCN_ERR_OUT_OF_RANGE       If @ref nnetId or @ref loopCnt are outside of sane values.
 * @return FLCN_NOT_SUPPORTED          If an invalid rounding mode is detected.
 * @return FLCN_OK                     If HW-based inference successfully returned.
 */
FLCN_STATUS
nneDescTriggerInferenceAndWait_GA10X
(
    LwU8                                         nnetId,
    LwU8                                         loopCnt,
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_CONFIG   *pInferenceCfg,
    LwU64_ALIGN32                               *pHwExelwtionTime
)
{
    LwU32         config;
    LwU8          loopIdx = loopCnt - 1;
    FLCN_STATUS   status  = FLCN_OK;

    // Sanity checking.
    if (pInferenceCfg == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescTriggerInferenceAndWait_GA10X_exit;
    }

    if (nnetId > LW_CPWR_THERM_DLPPE_NNET_ID_REQ_NETWORK__SIZE_1)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto nneDescTriggerInferenceAndWait_GA10X_exit;
    }

    if ((loopIdx < LW_CPWR_THERM_DLPPE_NNET_LOOP_START_MIN) ||
        (loopIdx > LW_CPWR_THERM_DLPPE_NNET_LOOP_START_MAX))
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto nneDescTriggerInferenceAndWait_GA10X_exit;
    }

    if (pHwExelwtionTime == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto nneDescTriggerInferenceAndWait_GA10X_exit;
    }

    // Configure the inference.
    switch (pInferenceCfg->roundingMode)
    {
        case LW2080_CTRL_NNE_NNE_DESC_INFERENCE_ROUNDING_MODE_TO_NEAREST_EVEN:
            config = DRF_DEF(_CPWR_THERM, _DLPPE_CFG, _ROUNDING_MODE, _TO_NEAREST_EVEN);
            break;
        case LW2080_CTRL_NNE_NNE_DESC_INFERENCE_ROUNDING_MODE_TOWARDS_ZERO:
            config = DRF_DEF(_CPWR_THERM, _DLPPE_CFG, _ROUNDING_MODE, _TOWARDS_ZERO);
            break;
        case LW2080_CTRL_NNE_NNE_DESC_INFERENCE_ROUNDING_MODE_TOWARDS_POSITIVE_INFINITY:
            config = DRF_DEF(_CPWR_THERM, _DLPPE_CFG, _ROUNDING_MODE, _TOWARDS_POSITIVE_INFINITY);
            break;
        case LW2080_CTRL_NNE_NNE_DESC_INFERENCE_ROUNDING_MODE_TOWARDS_NEGATIVE_INFINITY:
            config = DRF_DEF(_CPWR_THERM, _DLPPE_CFG, _ROUNDING_MODE, _TOWARDS_NEGATIVE_INFINITY);
            break;
        case LW2080_CTRL_NNE_NNE_DESC_INFERENCE_ROUNDING_MODE_TO_NEAREST_UP:
            config = DRF_DEF(_CPWR_THERM, _DLPPE_CFG, _ROUNDING_MODE, _TO_NEAREST_UP);
            break;
        case LW2080_CTRL_NNE_NNE_DESC_INFERENCE_ROUNDING_MODE_AWAY_FROM_ZERO:
            config = DRF_DEF(_CPWR_THERM, _DLPPE_CFG, _ROUNDING_MODE, _AWAY_FROM_ZERO);
            break;
        default:
            PMU_TRUE_BP();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto nneDescTriggerInferenceAndWait_GA10X_exit;
            break;
    }

    if (pInferenceCfg->bClampInfinityToNormal)
    {
        config = FLD_SET_DRF(_CPWR_THERM, _DLPPE_CFG, _CLAMP_INFINITY_TO_NORMAL, _ENABLE, config);
    }
    else
    {
        config = FLD_SET_DRF(_CPWR_THERM, _DLPPE_CFG, _CLAMP_INFINITY_TO_NORMAL, _DISABLE, config);
    }

    REG_WR32(CSB, LW_CPWR_THERM_DLPPE_CFG, config);

    // Ensure that the DLC is idle, else bail.
    config = REG_RD32(CSB, LW_CPWR_THERM_DLPPE_NNET);
    if (!FLD_TEST_DRF(_CPWR_THERM, _DLPPE_NNET, _ID_REQ, _DONE, config))
    {
        PMU_TRUE_BP();
        status = FLCN_ERROR;
        goto nneDescTriggerInferenceAndWait_GA10X_exit;
    }

    // Trigger the inference.
    config = REG_RD32(CSB, LW_CPWR_THERM_DLPPE_NNET);
    config = FLD_SET_DRF(    _CPWR_THERM, _DLPPE_NNET, _LOOP_START, _MIN,     config);
    config = FLD_SET_DRF_NUM(_CPWR_THERM, _DLPPE_NNET, _LOOP_END,   loopIdx,  config);
    config = FLD_SET_DRF_NUM(_CPWR_THERM, _DLPPE_NNET, _ID_REQ,
                             LW_CPWR_THERM_DLPPE_NNET_ID_REQ_NETWORK(nnetId), config);

    PMU_ASSERT_OK_OR_GOTO(status,
        nneSyncTriggerAndWait(&NneResident.nneSync,
                              &(NNE_SYNC_TRIGGER_PARAMS_AMPERE)
                              {
                                  .super =
                                  {
                                      .pSyncTimeNs = pHwExelwtionTime,
                                  },
                                  .nnet = config,
                              }.super,
                              LWRTOS_TIME_US_TO_TICKS(1000 * 1000)),
        nneDescTriggerInferenceAndWait_GA10X_exit);

nneDescTriggerInferenceAndWait_GA10X_exit:
    return status;
}

/*!
 * @brief   Triggers an inference in the DL co-processor
 *
 * @param[in,out]   pParams Parametesr required for triggering the co-processor
 *
 * @return  FLCN_OK Success
 * @retuen  Others  Failure
 */
FLCN_STATUS
nneDescInferenceTrigger_GA10X
(
    NNE_SYNC_TRIGGER_PARAMS *pParams
)
{
    const NNE_SYNC_TRIGGER_PARAMS_AMPERE *const pParamsAmpere =
        (NNE_SYNC_TRIGGER_PARAMS_AMPERE *)pParams;

    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        appTaskCriticalEnter();
    }

    REG_WR32(CSB, LW_CPWR_THERM_DLPPE_NNET, pParamsAmpere->nnet);

    if (PMUCFG_FEATURE_ENABLED(PMU_NNE_DESC_INFERENCE_PROFILING))
    {
        NNE_DESC_INFERENCE_PROFILE_BEGIN(pParamsAmpere->super.pSyncTimeNs);
        appTaskCriticalExit();
    }

    return FLCN_OK;
}

/*!
 * @brief   Cancel an outstanding inference
 */
void nneDescInferenceCancel_GA10X(void)
{
    const LwU32 nnet = REG_RD32(CSB, LW_CPWR_THERM_DLPPE_NNET);
    REG_WR32(CSB,
             LW_CPWR_THERM_DLPPE_NNET,
             FLD_SET_DRF(_CPWR_THERM, _DLPPE_NNET, _ID_REQ, _DONE, nnet));
}

/*!
 * @brief   Initialize interrupts for NNE descriptor usage.
 */
void
nneDescInterruptInit_GA10X(void)
{
    LwU32 thermIntrRoute;
    LwU32 thermIntrEn0;

    // Read-modify-write interrupt routing to send DLPPE interrupt to PMU.
    thermIntrRoute = REG_RD32(CSB, LW_CPWR_THERM_INTR_ROUTE);
    thermIntrRoute = FLD_SET_DRF(_CPWR_THERM, _INTR_ROUTE, _DLPPE_INTR, _PMU, thermIntrRoute);
    REG_WR32(CSB, LW_CPWR_THERM_INTR_ROUTE, thermIntrRoute);

    // Read-modify-write interrupt enable to enable DLPPE interrupt.
    thermIntrEn0 = REG_RD32(CSB, LW_CPWR_THERM_INTR_EN_0);
    thermIntrEn0 = FLD_SET_DRF(_CPWR_THERM, _INTR_EN_0, _DLPPE_INTR, _ENABLED, thermIntrEn0);
    REG_WR32(CSB, LW_CPWR_THERM_INTR_EN_0, thermIntrEn0);

    // Note: we are assuming the top-level THERM interrupt is alredy enabled.
}

/*!
 * @brief   Service NNE descriptor interrupts.
 */
void
nneDescInterruptService_GA10X(void)
{
    LwU32 thermIntr0 = REG_RD32(CSB, LW_CPWR_THERM_INTR_0);
    if (FLD_TEST_REF(LW_CPWR_THERM_INTR_0_DLPPE_INTR, _PENDING, thermIntr0))
    {
        nneSyncSignalFromISR(&NneResident.nneSync);

        REG_WR32(CSB,
                 LW_CPWR_THERM_INTR_0,
                 REF_DEF(LW_CPWR_THERM_INTR_0_DLPPE_INTR, _CLEAR));
    }
}

/*!
 * @brief   Parse hardware floating point exception state into hardware-agnostic
 *          exceptions.
 *
 * @return  Colwerted hardware floating point exceptions
 */
LW2080_CTRL_NNE_NNE_DESC_INFERENCE_FP_EXCEPTION_FLAGS
nneFpExceptionsParse_GA10X
(
    LwU32 hwExceptions
)
{
    LW2080_CTRL_NNE_NNE_DESC_INFERENCE_FP_EXCEPTION_FLAGS exceptions =
        LW2080_CTRL_NNE_NNE_DESC_INFERENCE_FP_EXCEPTION_NONE;

    if (FLD_TEST_DRF(_CPWR_THERM,
                     _DLPPE_PARM_DATA,
                     _FP_EXCEPTION_ILWALID,
                     _PENDING,
                     hwExceptions))
    {
        exceptions = FLD_SET_DRF(2080_CTRL_NNE,
                                 _NNE_DESC_INFERENCE,
                                 _FP_EXCEPTION_ILWALID,
                                 _PENDING,
                                 exceptions);
    }

    if (FLD_TEST_DRF(_CPWR_THERM,
                     _DLPPE_PARM_DATA,
                     _FP_EXCEPTION_UNDERFLOW,
                     _PENDING,
                     hwExceptions))
    {
        exceptions = FLD_SET_DRF(2080_CTRL_NNE,
                                 _NNE_DESC_INFERENCE,
                                 _FP_EXCEPTION_UNDERFLOW,
                                 _PENDING,
                                 exceptions);
    }

    if (FLD_TEST_DRF(_CPWR_THERM,
                     _DLPPE_PARM_DATA,
                     _FP_EXCEPTION_OVERFLOW,
                     _PENDING,
                     hwExceptions))
    {
        exceptions = FLD_SET_DRF(2080_CTRL_NNE,
                                 _NNE_DESC_INFERENCE,
                                 _FP_EXCEPTION_OVERFLOW,
                                 _PENDING,
                                 exceptions);
    }

    if (FLD_TEST_DRF(_CPWR_THERM,
                     _DLPPE_PARM_DATA,
                     _FP_EXCEPTION_INEXACT,
                     _PENDING,
                     hwExceptions))
    {
        exceptions = FLD_SET_DRF(2080_CTRL_NNE,
                                 _NNE_DESC_INFERENCE,
                                 _FP_EXCEPTION_INEXACT,
                                 _PENDING,
                                 exceptions);
    }

    return exceptions;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Helper function to access DLC RAM setup for auto-incrementing accesses.
 *
 * This helper function assumes that auto-incrementing accesses have been properly
 * configured.
 *
 * @param[IN]     dataReg   CSB address of the data register to repeatedly access.
 * @param[IN/OUT] pData     Buffer to be written to/read from.
 * @param[IN]     copySize  Size, in bytes, of the array passed in by @ref pData.
 * @param[IN]     bWrite    LW_TRUE if @ref pData should be written to parameter RAM,
 *                          LW_FALSE if parameter RAM should be read and copied into @ref pData.
 *
 * @return FLCN_OK   If the data access was successful.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_nneRamAutoIncrAccess_GA10X
(
    LwU32    dataReg,
    LwU32   *pData,
    LwU32    copySize,
    LwBool   bWrite
)
{
    LwU32       numDwords = copySize / sizeof(LwU32);
    LwU32       idx;
    FLCN_STATUS status    = FLCN_OK;

    // Sanity checking.
    if (pData == NULL)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_nneRamAutoIncrAccess_GA10X_exit;
    }

    if (LW_ALIGN_UP(copySize, sizeof(LwU32)) != copySize)
    {
        PMU_TRUE_BP();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_nneRamAutoIncrAccess_GA10X_exit;
    }

    //
    // Do the burst access by repeatedly accessing the data
    // register.
    //
    for (idx = 0; idx < numDwords; idx++)
    {
        if (bWrite)
        {
            REG_WR32(CSB, dataReg, pData[idx]);
        }
        else
        {
            pData[idx] = REG_RD32(CSB, dataReg);
        }
    }

s_nneRamAutoIncrAccess_GA10X_exit:
    return status;
}

/*!
 * @copydoc s_nneDescLayerEntryWeightRamSizeValidate_GA10X
 */
static FLCN_STATUS
s_nneDescLayerEntryWeightRamSizeValidate_GA10X
(
    LwU32 numNeurons,
    LwU32 numWeightsPerNeuron,
    LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_ENUM weightType,
    LwU32 weightRamSize
)
{
    FLCN_STATUS status;
    LwU32 weightSize;

    status = FLCN_OK;
    switch (weightType)
    {
        case LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_FP16:
            weightSize = sizeof(LwU16);
            break;
        case LW2080_CTRL_NNE_NNE_LAYER_WEIGHT_TYPE_FP32:
            weightSize = sizeof(LwU32);
            break;
        default:
            status = FLCN_ERR_ILWALID_STATE;
            break;
    }

    PMU_ASSERT_OK_OR_GOTO(status, status,
        s_nneDescLayerEntryWeightRamSizeValidate_GA10X_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        (numNeurons * numWeightsPerNeuron * weightSize == weightRamSize) ?
            FLCN_OK : FLCN_ERR_ILWALID_STATE,
        s_nneDescLayerEntryWeightRamSizeValidate_GA10X_exit);

s_nneDescLayerEntryWeightRamSizeValidate_GA10X_exit:
    return status;
}
