/*******************************************************************************
    Copyright (c) 2014-2019 LWpu Corporation

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

        The above copyright notice and this permission notice shall be
        included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#ifndef SDK_LWSTATUS_H
#define SDK_LWSTATUS_H

/* XAPIGEN - this file is not suitable for (nor needed by) xapigen.         */
/*           Rather than #ifdef out every such include in every sdk         */
/*           file, punt here.                                               */
#if !defined(XAPIGEN)        /* rest of file */

#ifdef __cplusplus
extern "C" {
#endif

#include "lwtypes.h"

typedef LwU32 LW_STATUS;

#define LW_STATUS_CODE( name, code, string ) name = (code),

enum 
{
    #include "lwstatuscodes.h"
};

#undef LW_STATUS_CODE

/*!
 * @def         LW_STATUS_LEVEL_OK
 * @see         LW_STATUS_LEVEL
 * @brief       Success: No error or special condition
 */
#define LW_STATUS_LEVEL_OK              0

/*!
 * @def         LW_STATUS_LEVEL_WARN
 * @see         LW_STATUS_LEVEL
 * @brief       Sucess, but there is an special condition
 *
 * @details     In general, LW_STATUS_LEVEL_WARN status codes are handled the
 *              same as LW_STATUS_LEVEL_OK, but are usefil to indicate that
 *              there is a condition that may be specially handled.
 *
 *              Therefore, in most cases, client function should test for
 *              status <= LW_STATUS_LEVEL_WARN or status > LW_STATUS_LEVEL_WARN
 *              to determine success v. failure of a call.
 */
#define LW_STATUS_LEVEL_WARN            1

/*!
 * @def         LW_STATUS_LEVEL_ERR
 * @see         LW_STATUS_LEVEL
 * @brief       Unrecoverable error condition
 */
#define LW_STATUS_LEVEL_ERR             3

/*!
 * @def         LW_STATUS_LEVEL
 * @see         LW_STATUS_LEVEL_OK
 * @see         LW_STATUS_LEVEL_WARN
 * @see         LW_STATUS_LEVEL_ERR
 * @brief       Level of the status code
 *
 * @warning     IMPORTANT: When comparing LW_STATUS_LEVEL(_S) against one of
 *              these constants, it is important to use '<=' or '>' (rather
 *              than '<' or '>=').
 *
 *              For example. do:
 *                  if (LW_STATUS_LEVEL(status) <= LW_STATUS_LEVEL_WARN)
 *              rather than:
 *                  if (LW_STATUS_LEVEL(status) < LW_STATUS_LEVEL_ERR)
 *
 *              By being consistent in this manner, it is easier to systematically
 *              add additional level constants.  New levels are likely to lower
 *              (rather than raise) the severity of _ERR codes.  For example,
 *              if we were to add LW_STATUS_LEVEL_RETRY to indicate hardware
 *              failures that may be recoverable (e.g. RM_ERR_TIMEOUT_RETRY
 *              or RM_ERR_BUSY_RETRY), it would be less severe than
 *              LW_STATUS_LEVEL_ERR the level to which these status codes now
 *              belong.  Using '<=' and '>' ensures your code is not broken in
 *              cases like this.
 */
#define LW_STATUS_LEVEL(_S)                                               \
    ((_S) == LW_OK?                               LW_STATUS_LEVEL_OK:     \
    ((_S) != LW_ERR_GENERIC && (_S) & 0x00010000? LW_STATUS_LEVEL_WARN:   \
                                                  LW_STATUS_LEVEL_ERR))

/*!
 * @def         LW_STATUS_LEVEL
 * @see         LW_STATUS_LEVEL_OK
 * @see         LW_STATUS_LEVEL_WARN
 * @see         LW_STATUS_LEVEL_ERR
 * @brief       Character representing status code level
 */
#define LW_STATUS_LEVEL_CHAR(_S)                           \
    ((_S) == LW_OK?                                '0':    \
    ((_S) != LW_ERR_GENERIC && (_S) & 0x00010000?  'W':    \
                                                   'E'))

// Function definitions
const char *lwstatusToString(LW_STATUS lwStatusIn);

#ifdef __cplusplus
}
#endif

#endif // XAPIGEN

#endif /* SDK_LWSTATUS_H */
