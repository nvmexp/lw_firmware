/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
 
#ifndef CONDITIONALCHECK_H
#define CONDITIONALCHECK_H



// Unsigned integer wrap checking macro for addition
#define CHECK_UNSIGNED_INTEGER_ADDITION_AND_GOTO_CLEANUP_IF_OUT_OF_BOUNDS(arg1, arg2, maxVal)           \
    do                                                                                                  \
    {                                                                                                   \
        if(((maxVal) - (arg1)) < (arg2))                                                                \
        {                                                                                               \
            goto Cleanup;                                                                               \
        }                                                                                               \
    } while (0)




#endif //CONDITIONALCHECK_H