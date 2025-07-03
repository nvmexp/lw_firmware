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

#pragma once

#ifndef INCLUDED_LWL_DEVIF_FACT_H
#define INCLUDED_LWL_DEVIF_FACT_H

#include "core/include/types.h"
#include "lwl_devif.h"
#include "lwl_libif.h"
#include "lwl_fabricmanager_libif.h"
#include <vector>
#include <string>

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Factory namespace for creating various types of LwLink device
    //! interfaces.  Interfaces register themselves during static construction
    //! time
    namespace Factory
    {
        typedef Interface * (*DevIfFactoryFunc)(void);
        typedef LwLinkLibIfPtr (*LwLinkLibIfFactoryFunc)(void);
        typedef FMLibIfPtr (*FMLibIfFactoryFunc)(void);

        //----------------------------------------------------------------------
        //! \brief Class that is used to register a device interface creation
        //! function with the factory.  Each type of device interface should
        //! define an instance of this class in order to register with the
        //! factory and have an instance created
        class DevIfFactoryFuncRegister
        {
            public:
                DevIfFactoryFuncRegister(UINT32 order, string name, DevIfFactoryFunc f);
        };
        void CreateAllDevInterfaces(vector<InterfacePtr> *pInterfaces);

        //----------------------------------------------------------------------
        //! \brief Class that is used to register an interface creation function
        //! with the factory.  Each type of interface should define an instance
        //! of this class in order to register with the factory and have an
        //! instance created
        class LwLinkLibIfFactoryFuncRegister
        {
            public:
                LwLinkLibIfFactoryFuncRegister(string name, LwLinkLibIfFactoryFunc f);
        };
        LwLinkLibIfPtr CreateLwLinkLibInterface();
    
        //----------------------------------------------------------------------
        //! \brief Class that is used to register an interface creation function
        //! with the factory.  Each type of interface should define an instance
        //! of this class in order to register with the factory and have an
        //! instance created
        class FMLibIfFactoryFuncRegister
        {
            public:
                FMLibIfFactoryFuncRegister(string name, FMLibIfFactoryFunc f);
        };
        FMLibIfPtr CreateFMLibInterface();

        //----------------------------------------------------------------------
        //! \brief Schwartz counter class to ensure factory data is initialized
        //! during static cconstruction time prior to any factory functions
        //! being registered
        class Initializer
        {
            public:
                Initializer();
                ~Initializer();
        };
    };
};

//! Schwartz counter instance needs to be defined in header so that all users of
//! the factory ensure factory data is created first
static LwLinkDevIf::Factory::Initializer s_LwLinkDevIfFactoryInit;
#endif
