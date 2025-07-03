/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2018 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "pmsmcengine.h"

//----------------------------------------------------------------------
//! \brief constructor
//!
PmSmcEngine::PmSmcEngine
(
    PolicyManager * pPolicyManager,
    PmTest * pTest
):
    m_pPolicyManager(pPolicyManager),
    m_pTest(pTest)
{
    MASSERT(pPolicyManager);
}

