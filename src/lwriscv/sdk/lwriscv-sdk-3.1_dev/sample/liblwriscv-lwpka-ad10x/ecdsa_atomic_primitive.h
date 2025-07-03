/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ECDSA_ATOMIC_PRIMITIVE_H
#define ECDSA_ATOMIC_PRIMITIVE_H

/* Comment this if you don't want to test key insertion (Eg: attestation usecase when key is inserted by BR) */
#define TEST_KEY_INSERTION

void verifyEcdsaSigningAtomicPrimitive(void);

#endif  // ECDSA_ATOMIC_PRIMITIVE_H
