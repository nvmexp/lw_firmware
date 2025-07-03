/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
#ifndef HDCPMC_CONSTANTS_H
#define HDCPMC_CONSTANTS_H

#include "sec2sw.h"
#include "lwuproc.h"
#include "tsec_drv.h"

/*!
 * Scratch Buffer Layout
 *
 * The scratch buffer is allocated by the client in FB and used by HDCP task in
 * SEC2 to store the intermediate session details. Along with the session
 * details, the HDCP task also stores other details such as keys and a header
 * describing the session store. All the individual items need to be properly
 * laid out in the scratch buffer with 256 alignment restriction. Modifying the
 * position or size of one item affects all the other items in the layout.
 */

/*!
 * Offset within the scratch buffer where the keys are stored.
 */
#define HDCP_SB_KEYS_OFFSET                                                   0

/*!
 * Size of the keys section in the scratch buffer.
 */
#define HDCP_SB_KEYS_BLOCK_SIZE                                             256

/*!
 * Offset within the scratch buffer where the header is stored.
 */
#define HDCP_SB_HEADER_OFFSET   (HDCP_SB_KEYS_OFFSET + HDCP_SB_KEYS_BLOCK_SIZE)

/*!
 * Size of the header section in the scratch buffer.
 */
#define HDCP_SB_HEADER_SIZE                                                 512

/*!
 * Offset within the scratch buffer where the session store begins.
 */
#define HDCP_SB_SESSION_OFFSET    (HDCP_SB_HEADER_OFFSET + HDCP_SB_HEADER_SIZE)

/*!
 * Size of each session. This value needs to be greater than or equal to the
 * size of HDCP_SESSION.
 *
 * !!! THE CLASS HEADER NEEDS TO BE CHANGED WHENEVER THE VALUE IS UPDATED !!!
 */
#define HDCP_SB_SESSION_SIZE                                                512

/*!
 * Size of the temporary buffer used by method handlers.
 *
 * See @ref hdcpTempBuffer for more details.
 */
#define HDCP_SIZE_TEMP_BUFFER                       HDCP_SIZE_ENCRYPTION_BUFFER

/*!
 * Current binary build mode.
 */
#if SEC2CFG_FEATURE_ENABLED(SEC2_HDCPMC_VERIF)
#define HDCP_READ_CAPS_LWRRENT_BUILD_MODE                                     \
    LW95A1_HDCP_READ_CAPS_LWRRENT_BUILD_MODE_DEBUG_1
#else
#define HDCP_READ_CAPS_LWRRENT_BUILD_MODE                                     \
    LW95A1_HDCP_READ_CAPS_LWRRENT_BUILD_MODE_PROD
#endif

/*!
 * The maximum number of sessions supported by the HDCP task. Modifying this
 * number will affect the scratch buffer size.
 */
#define HDCP_MAX_SESSIONS                                                     3

/*!
 * The maximum number of receivers supported by the HDCP task. Increasing this
 * number will directly affect the DMEM usage. Must always be less than
 * HDCP_MAX_SESSIONS.
 */
#define HDCP_POR_NUM_RECEIVERS                                                2

#if (HDCP_POR_NUM_RECEIVERS > HDCP_MAX_SESSIONS)
#error "HDCP_POR_NUM_RECEIVERS must be less than HDCP_MAX_SESSIONS!"
#endif

/*!
 * Number of test receivers lwrrently known to the HDCP app.
 */
#define HDCP_TEST_RECEIVER_COUNT                                              2

/*!
 * The maximum number of streams per receiver. This value is shared with the
 * client.
 */
#define HDCP_MAX_STREAMS_PER_RECEIVER          LW95A1_HDCP_MAX_STREAMS_PER_RCVR

/*!
 * Size of the signature data member of the @ref HDCP_SESSION data structure.
 * Used to verify the session has not been tampered with while stored in the
 * frame buffer (size: 256 bits).
 */
#define HDCP_SIZE_SESSION_SIGNATURE                                          32

/*!
 * Number of bytes required to create a session status mask, where each bit
 * specifies if a session is available (0) or in use (1).
 */
#define HDCP_SIZE_SESSION_STATUS_MASK             ((HDCP_MAX_SESSIONS + 7) / 8)

/*!
 * Maximum size of a DMA transfer for HDCP.
 */
#define HDCP_SIZE_MAX_DMA                                                   256

/*!
 * Depth of the queue used for loading input contents from FB for encryption.
 * Increasing this count will reduce the DMA transactional delay, but it also
 * increases heap usage.
 */
#define HDCP_ENCRYPTION_BUFFER_DEPTH                                          4

/*!
 * Size of the buffer used for encryption.
 */
#define HDCP_SIZE_ENCRYPTION_BUFFER                                           \
    ((HDCP_SIZE_MAX_DMA * HDCP_ENCRYPTION_BUFFER_DEPTH) * 2)

/*!
 * Session key (ks) size in bytes (size: 128 bits).
 */
#define HDCP_SIZE_KS                                                         16

/*!
 * TODO-JBH: Document (size: 128 bits).
 */
#define HDCP_SIZE_IV                                                         16

/*!
 * Master key (km) size in bytes (size: 128 bits).
 */
#define HDCP_SIZE_KM                                                         16

/*!
 * Key derivation (kd) size in bytes (size: 256 bits).
 */
#define HDCP_SIZE_KD                                                         32

/*!
 * Global constant size in bytes (size: 128 bits).
 */
#define HDCP_SIZE_LC                                                         16

// Receiver Private Key Sizes
#define HDCP_SIZE_D                                                          64
#define HDCP_SIZE_P                                                          64
#define HDCP_SIZE_Q                                                          64


/*!
 * Size of the DPC LLC signature found on the receiver's certifate in bytes
 * (size: 3072 bits).
 */
#define HDCP_SIZE_RX_SIGNATURE                                              384

/*!
 * Size of the modulus component of the receiver's public key in bytes
 * (size: 1024 bits).
 */
#define HDCP_SIZE_RX_MODULUS                                                128

/*!
 * Size of the modulus root.
 */
#define HDCP_SIZE_RX_MODULUS_ROOT                                           384

/*!
 * Size of the exponent component of the receiver's public key in bytes
 * (size: 24 bits)
 */
#define HDCP_SIZE_RX_EXPONENT                                                 3

/*!
 * Size of the receiver's private key signature in bytes (size: 160 bits).
 */
#define HDCP_SIZE_RX_PRIV_KEY_SIGNATURE                                      20

/*!
 * Size of the receiver's private key exponent in bytes (size: 512 bits).
 */
#define HDCP_SIZE_RX_PRIV_EXPONENT                                           64

/*!
 * Size of H' in bytes.
 */
#define HDCP_SIZE_HPRIME                              LW95A1_HDCP_SIZE_HPRIME_8

/*!
 * Size of the receiver's ID in bytes. Shares defined size with clients.
 */
#define HDCP_SIZE_RCVR_ID                            LW95A1_HDCP_SIZE_RECV_ID_8

/*!
 * Size of M in bytes.
 */
#define HDCP_SIZE_M                                        LW95A1_HDCP_SIZE_M_8

/*!
 * Size of Ekh(Km) in bytes.
 */
#define HDCP_SIZE_EKH_KM                              LW95A1_HDCP_SIZE_EKH_KM_8

 /*!
 * Size of the PES header in bytes.
 */
#define HDCP_SIZE_PES_HDR                            LW95A1_HDCP_SIZE_PES_HDR_8

/*!
 * Size of the input counter in bytes.
 */
#define HDCP_SIZE_INPUT_CTR                                                   8

/*!
 * Size of the stream counter in bytes.
 */
#define HDCP_SIZE_STREAM_CTR                                                  4

/*!
 * Size of L/L' in bytes. Shares the defined size with clients.
 */
#define HDCP_SIZE_LPRIME                              LW95A1_HDCP_SIZE_LPRIME_8

/*!
 * 64-bit pseudo-random number used during the Session Key Exchange phase of
 * authentication. Shares the defined size with clients.
 */
#define HDCP_SIZE_RIV                                    LW95A1_HDCP_SIZE_RIV_8

/*!
 * Size of DCP Kpub in bytes. Shares the defined size with clients.
 */
#define HDCP_SIZE_DCP_KPUB                          LW95A1_HDCP_SIZE_DCP_KPUB_8

/*!
 * Size of the dkey as defined by the HDCP specification.
 */
#define HDCP_SIZE_DKEY                                  LW95A1_HDCP_SIZE_DKEY_8

/*!
 * Size of the epair.
 */
#define HDCP_SIZE_EPAIR                                LW95A1_HDCP_SIZE_EPAIR_8
/*!
 * Size of the value of rtx, a pseudo-random number generated by the
 * transmitter (size: 64-bit).
 */
#define HDCP_SIZE_RTX                                    LW95A1_HDCP_SIZE_RTX_8

/*!
 * Size of the value of rrx, a pseudo-random number generated by the
 * receiver (size: 64-bit).
 */
#define HDCP_SIZE_RRX                                    LW95A1_HDCP_SIZE_RRX_8

/*!
 * Size of rn, a pseudo-random nonce used in the locality check phase of
 * authentication (size: 64-bit).
 */
#define HDCP_SIZE_RN                                                          8

/*!
 * Size of a block used during AES encryption.
 */
#define HDCP_SIZE_AES_ENCRYPTION_BLOCK                                       16

/*!
 * Size of the sequence number for M.
 */
#define HDCP_SIZE_SEQ_NUM_M                        LW95A1_HDCP_SIZE_SEQ_NUM_M_8

/*!
 * Size of M'.
 */
#define HDCP_SIZE_MPRIME                              LW95A1_HDCP_SIZE_MPRIME_8

/*!
 * Size of the content ID.
 */
#define HDCP_SIZE_CONTENT_ID                      LW95A1_HDCP_SIZE_CONTENT_ID_8

/*!
 * Size of the content type.
 */
#define HDCP_SIZE_CONTENT_TYPE                  LW95A1_HDCP_SIZE_CONTENT_TYPE_8

#define HDCP_SIZE_STREAM_ID_MPRIME                                            \
    (HDCP_SIZE_CONTENT_ID + HDCP_SIZE_CONTENT_TYPE + HDCP_SIZE_STREAM_CTR)

/*!
 * Size of M' data.
 */
#define HDCP_SIZE_MPRIME_DATA                                                 \
    ((HDCP_SIZE_STREAM_ID_MPRIME * HDCP_MAX_STREAMS_PER_RECEIVER) +           \
    HDCP_SIZE_SEQ_NUM_M)

/*!
 * Size of the Stream ID type.
 */
#define HDCP_SIZE_STREAM_ID_TYPE                                              2

/*!
 * Size of the stream counter in bytes.
 */
#define HDCP_SIZE_STREAM_CTR                                                  4

 /*!
 * Size of the Bksv list in bytes.
 */
#define HDCP_SIZE_BKSV_LIST                                                 635

 /*!
 * Size of V' for HDCP 2.x in bytes.
 */
#define HDCP_SIZE_VPRIME_2X                        LW95A1_HDCP_SIZE_VPRIME_2X_8

#define HDCP_SIZE_HDMI_22_RXINFO              LW95A1_HDCP_SIZE_HDMI_22_RXINFO_8

 /*!
 * Size of the field that stores the sequence number for V in bytes.
 */
#define HDCP_SIZE_SEQ_NUM_V                        LW95A1_HDCP_SIZE_SEQ_NUM_V_8

 /*!
 * Maximum value for the sequence number for V.
 */
#define HDCP_SEQ_NUM_V_MAX                                             0xFFFFFF

 /*!
 * Determines the number of 'type' elements needed to store the specified
 * number of bytes.
 *
 * @param [in]  type    The data type to determine
 * @param [in]  bytes   The number of bytes to store as the new data type.
 *
 * @return The minimum number of 'type' elements to full store the specified
 *         number of 'bytes'. (sizeof(type[return_val]) >= bytes.)
 */
#define HDCP_SIZE_AS(type, bytes)                                             \
    (((bytes) + (sizeof(type) - 1)) / sizeof(type))

/*!
 * Callwlates the number of LwU8 needed to store the specified number of bytes.
 *
 * @param [in]  bytes   The number of bytes to colwert.
 *
 * @return The number of LwU8 required to store the specified number of bytes.
 */
#define HDCP_SIZE_LwU8(size)                           HDCP_SIZE_AS(LwU8, size)

/*!
 * Callwlates the number of LwU16 needed to store the specified number of bytes.
 *
 * @param [in]  bytes   The number of bytes to colwert.
 *
 * @return The number of LwU16 required to store the specified number of bytes.
 */
#define HDCP_SIZE_LwU16(size)                         HDCP_SIZE_AS(LwU16, size)

/*!
 * Callwlates the number of LwU32 needed to store the specified number of bytes.
 *
 * @param [in]  bytes   The number of bytes to colwert.
 *
 * @return The number of LwU32 required to store the specified number of bytes.
 */
#define HDCP_SIZE_LwU32(size)                         HDCP_SIZE_AS(LwU32, size)

/*!
 * Callwlates the number of LwU64 needed to store the specified number of bytes.
 *
 * @param [in]  bytes   The number of bytes to colwert.
 *
 * @return The number of LwU64 required to store the specified number of bytes.
 */
#define HDCP_SIZE_LwU64(bytes)                       HDCP_SIZE_AS(LwU64, bytes)

#endif // HDCPMC_CONSTANTS_H
#endif
