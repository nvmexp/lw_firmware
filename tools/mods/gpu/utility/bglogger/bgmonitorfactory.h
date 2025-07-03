/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2015,2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   bgmonitorfactory.h
 *
 * @brief  Background logging utility.
 *
 */

#pragma once

#ifndef INCLUDED_BGMONITORFACTORY_H
#define INCLUDED_BGMONITORFACTORY_H

#include "gpu/utility/bglogger.h"
#include "bgmonitor.h"
#include <map>

//-----------------------------------------------------------------------------
class BgMonitorFactoryBase
{
public:
    BgMonitorFactoryBase(BgMonitorType type)
        : m_Type(type)
    {}

    virtual ~BgMonitorFactoryBase()
    {}

    virtual unique_ptr<BgMonitor> CreateInstance() const = 0;

protected:
    BgMonitorType m_Type;
};

//-----------------------------------------------------------------------------
template <class BgMonitorDerived>
class BgMonitorFactory : public BgMonitorFactoryBase
{
    BgMonitorFactory(const BgMonitorFactory&) = delete;
    BgMonitorFactory& operator=(const BgMonitorFactory&) = delete;

public:
    using BgMonitorFactoryBase::BgMonitorFactoryBase;

    virtual unique_ptr<BgMonitor> CreateInstance() const
    {
        return make_unique<BgMonitorDerived>(m_Type);
    }
};

//-----------------------------------------------------------------------------
class BgMonitorFactories
{
    typedef map<BgMonitorType, const BgMonitorFactoryBase *> Cont;
public:
    unique_ptr<BgMonitor> CreateBgMonitor(BgMonitorType type) const
    {
        unique_ptr<BgMonitor> pMonitor;
        const auto it = bgMonitorFactories.find(type);
        if (bgMonitorFactories.end() != it)
        {
            pMonitor = it->second->CreateInstance();
        }
        return pMonitor;
    }

    void AddFactory(const BgMonitorFactoryBase *factory, BgMonitorType type)
    {
        bgMonitorFactories[type] = factory;
    }

    static BgMonitorFactories& GetInstance()
    {
        static BgMonitorFactories instance;
        return instance;
    }

private:
    BgMonitorFactories() = default;
    Cont bgMonitorFactories;
};

//-----------------------------------------------------------------------------
template <class BgMonitorDerived>
class BgMonFactoryRegistrator
{
    BgMonFactoryRegistrator(const BgMonFactoryRegistrator &) = delete;
    BgMonFactoryRegistrator& operator=(const BgMonFactoryRegistrator &) = delete;

public:
    BgMonFactoryRegistrator(
        BgMonitorFactories &factories
      , BgMonitorType type
    )
    {
        static BgMonitorFactory<BgMonitorDerived> factory(type);
        factories.AddFactory(&factory, type);
    }
};

#endif
