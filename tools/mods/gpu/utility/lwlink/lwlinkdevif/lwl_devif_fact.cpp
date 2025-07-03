/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2018,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwl_devif_fact.h"
#include "core/include/rc.h"
#include "core/include/tee.h"
#include <map>

namespace LwLinkDevIf
{
    namespace Factory
    {
        //! Structure describing a registered factory function for creating
        //! a LwLink device interface
        struct DevIfFactoryFuncData
        {
            string           name;     //!< Name of the device interface
            DevIfFactoryFunc factFunc; //!< Pointer to function that creates the device interface
        };

        //! Structure describing a registered factory function for creating
        //! a LwLink library interface
        struct LwLinkLibIfFactoryFuncData
        {
            string                 name;               //!< Name of the lwlink library interface
            LwLinkLibIfFactoryFunc factFunc = nullptr; //!< Pointer to function that creates the
                                                       //!< lwlink library interface
        };
        
        //! Structure describing a registered factory function for creating
        //! an FM library interface
        struct FMLibIfFactoryFuncData
        {
            string             name;               //!< Name of the fabricmanager library interface
            FMLibIfFactoryFunc factFunc = nullptr; //!< Pointer to function that creates the
                                                   //!< fabricmanager library interface
        };

        //! Factory data.  Note that these data members are used by the Schwartz
        //! counter so it is important that their initializations here are to
        //! builtin types rather than classes.  If there initializations are to
        //! classes then the Schwartz counter will not work since class based
        //! initializations are performed as the last step of static
        //! construction and the data members will actually be re-initialized
        //! by static construction after they are filled in by the Schwartz
        //! counter class.
        UINT32 m_FactInitCounter = 0;
        map<UINT32, DevIfFactoryFuncData> * m_DevIfFactoryFunctions = 0;
        LwLinkLibIfFactoryFuncData * m_pLwLinkLibIfFactoryFuncData  = 0;
        FMLibIfFactoryFuncData * m_pFMLibIfFactoryFuncData  = 0;

        //----------------------------------------------------------------------
        //! \brief Initialize factory data, used by Schwartz counter
        //!
        void Initialize()
        {
            m_DevIfFactoryFunctions = new map<UINT32, DevIfFactoryFuncData>;
            m_pLwLinkLibIfFactoryFuncData = new LwLinkLibIfFactoryFuncData;
            m_pFMLibIfFactoryFuncData = new FMLibIfFactoryFuncData;
        }

        //----------------------------------------------------------------------
        //! \brief Shutdown factory data, used by Schwartz counter
        //!
        void Shutdown()
        {
            m_DevIfFactoryFunctions->clear();
            delete m_DevIfFactoryFunctions;
            m_DevIfFactoryFunctions = nullptr;

            delete m_pLwLinkLibIfFactoryFuncData;
            m_pLwLinkLibIfFactoryFuncData = nullptr;
            
            delete m_pFMLibIfFactoryFuncData;
            m_pFMLibIfFactoryFuncData = nullptr;
        }
    };
};

//------------------------------------------------------------------------------
//! \brief Factory initializer Schwartz counter, initialize factory data the
//! first time one of the classes is constructed
//!
LwLinkDevIf::Factory::Initializer::Initializer()
{
    if (0 == m_FactInitCounter++)
    {
        LwLinkDevIf::Factory::Initialize();
    }
}

//------------------------------------------------------------------------------
//! \brief Free factory data the last time one of the classes is destructed
//!
LwLinkDevIf::Factory::Initializer::~Initializer()
{
    if (0 == --m_FactInitCounter)
    {
        LwLinkDevIf::Factory::Shutdown();
    }
}

//------------------------------------------------------------------------------
//! \brief Constructor that registers a device interface creation function with
//! the factory
//!
//! \param order : Order in which the device interface should be created
//! \param name  : Name of the device interface
//! \param f     : Factory function to call
//!
LwLinkDevIf::Factory::DevIfFactoryFuncRegister::DevIfFactoryFuncRegister
(
    UINT32 order,
    string name,
    DevIfFactoryFunc f
)
{
    if (m_DevIfFactoryFunctions->find(order) != m_DevIfFactoryFunctions->end())
    {
        auto pEntry = m_DevIfFactoryFunctions->find(order);
        Printf(Tee::PriHigh,
               "Failed to register %s, %s already exists with order %u!!\n",
               name.c_str(),
               pEntry->second.name.c_str(),
               order);
        return;
    }
    DevIfFactoryFuncData factData = { name, f };
    m_DevIfFactoryFunctions->insert(make_pair(order, factData));

    // Note : cannot use modules here since this is called during static
    // construction and the module may not have been created yet
    Printf(Tee::PriLow,
           "Registered LwLink device interface %s with order %u!!\n",
           name.c_str(),
           order);
}

//------------------------------------------------------------------------------
//! \brief Create all device interfaces
//!
//! \param pInterfaces : Pointer to returned vector of interface pointers
//!
void LwLinkDevIf::Factory::CreateAllDevInterfaces(vector<InterfacePtr> *pInterfaces)
{
    for (auto const & factEntry : *m_DevIfFactoryFunctions)
    {
        InterfacePtr pIf(factEntry.second.factFunc());
        pInterfaces->push_back(pIf);

        Printf(Tee::PriLow, GetTeeModule()->GetCode(),
               "%s : Created LwLink device interface %s\n",
               __FUNCTION__, factEntry.second.name.c_str());
    }
}

//------------------------------------------------------------------------------
//! \brief Constructor that registers a lwlink library interface creation
//! function with the factory
//!
//! \param name  : Name of the lwlink library interface
//! \param f     : Factory function to call
//!
LwLinkDevIf::Factory::LwLinkLibIfFactoryFuncRegister::LwLinkLibIfFactoryFuncRegister
(
    string                 name,
    LwLinkLibIfFactoryFunc f
)
{
    if (m_pLwLinkLibIfFactoryFuncData->factFunc)
    {
        Printf(Tee::PriError,
               "Failed to register %s, %s already exists!!\n",
               name.c_str(),
               m_pLwLinkLibIfFactoryFuncData->name.c_str());
        return;
    }
    m_pLwLinkLibIfFactoryFuncData->name     = name;
    m_pLwLinkLibIfFactoryFuncData->factFunc = f;

    // Note : cannot use modules here since this is called during static
    // construction and the module may not have been created yet
    Printf(Tee::PriLow,
           "Registered LwLink library interface %s!!\n",
           name.c_str());
}

//------------------------------------------------------------------------------
//! \brief Create the lwlink library interface
//!
//! \param pInterfaces : Pointer to returned vector of interface pointers
//!
LwLinkDevIf::LwLinkLibIfPtr LwLinkDevIf::Factory::CreateLwLinkLibInterface()
{
    if (!m_pLwLinkLibIfFactoryFuncData->factFunc)
        return LwLinkLibIfPtr();

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Creating LwLink library interface %s\n",
           __FUNCTION__, m_pLwLinkLibIfFactoryFuncData->name.c_str());
    return LwLinkLibIfPtr(m_pLwLinkLibIfFactoryFuncData->factFunc());
}

//------------------------------------------------------------------------------
//! \brief Constructor that registers a fabricmanager library interface creation
//! function with the factory
//!
//! \param name  : Name of the fm library interface
//! \param f     : Factory function to call
//!
LwLinkDevIf::Factory::FMLibIfFactoryFuncRegister::FMLibIfFactoryFuncRegister
(
    string          name,
    FMLibIfFactoryFunc f
)
{
    if (m_pFMLibIfFactoryFuncData->factFunc)
    {
        Printf(Tee::PriError,
               "Failed to register %s, %s already exists!!\n",
               name.c_str(),
               m_pFMLibIfFactoryFuncData->name.c_str());
        return;
    }
    m_pFMLibIfFactoryFuncData->name     = name;
    m_pFMLibIfFactoryFuncData->factFunc = f;

    // Note : cannot use modules here since this is called during static
    // construction and the module may not have been created yet
    Printf(Tee::PriLow,
           "Registered FM library interface %s!!\n",
           name.c_str());
}

//------------------------------------------------------------------------------
//! \brief Create the fabricmanager library interface
//!
//! \param pInterfaces : Pointer to returned vector of interface pointers
//!
LwLinkDevIf::FMLibIfPtr LwLinkDevIf::Factory::CreateFMLibInterface()
{
    if (!m_pFMLibIfFactoryFuncData->factFunc)
        return FMLibIfPtr();

    Printf(Tee::PriLow, GetTeeModule()->GetCode(),
           "%s : Creating FM library interface %s\n",
           __FUNCTION__, m_pFMLibIfFactoryFuncData->name.c_str());
    return FMLibIfPtr(m_pFMLibIfFactoryFuncData->factFunc());
}
