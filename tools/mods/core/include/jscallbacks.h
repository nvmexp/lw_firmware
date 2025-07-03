/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/type_traits/function_traits.hpp>

#include "jscntxt.h"

#include "core/include/massert.h"
#include "core/include/types.h"

// helps make hook implementation methods Hook, Init and ShutDown private, also
// allows to omit Init and ShutDown
class JSHookAccess
{
    template <typename Impl, typename JSSetHookFunType, JSSetHookFunType *JSSetHookFunPtr>
    friend class JSHookBase;

    JSHookAccess() = delete;

    template <typename JSHookImpl, typename... Args>
    static
    typename enable_if<JSHookImpl::HasInit>::type
    Init(JSHookImpl *jh, JSRuntime *rt, Args &&...args)
    {
        jh->Init(rt, forward<Args>(args)...);
    }

    template <typename JSHookImpl>
    static
    typename enable_if<JSHookImpl::HasShutDown>::type
    ShutDown(JSHookImpl *jh)
    {
        jh->ShutDown();
    }

    template <typename JSHookImpl, typename... Args>
    static
    typename enable_if<!JSHookImpl::HasInit>::type
    Init(JSHookImpl *jh, JSRuntime *rt, Args &&...args)
    {}

    template <typename JSHookImpl>
    static
    typename enable_if<!JSHookImpl::HasShutDown>::type
    ShutDown(JSHookImpl *jh)
    {}

    template <typename JSHookImpl, typename... Args>
    static
    typename JSHookImpl::CallbackRes Hook(JSHookImpl *jh, Args &&...args)
    {
        return jh->Hook(forward<Args>(args)...);
    }
};

// Adds a capability of shutting down all callback chains.
class AllJSHookChains
{
public:
    template <typename Derived>
    AllJSHookChains(const Derived*)
    {
        // The idea is that there will be physically different functions
        // AllJSHookChains::AllJSHookChains per each template instantiation.
        // Using a static local variable we detect the first call and memorize
        // the shutdown routine for this particular chain.
        static bool thisTypeIsinstantiated = false;
        if (thisTypeIsinstantiated) return;
        m_ShutDownRoutines.push_back(Derived::ShutDownAll);
        thisTypeIsinstantiated = true;
    }

    static
    void ShutDownAllChains()
    {
        for (auto shutDown : m_ShutDownRoutines) shutDown();
    }

private:
    static
    vector<void(*)()> m_ShutDownRoutines;
};

//! Makes a chain of callbacks shared between different hook classes with the
//! same JSSetHookFunPtr. It also serves as a container for the actual callback
//! address we call when we process a chain. The JavaScript engine will [almost]
//! always call m_Head->m_CallbackFun(..., m_Head), which will end up in one of
//! the descendant of this class. The descendant then will call the next
//! m_CallbackFun in the chain, etc. The assumption about the last argument in
//! m_Head->m_CallbackFun(..., m_Head) is not true for JS call callbacks. The
//! engine calls these callbacks twice: before the call and after the call.
//! However after the call the last argument is not the one set by
//! JS_SetCallHook, but the one returned by the first callback call. If someone
//! calls JS_SetCallHook between before and after callbacks, the last argument
//! of the after callback will be not the closure passed in JS_SetCallHook.
template <typename JSSetHookFun, JSSetHookFun *HookFunPtr>
class JSHookCallbackChain : public AllJSHookChains
{
    template <typename Impl, typename Fun, Fun *FunPtr>
    friend class JSHookBase;

    typedef void(*ShutDownDispatcherFun)(void *);

public:
    typedef typename boost::function_traits<JSSetHookFun>::arg2_type CallbackFun;
    typedef typename boost::function_traits<remove_pointer_t<CallbackFun>>::result_type CallbackRes;

    typedef JSSetHookFun JSSetHookFunType;
    static constexpr JSSetHookFunType *JSSetHookFunPtr = HookFunPtr;

    JSHookCallbackChain(CallbackFun callbackFun, ShutDownDispatcherFun shutDown)
      : AllJSHookChains(this)
      , m_CallbackFun(callbackFun)
      , m_StaticShutDownFun(shutDown)
    {}

    JSHookCallbackChain(CallbackFun callbackFun, ShutDownDispatcherFun shutDown, JSRuntime *rt)
      : AllJSHookChains(this)
      , m_CallbackFun(callbackFun)
      , m_StaticShutDownFun(shutDown)
      , m_JSRuntime(rt)
    {}

    // shuts down all callbacks in this particular chain
    static
    void ShutDownAll()
    {
        while (m_Head)
        {
            m_Head->m_StaticShutDownFun(m_Head);
        }
    }

protected:
    JSRuntime * GetJSRuntime() const { return m_JSRuntime; }
    void SetJSRuntime(JSRuntime *rt)
    {
        if (nullptr == m_JSRuntime)
        {
            m_JSRuntime = rt;
        }
    }
    JSHookCallbackChain * GetHead() { return m_Head; }

    static
    JSHookCallbackChain * GetLwr()
    {
        if (nullptr == m_LwrNode)
        {
            m_LwrNode = &m_Head;
        }
        return *m_LwrNode;
    }

    static
    bool AdvanceLwr()
    {
        if (nullptr == m_LwrNode) return false;
        if (nullptr == *m_LwrNode || nullptr == (*m_LwrNode)->m_Next)
        {
            m_LwrNode = nullptr;
            return false;
        }
        m_LwrNode = &(*m_LwrNode)->m_Next;

        return true;
    }

    template <typename... Args>
    CallbackRes CallChained(Args ...args)
    {
        return m_CallbackFun(args...);
    }

    void AddToChain()
    {
        m_Next = m_Head; // the next node was at the head
        m_Head = this;   // this node is now at the head
        SetHookToHead(m_JSRuntime);
    }

    void RemoveFromChain()
    {
        // remove this object from the linked list
        bool found = false;
        for (auto **ppNode = &m_Head; !found && (*ppNode != nullptr); ppNode = &(*ppNode)->m_Next)
        {
            // ppNode iterates over m_Next fields in the objects in the linked
            // list. If we find someone's m_Next points to us, we correct it
            // to the next node in the list.
            if (this == *ppNode)
            {
                *ppNode = (*ppNode)->m_Next;
                found = true;
            }
        }
        MASSERT(found || !"An attempt to remove a callback handler that is not the handler chain");
        SetHookToHead(m_JSRuntime);
    }

    bool IsQuiescentState() const { return m_JSRuntime == nullptr; }
    void ReturnToQuiescentState()
    {
        m_JSRuntime = nullptr;
        m_Next = nullptr;
    }

private:
    static
    void SetHookToHead(JSRuntime *rt)
    {
        if (m_Head)
        {
            HookFunPtr(m_Head->m_JSRuntime, m_Head->m_CallbackFun, m_Head);
        }
        else
        {
            HookFunPtr(rt, nullptr, nullptr);
        }
    }

    // pointer to JSHookBase::Hook, it's necessary, since one chain can be
    // shared between several JSHookBase template instantiations
    CallbackFun                 m_CallbackFun;
    // same is true for shutdown
    ShutDownDispatcherFun       m_StaticShutDownFun;
    JSRuntime                  *m_JSRuntime = nullptr;

    static JSHookCallbackChain  *m_Head;
    static JSHookCallbackChain **m_LwrNode; // A pointer to the current node during the calls
                                            // through the callback chain. Solves two issues:
                                            // (i) the aforementioned problem with the JS call
                                            // callbacks; (ii) a hook can remove itself from the
                                            // chain in the middle of the callbacks processing.
    JSHookCallbackChain         *m_Next = nullptr;
};

template <typename Impl, typename JSSetHookFunType, JSSetHookFunType *JSSetHookFunPtr>
class JSHookBase
  : public JSHookCallbackChain<JSSetHookFunType, JSSetHookFunPtr>
{
public:
    typedef JSHookCallbackChain<JSSetHookFunType, JSSetHookFunPtr> Base;
    typedef typename Base::CallbackFun CallbackFun;
    typedef typename Base::CallbackRes CallbackRes;

    JSHookBase()
      : Base(static_cast<CallbackFun>(Hook), ShutDownDispatcher)
    {}    // ^ static_cast to choose the right Hook from the overloaded bunch

    template <typename... Args>
    JSHookBase(JSRuntime *rt, Args &&...args)
      : Base(static_cast<CallbackFun>(Hook), ShutDownDispatcher, rt)
    {
        InitHook(rt, forward<Args>(args)...);
    }

    ~JSHookBase()
    {
        ShutDownHook();
    }

    template <typename... Args>
    void InitHook(JSRuntime *rt, Args &&...args)
    {
        MASSERT(nullptr != rt);

        this->SetJSRuntime(rt);
        this->AddToChain();

        JSHookAccess::Init(ImplPtr(), rt, forward<Args>(args)...);
    }

    void ShutDownHook()
    {
        if (this->IsQuiescentState()) return;

        JSHookAccess::ShutDown(ImplPtr());

        this->RemoveFromChain();
        this->ReturnToQuiescentState();
    }

private:
    static
    void ShutDownDispatcher(void *thisParam)
    {
        auto thisPtr = reinterpret_cast<JSHookBase *>(thisParam);
        thisPtr->ShutDownHook();
    }

    // helper to substitute the last parameter of the hook (i.e. void *closure)
    template <typename Tuple, size_t... idx>
    static
    CallbackRes CallNextHook(Base *next, Tuple&& tuple, index_sequence<idx...>)
    {
        return next->CallChained(
            get<idx>(forward<Tuple>(tuple))..., next
        );
    }

    // a helper to call the member function Hook defined in a descendant
    template <typename Tuple, size_t... idx>
    static
    CallbackRes CallMemberHook(Impl *This, Tuple&& tuple, index_sequence<idx...>)
    {
        return JSHookAccess::Hook(This, get<idx>(forward<Tuple>(tuple))...);
    }

    // the centerpiece: this function receives the calls from the engine, calls
    // the hook implementation and continues the chain
    template <typename... Args>
    static
    CallbackRes Hook(Args... args)
    {
        // For the very first call we cannot use the last argument of the
        // callback to get the address of a JSHookBase instance, because of the
        // JS call callbacks. Before a JS call JS engine uses
        // JSRuntime::callHookData as the last argument of the callback, but
        // after a JS call it uses the data returned by the callback the first
        // time. If someone calls JS_SetCallHook between these callbacks, the
        // last argument of the new callback call will be NOT the closure passed
        // in JS_SetCallHook. Use GetLwr() to check if we are at the top of the
        // calling chain and use GetHead() if we are.

        auto lwr = Base::GetLwr();

        JSHookBase *This = static_cast<JSHookBase*>(lwr);

        // Pack to tuple to get rid of the last argument. make_index_sequence
        // helps to unpack the tuple back to the parameters pack.
        CallbackRes res = CallMemberHook(
            This->ImplPtr()
          , forward_as_tuple(args...)
          , make_index_sequence<sizeof...(args) - 1>()
        );

        // The above call could have removed `This` from the call chain. Detect
        // this. If there was a deletion we should not advance the current node.
        bool chainChanged = lwr != Base::GetLwr();
        bool nextCall = true;

        if (!chainChanged || nullptr == Base::GetLwr())
        {
            nextCall = Base::AdvanceLwr();
        }

        if (nextCall)
        {
            // Pack to tuple to get rid of the last argument and substitute it
            // later in CallNextHook with a pointer to the next callback in
            // chain. make_index_sequence helps to unpack the tuple back to the
            // parameters pack.
            return CallNextHook(
                Base::GetLwr()
              , forward_as_tuple(args...)
              , make_index_sequence<sizeof...(args) - 1>()
            );
        }

        return res;
    }

    Impl *ImplPtr()
    {
        return static_cast<Impl *>(this);
    }

    Impl const *ImplPtr() const
    {
        return static_cast<const Impl *>(this);
    }
};
