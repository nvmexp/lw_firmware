/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef TRACE_3D_RESOURCE_CONTAINER
#define TRACE_3D_RESOURCE_CONTAINER

#include <string>
#include <map>
#include "mdiag/utils/types.h"
#include "gpu/include/gpudev.h"
#include "core/include/lwrm.h"
#include "lwos.h"
#include "class/cl90f1.h"
#include "class/cle3f1.h"
#include "core/include/tee.h"
#include "core/include/rc.h"
#include <memory>

template <typename T>
class Trace3DResourceContainer
{
public:
    Trace3DResourceContainer() {};
    Trace3DResourceContainer(LWGpuResource * pGpuResource,
                     LwRm* pLwRm) { m_pGpuResource = pGpuResource; m_pLwRm = pLwRm; }
    std::shared_ptr<T> CreateResourceObject(UINT32 testId, const string &name);
    LWGpuResource * GetGpuResource() const { return m_pGpuResource; }
    std::shared_ptr<T> GetResourceObject(UINT32 testId, const string &name);
    std::shared_ptr<T> CreateResourceObjectAlias(UINT32 refTestId, const string &refName,
                                  UINT32 testId, const string &name);
    typedef typename vector<std::pair<UINT32, typename std::shared_ptr<T> > >::iterator iterator;
    iterator begin() { return m_ResourceObjectVectors.begin(); }
    iterator end() { return m_ResourceObjectVectors.end(); }
    void FreeObjectInTest(UINT32 testId);
    bool IsEmpty() const { return (m_ResourceObjects.empty() && m_ResourceObjectVectors.empty()); }
private:
    typedef std::pair<UINT32, string> ResourceKey;
    map<ResourceKey, typename std::shared_ptr<T> > m_ResourceObjects;
    vector<std::pair<UINT32, typename std::shared_ptr<T> > > m_ResourceObjectVectors;
    LWGpuResource * m_pGpuResource;
    LwRm* m_pLwRm;
};

// Cteate resource object and return its pointer
template <typename T>
std::shared_ptr<T> Trace3DResourceContainer<T>::CreateResourceObject(UINT32 testId, const string &name)
{
    const ResourceKey &key = std::make_pair(testId, name);
    if (m_ResourceObjects.find(key) == m_ResourceObjects.end())
    {
        std::shared_ptr<T> item = std::shared_ptr<T>(new T(GetGpuResource(), m_pLwRm, testId, name));
        m_ResourceObjects[key] = item;
        m_ResourceObjectVectors.push_back(std::make_pair(testId, item));
    }
    else
    {
        Printf(Tee::PriDebug, "object %s in testId %d has been created before.\n",
               name.c_str(), testId);
    }

    return m_ResourceObjects[key];
}

//--------------------------------------------------------------------
//! \Get the Handle according to the name and testId
//! \Input: testId: trace3d testId
//! \Input: name: vaspace name or subctx name
//! \Output:LwRm::Handle: rm handle
//!
template <typename T>
std::shared_ptr<T> Trace3DResourceContainer<T>::GetResourceObject(UINT32 testId, const string &name)
{
    const ResourceKey &key = std::make_pair(testId, name);
    if (m_ResourceObjects.find(key) != m_ResourceObjects.end())
    {
        return m_ResourceObjects[key];
    }

    Printf(Tee::PriNormal, "Warning: Get the NULL Object, testId %d, name %s.\n", testId, name.c_str());
    return std::shared_ptr<T>();
}

template <typename T>
std::shared_ptr<T> Trace3DResourceContainer<T>::CreateResourceObjectAlias
(
    UINT32 refTestId,
    const string &refName,
    UINT32 testId,
    const string &name
)
{
    const ResourceKey &key = std::make_pair(refTestId, refName);
    std::shared_ptr<T> object = GetResourceObject(testId, name);
    if (!object)
    {
        ErrPrintf("object %s with testId %d doesn't exist\n", name.c_str(), testId);
        MASSERT(0);
    }

    m_ResourceObjects[key] = object;
    return object;
}

template <typename T>
void Trace3DResourceContainer<T>::FreeObjectInTest(UINT32 testId)
{
    // 1. reset the m_ResourceObjects
    typename map<ResourceKey, typename std::shared_ptr<T> >::iterator it;
    for (it = m_ResourceObjects.begin(); it != m_ResourceObjects.end(); )
    {
        if(it->first.first == testId)
        {
            m_ResourceObjects.erase(it++);
        }
        else
            it++;
    }

    // 2. reset the m_ResourceObjectVectors
    for(iterator it = begin(); it != end();)
    {
        if(it->first == testId)
        {
            it = m_ResourceObjectVectors.erase(it);
        }
        else
            it++;
    }

}
#endif

