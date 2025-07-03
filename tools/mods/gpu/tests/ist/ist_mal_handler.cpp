/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ist_mal_handler.h"
#include <STORE/MATHS_Store_Impl.h>
#include <DUT_Manager/DUT_Manager_impl.h>
#include <IOBlock/ioblock.h>
#include <ExecBlock/ExecTP.h>
#include <ProcBlock/MATHS_ProcBlock_Impl.h>
#include <Logger/MODS_LOG_impl.h>

namespace MATHS_CORE_ns
{

    MalHandler::MalHandler()
        : m_pStore(std::make_shared<MATHS_store_ns::MATHS_STORE_impl>(this)),
        m_pExecBlock(std::make_shared<MATHS_execblock_ns::ExecTP>(this)),
        m_pProcBlock(std::make_shared<MATHS_procblock_ns::MATHS_ProcBlock_impl>(this, 1)),
        m_pLogger(std::make_shared<MATHS_LOG_ns::MODS_LOG>()),
        m_pDutManager(std::make_shared<MATHS_DUT_Manager_ns::DUT_Manager_impl>()),
        m_pIOBlock(std::make_shared<MATHS_ioblock_ns::MATHS_IOBlock_impl>(this))
    {
    }

    MalHandler::~MalHandler()
    {
    }

} // namespace MATHS_CORE_ns
