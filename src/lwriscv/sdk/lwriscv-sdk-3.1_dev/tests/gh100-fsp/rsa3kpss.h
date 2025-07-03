/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef RSA3KPSS_H
#define RSA3KPSS_H

/* Comment this if you don't want to test LWPKA mutex */
#define TEST_MUTEX

int rsa3kPssVerify(void);

#endif  // RSA3KPSS_H
