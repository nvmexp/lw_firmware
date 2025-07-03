/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lib_hdcpauth.h
 * @brief   helps validation of srm and revocation check.
 */

#ifndef LIB_HDCPAUTH_H
#define LIB_HDCPAUTH_H

/* ------------------------ System Includes --------------------------------- */
#include "lwuproc.h"

/* ------------------------ Application Includes ---------------------------- */
#include "flcntypes.h"
#include "flcnretval.h"
#include "flcnifhdcp.h"
#include "lib_sha1.h"

/* ------------------------ Defines ---------------------------------------- */
#define SRM_MODULUS_SIZE            128
#define SRM_DIVISOR_SIZE            20
#define SRM_GENERATOR_SIZE          128
#define SRM_PUBLIC_KEY_SIZE         128

// Define required for SRM validation
#define LW_HDCPSRM_SRM_ID           0
#define LW_HDCPSRM_RESERVED1        1
#define LW_HDCPSRM_SRM_VERSION      2
#define LW_HDCPSRM_RESERVED2        4
#define LW_HDCPSRM_VRL_LENGTH       5
#define LW_HDCPSRM_VRL_LENGTH_HI    5
#define LW_HDCPSRM_VRL_LENGTH_MED   6
#define LW_HDCPSRM_VRL_LENGTH_LO    7
#define LW_HDCPSRM_NUM_DEVICES      8
#define LW_HDCPSRM_RESERVED3        0x80
#define LW_HDCPSRM_DEVICE_KSV_LIST  9
#define LW_HDCPSRM_SIGNATURE_SIZE   20
#define LW_HDCPSRM_MIN_LENGTH       48
#define LW_HDCP_SRM_MAX_ENTRIES     128

#define LW_HDCP_L_SIZE              20

#define LW_HDCPSRM_SIZE             32
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
 * Note: This local macro below, in addition for being used for DMA alignment,
 * is also used to decide the size of a local array. Hence, instead basing its
 * value off of the DMA read alignment value used in RTOS code (which is
 * arch-dependent), we will make an exception for it and hardcode it to 16.
 * That is what it was intended to be, when the code was originally written.
 */
#define HDCP_DMA_READ_ALIGNMENT         16
#define HDCP_SHA1_BUFFER_SIZE           64

#define HDCP_RCV_BKSV_SIZE              5
#define NUM_ENTRIES_PER_CHUNK           8
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

/*!
 * Parameter for the SHA-1 callback when callwlating the value of V.
 */
typedef struct
{
    PRM_FLCN_MEM_DESC pKsvList;       //!< Location of the KSV list in system memory
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
    LwU8        *pV;            //!< V, if repeater; null, if not
    LwU8        streamId;       //!< Stream ID
} LCallbackInfo;

/* ----------------------- External Definations ---------------------------- */
FLCN_STATUS hdcpValidateSrm(PRM_FLCN_MEM_DESC, LwU32, LwU32)
    GCC_ATTRIB_SECTION("imem_libHdcpauth", "hdcpValidateSrm");
LwU32 hdcpAuthDmaRead(LwU8 *pDst, PRM_FLCN_MEM_DESC pSrc, LwU32 srcOffset, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libHdcpauth", "hdcpAuthDmaRead");
void libSha1(LwU8* pHash, void *pData, LwU32 nBytes, FlcnSha1CopyFunc (copyFunc))
    GCC_ATTRIB_SECTION("imem_libHdcpauth", "libSha1");

#endif // LIB_HDCPAUTH_H
