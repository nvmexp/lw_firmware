/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! @file    linxncrc.cpp
//! @brief   Implement cross platform portion of NlwrsesConsole
//!
//! For detailed comments on each of the public interfaces, see console.h

#include "core/include/nlwrcnsl.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"
#include "core/include/massert.h"
#include "core/include/cnslmgr.h"

#include <nlwrses.h>
#include <deque>

//-----------------------------------------------------------------------------
//! \brief Blocking call to get a character from stdin
//!
//! \param bYield should be set to true if the routine should yield while
//!               waiting for a character, before reading the character
//!
//! \return the value read from stdin
/* virtual */ INT32 NlwrsesConsole::GetChar(bool bYield)
{
    INT32 Key;

    if (bYield)
    {
        do
        {
            Tasker::Yield();
            Key = getch();
        } while (ERR == Key);
    }
    else
    {
        Key = getch();
    }

    return Key;
}

//-----------------------------------------------------------------------------
//! \brief Blocking call to flush all characters from stdin into a queue
//!
//! \param pKeyQueue is a pointer to the key queue push keys into
//! \param RawKeyMap is the translation keymap to use to colwert the raw input
//!        before pushing the values into the queue
//!
/* virtual */ void NlwrsesConsole::FlushChars
(
    deque<Console::VirtualKey> *pKeyQueue,
    const Console::VirtualKey  *RawKeyMap
)
{
    MASSERT(!"NlwrsesConsole::FlushChars not implemented on Linux\n");
}

//-----------------------------------------------------------------------------
//! \brief Private function to determie whether the keyboard has been hit, may
//!        be used to implement keyboard hit functionality that is specific to
//!        a particular subclass of RawConsole
//!
//! \return true if the keyboard has been hit, false otherwise
/* virtual */  bool NlwrsesConsole::PrivKeyboardHit()
{
    int c = getch();

    if (ERR != c)
    {
        ungetch(c);
        return true;
    }
    return false;
}

