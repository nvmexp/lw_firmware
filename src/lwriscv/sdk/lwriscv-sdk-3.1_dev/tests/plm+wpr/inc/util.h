/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_UTIL_H
#define PLM_WPR_UTIL_H

/*!
 * @file        util.h
 * @brief       Miscellaneous utilities.
 */

// Standard includes.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// LIBLWRISCV includes.
#include <liblwriscv/memutils.h>


/*
 * @brief Combines the passed text into a single token (non-expanding).
 *
 * @param[in] left      The text to form the leftmost portion of the token.
 * @param[in] centre    The text to form the centre portion of the token.
 * @param[in] right     The text to form the rightmost portion of the token.
 *
 * @return
 *    The result of the concatenating the passed tokens.
 *
 * @note Does not expand arguments before joining.
 */
#define UTIL_JOIN_NE(left,centre,right) left##centre##right

/*
 * @brief Combines the passed text into a single token.
 *
 * @param[in] left      The text to form the leftmost portion of the token.
 * @param[in] centre    The text to form the centre portion of the token.
 * @param[in] right     The text to form the rightmost portion of the token.
 *
 * @return
 *    The result of the concatenating the passed tokens.
 *
 * @note Expands arguments before joining.
 */
#define UTIL_JOIN(left,centre,right) UTIL_JOIN_NE(left,centre,right)

/*
 * @brief Checks whether a particular bit is present in a bitmask.
 *
 * @param[in] mask  The bitmask to check.
 * @param[in] bit   The bit to check for.
 *
 * @return
 *    true  if bit is present (set) in mask.
 *    false if bit is not present (cleared) in mask.
 *
 * @note If the bit argument has multiple bits set, this macro returns true
 *       only if all bits are present in mask.
 */
#define UTIL_IS_SET(mask, bit) (((mask) & (bit)) == (bit))

/*
 * @brief Colwerts the passed token into a quoted string literal (non-expanding).
 *
 * @param[in] token  The text to string-ize.
 *
 * @return
 *    A quoted string literal containing (only) the passed token.
 *
 * @note Does not expand arguments before string-izing.
 */
#define UTIL_STRINGIZE_NE(token) #token

/*
 * @brief Colwerts the passed token into a quoted string literal.
 *
 * @param[in] token  The text to string-ize.
 *
 * @return
 *    A quoted string literal containing (only) the passed token.
 *
 * @note Expands argument before string-izing.
 */
#define UTIL_STRINGIZE(token) UTIL_STRINGIZE_NE(token)

#endif // PLM_WPR_UTIL_H
