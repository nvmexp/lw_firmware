/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2022 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "chanwmgr.h"
#include "atomwrap.h"
#ifdef INCLUDE_MDIAG
#include "mdiag/advschd/pmchwrap.h"
#ifdef INCLUDE_MDIAGUTL
#include "mdiag/utl/utlchanwrap.h"
#endif
#endif
#include "runlwrap.h"
#include "semawrap.h"
#include "core/include/script.h"

ChannelWrapperMgr *ChannelWrapperMgr::m_pInstance = NULL;

//--------------------------------------------------------------------
//! \brief constructor
//!
ChannelWrapperMgr::ChannelWrapperMgr
(
) :
    m_UseAtomWrapper(false),
    m_UsePmWrapper(false),
    m_UseRunlistWrapper(false),
    m_UseSemaphoreWrapper(false),
    m_UseUtlWrapper(false)
{
}

//--------------------------------------------------------------------
//! \brief Singleton access method
//!
ChannelWrapperMgr *ChannelWrapperMgr::Instance()
{
    if (m_pInstance == NULL)
    {
        m_pInstance = new ChannelWrapperMgr();
    }
    return m_pInstance;
}

//--------------------------------------------------------------------
//! \brief Singleton destruction method
//!
void ChannelWrapperMgr::ShutDown()
{
    if (m_pInstance != NULL)
    {
        delete m_pInstance;
        m_pInstance = NULL;
    }
    AtomFsm::ShutDown();
}

//--------------------------------------------------------------------
//! \brief Apply all necessary wrappers around the indicated channel
//!
//! This method should be called when a channel is first created.  It
//! applies whatever wrappers the channel needs (if any), and returns
//! the wrapped channel.  If the channel is already wrapped, or if no
//! wrappers are needed, then this method simply returns pCh.
//!
//! \param pCh The channel to wrap
//! \return The wrapped channel
//!
Channel *ChannelWrapperMgr::WrapChannel(Channel *pCh)
{
    MASSERT(pCh != NULL);
    MASSERT(pCh == pCh->GetWrapper());

    // Determine which wrappers are requested *and* supported.
    //
    bool useAtomWrapper      = (m_UseAtomWrapper &&
                                AtomChannelWrapper::IsSupported(pCh));
#if defined(INCLUDE_MDIAG)
    bool usePmWrapper        = (m_UsePmWrapper &&
                                PmChannelWrapper::IsSupported(pCh));
#else
    bool usePmWrapper        = false;
#endif
#if defined(INCLUDE_MDIAG) && defined(INCLUDE_MDIAGUTL)
    bool useUtlWrapper       = (m_UseUtlWrapper &&
                                UtlChannelWrapper::IsSupported(pCh));
#else
    bool useUtlWrapper       = false;
#endif
    bool useRunlistWrapper   = (m_UseRunlistWrapper &&
                                RunlistChannelWrapper::IsSupported(pCh));
    bool useSemaphoreWrapper = (m_UseSemaphoreWrapper &&
                                SemaphoreChannelWrapper::IsSupported(pCh));

    // PmChannelWrapper, RunlistChannelWrapper, and UtlChannelWrapper all
    // require a SemaphoreChannelWrapper in order to work.
    //
    if (useRunlistWrapper || usePmWrapper || useUtlWrapper)
    {
        MASSERT(SemaphoreChannelWrapper::IsSupported(pCh));
        useSemaphoreWrapper = true;
    }

    // If it's supported, we should use an AtomChannelWrapper whenever
    // we have a channel wrapper that can insert methods.
    //
    if (usePmWrapper || useRunlistWrapper || useSemaphoreWrapper || useUtlWrapper)
    {
        if (!useAtomWrapper && AtomChannelWrapper::IsSupported(pCh))
        {
            useAtomWrapper = true;
        }
    }

    // Apply the wrappers.
    //
    // There's some logic to how this code arranges the wrappers.  It
    // puts PmChannelWrapper on the outside, since PmChannelWrapper
    // tends to break Writes into smaller writes, and the inner
    // wrappers (esp. SemaphoreChannelWrapper) work better if they get
    // the "broken" writes.  Similarly, RunlistChannelWrapper
    // primarily modifies the method stream, while
    // SemaphoreChannelWrapper primarily monitors the stream for data,
    // so Runlist goes outside Semaphore.  AtomChannelWrapper has to
    // be inside any wrapper that can insert methods.
    //
    if (useAtomWrapper && pCh->GetAtomChannelWrapper() == NULL)
    {
        pCh = new AtomChannelWrapper(pCh);
    }

    if (useSemaphoreWrapper && pCh->GetSemaphoreChannelWrapper() == NULL)
    {
        pCh = new SemaphoreChannelWrapper(pCh);
    }

    if (useRunlistWrapper && pCh->GetRunlistChannelWrapper() == NULL)
    {
        pCh = new RunlistChannelWrapper(pCh);
    }

    // For consistency in event handling order, UtlChannelWrapper needs to be 
    // created before PmChannelWrapper since we want PolicyManager to handle 
    // any events (e.g. MethodWriteEvent) before Utl.
#ifdef INCLUDE_MDIAG
#ifdef INCLUDE_MDIAGUTL
    if (useUtlWrapper && pCh->GetUtlChannelWrapper() == NULL)
    {
        pCh = new UtlChannelWrapper(pCh);
    }
#endif

    if (usePmWrapper && pCh->GetPmChannelWrapper() == NULL)
    {
        pCh = new PmChannelWrapper(pCh);
    }
#endif

    return pCh;
}

//--------------------------------------------------------------------
// Javascript interface
//
JS_CLASS(ChannelWrapperMgr);

SObject ChannelWrapperMgr_Object
(
   "ChannelWrapperMgr",
   ChannelWrapperMgrClass,
   0,
   0,
   "Interface to ChannelWrapper manager."
);

JS_SMETHOD_LWSTOM
(
    ChannelWrapperMgr,
    UseRunlistWrapper,
    0,
    "Wrap channels in a Runlist wrapper, so they use Runlist scheduling."
)
{
    ChannelWrapperMgr::Instance()->UseRunlistWrapper();
    return JS_TRUE;
}
