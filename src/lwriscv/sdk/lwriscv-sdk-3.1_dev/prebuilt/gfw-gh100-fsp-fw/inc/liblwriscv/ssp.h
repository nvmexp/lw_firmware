/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_SSP_H
#define LIBLWRISCV_SSP_H
#include <stdint.h>
#include <lwriscv/gcc_attrs.h>
#include <lwriscv/status.h>

#if LWRISCV_FEATURE_SSP_FORCE_SW_CANARY
static inline LWRV_STATUS sspGenerateAndSetCanaryWithInit(void) GCC_ATTR_WARN_UNUSED;
static inline LWRV_STATUS sspGenerateAndSetCanaryWithInit(void)
{
    return LWRV_ERR_NOT_SUPPORTED;
}

static inline LWRV_STATUS sspGenerateAndSetCanary(void) GCC_ATTR_WARN_UNUSED;
static inline LWRV_STATUS sspGenerateAndSetCanary(void)
{
    return LWRV_ERR_NOT_SUPPORTED;
}
#else // LWRISCV_FEATURE_SSP_FORCE_SW_CANARY
/*!
 * \brief Initializes HWRNG, generates and sets stack canary.
 * \return
 *      LWRV_OK                success, canary is set
 *      LWRV_ERR_NOT_SUPPORTED if SCP driver is not enabled
 *      Other                  passed-through SCP errors
 *
 * This function:
 * - Initializes HWRNG inside SCP (via SCP driver)
 * - Generates and sets random with sspGenerateAndSetCanary()
 * - Shuts down HWRNG
 *
 * \warning It should be called early at the start of module.
 * \warning It should not be used, if module is using SCP for other purposes -
 * in that case module should take care of SCP and just call sspGenerateAndSetCanary()
 * \note Errors reported by this function should be considered fatal.
 */
LWRV_STATUS sspGenerateAndSetCanaryWithInit(void) GCC_ATTR_WARN_UNUSED;

/*!
 * \brief Generates and sets stack canary.
 * \return
 *      LWRV_OK                success, canary is set
 *      LWRV_ERR_NOT_SUPPORTED if SCP driver is not enabled
 *      Other                  passed-through SCP errors
 *
 * This function generates random number using SCP HWRNG, then writes it to
 * canary.
 *
 * \warning There are requirements regarding symbols used by this function when
 * used.
 * \warning This function assumes SSP is initialized and running.
 * \note Errors reported by this function should be considered fatal.
 */
LWRV_STATUS sspGenerateAndSetCanary(void) GCC_ATTR_WARN_UNUSED;
#endif // LWRISCV_FEATURE_SSP_FORCE_SW_CANARY

/*!
 * \brief Returns current canary value.
 * \return canary value
 *
 * This function shuld be used to retrieve canary value for storage during
 * partition or context switches (if canary can't be regenerated).
 */
uintptr_t sspGetCanary(void);

/*!
 * \brief Sets current canary value.
 * \param[in] canary
 *
 * This function should be used to:
 * - Restore stack canary after partition or context switches
 * - Configure canary if SCP is not used or not available.
 */
void sspSetCanary(uintptr_t canary);

#if LWRISCV_FEATURE_SSP_ENABLE_FAIL_HOOK
/*!
 * \brief Hook called if canary fails.
 *
 * \warning This hook shuold be (most likely) implemented in assembly and make
 * sure stack is not used (as stack pointer is not reliable when this function
 * is reached).
 * \warning This hook should not return.
 */
extern void sspCheckFailHook(void) GCC_ATTR_NORETURN;
#endif // LWRISCV_FEATURE_SSP_ENABLE_FAIL_HOOK

#endif // LIBLWRISCV_SSP_H
