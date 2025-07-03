/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   util.h
 * @brief  Utility functions and macros.
 */

#ifndef UTIL_H
#define UTIL_H

#include <engine.h>
#include <lwmisc.h>

#include "bootloader.h"

/*!
 * @brief A per-engine implementation of FLD_TEST_DRF().
 *
 * @param[in]   d   The device the target register belongs to (e.g. _SCP).
 * @param[in]   r   The register the target field belongs to (e.g. _CTL_STAT).
 * @param[in]   f   The target field of interest (e.g. _DEBUG_MODE).
 * @param[in]   c   The manual constant to compare against (e.g. _DISABLED).
 * @param[in]   v   The register value to test (e.g. from csbReadPA()).
 *
 * @return True if the location specified by ENGINE_REG(drf) in 'v' has the
 *         same value as ENGINE_REG(drfc).
 */
#define FLD_TEST_DRF_ENG(d,r,f,c,v) FLD_TEST_REF_WRAP1(ENGINE_REG(d),r,f,c,v)

/*!
 * @brief A wrapper used by FLD_TEST_DRF_ENG() to force ENGINE_REG() to expand.
 *
 * @param[in]   d   The fully-prefixed device name (e.g. LW_PGSP_SCP).
 * @param[in]   r   The register the target field belongs to (e.g. _CTL_STAT).
 * @param[in]   f   The target field of interest (e.g. _DEBUG_MODE).
 * @param[in]   c   The manual constant to compare against (e.g. _DISABLED).
 * @param[in]   v   The register value to test (e.g. from csbReadPA()).
 *
 * @return True if the location specified by drf in 'v' has the same value as
 *         drfc.
 */
#define FLD_TEST_REF_WRAP1(d,r,f,c,v) FLD_TEST_REF_WRAP2(d,r,f,c,v)

/*!
 * @brief A wrapper used by FLD_TEST_REF_WRAP1() to concatenate d, r, and f.
 *
 * @param[in]   d   The fully-prefixed device name (e.g. LW_PGSP_SCP).
 * @param[in]   r   The register the target field belongs to (e.g. _CTL_STAT).
 * @param[in]   f   The target field of interest (e.g. _DEBUG_MODE).
 * @param[in]   c   The manual constant to compare against (e.g. _DISABLED).
 * @param[in]   v   The register value to test (e.g. from csbReadPA()).
 *
 * @return True if the location specified by drf in 'v' has the same value as
 *         drfc.
 */
#define FLD_TEST_REF_WRAP2(d,r,f,c,v) FLD_TEST_REF(d##r##f,c,v)

/*!
 * @brief Check if a memory base+size would cause integer overflow.
 *
 * @param[in]   base    Base address of the region to check.
 * @param[in]   size    Size of the region to check, in bytes.
 *
 * @return True if the region causes an integer overflow.
 */
static inline __attribute__((__always_inline__))
LwBool
utilPtrDoesOverflow
(
    LwUPtr base,
    LwUPtr size
)
{
    return ((base + size) < base);
}

/*!
 * @brief Checks whether two memory regions overlap.
 *
 * This DOES NOT check for integer overflow! Use utilPtrDoesOverflow to sanitize these inputs first.
 *
 * @param[in]    firstAddr      Base address of the first region.
 * @param[in]    firstSize      Size in bytes of the first region.
 * @param[in]    secondAddr     Base address of the second region.
 * @param[in]    secondSize     Size in bytes of the second region.
 *
 * @return True if they overlap, false if they do not.
 */
static inline __attribute__((__always_inline__))
LwBool
utilCheckOverlap
(
    LwUPtr firstAddr,
    LwUPtr firstSize,
    LwUPtr secondAddr,
    LwUPtr secondSize
)
{
    return ((secondAddr < ( firstAddr +  firstSize)) &&
            (firstAddr  < (secondAddr + secondSize)));
}

/*!
 * @brief Checks whether the second memory region is fully contained wihin the first.
 *
 * This DOES NOT check for integer overflow! Use utilPtrDoesOverflow to sanitize these inputs first.
 *
 * @param[in]    containerBase  Base address of the container region.
 * @param[in]    containerSize  Size in bytes of the container region.
 * @param[in]    checkBase      Base address of the region to check.
 * @param[in]    checkSize      Size in bytes of the region to check.
 *
 * @return True if check region lies within container region.
 */
static inline __attribute__((__always_inline__))
LwBool
utilCheckWithin
(
    LwUPtr containerBase,
    LwUPtr containerSize,
    LwUPtr checkBase,
    LwUPtr checkSize
)
{
    const LwUPtr containerEnd = containerBase + containerSize;
    const LwUPtr checkEnd = checkBase + checkSize;

    return ((checkBase >= containerBase) &&
            (checkEnd  <= containerEnd));
}

/*!
 * @brief Checks whether an address is contained wihin a memory region.
 *
 * This DOES NOT check for integer overflow! Use utilPtrDoesOverflow to sanitize these inputs first.
 *
 * @param[in]    containerBase  Base address of the container region.
 * @param[in]    containerSize  Size in bytes of the container region.
 * @param[in]    checkAddr      The address to check.
 *
 * @return True if check address lies within container region.
 */
static inline __attribute__((__always_inline__))
LwBool
utilCheckAddrWithin
(
    LwUPtr containerBase,
    LwUPtr containerSize,
    LwUPtr checkAddr
)
{
    const LwUPtr containerEnd = containerBase + containerSize;

    return ((checkAddr >= containerBase) &&
            (checkAddr < containerEnd));
}

#endif // UTIL_H
