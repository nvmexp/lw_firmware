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
 * @file        scp_sanity.c
 * @brief       Global sanity checks for the SCP driver.
 */

#if !LWRISCV_FEATURE_SCP
#error "Attempting to build scp_sanity.c but the SCP driver is disabled!"
#endif // LWRISCV_FEATURE_SCP


// SCP includes.
#include <liblwriscv/scp/scp_common.h>
#include <liblwriscv/scp/scp_crypt.h>
#include <liblwriscv/scp/scp_direct.h>
#include <liblwriscv/scp/scp_private.h>
#include <liblwriscv/scp/scp_rand.h>


//
// Keeping these constants distinct helps insulate external clients against
// future hardware changes, but also complicates the internal driver logic
// somewhat. To get around this, we simply assume them to be equivalent for
// now and defer any complicated logic updates until that assumption no longer
// holds.
//
_Static_assert((SCP_BLOCK_SIZE       == SCP_REGISTER_SIZE) &&
               (SCP_BUFFER_ALIGNMENT == SCP_REGISTER_SIZE) &&
               (SCP_KEY_SIZE         == SCP_REGISTER_SIZE) &&
               (SCP_RAND_SIZE        == SCP_REGISTER_SIZE) ,
    "One or more of the core size constants used by the SCP driver no longer "
    "match the SCP register size. Substantial updates may need to be made to "
    "the internal driver logic to accommodate."
);

//
// The enumeration constants for the SCP GPRs have both short- and long-form
// names as a compromise between the stylistic consistency perferred by some
// clients and the brevity preferred by others. This check loosely ensures that
// the two formats remain in sync with each other.
//
_Static_assert(SCP_RCOUNT == SCP_REGISTER_INDEX_COUNT,
    "Mismatch detected between the short and long forms of the "
    "SCP_REGISTER_INDEX enumeration constants."
);

//
// The specific value of this constant is mandated by the microcode security
// bar and is also assumed by the SCP driver in order to simplify certain input
// -validation checks. It should therefore only be changed with care and prior
// approval.
//
_Static_assert(SCP_SECRET_INDEX_CLEAR == 0,
    "The secret index selected by the SCP driver for clearing registers in "
    "secure mode does not match the value mandated by the microcode security "
    "bar. Please ensure that you have received the proper prior approvals "
    "before making this change, and remember to update all affected input-"
    "validation routines and API documentation accordingly."
);
