/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2007-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * The code in this file refers to the "HDCP DisplayPort spec." The actual
 * document is entitled "High-bandwidth Digital Content Protection System v1.3,
 * Amendment for DisplayPort" Revision 1.1 with a publication date of 15
 * January, 2010 by Digital Content Protection LLC.
 */

/*!
 * @file    pmu_auth.c
 * @brief   Supplies HDCP authentication \& verification functions.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwostask.h"

/* ------------------------- Application Includes --------------------------- */
#include "hdcp/hdcp_types.h"
#include "hdcp/pmu_bigint.h"
#include "pmu_sha1.h"
#include "hdcp/pmu_auth.h"

extern LwU32 CtlOptions;

/* ------------------------- Private Functions ------------------------------ */

/*
 * Big integer Montgomery constants.  The HDCP signature constants have been
 * pre-packaged to conform with the big integer Montgomery paper.
 * Big integers are associated with pre-callwlated constants to aid in
 * big integer arithmetic.
 */

/*!
 * @brief Prime divisor value.
 *
 * Prime divisor value as defined by the HDCP DisplayPort Spec, pg. 70.
 * Includes the Montgomery constant. Both values are stored in little
 * endian format.
 */

static BigIntModulus HDCP_Divisor =
{
    5,
    0x4fc27529,
    { 0xf9c708e7, 0x97ef3f4d, 0xcd6d14e2, 0x5e6db56a, 0xee8af2ce },
    { 0x216222aa, 0xb7696a56, 0x32520fa6, 0x571e4275, 0xc6da1382 }
};


/*!
 * @brief Prime modulus value.
 *
 * Prime modulus value as defined by the HDCP DisplayPort Spec, pg. 70.
 * Includes the Montgomery constant. Both values are stored in little
 * endian format.
 */

static BigIntModulus HDCP_Modulus =
{
    32,
    0x43ae5569,
    {
        0xf3287527, 0x8c59802b, 0x46edc211, 0x2a39951c,
        0x96891954, 0x028a49fd, 0x3275733b, 0x5c7b9c14,
        0xb9982049, 0xa73f3207, 0xb3721530, 0x10715509,
        0xd1974c3a, 0xf404a0bc, 0x5447cf35, 0xe52ba70e,
        0xd4c6b983, 0xb844c747, 0xae7c7667, 0x4f34dc0c,
        0x1d969e4b, 0xa0d28482, 0xf500e0dc, 0x8e7fa164,
        0x6a7058ff, 0xa1a24fc3, 0x5a52c7b8, 0x17395b35,
        0x9343786b, 0x018d75f7, 0xfd1761b7, 0xd3c3f5b2
    },
    {
        0x37b05821, 0x8464acc9, 0x70af59dc, 0x93804c0c,
        0xd8941b25, 0x2d034a86, 0xfcfb5c58, 0x7b31fe62,
        0x9a09de8b, 0x28c7998d, 0x2067722f, 0xe1e57124,
        0x7e979475, 0x60ee4474, 0x4ef3779a, 0x194d131e,
        0x07b3186a, 0xa933120b, 0xc9179238, 0x25da1db3,
        0x99fb1b78, 0x90d6a2de, 0xd3ec24ef, 0x1991fca0,
        0x1871b5cc, 0xd8ac0cfa, 0x1350d85e, 0x71899867,
        0x360e32c8, 0x053c8444, 0x26e4e64b, 0x03674270
    }
};


/*!
 * @brief Generator value.
 *
 * Generator values as defined by the HDCP DisplayPort Spec, pg. 70.
 * The value is stored in little endian format.
 */

static LwU8 g_HDCP_SRM_Generator[SRM_GENERATOR_SIZE] =
{
    0xd9, 0x0b, 0xba, 0xc2, 0x42, 0x24, 0x46, 0x69,
    0x5b, 0x40, 0x67, 0x2f, 0x5b, 0x18, 0x3f, 0xb9,
    0xe8, 0x6f, 0x21, 0x29, 0xac, 0x7d, 0xfa, 0x51,
    0xc2, 0x9d, 0x4a, 0xab, 0x8a, 0x9b, 0x8e, 0xc9,
    0x42, 0x42, 0xa5, 0x1d, 0xb2, 0x69, 0xab, 0xc8,
    0xe3, 0xa5, 0xc8, 0x81, 0xbe, 0xb6, 0xa0, 0xb1,
    0x7f, 0xba, 0x21, 0x2c, 0x64, 0x35, 0xc8, 0xf7,
    0x5f, 0x58, 0x78, 0xf7, 0x45, 0x29, 0xdd, 0x92,
    0x9e, 0x79, 0x3d, 0xa0, 0x0c, 0xcd, 0x29, 0x0e,
    0xa9, 0xe1, 0x37, 0xeb, 0xbf, 0xc6, 0xed, 0x8e,
    0xa8, 0xff, 0x3e, 0xa8, 0x7d, 0x97, 0x62, 0x51,
    0xd2, 0xa9, 0xec, 0xbd, 0x4a, 0xb1, 0x5d, 0x8f,
    0x11, 0x86, 0x27, 0xcd, 0x66, 0xd7, 0x56, 0x5d,
    0x31, 0xd7, 0xbe, 0xa9, 0xac, 0xde, 0xaf, 0x02,
    0xb5, 0x1a, 0xde, 0x45, 0x24, 0x3e, 0xe4, 0x1a,
    0x13, 0x52, 0x4d, 0x6a, 0x1b, 0x5d, 0xf8, 0x92
};


/*!
 * @brief DSA public key.
 *
 * Public Key as defined by the HDCP DisplayPort spec, pg. 70.
 * The value is stored in little endian format.
 */

static LwU8 g_HDCP_SRM_PublicKey[SRM_MODULUS_SIZE] =
{
    // Production public key
    0x99, 0x37, 0xe5, 0x36, 0xfa, 0xf7, 0xa9, 0x62,
    0x83, 0xfb, 0xb3, 0xe9, 0xf7, 0x9d, 0x8f, 0xd8,
    0xcb, 0x62, 0xf6, 0x66, 0x8d, 0xdc, 0xc8, 0x95,
    0x10, 0x24, 0x6c, 0x88, 0xbd, 0xff, 0xb7, 0x7b,
    0xe2, 0x06, 0x52, 0xfd, 0xf7, 0x5f, 0x43, 0x62,
    0xe6, 0x53, 0x65, 0xb1, 0x38, 0x90, 0x25, 0x87,
    0x8d, 0xa4, 0x9e, 0xfe, 0x56, 0x08, 0xa7, 0xa2,
    0x0d, 0x4e, 0xd8, 0x43, 0x3c, 0x97, 0xba, 0x27,
    0x6c, 0x56, 0xc4, 0x17, 0xa4, 0xb2, 0x5c, 0x8d,
    0xdb, 0x04, 0x17, 0x03, 0x4f, 0xe1, 0x22, 0xdb,
    0x74, 0x18, 0x54, 0x1b, 0xde, 0x04, 0x68, 0xe1,
    0xbd, 0x0b, 0x4f, 0x65, 0x48, 0x0e, 0x95, 0x56,
    0x8d, 0xa7, 0x5b, 0xf1, 0x55, 0x47, 0x65, 0xe7,
    0xa8, 0x54, 0x17, 0x8a, 0x65, 0x76, 0x0d, 0x4f,
    0x0d, 0xff, 0xac, 0xa3, 0xe0, 0xfb, 0x80, 0x3a,
    0x86, 0xb0, 0xa0, 0x6b, 0x52, 0x00, 0x06, 0xc7
};

/*!
 * @brief DSA test public key for use with test vectors.
 */

static LwU8 g_HDCP_SRM_TestPublicKey[SRM_MODULUS_SIZE] =
{
    // Test public key
    0x46, 0xb9, 0xc2, 0xe5, 0xbe, 0x57, 0x3b, 0xa6,
    0x22, 0x7b, 0xaa, 0x83, 0x81, 0xa9, 0xd2, 0x0f,
    0x03, 0x2e, 0x0b, 0x70, 0xac, 0x96, 0x42, 0x85,
    0x4e, 0x78, 0x8a, 0xdf, 0x65, 0x35, 0x97, 0x6d,
    0xe1, 0x8d, 0xd1, 0x7e, 0xa3, 0x83, 0xca, 0x0f,
    0xb5, 0x8e, 0xa4, 0x11, 0xfa, 0x14, 0x6d, 0xb1,
    0x0a, 0xcc, 0x5d, 0xff, 0xc0, 0x8c, 0xd8, 0xb1,
    0xe6, 0x95, 0x72, 0x2e, 0xbd, 0x7c, 0x85, 0xde,
    0xe8, 0x52, 0x69, 0x92, 0xa0, 0x22, 0xf7, 0x01,
    0xcd, 0x79, 0xaf, 0x94, 0x83, 0x2e, 0x01, 0x1c,
    0xd7, 0xef, 0x86, 0x97, 0xa3, 0xbb, 0xcb, 0x64,
    0xa6, 0xc7, 0x08, 0x5e, 0x8e, 0x5f, 0x11, 0x0b,
    0xc0, 0xe8, 0xd8, 0xde, 0x47, 0x2e, 0x75, 0xc7,
    0xaa, 0x8c, 0xdc, 0xb7, 0x02, 0xc4, 0xdf, 0x95,
    0x31, 0x74, 0xb0, 0x3e, 0xeb, 0x95, 0xdb, 0xb0,
    0xce, 0x11, 0x0e, 0x34, 0x9f, 0xe1, 0x13, 0x8d
};

/*!
 * Note: This local macro below, in addition for being used for DMA alignment
 * by functions in this file, is also used to decide the size of a local array.
 * Hence, instead basing its value off of the DMA read alignment value used in
 * RTOS code (which is arch-dependent), we will make an exception for it and
 * hardcode it to 16. That is what it was intended to be, when the code was
 * originally written.
 */
#define HDCP_DMA_READ_ALIGNMENT 16

#define HDCP_SHA1_BUFFER_SIZE   64
#define HDCP_RCV_BKSV_SIZE      5
#define NUM_ENTRIES_PER_CHUNK   8
#define CHUNK_SIZE              (HDCP_RCV_BKSV_SIZE * NUM_ENTRIES_PER_CHUNK)

#define HDCP_L_STREAM_STATUS_OFFSET     0
#define HDCP_L_STREAM_STATUS_SIZE       2
#define HDCP_L_CLIENT_ID_OFFSET         2
#define HDCP_L_CLIENT_ID_SIZE           7
#define HDCP_L_STREAM_ID_OFFSET         9
#define HDCP_L_STREAM_ID_SIZE           1
#define HDCP_L_AN_OFFSET                10
#define HDCP_L_AN_SIZE                  8
#define HDCP_L_BKSV_OFFSET              18
#define HDCP_L_BKSV_SIZE                5
#define HDCP_L_V_OFFSET                 23
#define HDCP_L_V_SIZE(x)                (((x)->pV) ? 20 : 0)
#define HDCP_L_M0_OFFSET(x)             (((x)->pV) ? 43 : 23)
#define HDCP_L_M0_SIZE                  8

#define HDCP_V_KSV_LIST_OFFSET          0
#define HDCP_V_KSV_LIST_SIZE(x)         ((x)->ksvListSize)
#define HDCP_V_BINFO_OFFSET(x)          ((x)->ksvListSize)
#define HDCP_V_BINFO_SIZE               2
#define HDCP_V_M0_OFFSET(x)             ((x)->ksvListSize + HDCP_V_BINFO_SIZE)
#define HDCP_V_M0_SIZE                  8

static LwU32 s_hdcpDmaRead(LwU8 *pDst, RM_FLCN_MEM_DESC *pSrc, LwU32 srcOffset, LwU32 size);

static LwU32 s_hdcpLCallback(LwU8 *pBuff, LwU32 offset, LwU32 size, void *pInfo)
    GCC_ATTRIB_USED();

static LwU32 s_hdcpSrmCopyCallback(LwU8 *pBuff, LwU32 offset, LwU32 size, void *pInfo)
    GCC_ATTRIB_USED();

static LwU32 s_hdcpVCallback(LwU8 *pBuff, LwU32 offset, LwU32 size, void *pInfo)
    GCC_ATTRIB_USED();

/*!
 * @brief Reverses the byte order of a 32-bit word.
 *
 * @param a 32-bit word to reverse the byte order of (i.e. switch from little
 *      endian to big endian and vice-versa.
 *
 * @return A 32-bit word with its endianness reversed.
 */

#define REVERSE_BYTE_ORDER(a) \
    (((a) >> 24) | ((a) << 24) | (((a) >> 8) & 0xFF00) | \
    (((a) << 8) & 0xFF0000))


/*!
 * Parameter for the SHA-1 callback when callwlating the value of V.
 */

typedef struct
{
    RM_FLCN_MEM_DESC *pKsvList;       //!< Location of the KSV list in system memory
    LwU32             ksvListSize;    //!< The size of the KSV list in bytes
    LwU32             bksvListSize;   //!< The size in bytes of the prepended bksvs
    LwU16             bInfo;          //!< The value of B<sub>Info</sub>
    LwU64             m0;             //!< The value of M<sub>0</sub>
} VCallbackInfo;


/*!
 * Parameter for the SHA-1 callback when callwlating the value of L.
 */

typedef struct
{
    LwU64       clientId;       //!< Client ID (a 56-bit value)
    LwU64       An;             //!< An
    LwU64       M0;             //!< M0
    LwU64       Bksv;           //!< Bksv (a 40-bit value)
    LwU16       streamStatus;   //!< Stream Status
    LwU8       *pV;             //!< V, if repeater; null, if not
    LwU8        streamId;       //!< Stream ID
} LCallbackInfo;


/*!
 * @brief Copy callback function for SHA-1 when callwlating V.
 *
 * The value of V is callwlated by taking the SHA-1 hash of the KSV list,
 * B<sub>info</sub>, and M<sub>0</sub>. The KSV list is stored in system
 * memory, which requires DMA transfers to get the data. B<sub>info</sub> is
 * passed to the PMU with the command, and M<sub>0</sub> is obtained using the
 * display private bus.
 *
 * @param[out]  pBuff
 *      A memory buffer local to the SHA-1 function.
 *
 * @param[in]   offset
 *      The offset within the source data to begin copying from.
 *
 * @param[in]   size
 *      The size, in bytes, to copy.
 *
 * @param[in]   pInfo
 *      Pointer to a structure that contains the information needed to perform
 *      a DMA transfer of the KSV list, B<sub>info</sub>, and M<sub>0</sub>.
 *
 * @return The number of bytes copied into the buffer.
 */

static LwU32
s_hdcpVCallback
(
    LwU8   *pBuff,
    LwU32   offset,
    LwU32   size,
    void   *pInfo
)
{
    VCallbackInfo *pCb = (VCallbackInfo *)pInfo;

    LwU32 copySize;
    LwU32 startOffset;
    LwU32 endOffset;
    LwU32 totalSize = 0;

    LwU32 bufferOffset;  // offset within the destination buffer
    LwU32 i;

    bufferOffset = 0;
    while (size > 0)
    {
        //
        // Need to copy the KSV list into the buffer to pass back to the SHA-1
        // function. Need to skip over any BKsvs that are being stored at the
        // beginning of the KSV list, as they are not part of the list itself.
        // The KSV list is pCb->ksvListSize in size.
        //
        if (offset < (HDCP_V_KSV_LIST_OFFSET + HDCP_V_KSV_LIST_SIZE(pCb)))
        {
            copySize      = LW_MIN(size, HDCP_V_KSV_LIST_SIZE(pCb) - offset);
            bufferOffset += s_hdcpDmaRead(pBuff, pCb->pKsvList,
                                         pCb->bksvListSize + offset, copySize);
        }

        //
        // The offset is within the Binfo, which is 2 bytes in size.
        //
        else if (offset < (HDCP_V_BINFO_OFFSET(pCb) + HDCP_V_BINFO_SIZE))
        {
            copySize    = LW_MIN(size, HDCP_V_BINFO_SIZE);
            startOffset = offset - HDCP_V_BINFO_OFFSET(pCb);
            endOffset   = LW_MIN(startOffset + copySize, HDCP_V_BINFO_SIZE);

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[bufferOffset] = (LwU8) ((pCb->bInfo >> (i << 3)) & 0xFF);
                bufferOffset++;
            }
        }

        //
        // Final field in the SHA-1 input stream is M0, which is 8 bytes in
        // size.
        //
        else if (offset < (HDCP_V_M0_OFFSET(pCb) + HDCP_V_M0_SIZE))
        {
            copySize    = LW_MIN(size, HDCP_V_M0_SIZE);
            startOffset = offset - HDCP_V_M0_OFFSET(pCb);
            endOffset   = LW_MIN(startOffset + copySize, HDCP_V_M0_SIZE);

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[bufferOffset] = (LwU8) ((pCb->m0 >> (i << 3)) & 0xFF);
                bufferOffset++;
            }
        }

        // invalid offset
        else
        {
            break;
        }

        size      -= copySize;
        offset    += copySize;
        totalSize += copySize;
    }
    return totalSize;
}


/*!
 * @brief Reads data from DMA.
 *
 * This function abstracts dmaRd. It does not require the calling function
 * to have aligned the destination data or set the size to a multiple of
 * DMA_MIN_READ_ALIGNMENT.
 *
 * @param[out]  pDst
 *      Buffer to hold the data read from DMA.
 *
 * @param[in]   pSrc
 *      Pointer to the DMA space to read from.
 *
 * @param[in]   size
 *      The number of bytes to read.
 *
 * @return The number of bytes from from DMA.
 */

static LwU8 HdcpDmaReadBuffer[HDCP_SHA1_BUFFER_SIZE]
    GCC_ATTRIB_ALIGN(HDCP_DMA_READ_ALIGNMENT);

static LwU32
s_hdcpDmaRead
(
    LwU8                *pDst,
    RM_FLCN_MEM_DESC    *pSrc,
    LwU32                srcOffset,
    LwU32                size
)
{
    RM_FLCN_MEM_DESC  desc;
    LwU32             offset = 0;
    LwU32             bytesRead = 0;
    LwU32             copySize;
    LwU32             dmaSize;
    LwU16             dmaOffset;
    LwU8              unalignedBytes;

    desc = *pSrc;
    srcOffset += (LwU8)desc.address.lo;
    desc.address.lo &= 0xFFFFFF00;
    unalignedBytes = srcOffset & (HDCP_DMA_READ_ALIGNMENT - 1);

    // Its recommended to use DMA section when task is doing back to back DMA.
    lwosDmaLockAcquire();
    {
        while (size > 0)
        {
            dmaOffset = ALIGN_DOWN(srcOffset + offset, HDCP_DMA_READ_ALIGNMENT);

            dmaSize = LW_MIN(ALIGN_UP(size + unalignedBytes,
                                      HDCP_DMA_READ_ALIGNMENT),
                             HDCP_SHA1_BUFFER_SIZE);
            copySize = LW_MIN(size, HDCP_SHA1_BUFFER_SIZE - unalignedBytes);

            if (FLCN_OK != dmaRead(HdcpDmaReadBuffer, &desc, dmaOffset, dmaSize))
            {
                break;
            }
            memcpy((void *)(pDst + offset),
                   (void *)(HdcpDmaReadBuffer + unalignedBytes), copySize);

            size -= copySize;
            offset += copySize;
            bytesRead += copySize;
            unalignedBytes = 0;
        }
    }
    lwosDmaLockRelease();

    return bytesRead;
}

/*!
 * @brief   Colwerts a byte array into a big integer.
 *
 * @param[in,out]   pBuf
 *      Pointer to a 20 byte array with the number represented in big endian
 *      format. The function will colwert this to a format usable by the
 *      big integer library.
 */
static void
s_hdcpToBigInt
(
    LwU8 *pBuf
)
{
    LwU8    tmp;
    LwU8    i;
    LwU8    j;

    // byte swap individual words
    for (i = 0, j = (LW_HDCPSRM_SIGNATURE_SIZE - 1);
         i < (LW_HDCPSRM_SIGNATURE_SIZE / 2);
         i++, j--)
    {
        tmp     = pBuf[i];
        pBuf[i] = pBuf[j];
        pBuf[j] = tmp;
    }
}

/*!
 * Returns the number of devices in the revocation list
 *
 * @param[in]   srm
 *      The location in system memory of the SRM.
 *
 * @return The number of devices in the revocation list.
 */

static LwU32
s_hdcpGetRevoListSize(RM_FLCN_MEM_DESC *pSrm)
{
    LwU8 srmChunk[HDCP_DMA_READ_ALIGNMENT];

    s_hdcpDmaRead(srmChunk, pSrm, 0, HDCP_DMA_READ_ALIGNMENT);

    //
    // LW_HDCPSRM_NUM_DEVICES shares the same byte as LW_HDCPSRM_RESERVED3.
    // Therefore, we want to remove the reserved bits from the value.
    //
    return (srmChunk[LW_HDCPSRM_NUM_DEVICES] & ~LW_HDCPSRM_RESERVED3);
}


/*!
 * @brief Copy callback function for SHA-1 when callwlating L.
 *
 * The value of L is callwlated by taking the SHA-1 hash of the stream status,
 * the client supplied nonce, stream ID, <em>An</em>, <em>Bksv</em>, V (if a
 * repeater), and M<sub>0</sub>. <em>Bksv</em> and V are stored in system
 * memory, which requires DMA transfers to get the data. All other values,
 * except M<sub>0</sub> are passed to the PMU with the command, while
 * M<sub>0</sub> is obtained using the display private bus.
 *
 * All values passed back to the SHA-1 function need to be in little endian
 * format.
 *
 * @param[out]  pBuff
 *      A memory buffer local to the SHA-1 function.
 *
 * @param[in]   offset
 *      The offset within the source data to begin copying from.
 *
 * @param[in]   size
 *      The size, in bytes, to copy.
 *
 * @param[in]   pInfo
 *      Pointer to a structure that contains the information needed to copy
 *      the appropriate values into the SHA-1 buffer.
 *
 * @return The number of bytes copied into the buffer.
 */

static LwU32
s_hdcpLCallback
(
    LwU8   *pBuff,
    LwU32   offset,
    LwU32   size,
    void   *pInfo
)
{
    LCallbackInfo *pCb = (LCallbackInfo *)pInfo;

    LwU32 copySize;
    LwU32 startOffset;
    LwU32 endOffset;
    LwU32 totalSize = 0;

    int   i;
    int   ofs;

    ofs = 0;
    while (size > 0)
    {
        // Stream Status: 2 bytes
        if (offset < (HDCP_L_STREAM_STATUS_OFFSET + HDCP_L_STREAM_STATUS_SIZE))
        {
            copySize    = LW_MIN(size, HDCP_L_STREAM_STATUS_SIZE);
            startOffset = offset - HDCP_L_STREAM_STATUS_OFFSET;
            endOffset   = startOffset + copySize;

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[ofs] = (LwU8) ((pCb->streamStatus >> (i * 8)) & 0xFF);
                ofs++;
            }
        }

        // Client-supplied Nonce (Q_id): 7 bytes
        else if (offset < (HDCP_L_CLIENT_ID_OFFSET + HDCP_L_CLIENT_ID_SIZE))
        {
            copySize    = LW_MIN(size, HDCP_L_CLIENT_ID_SIZE);
            startOffset = offset - HDCP_L_CLIENT_ID_OFFSET;
            endOffset   = startOffset + copySize;

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[ofs] = (LwU8) ((pCb->clientId >> (i * 8)) & 0xFF);
                ofs++;
            }
        }

        // Stream ID (S_id): 1 byte
        else if (offset < (HDCP_L_STREAM_ID_OFFSET + HDCP_L_STREAM_ID_SIZE))
        {
            copySize    = LW_MIN(size, HDCP_L_STREAM_ID_SIZE);
            startOffset = offset - HDCP_L_STREAM_ID_OFFSET;
            endOffset   = startOffset + copySize;

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[ofs] = (LwU8) ((pCb->streamId >> (i * 8)) & 0xFF);
                ofs++;
            }
        }

        // An (Link_id): 8 bytes
        else if (offset < (HDCP_L_AN_OFFSET + HDCP_L_AN_SIZE))
        {
            copySize    = LW_MIN(size, HDCP_L_AN_SIZE);
            startOffset = offset - HDCP_L_AN_OFFSET;
            endOffset   = startOffset + copySize;

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[ofs] = (LwU8) ((pCb->An >> (i * 8)) & 0xFF);
                ofs++;
            }
        }

        // Bksv (Link_Pk): 5 bytes
        else if (offset < (HDCP_L_BKSV_OFFSET + HDCP_L_BKSV_SIZE))
        {
            copySize    = LW_MIN(size, HDCP_L_BKSV_SIZE);
            startOffset = offset - HDCP_L_BKSV_OFFSET;
            endOffset   = startOffset + copySize;

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[ofs] = (LwU8) ((pCb->Bksv >> (i * 8)) & 0xFF);
                ofs++;
            }
        }

        // V (Link_V): 20 bytes if present, 0 bytes if not
        else if (offset < (HDCP_L_V_OFFSET + HDCP_L_V_SIZE(pCb)))
        {
            //
            // if V is not present, the M0 offset will equal the V offset
            // and this code will be skipped
            //
            copySize    = LW_MIN(size, HDCP_L_V_SIZE(pCb));
            startOffset = offset - HDCP_L_V_OFFSET;
            endOffset   = startOffset + copySize;

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[ofs] = (LwU8) (pCb->pV[i]);
                ofs++;
            }
        }

        // M0 (Link_s): 8 bytes
        else
        {
            copySize    = LW_MIN(size, HDCP_L_M0_SIZE);
            startOffset = offset - HDCP_L_M0_OFFSET(pCb);
            endOffset   = startOffset + copySize;

            for (i = startOffset; i < endOffset; i++)
            {
                pBuff[ofs] = (LwU8) ((pCb->M0 >> (i * 8)) & 0xFF);
                ofs++;
            }
        }

        size      -= copySize;
        offset    += copySize;
        totalSize += copySize;
    }

    return totalSize;
}


/*!
 * @brief   Callwlates V as defined in the HDCP spec.
 *
 * @param[in]  M0
 *          M<sub>0</sub> as defined in the HDCP spec.
 *
 * @param[in]  bInfo
 *          Value of B<sub>Info</sub> as defined in the HDCP spec.
 *
 * @param[in]  numKSVs
 *          Number of KSVs in the KSV List.
 *
 * @param[in]  pBKSVList
 *          Pointer to the KSVList.
 *
 * @param[out]  pV
 *          Pointer to the return value of V.
 */

void
hdcpCallwlateV
(
    LwU64             M0,
    LwU16             bInfo,
    LwU32             numKsvs,
    RM_FLCN_MEM_DESC *pBKSVList,
    LwU8              numBksvs,
    LwU8             *pV
)
{
    LwU32 size = 0;
    VCallbackInfo cbInfo;

    if ((!pV) || (numKsvs > LW_HDCP_SRM_MAX_ENTRIES))
    {
        return;
    }

    memset(pV, 0, LW_HDCPSRM_SIGNATURE_SIZE);

    // 1. Copy BKSVList
    cbInfo.pKsvList    = pBKSVList;
    cbInfo.ksvListSize = numKsvs * HDCP_RCV_BKSV_SIZE;
    cbInfo.bksvListSize = numBksvs * HDCP_RCV_BKSV_SIZE;

    // 2. Copy BStatus
    cbInfo.bInfo = bInfo;

    // 3. Copy M0
    cbInfo.m0 = M0;

    // 4. Compute SHA-1: we let the callback function copy the appropriate
    // data into the SHA-1 function.
    size = cbInfo.ksvListSize + sizeof(LwU16) + sizeof(LwU64);
    pmuSha1(pV, (void *)&cbInfo, size, s_hdcpVCallback);
}


/*!
 * @brief Copy callback for SRM signature verification.
 *
 * Since the SRM is stored in system memory, DMA transfers are required to
 * pass the data into the SHA-1 function.
 *
 * @param[out]  pBuff
 *      The buffer local to the SHA-1 function.
 *
 * @param[in]   offset
 *      The offset within the SRM to begin copying from.
 *
 * @param[in]   size
 *      The number of bytes to copy.
 *
 * @param[in]   pInfo
 *      Pointer to the SRM in system memory.
 *
 * @return The number of bytes copied.
 */

static LwU32
s_hdcpSrmCopyCallback
(
    LwU8   *pBuff,
    LwU32   offset,
    LwU32   size,
    void   *pInfo
)
{
    return s_hdcpDmaRead(pBuff, (RM_FLCN_MEM_DESC *)pInfo, offset, size);
}


/*!
 * @brief Verifies the SRM signature.
 *
 * @param[in]  pSRM
 *          Pointer to an SRM.
 *
 * @param[in]  dwSRMDataLength
 *          The size, in bytes, of the SRM.
 *
 * @returns FLCN_STATUS: FLCN_OK indicates that SRM has been verified.
 */

FLCN_STATUS
hdcpValidateSrm
(
    RM_FLCN_MEM_DESC *pSRM,
    LwU32             dwSRMDataLength
)
{
    LwU32  dwHeader;
    LwU32  dwVRLLength;

    LwU8  hdcpSrmSig_R[LW_HDCPSRM_SIGNATURE_SIZE];
    LwU8  hdcpSrmSig_S[LW_HDCPSRM_SIGNATURE_SIZE];
    LwU8  hdcpSrmSha1Hash[LW_HDCPSRM_SIGNATURE_SIZE];
    LwU8  srmTemp[32];
    FLCN_STATUS ret;

    LwU8  u1[SRM_MODULUS_SIZE] = { 0 };
    LwU8  u2[SRM_MODULUS_SIZE] = { 0 };
    LwU8  w[SRM_MODULUS_SIZE];

    // Verify instance and length
    if (!pSRM || dwSRMDataLength < LW_HDCPSRM_MIN_LENGTH)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    if (s_hdcpDmaRead(srmTemp, pSRM, 0, 32) == 0)
    {
        return FLCN_ERROR;
    }

    // Verify ID
    dwHeader = (srmTemp[LW_HDCPSRM_SRM_ID] << 8) ^
                srmTemp[LW_HDCPSRM_RESERVED1];
    if (dwHeader != 0x8000)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    dwVRLLength = (srmTemp[LW_HDCPSRM_VRL_LENGTH_HI] << 16) ^
        (srmTemp[LW_HDCPSRM_VRL_LENGTH_MED] << 8) ^
        srmTemp[LW_HDCPSRM_VRL_LENGTH_LO];

    if (dwSRMDataLength != dwVRLLength + LW_HDCPSRM_VRL_LENGTH)
    {
        //
        // SRM length should equal to VRL length + 5 (header + version +
        // reserved)
        //
        return FLCN_ERROR;
    }

    // Callwlate the SHA-1
    pmuSha1(hdcpSrmSha1Hash, (void *)pSRM, dwSRMDataLength -
            (LW_HDCPSRM_SIGNATURE_SIZE << 1), s_hdcpSrmCopyCallback);

    s_hdcpToBigInt(hdcpSrmSha1Hash);

    // Get the R portion of the signature
    s_hdcpDmaRead(hdcpSrmSig_R, pSRM,
                 dwSRMDataLength - LW_HDCPSRM_SIGNATURE_SIZE * 2,
                 LW_HDCPSRM_SIGNATURE_SIZE);
    s_hdcpToBigInt(hdcpSrmSig_R);

    // Get the S portion of the signature
    s_hdcpDmaRead(hdcpSrmSig_S, pSRM,
                 dwSRMDataLength - LW_HDCPSRM_SIGNATURE_SIZE,
                 LW_HDCPSRM_SIGNATURE_SIZE);
    s_hdcpToBigInt(hdcpSrmSig_S);

    //
    // Verify the DSA signature.
    //
    // The following values need to be made available to the signature
    // verifier.
    //
    // p = prime modulus: Provided by HDCP spec (HDCP_Modulus)
    // q = prime divisor: Provided by HDCP spec (HDCP_Divisor)
    // g = generator: Provided by HDCP spec (g_HDCP_SRM_Generator)
    // y = public key: Provided by sender (g_HDCP_SRM_PublicKey)
    // r = SRM signature, "r" value: Found in SRM (hdcpSrmSig_R)
    // s = SRM signature, "s" value: Found in SRM (hdcpSrmSig_S)
    // h = SHA-1(message): Callwlated from SRM (hdcpSrmSha1Hash)
    //
    // Steps needed to verify the DSA signature:
    //
    //  w = pow(s, -1) % q
    // u1 = (sha1(m) * w) % q
    // u2 = (r * w) % q
    //  v = ((pow(g, u1) * pow(y, u2)) % p) % q
    //
    // if (v == r) then the signature is verified, and the verifier can have
    // high confidence that the received message was sent by the party holding
    // the public key
    //

    // w = pow(s, -1) % q
    bigIntIlwerseMod((LwU32*)w, (LwU32*)hdcpSrmSig_S, &HDCP_Divisor);

    // u1 = (sha1(m) * w) % q
    bigIntMultiplyMod((LwU32*)u1, (LwU32*)hdcpSrmSha1Hash, (LwU32*)w,
        &HDCP_Divisor);

    // u2 = (r * w) % q
    bigIntMultiplyMod((LwU32*)u2, (LwU32*)hdcpSrmSig_R, (LwU32*)w,
        &HDCP_Divisor);

    //
    // Since (pow(g, u1) * pow(y, u2)) may too large for the buffers we use,
    // we need to rely on a property of mod to keep the values in more
    // manageable chunks.
    //
    // (a * b) % c = ((a % c) * (b % c)) % c
    //

    //
    // Don't need the value stored in w anymore; reuse the variable
    // w = pow(g, u1) % p
    //
    bigIntPowerMod((LwU32*)w, (LwU32*)g_HDCP_SRM_Generator, (LwU32*)u1,
        &HDCP_Modulus, SRM_MODULUS_SIZE / sizeof(LwU32));

    //
    // Don't need the value stored in u1 anymore; reuse the variable
    // u1 = pow(y, u2) % p
    //
    if (FLD_TEST_DRF(_FLCN, _HDCP_CTL_OPTIONS, _KEY_TYPE, _PROD, CtlOptions))
    {
        bigIntPowerMod((LwU32*)u1, (LwU32*)g_HDCP_SRM_PublicKey, (LwU32*)u2,
            &HDCP_Modulus, SRM_MODULUS_SIZE / sizeof(LwU32));
    }
    else
    {
        bigIntPowerMod((LwU32*)u1, (LwU32*)g_HDCP_SRM_TestPublicKey,
             (LwU32*)u2, &HDCP_Modulus, SRM_MODULUS_SIZE / sizeof(LwU32));
    }

    //
    // Don't need the value store in u2 anymore; reuse the variable
    // u2 = (w * u1) % p
    //
    bigIntMultiplyMod((LwU32*)u2, (LwU32*)w, (LwU32*)u1, &HDCP_Modulus);

    // At this point, we have v = u2 % q; reuse w as v
    bigIntMod((LwU32*)w, (LwU32*)u2, &HDCP_Divisor,
        SRM_MODULUS_SIZE / sizeof(LwU32));

    // If (w == r), the signature is verified
    ret = (bigIntCompare((LwU32*)w, (LwU32*)hdcpSrmSig_R,
        SRM_DIVISOR_SIZE / sizeof(LwU32)) == 0) ? FLCN_OK : FLCN_ERROR;

    return ret;
}


/*!
 * @brief Verifies whether an entry in the KSV list is on the SRM's revocation
 * list or not.
 *
 * Since the KSV list and SRM are located in system memory, the function will
 * read chunks of each, scanning each until it either finds a match or reaches
 * the end of both lists.
 *
 * @param srmList
 *      The SRM, stored in system memory.
 *
 * @param srmLength
 *      The size of the SRM in bytes.
 *
 * @param ksvList
 *      The KSV list, stored in system memory.
 *
 * @param ksvListSize
 *      The number of KSV entries.
 *
 * @return If a match was not found, FLCN_OK is returned; otherwise, FLCN_ERROR
 *      is returned.
 */

FLCN_STATUS
hdcpValidateRevocationList
(
    RM_FLCN_MEM_DESC  *srmList,
    LwU32              srmLength,
    RM_FLCN_MEM_DESC  *ksvList,
    LwU32              ksvListSize
)
{
    LwU8        srmChunk[CHUNK_SIZE];
    LwU8        ksvChunk[CHUNK_SIZE];
    LwU32       srmListSize = s_hdcpGetRevoListSize(srmList);
    LwU32       ksvIterSize;
    LwU32       srmOffset;
    LwU32       ksvOffset;

    LwU8        numSrmChunks;
    LwU8        numKsvChunks;
    LwU8        srmChunkEntries;
    LwU8        ksvChunkEntries;
    LwU8        srmChunkOffset;
    LwU8        ksvChunkOffset;

    LwU8        srmChunkIterator;
    LwU8        ksvChunkIterator;
    LwU8        srmEntryIterator;
    LwU8        ksvEntryIterator;

    LwU8        i;
    LwU8        j;

    //
    // Determine the number of DMA reads we need to do to read in the full
    // SRM revocation and KSV list. Since we are limited in memory and these
    // lists may be very large, they will need to be read in as chunks. The
    // chunks are designed to read in 8 entries from the list at a time.
    //
    numSrmChunks = (srmListSize) ?
                   ((srmListSize - 1) / NUM_ENTRIES_PER_CHUNK) + 1 : 0;
    numKsvChunks = (ksvListSize) ?
                   ((ksvListSize - 1) / NUM_ENTRIES_PER_CHUNK) + 1 : 0;

    srmOffset = LW_HDCPSRM_DEVICE_KSV_LIST;

    for (srmChunkIterator = 0; srmChunkIterator < numSrmChunks;
         srmChunkIterator++)
    {
        //
        // For each SRM chunk, we need to iterate across the KSV list to see
        // if any of the KSV entries match the set of entries found in the SRM
        // chunk.
        //
        s_hdcpDmaRead(srmChunk, srmList, srmOffset, CHUNK_SIZE);
        srmChunkEntries = (srmListSize < NUM_ENTRIES_PER_CHUNK) ?
                           srmListSize : NUM_ENTRIES_PER_CHUNK;

        ksvOffset = 0;
        ksvIterSize = ksvListSize;
        for (ksvChunkIterator = 0; ksvChunkIterator < numKsvChunks;
             ksvChunkIterator++)
        {
            s_hdcpDmaRead(ksvChunk, ksvList, ksvOffset, CHUNK_SIZE);
            ksvChunkEntries = (ksvIterSize < NUM_ENTRIES_PER_CHUNK) ?
                               ksvIterSize : NUM_ENTRIES_PER_CHUNK;

            //
            // Now that we have a chunk from both the SRM and KSV, verify
            // none of the entries in the chunks match. If a match is found,
            // it means that one of the devices in the KSV list has been
            // revoked.
            //
            for (srmEntryIterator = 0, srmChunkOffset = 0;
                 srmEntryIterator < srmChunkEntries;
                 srmEntryIterator++, srmChunkOffset += HDCP_RCV_BKSV_SIZE)
            {
                for (ksvEntryIterator = 0, ksvChunkOffset = 0;
                     ksvEntryIterator < ksvChunkEntries;
                     ksvEntryIterator++, ksvChunkOffset += HDCP_RCV_BKSV_SIZE)
                {
                    //
                    // The revoked devices in the SRM as stored as big endian
                    // values, whereas the KSV entries are stored as little
                    // endian values. Need to take this into consideration
                    // when performing the comparison.
                    //
                    for (i = 0, j = HDCP_RCV_BKSV_SIZE - 1;
                         i < HDCP_RCV_BKSV_SIZE;
                         i++, j--)
                    {
                        if (srmChunk[srmChunkOffset+j] !=
                            ksvChunk[ksvChunkOffset+i])
                        {
                            break;
                        }
                    }

                    if (i == HDCP_RCV_BKSV_SIZE)
                    {
                        //
                        // We've managed to compare HDCP_RCV_BKSV_SIZE bytes
                        // and the values are equal. That means we have a
                        // match and a revoked device. We can stop scanning at
                        // this point, since it doesn't matter how many
                        // additional devices have been revoked.
                        //
                        return FLCN_ERROR;
                    }
                }
            }

            ksvOffset   += CHUNK_SIZE;
            ksvIterSize -= ksvChunkEntries;
        }

        srmOffset   += CHUNK_SIZE;
        srmListSize -= srmChunkEntries;
    }

    return FLCN_OK;
}
