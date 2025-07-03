/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _CIFACE_H_
#define _CIFACE_H_

class CIfaceObject
{
private:
    int m_ref;

public:
    CIfaceObject(void) : m_ref(0) {};
    virtual ~CIfaceObject() {};

public:
    virtual void AddRef() { m_ref++; };
    virtual void Release()
    {
        if(m_ref > 0)
        {
            --m_ref;
            if(!m_ref)
                delete this;
        }
    };
};

#define ADD_BASE_INTERFACE_NULLQUERY                                \
public:                                                             \
    virtual void AddRef(void) { CIfaceObject::AddRef(); };          \
    virtual void Release(void) { CIfaceObject::Release(); };        \
    virtual IIfaceObject* QueryIface(IID_TYPE id) { return 0;};

#define ADD_QUERY_INTERFACE_METHOD(IID, IFACENAME)                  \
        case IID:                                                   \
            ret = static_cast<IFACENAME*>(this);                    \
            break;

#define BEGIN_BASE_INTERFACE                                        \
public:                                                             \
    virtual void AddRef(void) { CIfaceObject::AddRef(); };          \
    virtual void Release(void) { CIfaceObject::Release(); };        \
    virtual IIfaceObject* QueryIface(IID_TYPE id)                   \
    {                                                               \
        IIfaceObject *ret = 0;                                      \
        switch(id)                                                  \
        {                                                           \
        default:                                                    \
            ret = 0;                                                \
            break;

#define END_BASE_INTERFACE                                          \
        }                                                           \
        if(ret)                                                     \
            ret->AddRef();                                          \
        return ret;                                                 \
    }

#endif // _CIFACE_H_
