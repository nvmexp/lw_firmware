/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_lwlink_libif_kernel.h"
#include "lwl_devif.h"
#include "lwl_devif_fact.h"
#include "lwlink.h"
#include "lwlink_export.h"
#include "lwlink_lib_ctrl.h"

extern "C"
{
    #include "lwlink_user_api.h"
}

namespace
{
    //--------------------------------------------------------------------------
    //! \brief Create a lwlink user library interface
    //!
    //! \return Pointer to new lwlink user library interface
    LwLinkDevIf::LwLinkLibIfPtr CreateLwLinkLibIfKernel()
    {
        return make_shared<LwLinkDevIf::LwLinkLibIfKernel>();
    }

    //! Register the lwlink user library with the factory
    LwLinkDevIf::Factory::LwLinkLibIfFactoryFuncRegister
        m_LwLinkLibIfKernelFact("LwLinkLibIfKernel", CreateLwLinkLibIfKernel);
};

//------------------------------------------------------------------------------
//! \brief Call the LwLink core library control interface
//!
//! \param controlId : Control call to make
//! \param pParams   : Control parameters
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwLinkDevIf::LwLinkLibIfKernel::Control
(
    UINT32 controlId,
    void * pParams,
    UINT32 paramSize
)
{
    MASSERT(m_pSession);
    RC rc;
    LW_STATUS status;
    
    status = lwlink_api_control(m_pSession, controlId, pParams, paramSize);
    rc = LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status);
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : LwLink core library control %u returns %d!!\n",
               __FUNCTION__, controlId, rc.Get());
        return rc;
    }
    
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize the library interface
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwLinkLibIfKernel::Initialize()
{
    if (m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "%s : LwLink core library already initialized\n",
               __FUNCTION__);
        return RC::OK;
    }

    RC rc;
    LW_STATUS status;
    
    status = lwlink_api_init();
    rc = LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status);
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : LwLink core library user api init returns %d!!\n",
               __FUNCTION__, rc.Get());
        return rc;
    }
    
    status = lwlink_api_create_session(&m_pSession);
    rc = LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(status);
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, GetTeeModule()->GetCode(),
               "%s : LwLink core library user api create session returns %d!!\n",
               __FUNCTION__, rc.Get());
        return rc;
    }

    lwlink_set_node_id nodeIdParams = { };
    CHECK_RC(Control(CTRL_LWLINK_SET_NODE_ID, &nodeIdParams, sizeof(nodeIdParams)));

    m_bInitialized = true;
    return RC::OK;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the library interface
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwLinkLibIfKernel::Shutdown()
{
    if (!m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "%s : LwLink core library not initialized, skipping shutdown\n",
               __FUNCTION__);
        return RC::OK;
    }

    lwlink_api_free_session(&m_pSession);
    
    m_bInitialized = false;
    return RC::OK;
}

