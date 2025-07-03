
/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2008, 2018 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Defines the PmTest decoupling all the concrete test MODS types

#include "pmtest.h"

//--------------------------------------------------------------------
//! \brief Constructor
//!
PmTest::PmTest
(
    PolicyManager *pPolicyManager,
    LwRm* pLwRm
) :
    m_pPolicyManager(pPolicyManager),
    m_pLwRm(pLwRm)
{
    MASSERT(m_pPolicyManager);
    MASSERT(m_pLwRm);
}

//--------------------------------------------------------------------
//! \brief Destructor
//!
PmTest::~PmTest()
{
}
