/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_CHANWMGR_H
#define INCLUDED_CHANWMGR_H

class Channel;

//--------------------------------------------------------------------
//! \brief Manager class for ChannelWrappers.
//!
//! This class is mostly used to keep track of which wrappers should
//! be wrapped around newly-created channels.
//!
class ChannelWrapperMgr
{
public:
    static ChannelWrapperMgr *Instance();
    static void ShutDown();
    Channel *WrapChannel(Channel *pCh);

    void UseAtomWrapper()      { m_UseAtomWrapper = true; }
    void UsePmWrapper()        { m_UsePmWrapper = true; }
    void UseRunlistWrapper()   { m_UseRunlistWrapper = true; }
    void UseSemaphoreWrapper() { m_UseSemaphoreWrapper = true; }
    void UseUtlWrapper()       { m_UseUtlWrapper = true; }

    bool UsesAtomWrapper()      const { return m_UseAtomWrapper; }
    bool UsesUtlWrapper()       const { return m_UseUtlWrapper; }
    bool UsesRunlistWrapper()   const { return m_UseRunlistWrapper; }
    bool UsesSemaphoreWrapper() const { return m_UseSemaphoreWrapper; }

private:
    ChannelWrapperMgr();

    static ChannelWrapperMgr *m_pInstance;  //!< Instance of this singleton
    bool m_UseAtomWrapper;      //!< Wrap channels in a AtomChannelWrapper
    bool m_UsePmWrapper;        //!< Wrap channels in a PmChannelWrapper
    bool m_UseRunlistWrapper;   //!< Wrap channels in a RunlistChannelWrapper
    bool m_UseSemaphoreWrapper; //!< Wrap channels in a SemaphoreChannelWrapper
    bool m_UseUtlWrapper;       //!< Wrap channels in a UtlChannelWrapper
};

#endif // INCLUDED_CHANWMGR_H
