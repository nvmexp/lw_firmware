/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

 /*!
 * @file   lw_shared_mem.c
 * @brief  The file contains the variables and the implementations of the
 *         interface for accessing SPDM shared memory
 */

/* ------------------------- System Includes -------------------------------- */
#include "base.h"
#include "lwuproc.h"
#include "lwoslayer.h"
#include "lwrtos.h"
#include "lwos_utility.h"
#include "lwos_ovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "lw_apm_rts.h"
#include "lw_apm_const.h"
#include "lw_apm_spdm_common.h"

/* ------------------------ Macros and Defines ----------------------------- */

 // Struct with the IV table used by APM
typedef struct
{
    LwU8   ivSlot[APM_IV_TABLE_COUNT][APM_IV_BASE_SIZE_IN_BYTES];
    LwBool ivSlotValid[APM_IV_TABLE_COUNT];
} SHARED_IV_TABLE_T;

// Struct with the symmetric key used by APM
typedef struct
{
    LwU8   secretKey[APM_KEY_SIZE_IN_BYTES];
} SHARED_SECRET_KEY_T;

/*!
 * The following data structures are shared between SPDM and APM tasks.
 * To synchronize, they are in a separate DMEM overlay for shared data, and
 * guarded by g_apmSpdmSharedMutex, which is initialized during SEC2 init.
 * 
 * The boolean g_bApmSpdmSharedEnabled tracks whether the shared data can be
 * used, and each shared data structure has its own boolean to track whether
 * it has been enabled.
 */
static SHARED_IV_TABLE_T g_apmSpdmSharedIvTable
    GCC_ATTRIB_SECTION("dmem_apmSpdmShared", "g_sharedIvTable");

static SHARED_SECRET_KEY_T g_apmSpdmSharedSecretKey
    GCC_ATTRIB_SECTION("dmem_apmSpdmShared", "g_apmSpdmSharedSecretKey");

/*!
 * Define static synchronization variables for the shared APM SPDM overlay
 * This needs to be stored in the resident memory for the tasks not
 * to load other, larger overlays until they have been initialized
 * with the appropriate data
 */

static LwrtosSemaphoreHandle g_apmSpdmSharedMutex;
static LwBool                g_bApmSpdmSharedEnabled;
static LwBool                g_bApmSpdmSharedDataKeyValid;
static LwBool                g_bApmSpdmSharedIvTableValid;

/**
  @brief This function initializes the sychronization variables for sharing
         between SPDM and APM. This function should be called in SEC2's main.c.
 */
void
apm_spdm_shared_pre_init(void)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    flcnStatus = lwrtosSemaphoreCreateBinaryGiven(&g_apmSpdmSharedMutex,
                                                  OVL_INDEX_OS_HEAP);

    //
    // Consider data sharing enabled if mutex creation successful.
    // In either case, data is invalid until initialized later.
    //
    g_bApmSpdmSharedEnabled = (flcnStatus == FLCN_OK);
    g_bApmSpdmSharedDataKeyValid = LW_FALSE;
    g_bApmSpdmSharedIvTableValid = LW_FALSE;
}

/**
  @brief Writes the input buffer to the APM data encryption key buffer.
         This sets the key buffer as initialized, allowing it to be read.

  @note This function will load and attach the apmSpdmShared DMEM overlay for
        the duration of function exelwtion.

  @param[out] pDataKey      Input buffer to write to APM data encryption key.
                            Must be size APM_KEY_SIZE_IN_BYTES.
  @param[in]  mutexTimeout  Amount of ticks to wait for shared APM-SPDM
                            mutex acquisition. Returns failure on timeout.

  @returns  FLCN_OK on successful write of data key, relevant error otherwise.
 */
FLCN_STATUS
apm_spdm_shared_write_data_key
(
    IN      LwU8    *pDataKey,
    IN      LwU32    mutexTimeout
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       i          = 0;

    // Ensure shared data is enabled and valid.
    if (!g_bApmSpdmSharedEnabled)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }
    else if (pDataKey == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Acquire mutex.
    if (lwrtosSemaphoreTake(g_apmSpdmSharedMutex, mutexTimeout) != FLCN_OK)
    {
        return FLCN_ERR_MUTEX_ACQUIRED;
    }

    // Explicitly load and unload separate DMEM overlay containing data key.
    OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(OVL_INDEX_DMEM(apmSpdmShared));
    for (i = 0; i < sizeof(g_apmSpdmSharedSecretKey.secretKey); i++)
    {
        g_apmSpdmSharedSecretKey.secretKey[i] = pDataKey[i];
    }
    OSTASK_DETACH_DMEM_OVERLAY(OVL_INDEX_DMEM(apmSpdmShared));

    // The key is valid after writing.
    g_bApmSpdmSharedDataKeyValid = LW_TRUE;

    flcnStatus = lwrtosSemaphoreGive(g_apmSpdmSharedMutex);
    return flcnStatus;
}

/**
  @brief Reads the APM data encryption key to output buffer. Ensures the key
         is valid before writing to output buffer.

  @note This function will load and attach the apmSpdmShared DMEM overlay for
         the duration of function exelwtion.

  @param[out] pDataKeyOut   Output buffer where APM data encryption key will
                            be written. Must be size APM_KEY_SIZE_IN_BYTES.
  @param[in]  mutexTimeout  Amount of ticks to wait for shared APM-SPDM
                            mutex acquisition. Returns failure on timeout.

  @returns  FLCN_OK on successful read of data key, relevant error otherwise.
 */
FLCN_STATUS
apm_spdm_shared_read_data_key
(
    OUT     LwU8  *pDataKeyOut,
    IN      LwU32  mutexTimeout
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    FLCN_STATUS tmpStatus  = FLCN_OK;
    LwU32       i          = 0;

    // Ensure both tasks are enabled, and shared data is enabled and valid.
    if (!g_bApmSpdmSharedEnabled)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }
    else if (pDataKeyOut == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Acquire mutex
    if (lwrtosSemaphoreTake(g_apmSpdmSharedMutex, mutexTimeout) != FLCN_OK)
    {
        return FLCN_ERR_MUTEX_ACQUIRED;
    }

    // Now that mutex is acquired, ensure that data is valid.
    if (!g_bApmSpdmSharedDataKeyValid)
    {
        flcnStatus = FLCN_ERR_OBJECT_NOT_FOUND;
        goto ErrorExit;
    }

    // Explicitly load and unload separate DMEM overlay containing data key.
    OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(OVL_INDEX_DMEM(apmSpdmShared));
    for (i = 0; i < sizeof(g_apmSpdmSharedSecretKey.secretKey); i++)
    {
        pDataKeyOut[i] = g_apmSpdmSharedSecretKey.secretKey[i];
    }
    OSTASK_DETACH_DMEM_OVERLAY(OVL_INDEX_DMEM(apmSpdmShared));

ErrorExit:
    tmpStatus = lwrtosSemaphoreGive(g_apmSpdmSharedMutex);
    return (flcnStatus == FLCN_OK) ? tmpStatus : flcnStatus;
}

/**
  @brief Writes an IV to a given slot in the IV table.

  @note This function will load and attach the apmSpdmShared DMEM overlay for
        the duration of function exelwtion.

  @param[in] pIv          The buffer containing the IV of length APM_IV_BASE_SIZE_IN_BYTES
  @param[in] ivSlot       The destination IV slot
  @param[in] mutexTimeout The maximum waiting time for the synchronization
  @return FLCN_STATUS representing success, or relevant error.
 */
FLCN_STATUS
apm_spdm_shared_write_iv_table
(
    IN      LwU8  *pIv,
    IN      LwU32  ivSlot,
    IN      LwU32  mutexTimeout
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    LwU32       i          = 0;

    // Ensure both tasks are enabled, and shared data is enabled and valid.
    if (!g_bApmSpdmSharedEnabled)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }
    else if (pIv == NULL || ivSlot >= APM_IV_TABLE_COUNT)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Acquire mutex.
    if (lwrtosSemaphoreTake(g_apmSpdmSharedMutex, mutexTimeout) != FLCN_OK)
    {
        return FLCN_ERR_MUTEX_ACQUIRED;
    }

    // Explicitly load and unload separate DMEM overlay containing data key.
    OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(OVL_INDEX_DMEM(apmSpdmShared));
    for (i = 0; i < APM_IV_BASE_SIZE_IN_BYTES; i++)
    {
        g_apmSpdmSharedIvTable.ivSlot[ivSlot][i] = pIv[i];
    }

    g_apmSpdmSharedIvTable.ivSlotValid[ivSlot] = LW_TRUE;
    g_bApmSpdmSharedIvTableValid               = LW_TRUE;

    OSTASK_DETACH_DMEM_OVERLAY(OVL_INDEX_DMEM(apmSpdmShared));


    flcnStatus = lwrtosSemaphoreGive(g_apmSpdmSharedMutex);
    return flcnStatus;
}

/**
  @brief Reads an IV from a slot in the IV table.

   @note This function will load and attach the apmSpdmShared DMEM overlay for
         the duration of function exelwtion.

  @param[out] pIv          The destination buffer of length APM_IV_BASE_SIZE_IN_BYTES
  @param[in]  ivSlot       The destination IV slot
  @param[in]  mutexTimeout The maximum waiting time for the synchronization

  @return FLCN_STATUS representing success, or relevant error.
 */
FLCN_STATUS
apm_spdm_shared_read_iv_table
(
    OUT     LwU8  *pIvOut,
    IN      LwU32  ivSlot,
    IN      LwU32  mutexTimeout
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;
    FLCN_STATUS tmpStatus  = FLCN_OK;
    LwU32       i          = 0;

    // Ensure both tasks are enabled, and shared data is enabled and valid.
    if (!g_bApmSpdmSharedEnabled)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }
    else if (pIvOut == NULL || ivSlot >= APM_IV_TABLE_COUNT)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Acquire mutex
    if (lwrtosSemaphoreTake(g_apmSpdmSharedMutex, mutexTimeout) != FLCN_OK)
    {
        return FLCN_ERR_MUTEX_ACQUIRED;
    }

    // Now that mutex is acquired, ensure that data is valid.
    if (!g_bApmSpdmSharedIvTableValid)
    {
        flcnStatus = FLCN_ERR_OBJECT_NOT_FOUND;
        goto ErrorExit;
    }

    // Explicitly load and unload separate DMEM overlay containing data key.
    OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(OVL_INDEX_DMEM(apmSpdmShared));
    if (g_apmSpdmSharedIvTable.ivSlotValid[ivSlot])
    {
        for (i = 0; i < APM_IV_BASE_SIZE_IN_BYTES; i++)
        {
            pIvOut[i] = g_apmSpdmSharedIvTable.ivSlot[ivSlot][i];
        }
    }
    else
    {  
        //
        // We do not need GOTO because the error handler is exelwted right after
        // the overlay is detached.
        //
        flcnStatus = FLCN_ERR_OBJECT_NOT_FOUND;
    }
    OSTASK_DETACH_DMEM_OVERLAY(OVL_INDEX_DMEM(apmSpdmShared));

ErrorExit:
    tmpStatus = lwrtosSemaphoreGive(g_apmSpdmSharedMutex);
    return (flcnStatus == FLCN_OK) ? tmpStatus : flcnStatus;
}

/**
  @brief This function clears the overlay with the shared data.

  @note This function will load and attach the apmSpdmShared DMEM overlay for
        the duration of function exelwtion.

  @return FLCN_STATUS FLCN_OK if successful, relevant error otherwise.
 */
FLCN_STATUS
apm_spdm_shared_clear
(
    IN      LwU32  mutexTimeout
)
{
    FLCN_STATUS flcnStatus = FLCN_OK;

    // Acquire mutex.
    if (lwrtosSemaphoreTake(g_apmSpdmSharedMutex, mutexTimeout) != FLCN_OK)
    {
        return FLCN_ERR_MUTEX_ACQUIRED;
    }

    OSTASK_ATTACH_AND_LOAD_DMEM_OVERLAY(OVL_INDEX_DMEM(apmSpdmShared));

    g_bApmSpdmSharedIvTableValid = LW_FALSE;
    g_bApmSpdmSharedDataKeyValid = LW_FALSE;

    zero_mem(&g_apmSpdmSharedSecretKey, sizeof(g_apmSpdmSharedSecretKey));
    zero_mem(&g_apmSpdmSharedIvTable,  sizeof(g_apmSpdmSharedIvTable));

    OSTASK_DETACH_DMEM_OVERLAY(OVL_INDEX_DMEM(apmSpdmShared));

    flcnStatus = lwrtosSemaphoreGive(g_apmSpdmSharedMutex);
    return flcnStatus;
}
