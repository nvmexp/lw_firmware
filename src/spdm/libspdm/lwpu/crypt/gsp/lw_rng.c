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

// Included libSPDM copyright, as file is LW-authored uses on libSPDM code.

/*!
 * @file   lw_rng.c
 * @brief  File that provides RNG functionality from SEC2 to libSPDM.
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "library/cryptlib.h"

#include "tegra_se.h"
#include "tegra_lwpka_mod.h"
#include "tegra_lwpka_gen.h"

/* ------------------------- Public Functions ------------------------------- */
/**
  Sets up the seed value for the pseudorandom number generator.

  This function sets up the seed value for the pseudorandom number generator.
  If seed is not NULL, then the seed passed in is used.
  If seed is NULL, then default seed is used.
  If this interface is not supported, then return FALSE.

  @param[in]  seed      Pointer to seed value.
                        If NULL, default seed is used.
  @param[in]  seed_size  size of seed value.
                        If seed is NULL, this parameter is ignored.

  @retval TRUE   Pseudorandom number generator has enough entropy for random generation.
  @retval FALSE  Pseudorandom number generator does not have enough entropy for random generation.
  @retval FALSE  This interface is not supported.

**/
boolean random_seed(IN const uint8 *seed OPTIONAL, IN uintn seed_size)
{
  //
  // APM-TODO CONFCOMP-386: Implement RNG functionality.
  // Determine if we can leave this function unimplemented.
  //
  return FALSE;
}

/**
  Generates a pseudorandom byte stream of the specified size.

  If output is NULL, then return FALSE.
  If this interface is not supported, then return FALSE.

  @param[out]  output  Pointer to buffer to receive random value.
  @param[in]   size    size of random bytes to generate.

  @retval TRUE   Pseudorandom byte stream generated successfully.
  @retval FALSE  Pseudorandom number generator fails to generate due to lack of entropy.
  @retval FALSE  This interface is not supported.

**/
boolean random_bytes(OUT uint8 *output, IN uintn size)
{

    engine_t                     *engine     = NULL;
    engine_t                     *rngEngine  = NULL;
    struct se_engine_rng_context  rngContext = {0};
    struct se_data_params         rngParams  = {0};
    status_t  ret                            = NO_ERROR;

    if (output == NULL)
    {
        return FALSE;
    }
    if ((ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA) != NO_ERROR))
    {
        return FALSE;
    }
    if (lwpka_acquire_mutex(engine) != NO_ERROR)
    {
        return FALSE;
    }

    ret = ccc_select_engine((const struct engine_s **)&rngEngine, ENGINE_CLASS_AES, ENGINE_SE0_AES1); 
    if (ret != NO_ERROR)
    {
        goto fail;
    }

    rngContext.engine      = rngEngine;
    rngParams.dst          = output;
    rngParams.output_size  = size;

    ret = engine_genrnd_generate_locked(&rngParams, &rngContext);

fail:
    lwpka_release_mutex(engine); 
    return (ret == NO_ERROR);
}
