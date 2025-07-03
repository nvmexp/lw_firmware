/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/**
    Copyright Notice:
    Copyright 2021 DMTF. All rights reserved.
    License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
**/

// Included libspdm copyright, as file is LW-authored uses on libspdm code.

/*!
 * @file   lw_rng.c
 * @brief  File that provides RNG functionality from SEC2 to libspdm.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"

/* ------------------------- Application Includes --------------------------- */
#include "library/cryptlib.h"
#include "seapi.h"

/* ------------------------- Cryptlib.h Functions --------------------------- */
/**
  Sets up the seed value for the pseudorandom number generator.

  This function sets up the seed value for the pseudorandom number generator.
  If seed is not NULL, then the seed passed in is used.
  If seed is NULL, then default seed is used.
  If this interface is not supported, then return FALSE.

  @param[in] seed       Pointer to seed value.
                        If NULL, default seed is used.
  @param[in] seed_size  Size of seed value.
                        If seed is NULL, this parameter is ignored.

  @retval TRUE   Pseudorandom number generator has enough entropy for random generation.
  @retval FALSE  Pseudorandom number generator does not have enough entropy for random generation.
  @retval FALSE  This interface is not supported.

**/
boolean
random_seed
(
    IN const uint8 *seed OPTIONAL,
    IN uintn        seed_size
)
{
    // We do not need a random seed, leave this unimplemented.
    return LW_FALSE;
}

/**
  Generates a pseudorandom byte stream of the specified size.

  If output is NULL, then return FALSE.
  If this interface is not supported, then return FALSE.

  @param[out] output  Pointer to buffer to receive random value.
  @param[in]  size    size of random bytes to generate.

  @retval TRUE   Pseudorandom byte stream generated successfully.
  @retval FALSE  Pseudorandom number generator fails to generate due to lack of entropy.
  @retval FALSE  This interface is not supported.

**/
boolean
random_bytes
(
    OUT uint8 *output,
    IN  uintn  size
)
{
    LwU32  randWord    = 0;
    LwU32  bytesCopied = 0;
    LwU32  bytesToCopy = 0;
    LwU32  bytesLeft   = size;

    if (output == NULL)
    {
        return LW_FALSE;
    }

    while (bytesLeft > 0)
    {
        // Retrieve new random word.
        if (seTrueRandomGetNumber(&randWord, 1) != SE_OK)
        {
            return LW_FALSE;
        }

        // Copy as many bytes as are needed from word.
        bytesToCopy = (bytesLeft >= sizeof(randWord)) ? sizeof(randWord) : bytesLeft;

        copy_mem((LwU8 *)&(output[bytesCopied]), (LwU8 *)&randWord, bytesToCopy);
        bytesCopied += bytesToCopy;
        bytesLeft   -= bytesToCopy;
    }

    return LW_TRUE;
}
