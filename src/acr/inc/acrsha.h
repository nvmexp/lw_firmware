/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: acrsha.h
 */

//************************************************************************
// The HW IAS is at //hw/gpu_ip/doc/falcon/arch/TU10x_Falcon_SHA_IAS.docx
// ***********************************************************************

#ifndef _ACRSHA_H_
#define _ACRSHA_H_
#include <lwtypes.h>

#if defined(SEC2_PRESENT)
    #include "dev_sec_csb.h"
#elif defined(GSP_PRESENT)
    #include "dev_gsp_csb.h"
#endif

//
// pragama pack(1) can protect any padding bytes from compiler, and save memory usage.
//
#pragma pack(1)

#define LW_BYTESWAP32(a) (      \
    (((a) & 0xff000000)>>24) |  \
    (((a) & 0x00ff0000)>>8)  |  \
    (((a) & 0x0000ff00)<<8)  |  \
    (((a) & 0x000000ff)<<24) )

#define SHA_ENGINE_IDLE_TIMEOUT      (100000000) // sync with PTIMER, unit is ns, lwrrently 100ms is safe for the longest message
#define SHA_ENGINE_SW_RESET_TIMEOUT  (SHA_ENGINE_IDLE_TIMEOUT + 20) // sync with PTIMER, unit is ns, base on idle timeout, HW recommend 100ms + 20ns

#define BYTE_TO_BIT_LENGTH(a) (a << 3)

//
//  Although HW provide 4x32-bit registers to save message length (LW_CSEC_FALCON_SHA_MSG_LENGTH(i)) and
//  left message length(LW_CSEC_FALCON_SHA_MSG_LEFT(i)), the unit is bit.
//  And SW needs to update LW_CSEC_FALCON_SHA_MSG_LEFT(i) (i = 0~3) for every task.
//  If clinet wants to use 128-bit length mesage, SHA SW needs to implement 128-bit add/sub/shift functions.
//  This will make CPU spend much time and code size grows more.
//  We decide to only use 1 32-bit register to handle message length and left message length,
//  LW_CSEC_FALCON_SHA_MSG_LENGTH(0) and LW_CSEC_FALCON_SHA_MSG_LEFT(0).
//  The MAX message length will be limited to 0x1FFFFFFF bytes, and this size is enough to cover current requirement.
//
#define SHA_MSG_BYTES_MAX  (0xFFFFFFFF >> 3)

typedef enum
{
    SHA_ID_SHA_1       = (0),
    SHA_ID_SHA_224,     
    SHA_ID_SHA_256,     
    SHA_ID_SHA_384,     
    SHA_ID_SHA_512,     
    SHA_ID_SHA_512_224,
    SHA_ID_SHA_512_256,
    SHA_ID_LAST
} SHA_ID;

#if defined(SEC2_PRESENT)
    #define SHA_SRC_CONFIG_SRC_DMEM    LW_CSEC_FALCON_SHA_SRC_CONFIG_SRC_DMEM
    #define SHA_SRC_CONFIG_SRC_IMEM    LW_CSEC_FALCON_SHA_SRC_CONFIG_SRC_IMEM
    #define SHA_SRC_CONFIG_SRC_FB      LW_CSEC_FALCON_SHA_SRC_CONFIG_SRC_FB
#elif defined(GSP_PRESENT)
    #define SHA_SRC_CONFIG_SRC_DMEM    LW_PGSP_FALCON_SHA_SRC_CONFIG_SRC_DMEM
    #define SHA_SRC_CONFIG_SRC_IMEM    LW_PGSP_FALCON_SHA_SRC_CONFIG_SRC_IMEM
    #define SHA_SRC_CONFIG_SRC_FB      LW_PGSP_FALCON_SHA_SRC_CONFIG_SRC_FB
#endif

//
// For hash size and block size defines, please refer to SHA IAS
//   4.2.1 Data input, output
//   4.2.2 Session, task, operation
//   In multi task session, SW must ensure buffer size is multiple of SHA block size.
//
#define SHA_1_BLOCK_SIZE_BIT          (512)
#define SHA_1_BLOCK_SIZE_BYTE         (512/8)
#define SHA_1_HASH_SIZE_BIT           (160)
#define SHA_1_HASH_SIZE_BYTE          (SHA_1_HASH_SIZE_BIT/8)

#define SHA_224_BLOCK_SIZE_BIT        (512)
#define SHA_224_BLOCK_SIZE_BYTE       (512/8)
#define SHA_224_HASH_SIZE_BIT         (224)
#define SHA_224_HASH_SIZE_BYTE        (SHA_224_HASH_SIZE_BIT/8)

#define SHA_256_BLOCK_SIZE_BIT        (512)
#define SHA_256_BLOCK_SIZE_BYTE       (512/8)
#define SHA_256_HASH_SIZE_BIT         (256)
#define SHA_256_HASH_SIZE_BYTE        (SHA_256_HASH_SIZE_BIT/8)

#define SHA_384_BLOCK_SIZE_BIT        (1024)
#define SHA_384_BLOCK_SIZE_BYTE       (SHA_384_BLOCK_SIZE_BIT/8)
#define SHA_384_HASH_SIZE_BIT         (384)
#define SHA_384_HASH_SIZE_BYTE        (SHA_384_HASH_SIZE_BIT/8)

#define SHA_512_BLOCK_SIZE_BIT        (1024)
#define SHA_512_BLOCK_SIZE_BYTE       (SHA_512_BLOCK_SIZE_BIT/8)
#define SHA_512_HASH_SIZE_BIT         (512)
#define SHA_512_HASH_SIZE_BYTE        (SHA_512_HASH_SIZE_BIT/8)

#define SHA_512_224_BLOCK_SIZE_BIT    (1024)
#define SHA_512_224_BLOCK_SIZE_BYTE   (SHA_512_BLOCK_SIZE_BIT/8)
#define SHA_512_224_HASH_SIZE_BIT     (224)
#define SHA_512_224_HASH_SIZE_BYTE    (SHA_512_224_HASH_SIZE_BIT/8)

#define SHA_512_256_BLOCK_SIZE_BIT    (1024)
#define SHA_512_256_BLOCK_SIZE_BYTE   (SHA_512_BLOCK_SIZE_BIT/8)
#define SHA_512_256_HASH_SIZE_BIT     (256)
#define SHA_512_256_HASH_SIZE_BYTE    (SHA_512_256_HASH_SIZE_BIT/8)

// For SHA-512/t initial vector, please refer to SHA IAS, 4.2.3 SHA-512/t Support
#define SHA_512_224_IV_0          (0x19544DA2)
#define SHA_512_224_IV_1          (0x8C3D37C8)
#define SHA_512_224_IV_2          (0x89DCD4D6)
#define SHA_512_224_IV_3          (0x73E19966)
#define SHA_512_224_IV_4          (0x32FF9C82)
#define SHA_512_224_IV_5          (0x1DFAB7AE)
#define SHA_512_224_IV_6          (0x582F9FCF)
#define SHA_512_224_IV_7          (0x679DD514)
#define SHA_512_224_IV_8          (0x7BD44DA8)
#define SHA_512_224_IV_9          (0x0F6D2B69)
#define SHA_512_224_IV_10         (0x04C48942)
#define SHA_512_224_IV_11         (0x77E36F73)
#define SHA_512_224_IV_12         (0x6A1D36C8)
#define SHA_512_224_IV_13         (0x3F9D85A8)
#define SHA_512_224_IV_14         (0x91D692A1)
#define SHA_512_224_IV_15         (0x1112E6AD)

#define SHA_512_256_IV_0          (0xFC2BF72C)
#define SHA_512_256_IV_1          (0x22312194)
#define SHA_512_256_IV_2          (0xC84C64C2)
#define SHA_512_256_IV_3          (0x9F555FA3)
#define SHA_512_256_IV_4          (0x6F53B151)
#define SHA_512_256_IV_5          (0x2393B86B)
#define SHA_512_256_IV_6          (0x5940EABD)
#define SHA_512_256_IV_7          (0x96387719)
#define SHA_512_256_IV_8          (0xA88EFFE3)
#define SHA_512_256_IV_9          (0x96283EE2)
#define SHA_512_256_IV_10         (0x53863992)
#define SHA_512_256_IV_11         (0xBE5E1E25)
#define SHA_512_256_IV_12         (0x2C85B8AA)
#define SHA_512_256_IV_13         (0x2B0199FC)
#define SHA_512_256_IV_14         (0x81C52CA2)
#define SHA_512_256_IV_15         (0x0EB72DDC)


//
//   srcType       : The source address type of message, can be IMEM, DMEM or FB.
//   defaultHashIv : To indicate task needs to use default initial vector or not, non-zero values means YES.
//   dmaIdx:       : When scrType is FB, SHA engine needs dmaIdx to program DMA engine.
//   size          : Current message size in task, byte unit.
//   addr          : The source address of message.
//
typedef struct _SHA_TASK_CONFIG
{
    LwU8   srcType;
    LwU8   defaultHashIv;
    LwU8   pad[2];
    LwU32  dmaIdx;
    LwU32  size;
    LwU64  addr;
} SHA_TASK_CONFIG, *PSHA_TASK_CONFIG;

//
//  algoId  : SW SHA algorithm ID, SHA_ID_SHA_1, SHA_ID_SHA_224 ... etc.
//  msgSize : Total message size in bytes.
//
typedef struct _SHA_CONTEXT
{
    SHA_ID algoId;
    LwU32  msgSize;
} SHA_CONTEXT, *PSHA_CONTEXT;

#pragma pack()

#endif