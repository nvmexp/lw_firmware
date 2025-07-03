/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_GDMA_H
#define LIBLWRISCV_GDMA_H

#if (!LWRISCV_HAS_GDMA) || (!LWRISCV_FEATURE_DMA)
#error "This header cannot be used on an engine which does not support GDMA."
#endif //(!LWRISCV_HAS_GDMA) || (!LWRISCV_FEATURE_DMA)

#include <stdint.h>
#include <stdbool.h>
#include <lwriscv/status.h>
#include <liblwriscv/io.h>

#define GDMA_MAX_NUM_CHANNELS          4
#define GDMA_NUM_SUBCHANNELS           2
#define GDMA_ADDR_ALIGN                (1<<2)
#define GDMA_MAX_XFER_LENGTH           0xFFFFFC

/*!
 * The configuration for a GDMA channel (0-3)
 */
typedef struct
{
    /*!
    * Round-robin weight of this channel (0-16).
    * Higher values will result in more bandwidth being spent on this channel
    * when it is competing with another channel for a common interface.
    */
    uint8_t rrWeight;
    /*!
    * Maximum inflight transaction size (0-7). It is useful when
    * the bandwidth of destination does not match that of global memory.
    *   max = 0 -> unlimited, issue until back-pressured
    *   max > 0 -> (64 << (max-1)), support 64B ~ 4KB
    *   Recommended configurations:
    *   ==============================================================
    *   |  Source    |  Destination |           MAX_INFLIGHT         |
    *   --------------------------------------------------------------
    *   | Global MEM |  Local MEM   |           Unlimited            |
    *   --------------------------------------------------------------
    *   | Global MEM |  IO posted   |               4KB              |
    *   --------------------------------------------------------------
    *   | Global MEM | IO nonposted |               64B              |
    *   --------------------------------------------------------------
    *   |            |              | When using FBIF, the total     |
    *   |            |              | inflight size of all channels  |
    *   | Global MEM |  Global MEM  | MUST be less than FBIF reorder |
    *   |            |              | -buffer size (typical 4KB) to  |
    *   |            |              | avoid deadlock!                |
    *   ==============================================================
    */
    uint8_t maxInFlight;
    /*!
    * channel-level IRQ enable (Raised on DMA Completion or Error).
    * Note: bIrqEn for individual transfers must also be true for an interrupt to fire.
    */
    bool bIrqEn;
    /*!
    * LW_TRUE = Memory Queue Mode
    * LW_FALSE = Register Mode
    */
    bool bMemq;
    // More memq configs will be added here when we add support for memq transfers
} GDMA_CHANNEL_CFG;

/*!
 * The priveleged configuration for a GDMA channel (0-3).
 * Meant to be called by a privileged mode (eg: kernel).
 */
typedef struct
{
    /*!
    * Whether the channel uses RISC-V PA or VA
    */
    bool bVirtual;
    /*!
    * RISC-V ASID used by the channel
    */
    uint8_t asid;
} GDMA_CHANNEL_PRIV_CFG;

/*!
 * Addressing mode used for the source or destination of a transfer.
 */
typedef enum
{
    /*!
    * Increment by one byte after each byte (Same as Falcon DMA Engine)
    */
    GDMA_ADDR_MODE_INC,
    /*!
    * Access the same address (no increment).
    * Only available when transferring to/from PRI.
    */
    GDMA_ADDR_MODE_FIX,
    /*!
    * Like GDMA_ADDR_MODE_INC, but wrap back to start after reaching wrapLen.
    * Only available when transferring to/from PRI.
    */
    GDMA_ADDR_MODE_WRAP,
    /*!
    * Like GDMA_ADDR_MODE_INC, but increment by a stride (See strideLogLen).
    * Only available when transferring to/from PRI.
    */
    GDMA_ADDR_MODE_STRIDE
} GDMA_ADDR_MODE;

#ifdef LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_COHERENT
/*!
 *  L2 Cache eviction policies for external memory accesses
 */
typedef enum
{
    /*!
     * L2 Cache eviction class: evict first
     */
    GDMA_L2C_L2_EVICT_FIRST,
    /*!
     * L2 Cache eviction class: evict normal
     */
    GDMA_L2C_L2_EVICT_NORMAL,
    /*!
     * L2 Cache eviction class: evict last
     */
    GDMA_L2C_L2_EVICT_LAST
} GDMA_L2C;

/*!
 * External Addressing mode configuration used for a transfer (dGPU version)
 */
typedef struct
{
    /*!
     * LW_TRUE signifies that accesses to external memory will be coherent.
     * LW_FALSE signifies that no coherence is guaranteed.
     */
    bool fbCoherent;
    /*!
     * L2 Cache eviction class for src reads or dest writes.
     */
    GDMA_L2C fbL2cRd;
    /*!
     * WPR ID (0-3) to use for the src reads or dest writes.
     */
    uint8_t fbWpr;
} GDMA_EXT_ADDR_CFG;

#else // LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_COHERENT
/*!
 * External Addressing mode configuration used for a transfer (CheetAh version)
 */
typedef struct
{
    /*!
     * The GSC ID for the address (5 bits)
     */
    uint8_t dbbGscid;
    /*!
     * The VPR for the address
     */
    bool dbbVpr;
} GDMA_EXT_ADDR_CFG;
#endif // LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_COHERENT

/*!
 * Addressing mode configuration used for a transfer
 */
typedef struct
{
    /*!
    * The addressing mode used for the source
    */
    GDMA_ADDR_MODE srcAddrMode;
    /*!
    * The addressing mode used for the destination
    */
    GDMA_ADDR_MODE destAddrMode;
    /*!
    * When GDMA_ADDR_MODE_WRAP is used, this field specifies the length of the wrap.
    * The length of the wrap is (wrapLen+1)*4 bytes
    */
    uint8_t wrapLen;
    /*!
     * When GDMA_ADDR_MODE_STRIDE is used, this field specifies the stride length.
     *
     * the stride length is (0x4 << strideLogLen),
     * e.g. 1. strideLogLen=0 => stride_length=0x4 (same as incremental);
     *  2. strideLogLen=18 => stride_length=0x100000
     * e.g. src is stride addressing mode, then its address pattern is:
     *  4B @ (src + 0*stride_length), 4B @ (src + 1*stride_length), ...
     */
    uint8_t strideLogLen;
    /*!
    * Use posted IO writes (Only applicable when target is IO)
    */
    bool bDestPosted;
    /*!
    * Configuration for the src address. Only applicable when src is external memory.
    */
    GDMA_EXT_ADDR_CFG srcExtAddrCfg;
    /*!
    * Configuration for the dest address. Only applicable when dest is external memory.
    */
    GDMA_EXT_ADDR_CFG destExtAddrCfg;
} GDMA_ADDR_CFG;

/*!
 * \brief Configure a DMA Channel
 *
 * @param[in] chanIdx The DMA channel to configure (0-3)
 * @param[in] cfg The configuration to that channel
 *
 * @return LWRV_OK on success
 *         LWRV_ERR_ILWALID_ARGUMENT if cfg is not sane
 *         LWRV_ERR_NOT_SUPPORTED if cfg is sane but not yet supported in the current phase
 *
 * Configuration can only be updated when the DMA channel is idle.
 * This call will block until that point.
 */
LWRV_STATUS gdmaCfgChannel (uint8_t chanIdx, const GDMA_CHANNEL_CFG *cfg);

/*!
 * \brief Configure the priveleged fields of a DMA Channel
 *
 * @param[in] chanIdx The DMA channel to configure (0-3)
 * @param[in] cfg The configuration to that channel
 *
 * @return LWRV_OK on success
 *         LWRV_ERR_ILWALID_ARGUMENT if cfg is not sane
 *         LWRV_ERR_NOT_SUPPORTED if cfg is sane but not yet supported in the current phase
 *
 * This function is meant to be called by a privileged mode (eg: kernel) to set up the
 * ASID and VA/PA config which will be used by a channel. gdmaCfgChannel() sets up the
 * non-privileged config which can be called from anywhere.
 *
 * Configuration can only be updated when the DMA channel is idle.
 * This call will block until that point.
 */
LWRV_STATUS gdmaCfgChannelPriv (uint8_t chanIdx, const GDMA_CHANNEL_PRIV_CFG *cfg);

/*!
 * \brief DMA transfer using GDMA Register Mode
 *
 * @param[in] src The source address. Must be aligned to GDMA_ADDR_ALIGN.
 * @param[in] cfg The source dmaIdx (0-7)
 *                Applicable if src is external memory, otherwise value is ignored.
 * @param[in] dest The destination address. Must be aligned to GDMA_ADDR_ALIGN.
 * @param[in] destCtxIdx The destination dmaIdx (0-7)
 *                Applicable if dest is external memory, otherwise value is ignored.
 * @param[in] length The number of bytes to transfer. Must be a multiple of GDMA_ADDR_ALIGN.
 * @param[in] pAddrCfg The addressing mode configuration.
 *                     Set to NULL to use the default Addressing mode (GDMA_ADDR_MODE_INC)
 * @param[in] chanIdx The DMA channel to use (0-3)
 * @param[in] subChanIdx The subchannel to use (0-1)
 * @param[in] bWaitComplete Indicates whether the call should block until the transfer completes.
 *                If LW_FALSE is used, the caller can check the transfer status by polling
 *                gdmaGetChannelStatus, or by waiting for the interrupt to fire and the
 *                completion handler to be called.
 * @param[in] bIrqEn indicates whether this request should trigger an interrupt upon completion.
 *                If it is set to LW_TRUE, the caller can expect the completion handler to be called.
 *
 * @return LWRV_OK on success.
 *         LWRV_ERR_ILWALID_ARGUMENT if any of the args are not sane.
 *         LWRV_ERR_NOT_SUPPORTED if cfg is sane but not yet supported in the current phase.
 *         LWRV_ERR_ILWALID_STATE if the channel is in an error state prior to the call.
 *            Client should call gdmaGetErrorAndResetChannel in this case.
 *         LWRV_ERR_GENERIC if the channel enters an error state as a result of this call.
 *            Client should call gdmaGetErrorAndResetChannel in this case.
 *
 * Limitations/prerequisites:
 * chanIdx is a channel which has been previously configured via gdmaCfgChannel.
 * src and dst may be RISC-V VAs or PAs. This is configured at the channel level in
 * GDMA_CHANNEL_CONFIG.bVirtual length Must be in the range of 4B to 16 MB.
 */
LWRV_STATUS gdmaXferReg (
    uint64_t src,
    uint64_t dest,
    uint32_t length,
    GDMA_ADDR_CFG *pAddrCfg,
    uint8_t chanIdx,
    uint8_t subChanIdx,
    bool bWaitComplete,
    bool bIrqEn
);

/*!
 * \brief Obtain the channel status
 *
 * @param[in] chanIdx The DMA channel to configure (0-3)
 * @param[out] reqProduce The number of DMA requests which SW has queued up.
 *         In Register Mode, it is incremented with every call to gdmaXferReg.
 *         In Memory Queue Mode, it is incremented when new DMA descriptors are added to the Memory Queue.
 * @param[out] reqConsume The number of DMA requests which the channel has pulled into its HW queue.
 *         In Register Mode, reqConsume = reqProduce.
 *         In Memory Queue Mode, it is incremented when the channel pulls DMA requests from the
 *            Memory Queue into its HW queue.
 * @param[out] reqComplete is the number of DMA requests the channel has completed.
 *         The channel is idle when reqComplete == reqProduce
 *
 * @return LWRV_OK on success.
 *         LWRV_ERR_ILWALID_ARGUMENT if chanIdx is not a valid index.
 *         LWRV_ERR_GENERIC if the channel is in an error state.
 *            Client should call gdmaGetErrorAndResetChannel in this case.
 *
 * This function can be used by a client which has queued one or more asynchronous transfers and
 * would like to check the status of those transfers.
 *
 * The client may call this in response to a completion/error Interrupt notification,
 * or the client may poll this repeatedly to busy wait for completion.
 *
 * The caller may provide NULL pointers for any of (reqProduce, reqConsume, reqComplete) if he is
 * not interested in that value. (eg: a caller using Register mode will not care about reqConsume)
 */
LWRV_STATUS gdmaGetChannelStatus(uint8_t chanIdx, uint32_t *reqProduce, uint32_t *reqConsume, uint32_t *reqComplete);

/*!
 * \brief Obtain the channel's error and reset it
 *
 * @param[in] chanIdx The DMA channel to configure (0-3)
 * @param[out] errorCause The cause of the error (One of the GDMA_CHAN_STATUS_ERROR_CAUSE_x constants)
 * @param[out] errorId The ID of the DMA request which caused the error.
 *      (ie: The value of reqProduce when this DMA request was queued up)
 *      A NULL pointer can be provided if the caller does not care about this value.
 *
 *
 * @return LWRV_OK on success.
 *         LWRV_ERR_ILWALID_ARGUMENT if chanIdx is not a valid index.
 *         LWRV_ERR_ILWALID_STATE if the channel is not in an error state.
 *
 * This function will be called after gdmaGetChannelStatus has returned an error, in order to reset the
 * channel into a working state. After the reset, the client is expected to reconfigure the channel.
 */
LWRV_STATUS gdmaGetErrorAndResetChannel(uint8_t chanIdx, uint8_t *errorCause, uint32_t *errorId);

#endif //LIBLWRISCV_GDMA_H
