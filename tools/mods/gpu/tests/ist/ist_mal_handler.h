/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once


#include "interfaces/MATHS_CORE_ifc.h"  // The interface which we must implement
#include <memory>
#include <map>
#include <string>
#include <iostream>


namespace MATHS_store_ns
{
    class MATHS_STORE_ifc;
}
namespace MATHS_memory_manager_ns
{
    class mem_manager_ifc;
}
namespace MATHS_execblock_ns
{
    class MATHS_ExecBlock_ifc;
}
namespace MATHS_procblock_ns
{
    class MATHS_ProcBlock_ifc;
}
namespace MATHS_LOG_ns
{
    class MATHS_LOG_ifc;
}
namespace MATHS_DUT_Manager_ns
{
    class DUT_Manager_ifc;
}
namespace MATHS_ioblock_ns
{
    class MATHS_IOBlock_ifc;
}
namespace MATHS_rpc_ns
{
    class MATHS_RPC_ifc;
}

using namespace MATHS_store_ns;
using namespace MATHS_memory_manager_ns;
using namespace MATHS_execblock_ns;
using namespace MATHS_procblock_ns;
using namespace MATHS_LOG_ns;
using namespace MATHS_DUT_Manager_ns;
using namespace MATHS_ioblock_ns;

namespace MATHS_CORE_ns
{
    class MalHandler : public MATHS_CORE_ifc
    {
    public:
        MalHandler();

        void set_MemManager(std::shared_ptr<mem_manager_ifc> pMemMgr) { m_pMemManager = pMemMgr; }

        MATHS_CORE_ifc& get_MemManager(std::shared_ptr<mem_manager_ifc>& pMemMgr) override
        {
            pMemMgr = m_pMemManager;
            return *this;
        }

        /// Returns shared pointer to the interface for the STORE Class
        MATHS_CORE_ifc& get_STORE(MATHS_STORE_ifc::sptr& pMATHS_Store) override
        {
            pMATHS_Store = m_pStore;
            return *this;
        }

        /// Returns shared pointer to the interface for the IO Block
        MATHS_CORE_ifc& get_IOBlock(std::shared_ptr<MATHS_IOBlock_ifc>& MATHS_Io) override
        {
            MATHS_Io =  m_pIOBlock;
            return *this;
        }

        /// Returns shared pointer to the interface for the Exec Block
        MATHS_CORE_ifc& get_ExecBlock(std::shared_ptr<MATHS_ExecBlock_ifc>& MATHS_Exe) override
        {
            MATHS_Exe = m_pExecBlock;
            return *this;
        }

        /// Returns shared pointer to the interface for the Processing Block
        MATHS_CORE_ifc& get_ProcessingBlock(std::shared_ptr<MATHS_ProcBlock_ifc>& MATHS_Proc) override 
        { 
            MATHS_Proc = m_pProcBlock;
            return *this;
        }

        /// Returns shared pointer to the interface for the RPC Block
        /*
         *  As of nnow a dummy implementation since the base class is pure abstract
         *  and MODS doesn't use it.
         */
        MATHS_CORE_ifc& get_RPCBlock(std::shared_ptr<MATHS_RPC_ifc>& MATHS_Server) override
        {
            throw "IMPLEMENTME!";
            return *this;
        }

        // new (direct return) Style
        /// Returns shared pointer to the interface for the Memory Manager
        std::shared_ptr<mem_manager_ifc> get_MemManager() override { return m_pMemManager; }

        /// Returns shared pointer to the interface for the STORE Class
        MATHS_STORE_ifc::sptr get_STORE() override { return m_pStore; }

        /// Returns shared pointer to the interface for the IO Block
        std::shared_ptr<MATHS_IOBlock_ifc> get_IOBlock() override {return m_pIOBlock; }

        /// Returns shared pointer to the interface for the Exec Block
        std::shared_ptr<MATHS_ExecBlock_ifc> get_ExecBlock() override 
        { 
            return m_pExecBlock;
        }

        /// Returns shared pointer to the interface for the Processing Block
        std::shared_ptr<MATHS_ProcBlock_ifc> get_ProcessingBlock() override 
        {
            return m_pProcBlock;
        }

        /// Returns shared pointer to the interface for the RPC Block
        /*
         *  As of nnow a dummy implementation since the base class is pure abstract
         *  and MODS doesn't use it.
         */
        std::shared_ptr<MATHS_RPC_ifc> get_RPCBlock() override
        {
            throw "IMPLEMENTME!";
            return m_pRPC;
        }

        /// Returns shared pointer to the interface for the LOG
        std::shared_ptr<MATHS_LOG_ifc>& get_LOG() override
        {
            return m_pLogger;
        }

        /// Returns shared pointer to the interface for the DUT Manager
        DUT_Manager_ifc::sptr get_DUTManager() override
        {
            return m_pDutManager;
        }

        /// Fill map<string, string> with additional informations from derived class (generic interface)
        void get_Info(std::map<std::string, std::string>& input) override
        {
            for (const auto& mit : m_config)
            {
                if (get_Info(mit.first) != "")
                {
                    input[mit.first] = mit.second;
                }
            }
        }

        /// Returns single requested information
        std::string get_Info(const std::string& input) override
        { 
            auto mit = m_config.find(input);
            return (mit != m_config.end()) ? mit->second : "";
        }

        void set_Info(const std::string& key, const std::string& value) override
        {
            m_config[key] = value;
        }

        void setMultiDUTMode(const bool& en) override
        { 
            _multiDUT = en; 
        }

        bool getMultiDUTMode() override
        { 
            return _multiDUT; 
        }

        bool isLWDomain() override 
        {
            return true;
        }

        bool isValidHw() override 
        {
            return true;
        }

        ~MalHandler();

    private:
        bool _multiDUT = false;
        std::map<std::string, std::string> m_config =
        {
            {"RunMode", "default"},
            {"proc-max-thread", "1"},
            {"lvx-seq-clone-num", "4095"},
            {"verbose", "true"},
            {"seqTimeReport", "false"},
            {"proc-time-out-start-seq", "1000"},   // ms, overriden by MODS args
            {"proc-time-out", "20000"},             // ms, overriden by MODS args
            {"proc-method", "batch"},
            {"bypass_rpc_constraints", "0"},
            {"exec-result-size-limit", "20000"},
            {"proc-polling-period", "1000"}         // us
        };



        std::shared_ptr<mem_manager_ifc> m_pMemManager;
        std::shared_ptr<MATHS_STORE_ifc> m_pStore;
        std::shared_ptr<MATHS_ExecBlock_ifc> m_pExecBlock;
        std::shared_ptr<MATHS_ProcBlock_ifc> m_pProcBlock;
        std::shared_ptr<MATHS_LOG_ifc> m_pLogger;
        std::shared_ptr<DUT_Manager_ifc> m_pDutManager;
        std::shared_ptr<MATHS_IOBlock_ifc> m_pIOBlock;
        std::shared_ptr<MATHS_RPC_ifc> m_pRPC;
    };
} // namespace MATHS_CORE_ns
