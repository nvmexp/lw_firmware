/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_SANITIZERCOV_TEST_H
#define LIB_SANITIZERCOV_TEST_H

#include <stdint.h>

typedef uint8_t LwU8;
typedef uint16_t LwU16;
typedef uint32_t LwU32;
typedef uint64_t LwU64;

typedef float LwF32;
typedef double LwF64;

typedef LwU8 FLCN_STATUS;
#define FLCN_OK (0x00U)
#define FLCN_ERR_ILWALID_ARGUMENT (0x02U)
#define FLCN_ERR_ILWALID_STATE (0x35U)

typedef LwU8 LwBool;
#define LW_FALSE ((LwBool)(0 != 0))
#define LW_TRUE ((LwBool)(0 == 0))

#define GCC_ATTRIB_SECTION(...)

#endif  // LIB_SANITIZERCOV_TEST_H