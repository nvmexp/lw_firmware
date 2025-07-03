/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009,2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! @file    macrawc.cpp
//! @brief   Implement cross platform portion of RawConsole
//!
//! For detailed comments on each of the public interfaces, see console.h

#include "core/include/rawcnsl.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"
#include "core/include/massert.h"
#include "core/include/cnslmgr.h"
#include "core/include/platform.h"
#include <stdio.h>
#include <deque>

#define CFNETWORK_NEW_API
extern "C" {
#include <ApplicationServices/ApplicationServices.h>
#include <IOKit/IOKitLib.h>
#include <CoreServices/CoreServices.h>
#include <time.h>
}

#ifdef INCLUDE_GPU
#include <Carbon/Carbon.h>

// Variables used to let us use GetKeys() to read keyboard when user
// interface is disabled.
//
static const INT32 MODIFIER_INDEX = 7;        // Modifier keys in s_KeysPressed
static const INT32 SHIFT_MASK = 0x03;         // shift/caps-lock in s_KeysPressed
static const INT32 CTRL_MASK  = 0x08;         // control key in s_KeysPressed
static KeyMapByteArray s_KeysPressed; // Bitmap of pressed keys

#endif

//-----------------------------------------------------------------------------
//! Keypress input queue
static deque<Console::VirtualKey> s_KeypressQueue;

//-----------------------------------------------------------------------------
//! Colwersion keymap to colwert raw keys from getch to a
//! Console::VirtualKey enumeration
static const Console::VirtualKey s_KeyMapForGetc[] =
{
    Console::VKC_CONTROL_AT           // 0x000
   ,Console::VKC_CONTROL_A            // 0x001
   ,Console::VKC_CONTROL_B            // 0x002
   ,Console::VKC_CONTROL_C            // 0x003
   ,Console::VKC_CONTROL_D            // 0x004
   ,Console::VKC_CONTROL_E            // 0x005
   ,Console::VKC_CONTROL_F            // 0x006
   ,Console::VKC_CONTROL_G            // 0x007
   ,Console::VKC_BACKSPACE            // 0x008
   ,Console::VKC_TAB                  // 0x009
   ,Console::VKC_LINEFEED             // 0x00a
   ,Console::VKC_CONTROL_K            // 0x00b
   ,Console::VKC_CONTROL_L            // 0x00c
   ,Console::VKC_RETURN               // 0x00d
   ,Console::VKC_CONTROL_N            // 0x00e
   ,Console::VKC_CONTROL_O            // 0x00f
   ,Console::VKC_CONTROL_P            // 0x010
   ,Console::VKC_CONTROL_Q            // 0x011
   ,Console::VKC_CONTROL_R            // 0x012
   ,Console::VKC_CONTROL_S            // 0x013
   ,Console::VKC_CONTROL_T            // 0x014
   ,Console::VKC_CONTROL_U            // 0x015
   ,Console::VKC_CONTROL_V            // 0x016
   ,Console::VKC_CONTROL_W            // 0x017
   ,Console::VKC_CONTROL_X            // 0x018
   ,Console::VKC_CONTROL_Y            // 0x019
   ,Console::VKC_CONTROL_Z            // 0x01a
   ,Console::VKC_ESCAPE               // 0x01b
   ,Console::VKC_CONTROL_BACKSLASH    // 0x01c
   ,Console::VKC_CONTROL_RBRACKET     // 0x01d
   ,Console::VKC_CONTROL_CARET        // 0x01e
   ,Console::VKC_CONTROL_UNDERSCORE   // 0x01f
   ,Console::VKC_SPACE                // 0x020
   ,Console::VKC_EXCLAMATIONPOINT     // 0x021
   ,Console::VKC_DOUBLEQUOTE          // 0x022
   ,Console::VKC_HASH                 // 0x023
   ,Console::VKC_DOLLAR               // 0x024
   ,Console::VKC_PERCENT              // 0x025
   ,Console::VKC_AMPERSAND            // 0x026
   ,Console::VKC_QUOTE                // 0x027
   ,Console::VKC_LPAREN               // 0x028
   ,Console::VKC_RPAREN               // 0x029
   ,Console::VKC_STAR                 // 0x02a
   ,Console::VKC_PLUS                 // 0x02b
   ,Console::VKC_COMMA                // 0x02c
   ,Console::VKC_DASH                 // 0x02d
   ,Console::VKC_PERIOD               // 0x02e
   ,Console::VKC_SLASH                // 0x02f
   ,Console::VKC_0                    //
   ,Console::VKC_1                    //
   ,Console::VKC_2                    //
   ,Console::VKC_3                    //
   ,Console::VKC_4                    //
   ,Console::VKC_5                    //
   ,Console::VKC_6                    //
   ,Console::VKC_7                    //
   ,Console::VKC_8                    //
   ,Console::VKC_9                    //
   ,Console::VKC_COLON                // 0x03a
   ,Console::VKC_SEMICOLON            // 0x03b
   ,Console::VKC_LANGLE               // 0x03c
   ,Console::VKC_EQUALS               // 0x03d
   ,Console::VKC_RANGLE               // 0x03e
   ,Console::VKC_QUESTIONMARK         // 0x03f
   ,Console::VKC_AT                   // 0x040
   ,Console::VKC_CAPITAL_A            //
   ,Console::VKC_CAPITAL_B            //
   ,Console::VKC_CAPITAL_C            //
   ,Console::VKC_CAPITAL_D            //
   ,Console::VKC_CAPITAL_E            //
   ,Console::VKC_CAPITAL_F            //
   ,Console::VKC_CAPITAL_G            //
   ,Console::VKC_CAPITAL_H            //
   ,Console::VKC_CAPITAL_I            //
   ,Console::VKC_CAPITAL_J            //
   ,Console::VKC_CAPITAL_K            //
   ,Console::VKC_CAPITAL_L            //
   ,Console::VKC_CAPITAL_M            //
   ,Console::VKC_CAPITAL_N            //
   ,Console::VKC_CAPITAL_O            //
   ,Console::VKC_CAPITAL_P            //
   ,Console::VKC_CAPITAL_Q            //
   ,Console::VKC_CAPITAL_R            //
   ,Console::VKC_CAPITAL_S            //
   ,Console::VKC_CAPITAL_T            //
   ,Console::VKC_CAPITAL_U            //
   ,Console::VKC_CAPITAL_V            //
   ,Console::VKC_CAPITAL_W            //
   ,Console::VKC_CAPITAL_X            //
   ,Console::VKC_CAPITAL_Y            //
   ,Console::VKC_CAPITAL_Z            //
   ,Console::VKC_LBRACKET             // 0x05b
   ,Console::VKC_BACKSLASH            // 0x05c
   ,Console::VKC_RBRACKET             // 0x05d
   ,Console::VKC_CARET                // 0x05e
   ,Console::VKC_UNDERSCORE           // 0x05f
   ,Console::VKC_BACKQUOTE            // 0x060
   ,Console::VKC_LOWER_A              //
   ,Console::VKC_LOWER_B              //
   ,Console::VKC_LOWER_C              //
   ,Console::VKC_LOWER_D              //
   ,Console::VKC_LOWER_E              //
   ,Console::VKC_LOWER_F              //
   ,Console::VKC_LOWER_G              //
   ,Console::VKC_LOWER_H              //
   ,Console::VKC_LOWER_I              //
   ,Console::VKC_LOWER_J              //
   ,Console::VKC_LOWER_K              //
   ,Console::VKC_LOWER_L              //
   ,Console::VKC_LOWER_M              //
   ,Console::VKC_LOWER_N              //
   ,Console::VKC_LOWER_O              //
   ,Console::VKC_LOWER_P              //
   ,Console::VKC_LOWER_Q              //
   ,Console::VKC_LOWER_R              //
   ,Console::VKC_LOWER_S              //
   ,Console::VKC_LOWER_T              //
   ,Console::VKC_LOWER_U              //
   ,Console::VKC_LOWER_V              //
   ,Console::VKC_LOWER_W              //
   ,Console::VKC_LOWER_X              //
   ,Console::VKC_LOWER_Y              //
   ,Console::VKC_LOWER_Z              //
   ,Console::VKC_LBRACE               // 0x07b
   ,Console::VKC_PIPE                 // 0x07c
   ,Console::VKC_RBRACE               // 0x07d
   ,Console::VKC_TILDE                // 0x07e
   ,Console::VKC_BACKSPACE            // 0x07f
};

//-----------------------------------------------------------------------------
//! Colwersion keymap for translatinge the bitmask read by GetKeys() into a
//! virtual key, for the case when neither control nor shift are pressed.
//!
//! Note: Assumes US QUERTY keyboard layout.  This table was generated
//! expermentally, by writing a small program and pressing every key.
//! (That's the best I came up with after spending half a day looking
//! for a better way to do the translation.  Based on some online
//! forums, at least two other developers have done the same thing.)
//
static const Console::VirtualKey s_KeyMapForKeycode[] =
{
    Console::VKC_LOWER_A              // 0x00
   ,Console::VKC_LOWER_S              // 0x01
   ,Console::VKC_LOWER_D              // 0x02
   ,Console::VKC_LOWER_F              // 0x03
   ,Console::VKC_LOWER_H              // 0x04
   ,Console::VKC_LOWER_G              // 0x05
   ,Console::VKC_LOWER_Z              // 0x06
   ,Console::VKC_LOWER_X              // 0x07
   ,Console::VKC_LOWER_C              // 0x08
   ,Console::VKC_LOWER_V              // 0x09
   ,Console::VKC_NONE                 // 0x0a
   ,Console::VKC_LOWER_B              // 0x0b
   ,Console::VKC_LOWER_Q              // 0x0c
   ,Console::VKC_LOWER_W              // 0x0d
   ,Console::VKC_LOWER_E              // 0x0e
   ,Console::VKC_LOWER_R              // 0x0f
   ,Console::VKC_LOWER_Y              // 0x10
   ,Console::VKC_LOWER_T              // 0x11
   ,Console::VKC_1                    // 0x12
   ,Console::VKC_2                    // 0x13
   ,Console::VKC_3                    // 0x14
   ,Console::VKC_4                    // 0x15
   ,Console::VKC_6                    // 0x16
   ,Console::VKC_5                    // 0x17
   ,Console::VKC_EQUALS               // 0x18
   ,Console::VKC_9                    // 0x19
   ,Console::VKC_7                    // 0x1a
   ,Console::VKC_DASH                 // 0x1b
   ,Console::VKC_8                    // 0x1c
   ,Console::VKC_0                    // 0x1d
   ,Console::VKC_RBRACKET             // 0x1e
   ,Console::VKC_LOWER_O              // 0x1f
   ,Console::VKC_LOWER_U              // 0x20
   ,Console::VKC_LBRACKET             // 0x21
   ,Console::VKC_LOWER_I              // 0x22
   ,Console::VKC_LOWER_P              // 0x23
   ,Console::VKC_NONE                 // 0x24
   ,Console::VKC_LOWER_L              // 0x25
   ,Console::VKC_LOWER_J              // 0x26
   ,Console::VKC_QUOTE                // 0x27
   ,Console::VKC_LOWER_K              // 0x28
   ,Console::VKC_SEMICOLON            // 0x29
   ,Console::VKC_BACKSLASH            // 0x2a
   ,Console::VKC_COMMA                // 0x2b
   ,Console::VKC_SLASH                // 0x2c
   ,Console::VKC_LOWER_N              // 0x2d
   ,Console::VKC_LOWER_M              // 0x2e
   ,Console::VKC_PERIOD               // 0x2f
   ,Console::VKC_TAB                  // 0x30
   ,Console::VKC_SPACE                // 0x31
   ,Console::VKC_BACKQUOTE            // 0x32
   ,Console::VKC_BACKSPACE            // 0x33
   ,Console::VKC_NONE                 // 0x34
   ,Console::VKC_ESCAPE               // 0x35
   ,Console::VKC_NONE                 // 0x36
   ,Console::VKC_NONE                 // 0x37 (apple)
   ,Console::VKC_NONE                 // 0x38 (shift)
   ,Console::VKC_NONE                 // 0x39 (caps lock)
   ,Console::VKC_NONE                 // 0x3a (option)
   ,Console::VKC_NONE                 // 0x3b (control)
   ,Console::VKC_NONE                 // 0x3c
   ,Console::VKC_NONE                 // 0x3d
   ,Console::VKC_NONE                 // 0x3e
   ,Console::VKC_NONE                 // 0x3f
   ,Console::VKC_NONE                 // 0x40
   ,Console::VKC_PERIOD               // 0x41 (keypad .)
   ,Console::VKC_NONE                 // 0x42
   ,Console::VKC_STAR                 // 0x43 (keypad *)
   ,Console::VKC_NONE                 // 0x44
   ,Console::VKC_PLUS                 // 0x45 (keypad +)
   ,Console::VKC_NONE                 // 0x46
   ,Console::VKC_NONE                 // 0x47 (keypad clear)
   ,Console::VKC_NONE                 // 0x48
   ,Console::VKC_NONE                 // 0x49
   ,Console::VKC_NONE                 // 0x4a
   ,Console::VKC_SLASH                // 0x4b (keypad /)
   ,Console::VKC_RETURN               // 0x4c (keypad enter)
   ,Console::VKC_NONE                 // 0x4d
   ,Console::VKC_DASH                 // 0x4e (keypad -)
   ,Console::VKC_NONE                 // 0x4f
   ,Console::VKC_NONE                 // 0x50
   ,Console::VKC_EQUALS               // 0x51 (keypad =)
   ,Console::VKC_0                    // 0x52 (keypad 0)
   ,Console::VKC_1                    // 0x53 (keypad 1)
   ,Console::VKC_2                    // 0x54 (keypad 2)
   ,Console::VKC_3                    // 0x55 (keypad 3)
   ,Console::VKC_4                    // 0x56 (keypad 4)
   ,Console::VKC_5                    // 0x57 (keypad 5)
   ,Console::VKC_6                    // 0x58 (keypad 6)
   ,Console::VKC_7                    // 0x59 (keypad 7)
   ,Console::VKC_NONE                 // 0x5a
   ,Console::VKC_8                    // 0x5b (keypad 8)
   ,Console::VKC_9                    // 0x5c (keypad 9)
   ,Console::VKC_NONE                 // 0x5d
   ,Console::VKC_NONE                 // 0x5e
   ,Console::VKC_NONE                 // 0x5f
   ,Console::VKC_F5                   // 0x60
   ,Console::VKC_F6                   // 0x61
   ,Console::VKC_F7                   // 0x62
   ,Console::VKC_F3                   // 0x63
   ,Console::VKC_F8                   // 0x64
   ,Console::VKC_F9                   // 0x65
   ,Console::VKC_NONE                 // 0x66
   ,Console::VKC_F11                  // 0x67
   ,Console::VKC_NONE                 // 0x68
   ,Console::VKC_NONE                 // 0x69 (F13)
   ,Console::VKC_NONE                 // 0x6a
   ,Console::VKC_NONE                 // 0x6b (F14)
   ,Console::VKC_NONE                 // 0x6c
   ,Console::VKC_F10                  // 0x6d
   ,Console::VKC_NONE                 // 0x6e
   ,Console::VKC_F12                  // 0x6f
   ,Console::VKC_NONE                 // 0x70
   ,Console::VKC_NONE                 // 0x71 (F15)
   ,Console::VKC_NONE                 // 0x72 (help)
   ,Console::VKC_HOME                 // 0x73
   ,Console::VKC_PAGEUP               // 0x74
   ,Console::VKC_DELETE               // 0x75
   ,Console::VKC_F4                   // 0x76
   ,Console::VKC_END                  // 0x77
   ,Console::VKC_F2                   // 0x78
   ,Console::VKC_PAGEDOWN             // 0x79
   ,Console::VKC_F1                   // 0x7a
   ,Console::VKC_LEFT                 // 0x7b
   ,Console::VKC_RIGHT                // 0x7c
   ,Console::VKC_DOWN                 // 0x7d
   ,Console::VKC_UP                   // 0x7e
   ,Console::VKC_NONE                 // 0x7f
};

//-----------------------------------------------------------------------------
//! Colwersion keymap for translatinge the bitmask read by GetKeys() into a
//! virtual key, for the case when the control key is pressed.
//!
//! Note: This table is also used when both control and shift are pressed.
//! It's a bit of a cheat, but there aren't any cases where key, ctrl-key,
//! and ctrl-shift-key would yield three different virtual keys, so we might as
//! well just collapse the ctrl-key and ctrl-shift-key cases into one table.
static const Console::VirtualKey s_KeyMapForCtrlKeycode[] =
{
    Console::VKC_CONTROL_A            // 0x00
   ,Console::VKC_CONTROL_S            // 0x01
   ,Console::VKC_CONTROL_D            // 0x02
   ,Console::VKC_CONTROL_F            // 0x03
   ,Console::VKC_BACKSPACE            // 0x04
   ,Console::VKC_CONTROL_G            // 0x05
   ,Console::VKC_CONTROL_Z            // 0x06
   ,Console::VKC_CONTROL_X            // 0x07
   ,Console::VKC_CONTROL_C            // 0x08
   ,Console::VKC_CONTROL_V            // 0x09
   ,Console::VKC_NONE                 // 0x0a
   ,Console::VKC_CONTROL_B            // 0x0b
   ,Console::VKC_CONTROL_Q            // 0x0c
   ,Console::VKC_CONTROL_W            // 0x0d
   ,Console::VKC_CONTROL_E            // 0x0e
   ,Console::VKC_CONTROL_R            // 0x0f
   ,Console::VKC_CONTROL_Y            // 0x10
   ,Console::VKC_CONTROL_T            // 0x11
   ,Console::VKC_1                    // 0x12
   ,Console::VKC_CONTROL_AT           // 0x13
   ,Console::VKC_3                    // 0x14
   ,Console::VKC_4                    // 0x15
   ,Console::VKC_CONTROL_CARET        // 0x16
   ,Console::VKC_5                    // 0x17
   ,Console::VKC_EQUALS               // 0x18
   ,Console::VKC_9                    // 0x19
   ,Console::VKC_7                    // 0x1a
   ,Console::VKC_CONTROL_UNDERSCORE   // 0x1b
   ,Console::VKC_8                    // 0x1c
   ,Console::VKC_0                    // 0x1d
   ,Console::VKC_CONTROL_RBRACKET     // 0x1e
   ,Console::VKC_CONTROL_O            // 0x1f
   ,Console::VKC_CONTROL_U            // 0x20
   ,Console::VKC_ESCAPE               // 0x21
   ,Console::VKC_TAB                  // 0x22
   ,Console::VKC_CONTROL_P            // 0x23
   ,Console::VKC_NONE                 // 0x24
   ,Console::VKC_CONTROL_L            // 0x25
   ,Console::VKC_LINEFEED             // 0x26
   ,Console::VKC_QUOTE                // 0x27
   ,Console::VKC_CONTROL_K            // 0x28
   ,Console::VKC_SEMICOLON            // 0x29
   ,Console::VKC_CONTROL_BACKSLASH    // 0x2a
   ,Console::VKC_COMMA                // 0x2b
   ,Console::VKC_SLASH                // 0x2c
   ,Console::VKC_CONTROL_N            // 0x2d
   ,Console::VKC_RETURN               // 0x2e
   ,Console::VKC_PERIOD               // 0x2f
   ,Console::VKC_TAB                  // 0x30
   ,Console::VKC_SPACE                // 0x31
   ,Console::VKC_BACKQUOTE            // 0x32
   ,Console::VKC_CONTROL_BACKSPACE    // 0x33
   ,Console::VKC_NONE                 // 0x34
   ,Console::VKC_ESCAPE               // 0x35
   ,Console::VKC_NONE                 // 0x36
   ,Console::VKC_NONE                 // 0x37 (apple)
   ,Console::VKC_NONE                 // 0x38 (shift)
   ,Console::VKC_NONE                 // 0x39 (caps lock)
   ,Console::VKC_NONE                 // 0x3a (option)
   ,Console::VKC_NONE                 // 0x3b (control)
   ,Console::VKC_NONE                 // 0x3c
   ,Console::VKC_NONE                 // 0x3d
   ,Console::VKC_NONE                 // 0x3e
   ,Console::VKC_NONE                 // 0x3f
   ,Console::VKC_NONE                 // 0x40
   ,Console::VKC_PERIOD               // 0x41 (keypad .)
   ,Console::VKC_NONE                 // 0x42
   ,Console::VKC_CONTROL_KPSTAR       // 0x43 (keypad *)
   ,Console::VKC_NONE                 // 0x44
   ,Console::VKC_CONTROL_KPPLUS       // 0x45 (keypad +)
   ,Console::VKC_NONE                 // 0x46
   ,Console::VKC_NONE                 // 0x47 (keypad clear)
   ,Console::VKC_NONE                 // 0x48
   ,Console::VKC_NONE                 // 0x49
   ,Console::VKC_NONE                 // 0x4a
   ,Console::VKC_CONTROL_KPSLASH      // 0x4b (keypad /)
   ,Console::VKC_RETURN               // 0x4c (keypad enter)
   ,Console::VKC_NONE                 // 0x4d
   ,Console::VKC_CONTROL_KPDASH       // 0x4e (keypad -)
   ,Console::VKC_NONE                 // 0x4f
   ,Console::VKC_NONE                 // 0x50
   ,Console::VKC_EQUALS               // 0x51 (keypad =)
   ,Console::VKC_0                    // 0x52 (keypad 0)
   ,Console::VKC_1                    // 0x53 (keypad 1)
   ,Console::VKC_2                    // 0x54 (keypad 2)
   ,Console::VKC_3                    // 0x55 (keypad 3)
   ,Console::VKC_4                    // 0x56 (keypad 4)
   ,Console::VKC_5                    // 0x57 (keypad 5)
   ,Console::VKC_6                    // 0x58 (keypad 6)
   ,Console::VKC_7                    // 0x59 (keypad 7)
   ,Console::VKC_NONE                 // 0x5a
   ,Console::VKC_8                    // 0x5b (keypad 8)
   ,Console::VKC_9                    // 0x5c (keypad 9)
   ,Console::VKC_NONE                 // 0x5d
   ,Console::VKC_NONE                 // 0x5e
   ,Console::VKC_NONE                 // 0x5f
   ,Console::VKC_CONTROL_F5           // 0x60
   ,Console::VKC_CONTROL_F6           // 0x61
   ,Console::VKC_CONTROL_F7           // 0x62
   ,Console::VKC_CONTROL_F3           // 0x63
   ,Console::VKC_CONTROL_F8           // 0x64
   ,Console::VKC_CONTROL_F9           // 0x65
   ,Console::VKC_NONE                 // 0x66
   ,Console::VKC_CONTROL_F11          // 0x67
   ,Console::VKC_NONE                 // 0x68
   ,Console::VKC_NONE                 // 0x69 (F13)
   ,Console::VKC_NONE                 // 0x6a
   ,Console::VKC_NONE                 // 0x6b (F14)
   ,Console::VKC_NONE                 // 0x6c
   ,Console::VKC_CONTROL_F10          // 0x6d
   ,Console::VKC_NONE                 // 0x6e
   ,Console::VKC_CONTROL_F12          // 0x6f
   ,Console::VKC_NONE                 // 0x70
   ,Console::VKC_NONE                 // 0x71 (F15)
   ,Console::VKC_NONE                 // 0x72 (help)
   ,Console::VKC_CONTROL_HOME         // 0x73
   ,Console::VKC_CONTROL_PAGEUP       // 0x74
   ,Console::VKC_CONTROL_DELETE       // 0x75
   ,Console::VKC_CONTROL_F4           // 0x76
   ,Console::VKC_CONTROL_END          // 0x77
   ,Console::VKC_CONTROL_F2           // 0x78
   ,Console::VKC_CONTROL_PAGEDOWN     // 0x79
   ,Console::VKC_CONTROL_F1           // 0x7a
   ,Console::VKC_CONTROL_LEFT         // 0x7b
   ,Console::VKC_CONTROL_RIGHT        // 0x7c
   ,Console::VKC_CONTROL_DOWN         // 0x7d
   ,Console::VKC_CONTROL_UP           // 0x7e
   ,Console::VKC_NONE                 // 0x7f
};

//-----------------------------------------------------------------------------
//! Colwersion keymap for translatinge the bitmask read by GetKeys() into a
//! virtual key, when the shift key is pressed and/or caps-lock is on.
//!
static const Console::VirtualKey s_KeyMapForShiftKeycode[] =
{
    Console::VKC_CAPITAL_A            // 0x00
   ,Console::VKC_CAPITAL_S            // 0x01
   ,Console::VKC_CAPITAL_D            // 0x02
   ,Console::VKC_CAPITAL_F            // 0x03
   ,Console::VKC_CAPITAL_H            // 0x04
   ,Console::VKC_CAPITAL_G            // 0x05
   ,Console::VKC_CAPITAL_Z            // 0x06
   ,Console::VKC_CAPITAL_X            // 0x07
   ,Console::VKC_CAPITAL_C            // 0x08
   ,Console::VKC_CAPITAL_V            // 0x09
   ,Console::VKC_NONE                 // 0x0a
   ,Console::VKC_CAPITAL_B            // 0x0b
   ,Console::VKC_CAPITAL_Q            // 0x0c
   ,Console::VKC_CAPITAL_W            // 0x0d
   ,Console::VKC_CAPITAL_E            // 0x0e
   ,Console::VKC_CAPITAL_R            // 0x0f
   ,Console::VKC_CAPITAL_Y            // 0x10
   ,Console::VKC_CAPITAL_T            // 0x11
   ,Console::VKC_EXCLAMATIONPOINT     // 0x12
   ,Console::VKC_AT                   // 0x13
   ,Console::VKC_HASH                 // 0x14
   ,Console::VKC_DOLLAR               // 0x15
   ,Console::VKC_CARET                // 0x16
   ,Console::VKC_PERCENT              // 0x17
   ,Console::VKC_PLUS                 // 0x18
   ,Console::VKC_LPAREN               // 0x19
   ,Console::VKC_AMPERSAND            // 0x1a
   ,Console::VKC_UNDERSCORE           // 0x1b
   ,Console::VKC_STAR                 // 0x1c
   ,Console::VKC_RPAREN               // 0x1d
   ,Console::VKC_RBRACE               // 0x1e
   ,Console::VKC_CAPITAL_O            // 0x1f
   ,Console::VKC_CAPITAL_U            // 0x20
   ,Console::VKC_LBRACE               // 0x21
   ,Console::VKC_CAPITAL_I            // 0x22
   ,Console::VKC_CAPITAL_P            // 0x23
   ,Console::VKC_NONE                 // 0x24
   ,Console::VKC_CAPITAL_L            // 0x25
   ,Console::VKC_CAPITAL_J            // 0x26
   ,Console::VKC_DOUBLEQUOTE          // 0x27
   ,Console::VKC_CAPITAL_K            // 0x28
   ,Console::VKC_COLON                // 0x29
   ,Console::VKC_PIPE                 // 0x2a
   ,Console::VKC_LANGLE               // 0x2b
   ,Console::VKC_QUESTIONMARK         // 0x2c
   ,Console::VKC_CAPITAL_N            // 0x2d
   ,Console::VKC_CAPITAL_M            // 0x2e
   ,Console::VKC_RANGLE               // 0x2f
   ,Console::VKC_BACKTAB              // 0x30
   ,Console::VKC_SPACE                // 0x31
   ,Console::VKC_TILDE                // 0x32
   ,Console::VKC_BACKSPACE            // 0x33
   ,Console::VKC_NONE                 // 0x34
   ,Console::VKC_ESCAPE               // 0x35
   ,Console::VKC_NONE                 // 0x36
   ,Console::VKC_NONE                 // 0x37 (apple)
   ,Console::VKC_NONE                 // 0x38 (shift)
   ,Console::VKC_NONE                 // 0x39 (caps lock)
   ,Console::VKC_NONE                 // 0x3a (option)
   ,Console::VKC_NONE                 // 0x3b (control)
   ,Console::VKC_NONE                 // 0x3c
   ,Console::VKC_NONE                 // 0x3d
   ,Console::VKC_NONE                 // 0x3e
   ,Console::VKC_NONE                 // 0x3f
   ,Console::VKC_NONE                 // 0x40
   ,Console::VKC_PERIOD               // 0x41 (keypad .)
   ,Console::VKC_NONE                 // 0x42
   ,Console::VKC_STAR                 // 0x43 (keypad *)
   ,Console::VKC_NONE                 // 0x44
   ,Console::VKC_PLUS                 // 0x45 (keypad +)
   ,Console::VKC_NONE                 // 0x46
   ,Console::VKC_NONE                 // 0x47 (keypad clear)
   ,Console::VKC_NONE                 // 0x48
   ,Console::VKC_NONE                 // 0x49
   ,Console::VKC_NONE                 // 0x4a
   ,Console::VKC_SLASH                // 0x4b (keypad /)
   ,Console::VKC_RETURN               // 0x4c (keypad enter)
   ,Console::VKC_NONE                 // 0x4d
   ,Console::VKC_DASH                 // 0x4e (keypad -)
   ,Console::VKC_NONE                 // 0x4f
   ,Console::VKC_NONE                 // 0x50
   ,Console::VKC_EQUALS               // 0x51 (keypad =)
   ,Console::VKC_0                    // 0x52 (keypad 0)
   ,Console::VKC_1                    // 0x53 (keypad 1)
   ,Console::VKC_2                    // 0x54 (keypad 2)
   ,Console::VKC_3                    // 0x55 (keypad 3)
   ,Console::VKC_4                    // 0x56 (keypad 4)
   ,Console::VKC_5                    // 0x57 (keypad 5)
   ,Console::VKC_6                    // 0x58 (keypad 6)
   ,Console::VKC_7                    // 0x59 (keypad 7)
   ,Console::VKC_NONE                 // 0x5a
   ,Console::VKC_8                    // 0x5b (keypad 8)
   ,Console::VKC_9                    // 0x5c (keypad 9)
   ,Console::VKC_NONE                 // 0x5d
   ,Console::VKC_NONE                 // 0x5e
   ,Console::VKC_NONE                 // 0x5f
   ,Console::VKC_SHIFT_F5             // 0x60
   ,Console::VKC_SHIFT_F6             // 0x61
   ,Console::VKC_SHIFT_F7             // 0x62
   ,Console::VKC_SHIFT_F3             // 0x63
   ,Console::VKC_SHIFT_F8             // 0x64
   ,Console::VKC_SHIFT_F9             // 0x65
   ,Console::VKC_NONE                 // 0x66
   ,Console::VKC_SHIFT_F11            // 0x67
   ,Console::VKC_NONE                 // 0x68
   ,Console::VKC_NONE                 // 0x69 (F13)
   ,Console::VKC_NONE                 // 0x6a
   ,Console::VKC_NONE                 // 0x6b (F14)
   ,Console::VKC_NONE                 // 0x6c
   ,Console::VKC_SHIFT_F10            // 0x6d
   ,Console::VKC_NONE                 // 0x6e
   ,Console::VKC_SHIFT_F12            // 0x6f
   ,Console::VKC_NONE                 // 0x70
   ,Console::VKC_NONE                 // 0x71 (F15)
   ,Console::VKC_NONE                 // 0x72 (help)
   ,Console::VKC_HOME                 // 0x73
   ,Console::VKC_PAGEUP               // 0x74
   ,Console::VKC_DELETE               // 0x75
   ,Console::VKC_SHIFT_F4             // 0x76
   ,Console::VKC_END                  // 0x77
   ,Console::VKC_SHIFT_F2             // 0x78
   ,Console::VKC_PAGEDOWN             // 0x79
   ,Console::VKC_SHIFT_F1             // 0x7a
   ,Console::VKC_LEFT                 // 0x7b
   ,Console::VKC_RIGHT                // 0x7c
   ,Console::VKC_DOWN                 // 0x7d
   ,Console::VKC_UP                   // 0x7e
   ,Console::VKC_NONE                 // 0x7f
};

//! Structure used to define an escape sequence
//!
struct EscapeSequence
{
    Console::VirtualKey outKey;   // Key represented by the esc. seq.
    UINT32              size;     // Size of the escape sequence
    Console::VirtualKey keys[4];  // Keys in the escape sequence
};

//! Table of escape sequences.  Used by RawConsole::HandleEscapeSequence().
//!
static const EscapeSequence s_EscapeSequences[] =
{
    {Console::VKC_UP,     3, {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_A}},
    {Console::VKC_DOWN,   3, {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_B}},
    {Console::VKC_RIGHT,  3, {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_C}},
    {Console::VKC_LEFT,   3, {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_D}},
    {Console::VKC_END,    3, {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_F}},
    {Console::VKC_HOME,   3, {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_H}},
    {Console::VKC_END,    4, {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_4,
                                                           Console::VKC_TILDE}},
    {Console::VKC_HOME,   4, {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_1,
                                                           Console::VKC_TILDE}},
    {Console::VKC_ALT_F1, 3, {Console::VKC_ESCAPE, Console::VKC_CAPITAL_O, Console::VKC_CAPITAL_P}}
};

//! Number of availabe escape sequences.
//!
static const int NUM_ESCAPE_SEQUENCES =
        sizeof(s_EscapeSequences) / sizeof(EscapeSequence);

/* virtual */  Console::VirtualKey RawConsole::GetKey()
{
    // Wait for a key to be hit
    //
    while (!KeyboardHit())
    {
        Tasker::Yield();
    }

    FlushChars(&s_KeypressQueue, s_KeyMapForGetc);

#ifdef INCLUDE_GPU
    if (Platform::IsUserInterfaceDisabled())
    {
        KeyMapByteArray keysPressed;
        GetKeys((BigEndianUInt32*)keysPressed);

        // Process one keypress if the bitmap of pressed keys has changed.
        //
        if (memcmp(s_KeysPressed, keysPressed, sizeof(keysPressed)) != 0)
        {
            const Console::VirtualKey *virtualKeyTable;
            Console::VirtualKey  virtualKey = VKC_NONE;
            int             numKeysPressed = 0;

            if (keysPressed[MODIFIER_INDEX] & CTRL_MASK)
                virtualKeyTable = s_KeyMapForCtrlKeycode;
            else if (keysPressed[MODIFIER_INDEX] & SHIFT_MASK)
                virtualKeyTable = s_KeyMapForShiftKeycode;
            else
                virtualKeyTable = s_KeyMapForKeycode;

            for (int keycode = 0; keycode <= 0x7f; keycode++)
            {
                if ( (keysPressed[keycode / 8] & (1 << (keycode % 8))) &&
                     (virtualKeyTable[keycode] != VKC_NONE) )
                {
                    virtualKey = virtualKeyTable[keycode];
                    numKeysPressed++;
                }
            }

            memcpy(s_KeysPressed, keysPressed, sizeof(keysPressed));
            if (numKeysPressed == 1)
                s_KeypressQueue.push_back(virtualKey);
        }
    }
#endif // INCLUDE_GPU

    // Check for an escape sequence
    Console::VirtualKey key = HandleEscapeSequence();

    // If there was no escape sequence, return the current key
    if ((key == Console::VKC_NONE) && !s_KeypressQueue.empty())
    {
        key = s_KeypressQueue.front();
        s_KeypressQueue.pop_front();
    }

    return key;
}
/* virtual */  bool RawConsole::KeyboardHit()
{
    if (!s_KeypressQueue.empty())
        return true;

    if (PrivKeyboardHit())
        return true;

#ifdef INCLUDE_GPU
    if (Platform::IsUserInterfaceDisabled())
    {
        KeyMapByteArray keysPressed;
        GetKeys((BigEndianUInt32*)keysPressed);

        if (memcmp(s_KeysPressed, keysPressed, sizeof(keysPressed)) != 0)
        {
            return true;
        }
    }
#endif // INCLUDE_GPU

    return false;
}

//-----------------------------------------------------------------------------
//! \brief Blocking call to get a character from stdin
//!
//! \param bYield should be set to true if the routine should yield while
//!               waiting for a character, before reading the character
//!
//! \return the value read from stdin
/* virtual */ INT32 RawConsole::GetChar(bool bYield)
{
    MASSERT(!"RawConsole::GetChar not implemented on Mac\n");
    return 0;
}

//-----------------------------------------------------------------------------
//! \brief Blocking call to flush all characters from stdin into a queue
//!
//! \param pKeyQueue is a pointer to the key queue push keys into
//! \param RawKeyMap is the translation keymap to use to colwert the raw input
//!        before pushing the values into the queue
//!
/* virtual */ void RawConsole::FlushChars
(
    deque<Console::VirtualKey> *pKeyQueue,
    const Console::VirtualKey *RawKeyMap
)
{
    INT32 c;
    timeval tv;
    fd_set files;
    FD_ZERO(&files);
    FD_SET(0, &files);    // Stream 0 is stdin

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    while (select(1, &files, NULL, NULL, &tv))
    {
        c = getc(stdin);
        if (c >= 0 && c <= 0x7f)
            pKeyQueue->push_back(RawKeyMap[c]);
    }
}

//-----------------------------------------------------------------------------
//! \brief Handle an escape key sequence
//!
//! \return the virtual key representing the escape sequence
/* virtual */ Console::VirtualKey RawConsole::HandleEscapeSequence()
{
    // If the keypresses at the start of s_KeypressQueue represents an
    // escape sequence, remove all keys that form the escape sequence and
    // return the virtual key.
    //
    for (int ii = 0; ii < NUM_ESCAPE_SEQUENCES; ii++)
    {
        const EscapeSequence &seq = s_EscapeSequences[ii];
        if (seq.size <= s_KeypressQueue.size() &&
            equal(&seq.keys[0], &seq.keys[seq.size], s_KeypressQueue.begin()))
        {
            for (unsigned int jj = 0; jj < seq.size; jj++)
                s_KeypressQueue.pop_front();
            return seq.outKey;
        }
    }

    return Console::VKC_NONE;
}

//-----------------------------------------------------------------------------
//! \brief Private function to determie whether the keyboard has been hit, may
//!        be used to implement keyboard hit functionality that is specific to
//!        a particular subclass of RawConsole
//!
//! \return true if the keyboard has been hit, false otherwise
/* virtual */ bool RawConsole::PrivKeyboardHit()
{
    timeval tv;
    fd_set files;
    FD_ZERO(&files);
    FD_SET(0, &files);    // Stream 0 is stdin

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    return select(1, &files, NULL, NULL, &tv) ? true : false;
}

//-----------------------------------------------------------------------------
//! Create a RawConsole and register it with ConsoleManager.
//! ConsoleManager owns the pointer so the console stays around during static
//! destruction
static ConsoleFactory s_RawConsole(RawConsole::CreateRawConsole, "raw", true);
