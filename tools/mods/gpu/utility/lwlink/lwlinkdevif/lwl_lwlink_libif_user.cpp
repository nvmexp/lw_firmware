/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_lwlink_libif_user.h"
#include "lwl_devif.h"
#include "lwl_devif_fact.h"
#include "lwlink.h"
#include "lwlink_export.h"
#include "lwlink_lib_ctrl.h"

namespace
{
    //--------------------------------------------------------------------------
    //! \brief Create a lwlink user library interface
    //!
    //! \return Pointer to new lwlink user library interface
    LwLinkDevIf::LwLinkLibIfPtr CreateLwLinkLibIfUser()
    {
        return make_shared<LwLinkDevIf::LwLinkLibIfUser>();
    }

    //! Register the lwlink user library with the factory
    LwLinkDevIf::Factory::LwLinkLibIfFactoryFuncRegister
        m_LwLinkLibIfUserFact("LwLinkLibIfUser", CreateLwLinkLibIfUser);
};

//------------------------------------------------------------------------------
//! \brief Call the LwLink core library control interface
//!
//! \param controlId : Control call to make
//! \param pParams   : Control parameters
//!
//! \return OK if successful, not OK otherwise
/* virtual */ RC LwLinkDevIf::LwLinkLibIfUser::Control
(
    UINT32 controlId,
    void * pParams,
    UINT32 paramSize
)
{
    struct lwlink_ioctrl_params ioctlParams = { };

    ioctlParams.cmd  = controlId;
    ioctlParams.buf  = pParams;
    ioctlParams.size = paramSize;

    const RC rc = LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(lwlink_lib_ioctl_ctrl(&ioctlParams)); //$
    Printf(Tee::PriDebug, GetTeeModule()->GetCode(),
            "%s : LwLink core library control %u returns %d!!\n",
           __FUNCTION__, controlId, rc.Get());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Initialize the library interface
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwLinkLibIfUser::Initialize()
{
    if (m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "%s : LwLink core library already initialized\n",
               __FUNCTION__);
        return OK;
    }

    RC rc;
    CHECK_RC(LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(lwlink_lib_initialize()));

    lwlink_set_node_id nodeIdParams = { };
    CHECK_RC(Control(CTRL_LWLINK_SET_NODE_ID, &nodeIdParams, sizeof(nodeIdParams)));

    m_bInitialized = true;
    return OK;
}

//------------------------------------------------------------------------------
//! \brief Shutdown the library interface
//!
//! \return OK if successful, not OK otherwise
//!
/* virtual */ RC LwLinkDevIf::LwLinkLibIfUser::Shutdown()
{
    if (!m_bInitialized)
    {
        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
                "%s : LwLink core library not initialized, skipping shutdown\n",
               __FUNCTION__);
        return OK;
    }

    RC rc;
    CHECK_RC(LwLinkDevIf::LibInterface::LwLinkLibRetvalToModsRc(lwlink_lib_unload()));

    m_bInitialized = false;
    return rc;
}

