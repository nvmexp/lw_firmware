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
    uint8_t rr_weight;
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
    uint8_t max_in_flight;
    /*!
    * channel-level IRQ enable (Raised on DMA Completion or Error).
    * Note: b_irq_en for individual transfers must also be true for an interrupt to fire.
    */
    bool b_irq_en;
    /*!
    * LW_TRUE = Memory Queue Mode
    * LW_FALSE = Register Mode
    */
    bool b_memq;
    // More memq configs will be added here when we add support for memq transfers
} gdma_channel_cfg_t;

/*!
 * The priveleged configuration for a GDMA channel (0-3).
 * Meant to be called by a privileged mode (eg: kernel).
 */
typedef struct
{
    /*!
    * Whether the channel uses RISC-V PA or VA
    */
    bool b_virtual;
    /*!
    * RISC-V ASID used by the channel
    */
    uint8_t asid;
} gdma_channel_priv_cfg_t;

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
    * Like GDMA_ADDR_MODE_INC, but wrap back to start after reaching wrap_len.
    * Only available when transferring to/from PRI.
    */
    GDMA_ADDR_MODE_WRAP,
    /*!
    * Like GDMA_ADDR_MODE_INC, but increment by a stride (See stride_log_len).
    * Only available when transferring to/from PRI.
    */
    GDMA_ADDR_MODE_STRIDE
} gdma_addr_mode_t;

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
} gdma_l2c_t;

/*!
 * External Addressing mode configuration used for a transfer (dGPU version)
 */
typedef struct
{
    /*!
     * LW_TRUE signifies that accesses to external memory will be coherent.
     * LW_FALSE signifies that no coherence is guaranteed.
     */
    bool fb_coherent;
    /*!
     * L2 Cache eviction class for src reads or dest writes.
     */
    gdma_l2c_t fb_l2c_rd;
    /*!
     * WPR ID (0-3) to use for the src reads or dest writes.
     */
    uint8_t fb_wpr;
} gdma_ext_addr_cfg_t;

#else // LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_COHERENT
/*!
 * External Addressing mode configuration used for a transfer (CheetAh version)
 */
typedef struct
{
    /*!
     * The GSC ID for the address (5 bits)
     */
    uint8_t dbb_gscid;
    /*!
     * The VPR for the address
     */
    bool dbb_vpr;
} gdma_ext_addr_cfg_t;
#endif // LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_COHERENT

/*!
 * Addressing mode configuration used for a transfer
 */
typedef struct
{
    /*!
    * The addressing mode used for the source
    */
    gdma_addr_mode_t src_addr_mode;
    /*!
    * The addressing mode used for the destination
    */
    gdma_addr_mode_t dest_addr_mode;
    /*!
    * When GDMA_ADDR_MODE_WRAP is used, this field specifies the length of the wrap.
    * The length of the wrap is (wrap_len+1)*4 bytes
    */
    uint8_t wrap_len;
    /*!
     * When GDMA_ADDR_MODE_STRIDE is used, this field specifies the stride length.
     *
     * the stride length is (0x4 << stride_log_len),
     * e.g. 1. stride_log_len=0 => stride_length=0x4 (same as incremental);
     *  2. stride_log_len=18 => stride_length=0x100000
     * e.g. src is stride addressing mode, then its address pattern is:
     *  4B @ (src + 0*stride_length), 4B @ (src + 1*stride_length), ...
     */
    uint8_t stride_log_len;
    /*!
    * Use posted IO writes (Only applicable when target is IO)
    */
    bool b_dest_posted;
    /*!
    * Configuration for the src address. Only applicable when src is external memory.
    */
    gdma_ext_addr_cfg_t src_ext_addr_cfg;
    /*!
    * Configuration for the dest address. Only applicable when dest is external memory.
    */
    gdma_ext_addr_cfg_t dest_ext_addr_cfg;
} gdma_addr_cfg_t;

/*!
 * \brief Configure a DMA Channel
 *
 * @param[in] chan_idx The DMA channel to configure (0-3)
 * @param[in] cfg The configuration to that channel
 *
 * @return LWRV_OK on success
 *         LWRV_ERR_ILWALID_ARGUMENT if cfg is not sane
 *         LWRV_ERR_NOT_SUPPORTED if cfg is sane but not yet supported in the current phase
 *
 * Configuration can only be updated when the DMA channel is idle.
 * This call will block until that point.
 */
LWRV_STATUS gdma_cfg_channel (uint8_t chan_idx, const gdma_channel_cfg_t *p_cfg);

/*!
 * \brief Configure the priveleged fields of a DMA Channel
 *
 * @param[in] chan_idx The DMA channel to configure (0-3)
 * @param[in] cfg The configuration to that channel
 *
 * @return LWRV_OK on success
 *         LWRV_ERR_ILWALID_ARGUMENT if cfg is not sane
 *         LWRV_ERR_NOT_SUPPORTED if cfg is sane but not yet supported in the current phase
 *
 * This function is meant to be called by a privileged mode (eg: kernel) to set up the
 * ASID and VA/PA config which will be used by a channel. gdma_cfg_channel() sets up the
 * non-privileged config which can be called from anywhere.
 *
 * Configuration can only be updated when the DMA channel is idle.
 * This call will block until that point.
 */
LWRV_STATUS gdma_cfg_channel_priv (uint8_t chan_idx, const gdma_channel_priv_cfg_t *p_cfg);

/*!
 * \brief DMA transfer using GDMA Register Mode
 *
 * @param[in] src The source address. Must be aligned to GDMA_ADDR_ALIGN.
 * @param[in] cfg The source dma_idx (0-7)
 *                Applicable if src is external memory, otherwise value is ignored.
 * @param[in] dest The destination address. Must be aligned to GDMA_ADDR_ALIGN.
 * @param[in] dest_ctx_idx The destination dma_idx (0-7)
 *                Applicable if dest is external memory, otherwise value is ignored.
 * @param[in] length The number of bytes to transfer. Must be a multiple of GDMA_ADDR_ALIGN.
 * @param[in] p_addr_cfg The addressing mode configuration.
 *                     Set to NULL to use the default Addressing mode (GDMA_ADDR_MODE_INC)
 * @param[in] chan_idx The DMA channel to use (0-3)
 * @param[in] sub_chan_idx The subchannel to use (0-1)
 * @param[in] b_wait_complete Indicates whether the call should block until the transfer completes.
 *                If LW_FALSE is used, the caller can check the transfer status by polling
 *                gdma_get_channel_status, or by waiting for the interrupt to fire and the
 *                completion handler to be called.
 * @param[in] b_irq_en indicates whether this request should trigger an interrupt upon completion.
 *                If it is set to LW_TRUE, the caller can expect the completion handler to be called.
 *
 * @return LWRV_OK on success.
 *         LWRV_ERR_ILWALID_ARGUMENT if any of the args are not sane.
 *         LWRV_ERR_NOT_SUPPORTED if cfg is sane but not yet supported in the current phase.
 *         LWRV_ERR_ILWALID_STATE if the channel is in an error state prior to the call.
 *            Client should call gdma_get_error_and_reset_channel in this case.
 *         LWRV_ERR_GENERIC if the channel enters an error state as a result of this call.
 *            Client should call gdma_get_error_and_reset_channel in this case.
 *
 * Limitations/prerequisites:
 * chan_idx is a channel which has been previously configured via gdma_cfg_channel.
 * src and dst may be RISC-V VAs or PAs. This is configured at the channel level in
 * GDMA_CHANNEL_CONFIG.b_virtual length Must be in the range of 4B to 16 MB.
 */
LWRV_STATUS gdma_xfer_reg (
    uint64_t src,
    uint64_t dest,
    uint32_t length,
    gdma_addr_cfg_t *p_addr_cfg,
    uint8_t chan_idx,
    uint8_t sub_chan_idx,
    bool b_wait_complete,
    bool b_irq_en
);

/*!
 * \brief Obtain the channel status
 *
 * @param[in] chan_idx The DMA channel to configure (0-3)
 * @param[out] p_req_produce The number of DMA requests which SW has queued up.
 *         In Register Mode, it is incremented with every call to gdma_xfer_reg.
 *         In Memory Queue Mode, it is incremented when new DMA descriptors are added to the Memory Queue.
 * @param[out] p_req_consume The number of DMA requests which the channel has pulled into its HW queue.
 *         In Register Mode, req_consume = req_produce.
 *         In Memory Queue Mode, it is incremented when the channel pulls DMA requests from the
 *            Memory Queue into its HW queue.
 * @param[out] p_req_complete is the number of DMA requests the channel has completed.
 *         The channel is idle when req_complete == req_produce
 *
 * @return LWRV_OK on success.
 *         LWRV_ERR_ILWALID_ARGUMENT if chan_idx is not a valid index.
 *         LWRV_ERR_GENERIC if the channel is in an error state.
 *            Client should call gdma_get_error_and_reset_channel in this case.
 *
 * This function can be used by a client which has queued one or more asynchronous transfers and
 * would like to check the status of those transfers.
 *
 * The client may call this in response to a completion/error Interrupt notification,
 * or the client may poll this repeatedly to busy wait for completion.
 *
 * The caller may provide NULL pointers for any of (p_req_produce, p_req_consume, p_req_complete) if he is
 * not interested in that value. (eg: a caller using Register mode will not care about req_consume)
 */
LWRV_STATUS gdma_get_channel_status(uint8_t chan_idx, uint32_t *p_req_produce, uint32_t *p_req_consume, uint32_t *p_req_complete);

/*!
 * \brief Obtain the channel's error and reset it
 *
 * @param[in] chan_idx The DMA channel to configure (0-3)
 * @param[out] error_cause The cause of the error (One of the GDMA_CHAN_STATUS_ERROR_CAUSE_x constants)
 * @param[out] error_id The ID of the DMA request which caused the error.
 *      (ie: The value of req_produce when this DMA request was queued up)
 *      A NULL pointer can be provided if the caller does not care about this value.
 *
 *
 * @return LWRV_OK on success.
 *         LWRV_ERR_ILWALID_ARGUMENT if chan_idx is not a valid index.
 *         LWRV_ERR_ILWALID_STATE if the channel is not in an error state.
 *
 * This function will be called after gdma_get_channel_status has returned an error, in order to reset the
 * channel into a working state. After the reset, the client is expected to reconfigure the channel.
 */
LWRV_STATUS gdma_get_error_and_reset_channel(uint8_t chan_idx, uint8_t *error_cause, uint32_t *error_id);

#if LWRISCV_FEATURE_LEGACY_API 
/*
 * Maintain legacy support for clients which use the old function names.
 * Include at the end so that it may access the types in this fiile and typedef them.
 */
#include <liblwriscv/legacy/gdma_legacy.h>
#endif //LWRISCV_FEATURE_LEGACY_API

#endif //LIBLWRISCV_GDMA_H
