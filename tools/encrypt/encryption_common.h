/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or diss_Closure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "lwdiagutils.h"
#include <stdio.h>
#include <vector>

namespace Encryption
{
    LwDiagUtils::EC LoadFile(FILE* file, vector<UINT08>* pBuffer);
}

// Obfuscate the real names of the functions to avoid exposing them in the exelwtable.
#define GetAESFileKey K8ajz
#define GetAESLogKey jG8_W
#define GetAESTraceFileKey zbY5j
#define GetAESKey kW8Sa

#ifdef __GNUC__
#define HIDDEN __attribute__((visibility("hidden")))
#else
#define HIDDEN
#endif

// These arrays holds two AES keys, one is the main AES key for encrypted
// files, the other is the AES key for encrypting logs.  Both arrays
// contain 3 conselwtive items, each 16 bytes in size:
// - the encrypted key
// - AES key to decrypt the above key
// - initialization vector for decrypting the key
// The arrays are generated at build time with gen_aes_key.py script.
// The names of the variables are illegible on purpose.
extern const unsigned char X_1[16 * 3];
extern const unsigned char m_6[16 * 3];
extern const unsigned char t_7[16 * 3];

HIDDEN void GetAESKey(UINT08* buf, const UINT08* encryptedKey);

static inline void GetAESFileKey(UINT08* buf)
{
    GetAESKey(buf, X_1);
}

static inline void GetAESLogKey(UINT08* buf)
{
    GetAESKey(buf, m_6);
}

static inline void GetAESTraceFileKey(UINT08* buf)
{
    GetAESKey(buf, t_7);
}

#define LOG_VERSION_LENGTH 48 // max length of version string saved in log file
#define LOG_ARCH_LENGTH 8     // max length of arch string saved in log file

// Arbitrary sequence of bytes used to verify whether decryption worked
#define LOG_ENCRYPTION_VERIF_STR "encrypted"
