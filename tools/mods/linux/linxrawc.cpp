/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! @file    linxrawc.cpp
//! @brief   Implement cross platform portion of RawConsole
//!
//! For detailed comments on each of the public interfaces, see console.h

#include "core/include/rawcnsl.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"
#include "core/include/massert.h"
#include "core/include/cnslmgr.h"
#include "core/include/regexp.h"
#include "linxrawc.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include <atomic>
#include <string>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <deque>
#include <string.h>

//-----------------------------------------------------------------------------
//! Colwersion keymap to colwert raw keys from K_Home to K_Delete to a
//! Console::VirtualKey enumeration
static Console::VirtualKey s_KeyMap1[] =
{
    Console::VKC_NONE                 //
   ,Console::VKC_HOME                 // K_Home                 0x001
   ,Console::VKC_CONTROL_B            // K_Control_B            0x002
   ,Console::VKC_CONTROL_C            // K_Control_C            0x003
   ,Console::VKC_DELETE               // K_Control_D            0x004
   ,Console::VKC_END                  // K_Control_E            0x005
   ,Console::VKC_CONTROL_F            // K_Control_F            0x006
   ,Console::VKC_CONTROL_G            // K_Control_G            0x007
   ,Console::VKC_BACKSPACE            // K_Control_H            0x008
   ,Console::VKC_TAB                  // K_Tab                  0x009
   ,Console::VKC_RETURN               // K_Return               0x00a // Enter Key
   ,Console::VKC_CONTROL_K            // K_Control_K            0x00b
   ,Console::VKC_NONE                 // None                   0x00c
   ,Console::VKC_RETURN               // K_Return               0x00d
   ,Console::VKC_CONTROL_N            // K_Control_N            0x00e
   ,Console::VKC_CONTROL_O            // K_Control_O            0x00f
   ,Console::VKC_CONTROL_P            // K_Control_P            0x010
   ,Console::VKC_CONTROL_Q            // K_Control_Q            0x011
   ,Console::VKC_CONTROL_R            // K_Control_R            0x012
   ,Console::VKC_CONTROL_S            // K_Control_S            0x013
   ,Console::VKC_CONTROL_T            // K_Control_T            0x014
   ,Console::VKC_CONTROL_U            // K_Control_U            0x015
   ,Console::VKC_CONTROL_V            // K_Control_V            0x016
   ,Console::VKC_CONTROL_W            // K_Control_W            0x017
   ,Console::VKC_CONTROL_X            // K_Control_X            0x018
   ,Console::VKC_CONTROL_Y            // K_Control_Y            0x019
   ,Console::VKC_CONTROL_Z            // K_Control_Z            0x01a
   ,Console::VKC_ESCAPE               // K_Escape               0x01b
   ,Console::VKC_NONE                 // NONE                   0x01c
   ,Console::VKC_NONE                 // NONE                   0x01d
   ,Console::VKC_NONE                 // NONE                   0x01e
   ,Console::VKC_NONE                 // NONE                   0x01f
   ,Console::VKC_SPACE                // K_Space                0x020
   ,Console::VKC_EXCLAMATIONPOINT     // K_ExclamationPoint     0x021
   ,Console::VKC_DOUBLEQUOTE          // K_DoubleQuote          0x022
   ,Console::VKC_HASH                 // K_Hash                 0x023
   ,Console::VKC_DOLLAR               // K_Dollar               0x024
   ,Console::VKC_PERCENT              // K_Percent              0x025
   ,Console::VKC_AMPERSAND            // K_Ampersand            0x026
   ,Console::VKC_QUOTE                // K_Quote                0x027
   ,Console::VKC_LPAREN               // K_LParen               0x028
   ,Console::VKC_RPAREN               // K_RParen               0x029
   ,Console::VKC_STAR                 // K_Star                 0x02a
   ,Console::VKC_PLUS                 // K_Plus                 0x02b
   ,Console::VKC_COMMA                // K_Comma                0x02c
   ,Console::VKC_DASH                 // K_Dash                 0x02d
   ,Console::VKC_PERIOD               // K_Period               0x02e
   ,Console::VKC_SLASH                // K_Slash                0x02f
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
   ,Console::VKC_COLON                // K_Colon                0x03a
   ,Console::VKC_SEMICOLON            // K_SemiColon            0x03b
   ,Console::VKC_LANGLE               // K_LAngle               0x03c
   ,Console::VKC_EQUALS               // K_Equals               0x03d
   ,Console::VKC_RANGLE               // K_RAngle               0x03e
   ,Console::VKC_QUESTIONMARK         // K_QuestionMark         0x03f
   ,Console::VKC_AT                   // K_At                   0x040
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
   ,Console::VKC_LBRACKET             // K_LBracket             0x05b
   ,Console::VKC_BACKSLASH            // K_BackSlash            0x05c
   ,Console::VKC_RBRACKET             // K_RBracket             0x05d
   ,Console::VKC_CARET                // K_Caret                0x05e
   ,Console::VKC_UNDERSCORE           // K_UnderScore           0x05f
   ,Console::VKC_BACKQUOTE            // K_BackQuote            0x060
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
   ,Console::VKC_LBRACE               // K_LBrace               0x07b
   ,Console::VKC_PIPE                 // K_Pipe                 0x07c
   ,Console::VKC_RBRACE               // K_RBrace               0x07d
   ,Console::VKC_TILDE                // K_Tilde                0x07e
   ,Console::VKC_BACKSPACE            // K_Delete               0x07f
};

//-----------------------------------------------------------------------------
//! Colwersion keymap to colwert raw keys from K_Alt_Escape to K_Alt_Enter to a
//! Console::VirtualKey enumeration
static Console::VirtualKey s_KeyMap2[] =
{
    Console::VKC_ALT_ESCAPE           // K_Alt_Escape           0x101
   ,Console::VKC_NONE                 //                        0x102
   ,Console::VKC_CONTROL_AT           // K_Control_At           0x103
   ,Console::VKC_NONE                 //                        0x104
   ,Console::VKC_NONE                 //                        0x105
   ,Console::VKC_NONE                 //                        0x106
   ,Console::VKC_NONE                 //                        0x107
   ,Console::VKC_NONE                 //                        0x108
   ,Console::VKC_NONE                 //                        0x109
   ,Console::VKC_NONE                 //                        0x10a
   ,Console::VKC_NONE                 //                        0x10b
   ,Console::VKC_NONE                 //                        0x10c
   ,Console::VKC_NONE                 //                        0x10d
   ,Console::VKC_ALT_BACKSPACE        // K_Alt_Backspace        0x10e
   ,Console::VKC_BACKTAB              // K_BackTab              0x10f
   ,Console::VKC_ALT_Q                // K_Alt_Q                0x110
   ,Console::VKC_ALT_W                // K_Alt_W                0x111
   ,Console::VKC_ALT_E                // K_Alt_E                0x112
   ,Console::VKC_ALT_R                // K_Alt_R                0x113
   ,Console::VKC_ALT_T                // K_Alt_T                0x114
   ,Console::VKC_ALT_Y                // K_Alt_Y                0x115
   ,Console::VKC_ALT_U                // K_Alt_U                0x116
   ,Console::VKC_ALT_I                // K_Alt_I                0x117
   ,Console::VKC_ALT_O                // K_Alt_O                0x118
   ,Console::VKC_ALT_P                // K_Alt_P                0x119
   ,Console::VKC_ALT_LBRACKET         // K_Alt_LBracket         0x11a
   ,Console::VKC_ALT_RBRACKET         // K_Alt_RBracket         0x11b
   ,Console::VKC_ALT_RETURN           // K_Alt_Return           0x11c
   ,Console::VKC_NONE                 //                        0x11d
   ,Console::VKC_ALT_A                // K_Alt_A                0x11e
   ,Console::VKC_ALT_S                // K_Alt_S                0x11f
   ,Console::VKC_ALT_D                // K_Alt_D                0x120
   ,Console::VKC_ALT_F                // K_Alt_F                0x121
   ,Console::VKC_ALT_G                // K_Alt_G                0x122
   ,Console::VKC_ALT_H                // K_Alt_H                0x123
   ,Console::VKC_ALT_J                // K_Alt_J                0x124
   ,Console::VKC_ALT_K                // K_Alt_K                0x125
   ,Console::VKC_ALT_L                // K_Alt_L                0x126
   ,Console::VKC_ALT_SEMICOLON        // K_Alt_Semicolon        0x127
   ,Console::VKC_ALT_QUOTE            // K_Alt_Quote            0x128
   ,Console::VKC_ALT_BACKQUOTE        // K_Alt_Backquote        0x129
   ,Console::VKC_NONE                 //                        0x12a
   ,Console::VKC_ALT_BACKSLASH        // K_Alt_Backslash        0x12b
   ,Console::VKC_ALT_Z                // K_Alt_Z                0x12c
   ,Console::VKC_ALT_X                // K_Alt_X                0x12d
   ,Console::VKC_ALT_C                // K_Alt_C                0x12e
   ,Console::VKC_ALT_V                // K_Alt_V                0x12f
   ,Console::VKC_ALT_B                // K_Alt_B                0x130
   ,Console::VKC_ALT_N                // K_Alt_N                0x131
   ,Console::VKC_ALT_M                // K_Alt_M                0x132
   ,Console::VKC_ALT_COMMA            // K_Alt_Comma            0x133
   ,Console::VKC_ALT_PERIOD           // K_Alt_Period           0x134
   ,Console::VKC_ALT_SLASH            // K_Alt_Slash            0x135
   ,Console::VKC_NONE                 //                        0x136
   ,Console::VKC_ALT_KPSTAR           // K_Alt_KPStar           0x137
   ,Console::VKC_NONE                 //                        0x138
   ,Console::VKC_NONE                 //                        0x139
   ,Console::VKC_NONE                 //                        0x13a
   ,Console::VKC_F1                   // K_F1                   0x13b
   ,Console::VKC_F2                   // K_F2                   0x13c
   ,Console::VKC_F3                   // K_F3                   0x13d
   ,Console::VKC_F4                   // K_F4                   0x13e
   ,Console::VKC_F5                   // K_F5                   0x13f
   ,Console::VKC_F6                   // K_F6                   0x140
   ,Console::VKC_F7                   // K_F7                   0x141
   ,Console::VKC_F8                   // K_F8                   0x142
   ,Console::VKC_F9                   // K_F9                   0x143
   ,Console::VKC_F10                  // K_F10                  0x144
   ,Console::VKC_NONE                 //                        0x145
   ,Console::VKC_NONE                 //                        0x146
   ,Console::VKC_HOME                 // K_Home                 0x147
   ,Console::VKC_UP                   // K_Up                   0x148
   ,Console::VKC_PAGEUP               // K_PageUp               0x149
   ,Console::VKC_ALT_KPMINUS          // K_Alt_KPMinus          0x14a
   ,Console::VKC_LEFT                 // K_Left                 0x14b
   ,Console::VKC_CENTER               // K_Center               0x14c
   ,Console::VKC_RIGHT                // K_Right                0x14d
   ,Console::VKC_ALT_KPPLUS           // K_Alt_KPPlus           0x14e
   ,Console::VKC_END                  // K_End                  0x14f
   ,Console::VKC_DOWN                 // K_Down                 0x150
   ,Console::VKC_PAGEDOWN             // K_PageDown             0x151
   ,Console::VKC_INSERT               // K_Insert               0x152
   ,Console::VKC_DELETE               // K_Delete               0x153
   ,Console::VKC_SHIFT_F1             // K_Shift_F1             0x154
   ,Console::VKC_SHIFT_F2             // K_Shift_F2             0x155
   ,Console::VKC_SHIFT_F3             // K_Shift_F3             0x156
   ,Console::VKC_SHIFT_F4             // K_Shift_F4             0x157
   ,Console::VKC_SHIFT_F5             // K_Shift_F5             0x158
   ,Console::VKC_SHIFT_F6             // K_Shift_F6             0x159
   ,Console::VKC_SHIFT_F7             // K_Shift_F7             0x15a
   ,Console::VKC_SHIFT_F8             // K_Shift_F8             0x15b
   ,Console::VKC_SHIFT_F9             // K_Shift_F9             0x15c
   ,Console::VKC_SHIFT_F10            // K_Shift_F10            0x15d
   ,Console::VKC_CONTROL_F1           // K_Control_F1           0x15e
   ,Console::VKC_CONTROL_F2           // K_Control_F2           0x15f
   ,Console::VKC_CONTROL_F3           // K_Control_F3           0x160
   ,Console::VKC_CONTROL_F4           // K_Control_F4           0x161
   ,Console::VKC_CONTROL_F5           // K_Control_F5           0x162
   ,Console::VKC_CONTROL_F6           // K_Control_F6           0x163
   ,Console::VKC_CONTROL_F7           // K_Control_F7           0x164
   ,Console::VKC_CONTROL_F8           // K_Control_F8           0x165
   ,Console::VKC_CONTROL_F9           // K_Control_F9           0x166
   ,Console::VKC_CONTROL_F10          // K_Control_F10          0x167
   ,Console::VKC_NONE                 // NONE                   0x168
   ,Console::VKC_ALT_F2               // K_Alt_F2               0x169
   ,Console::VKC_ALT_F3               // K_Alt_F3               0x16a
   ,Console::VKC_ALT_F4               // K_Alt_F4               0x16b
   ,Console::VKC_ALT_F5               // K_Alt_F5               0x16c
   ,Console::VKC_ALT_F6               // K_Alt_F6               0x16d
   ,Console::VKC_ALT_F7               // K_Alt_F7               0x16e
   ,Console::VKC_ALT_F8               // K_Alt_F8               0x16f
   ,Console::VKC_ALT_F9               // K_Alt_F9               0x170
   ,Console::VKC_ALT_F10              // K_Alt_F10              0x171
   ,Console::VKC_CONTROL_PRINT        // K_Control_Print         0x172
   ,Console::VKC_CONTROL_LEFT         // K_Control_Left         0x173
   ,Console::VKC_CONTROL_RIGHT        // K_Control_Right        0x174
   ,Console::VKC_CONTROL_END          // K_Control_End          0x175
   ,Console::VKC_CONTROL_PAGEDOWN     // K_Control_PageDown     0x176
   ,Console::VKC_CONTROL_HOME         // K_Control_Home         0x177
   ,Console::VKC_ALT_1                // K_Alt_1                0x178
   ,Console::VKC_ALT_2                // K_Alt_2                0x179
   ,Console::VKC_ALT_3                // K_Alt_3                0x17a
   ,Console::VKC_ALT_4                // K_Alt_4                0x17b
   ,Console::VKC_ALT_5                // K_Alt_5                0x17c
   ,Console::VKC_ALT_6                // K_Alt_6                0x17d
   ,Console::VKC_ALT_7                // K_Alt_7                0x17e
   ,Console::VKC_ALT_8                // K_Alt_8                0x17f
   ,Console::VKC_ALT_9                // K_Alt_9                0x180
   ,Console::VKC_ALT_0                // K_Alt_0                0x181
   ,Console::VKC_ALT_DASH             // K_Alt_Dash             0x182
   ,Console::VKC_ALT_EQUALS           // K_Alt_Equals           0x183
   ,Console::VKC_CONTROL_PAGEUP       // K_Control_PageUp       0x184
   ,Console::VKC_F11                  // K_F11                  0x185
   ,Console::VKC_F12                  // K_F12                  0x186
   ,Console::VKC_SHIFT_F11            // K_Shift_F11            0x187
   ,Console::VKC_SHIFT_F12            // K_Shift_F12            0x188
   ,Console::VKC_CONTROL_F11          // K_Control_F11          0x189
   ,Console::VKC_CONTROL_F12          // K_Control_F12          0x18a
   ,Console::VKC_ALT_F11              // K_Alt_F11              0x18b
   ,Console::VKC_ALT_F12              // K_Alt_F12              0x18c
   ,Console::VKC_CONTROL_UP           // K_Control_Up           0x18d
   ,Console::VKC_CONTROL_KPDASH       // K_Control_KPDash       0x18e
   ,Console::VKC_CONTROL_CENTER       // K_Control_Center       0x18f
   ,Console::VKC_CONTROL_KPPLUS       // K_Control_KPPlus       0x190
   ,Console::VKC_CONTROL_DOWN         // K_Control_Down         0x191
   ,Console::VKC_CONTROL_INSERT       // K_Control_Insert       0x192
   ,Console::VKC_CONTROL_DELETE       // K_Control_Delete       0x193
   ,Console::VKC_NONE                 //                        0x194
   ,Console::VKC_CONTROL_KPSLASH      // K_Control_KPSlash      0x195
   ,Console::VKC_CONTROL_KPSTAR       // K_Control_KPStar       0x196
   ,Console::VKC_ALT_EHOME            // K_Alt_EHome            0x197
   ,Console::VKC_ALT_EUP              // K_Alt_EUp              0x198
   ,Console::VKC_ALT_EPAGEUP          // K_Alt_EPageUp          0x199
   ,Console::VKC_NONE                 //                        0x19a
   ,Console::VKC_ALT_ELEFT            // K_Alt_ELeft            0x19b
   ,Console::VKC_NONE                 //                        0x19c
   ,Console::VKC_ALT_ERIGHT           // K_Alt_ERight           0x19d
   ,Console::VKC_NONE                 //                        0x19e
   ,Console::VKC_ALT_EEND             // K_Alt_EEnd             0x19f
   ,Console::VKC_ALT_EDOWN            // K_Alt_EDown            0x1a0
   ,Console::VKC_ALT_EPAGEDOWN        // K_Alt_EPageDown        0x1a1
   ,Console::VKC_ALT_EINSERT          // K_Alt_EInsert          0x1a2
   ,Console::VKC_ALT_EDELETE          // K_Alt_EDelete          0x1a3
   ,Console::VKC_ALT_KPSLASH          // K_Alt_KPSlash          0x1a4
   ,Console::VKC_ALT_TAB              // K_Alt_Tab              0x1a5
   ,Console::VKC_ALT_ENTER            // K_Alt_Enter            0x1a6
};

//-----------------------------------------------------------------------------
//! Colwersion keymap to colwert raw keys from K_EHome to K_Control_EDelete to
//! a Console::VirtualKey enumeration
static Console::VirtualKey s_KeyMap3[] =
{
    Console::VKC_EHOME                // K_EHome                0x247
   ,Console::VKC_EUP                  // K_EUp                  0x248
   ,Console::VKC_EPAGEUP              // K_EPageUp              0x249
   ,Console::VKC_NONE                 //                        0x24a
   ,Console::VKC_ELEFT                // K_ELeft                0x24b
   ,Console::VKC_NONE                 //                        0x24c
   ,Console::VKC_ERIGHT               // K_ERight               0x24d
   ,Console::VKC_NONE                 //                        0x24e
   ,Console::VKC_EEND                 // K_EEnd                 0x24f
   ,Console::VKC_EDOWN                // K_EDown                0x250
   ,Console::VKC_EPAGEDOWN            // K_EPageDown            0x251
   ,Console::VKC_EINSERT              // K_EInsert              0x252
   ,Console::VKC_EDELETE              // K_EDelete              0x253
   ,Console::VKC_NONE                 //                        0x254
   ,Console::VKC_NONE                 //                        0x255
   ,Console::VKC_NONE                 //                        0x256
   ,Console::VKC_NONE                 //                        0x257
   ,Console::VKC_NONE                 //                        0x258
   ,Console::VKC_NONE                 //                        0x259
   ,Console::VKC_NONE                 //                        0x25a
   ,Console::VKC_NONE                 //                        0x25b
   ,Console::VKC_NONE                 //                        0x25c
   ,Console::VKC_NONE                 //                        0x25d
   ,Console::VKC_NONE                 //                        0x25e
   ,Console::VKC_NONE                 //                        0x25f
   ,Console::VKC_NONE                 //                        0x260
   ,Console::VKC_NONE                 //                        0x261
   ,Console::VKC_NONE                 //                        0x262
   ,Console::VKC_NONE                 //                        0x263
   ,Console::VKC_NONE                 //                        0x264
   ,Console::VKC_NONE                 //                        0x265
   ,Console::VKC_NONE                 //                        0x266
   ,Console::VKC_NONE                 //                        0x267
   ,Console::VKC_NONE                 //                        0x268
   ,Console::VKC_NONE                 //                        0x269
   ,Console::VKC_NONE                 //                        0x26a
   ,Console::VKC_NONE                 //                        0x26b
   ,Console::VKC_NONE                 //                        0x26c
   ,Console::VKC_NONE                 //                        0x26d
   ,Console::VKC_NONE                 //                        0x26e
   ,Console::VKC_NONE                 //                        0x26f
   ,Console::VKC_NONE                 //                        0x270
   ,Console::VKC_NONE                 //                        0x271
   ,Console::VKC_NONE                 //                        0x272
   ,Console::VKC_CONTROL_ELEFT        // K_Control_ELeft        0x273
   ,Console::VKC_CONTROL_ERIGHT       // K_Control_ERight       0x274
   ,Console::VKC_CONTROL_EEND         // K_Control_EEnd         0x275
   ,Console::VKC_CONTROL_EPAGEDOWN    // K_Control_EPageDown    0x276
   ,Console::VKC_CONTROL_EHOME        // K_Control_EHome        0x277
   ,Console::VKC_NONE                 //                        0x278
   ,Console::VKC_NONE                 //                        0x279
   ,Console::VKC_NONE                 //                        0x27a
   ,Console::VKC_NONE                 //                        0x27b
   ,Console::VKC_NONE                 //                        0x27c
   ,Console::VKC_NONE                 //                        0x27d
   ,Console::VKC_NONE                 //                        0x27e
   ,Console::VKC_NONE                 //                        0x27f
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x280
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x281
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x282
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x283
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x284
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x285
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x286
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x287
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x288
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x289
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x28a
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x28b
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x28c
   ,Console::VKC_CONTROL_EUP          // K_Control_EUp          0x28d
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x28e
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x28f
   ,Console::VKC_CONTROL_EPAGEUP      // K_Control_EPageUp      0x290
   ,Console::VKC_CONTROL_EDOWN        // K_Control_EDown        0x291
   ,Console::VKC_CONTROL_EINSERT      // K_Control_EInsert      0x292
   ,Console::VKC_CONTROL_EDELETE      // K_Control_EDelete      0x293
};

//! Structure used to define an escape sequence
//!
struct EscapeSequence
{
    Console::VirtualKey outKey;   // Key represented by the esc. seq.
    UINT32              size;     // Size of the escape sequence
    UINT32              keyCount; // Number of keys that need to be read
                                  // by the escape sequence
    Console::VirtualKey keys[4];  // Keys in the escape sequence
};

//! Table of escape sequences.  Used by RawConsole::HandleEscapeSequence().
//!
//! This table MUST be sorted by the keyCount
static const EscapeSequence s_EscapeSequences[] =
{
    {Console::VKC_UP,             3, 3,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_A}},
    {Console::VKC_DOWN,           3, 3,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_B}},
    {Console::VKC_RIGHT,          3, 3,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_C}},
    {Console::VKC_LEFT,           3, 3,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_D}},
    {Console::VKC_CONTROL_UP,     3, 3,
        {Console::VKC_ESCAPE, Console::VKC_CAPITAL_O, Console::VKC_CAPITAL_A}},
    {Console::VKC_CONTROL_DOWN,   3, 3,
        {Console::VKC_ESCAPE, Console::VKC_CAPITAL_O, Console::VKC_CAPITAL_B}},
    {Console::VKC_END,            3, 3,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_F}},
    {Console::VKC_END,            3, 3,
        {Console::VKC_ESCAPE, Console::VKC_CAPITAL_O, Console::VKC_CAPITAL_F}},
    {Console::VKC_HOME,           3, 3,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_CAPITAL_H}},
    {Console::VKC_HOME,           3, 3,
        {Console::VKC_ESCAPE, Console::VKC_CAPITAL_O, Console::VKC_CAPITAL_H}},
    {Console::VKC_ALT_F1,         3, 3,
        {Console::VKC_ESCAPE, Console::VKC_CAPITAL_O, Console::VKC_CAPITAL_P}},
    {Console::VKC_INSERT,         3, 4,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_2}},
    {Console::VKC_DELETE,         3, 4,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_3}},
    {Console::VKC_END,            3, 4,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_4}},
    {Console::VKC_PAGEUP,         3, 4,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_5}},
    {Console::VKC_PAGEDOWN,       3, 4,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_6}},
    {Console::VKC_HOME,           4, 4,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_1,
                                                          Console::VKC_TILDE}},
    {Console::VKC_F5,             4, 5,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_1,
                                                          Console::VKC_5}},
    {Console::VKC_F6,             4, 5,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_1,
                                                          Console::VKC_7}},
    {Console::VKC_F7,             4, 5,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_1,
                                                          Console::VKC_8}},
    {Console::VKC_F8,             4, 5,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_1,
                                                          Console::VKC_9}},
    {Console::VKC_F9,             4, 5,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_2,
                                                          Console::VKC_0}},
    {Console::VKC_F6,             3, 5,
        {Console::VKC_ESCAPE, Console::VKC_LBRACKET, Console::VKC_1}}
};

//! Number of availabe escape sequences.
//!
static const int NUM_ESCAPE_SEQUENCES =
        sizeof(s_EscapeSequences) / sizeof(EscapeSequence);

namespace
{
    atomic<UINT32> s_EchoDisableCount{0};
    struct termios s_SavedStdinSettings;
}

void RawConsole::EnterSingleCharMode(bool echo)
{
    if ((s_EchoDisableCount++ == 0) && isatty(fileno(stdin)))
    {
        // Disable canonical mode and echo
        struct termios newSettings;
        tcgetattr(fileno(stdin), &s_SavedStdinSettings);
        newSettings = s_SavedStdinSettings;
        newSettings.c_lflag &= ~ICANON;
        if (!echo)
        {
            newSettings.c_lflag &= ~ECHO;
        }
        tcsetattr(fileno(stdin), TCSANOW, &newSettings);
    }
}

void RawConsole::ExitSingleCharMode()
{
    if ((--s_EchoDisableCount == 0) && isatty(fileno(stdin)))
    {
        tcsetattr(fileno(stdin), TCSANOW, &s_SavedStdinSettings);
    }
}

/* virtual */  Console::VirtualKey RawConsole::GetKey()
{
    EnterSingleCharMode(false);
    DEFER
    {
        ExitSingleCharMode();
    };

    INT32 Key = GetChar(true);

    // Handle special case (escape sequence)
    if (Key == Console::VKC_ESCAPE)
    {
       return HandleEscapeSequence();
    }

    // Map the key to a VirtualKey.
    if ((0x001 <= Key) && (Key <= 0x07F))
       return s_KeyMap1[Key];

    if ((0x101 <= Key) && (Key <= 0x1a6))
       return s_KeyMap2[Key - 0x101];

    if ((0x247 <= Key) && (Key <= 0x293))
       return s_KeyMap3[Key - 0x247];

    // We should never reach this code.
    Printf(Tee::PriNormal, "Error: Unknown key code 0x%x\n", Key);
    MASSERT(false);
    return Console::VKC_NONE;
}

/* virtual */  bool RawConsole::KeyboardHit()
{
    EnterSingleCharMode(false);
    const bool hit = PrivKeyboardHit();
    ExitSingleCharMode();
    return hit;
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
    if (bYield)
    {
        INT32 Key;
        while(true)
        {
            if(KeyboardHit())
            {
                Key = getc(stdin);

                // For some reason, we get EOF a few times if we're running
                // alongside GFSDK
                if (EOF != Key)
                {
                   break;
                }
            }
            Tasker::Yield();
        }
        return Key;
    }

    return getc(stdin);
}

//-----------------------------------------------------------------------------
//! \brief Blocking call to flush all characters from stdin into a queue
//!
//! \param pKeyQueue is a pointer to the key queue push keys into
//! \param RawKeyMap is the translation keymap to use to colwert the raw input
//!        before pushing the values into the queue
//!
/* virtual */ void  RawConsole::FlushChars
(
    deque<Console::VirtualKey> *pKeyQueue,
    const Console::VirtualKey *RawKeyMap
)
{
    MASSERT(!"RawConsole::FlushChars not implemented on Linux\n");
}

//-----------------------------------------------------------------------------
//! \brief Handle an escape key sequence
//!
//! \return the virtual key representing the escape sequence
/* virtual */ Console::VirtualKey RawConsole::HandleEscapeSequence()
{
    deque<Console::VirtualKey> escSeq;

    // Retrieve enough characters initially to identify an escape sequence
    escSeq.push_back(Console::VKC_ESCAPE);
    escSeq.push_back((Console::VirtualKey)GetChar(false));
    escSeq.push_back((Console::VirtualKey)GetChar(false));

    for (INT32 ii = 0; ii < NUM_ESCAPE_SEQUENCES; ii++)
    {
        const EscapeSequence &seq = s_EscapeSequences[ii];
        UINT32 compSize = (seq.size < escSeq.size()) ? seq.size : escSeq.size();

        // If the first 3 keys (or however many keys are in the sequence,
        // whichever is less) are equal then retrieve enough characters
        // to do a full sequence comparison.
        if (equal(&seq.keys[0], &seq.keys[compSize], escSeq.begin()))
        {
            while (seq.keyCount > escSeq.size())
            {
                escSeq.push_back((Console::VirtualKey)GetChar(false));
            }
        }
        if (equal(&seq.keys[0], &seq.keys[seq.size], escSeq.begin()))
        {
            return seq.outKey;
        }
    }

    return Console::VKC_ESCAPE;
}

//-----------------------------------------------------------------------------
//! \brief Private function to determie whether the keyboard has been hit, may
//!        be used to implement keyboard hit functionality that is specific to
//!        a particular subclass of RawConsole
//!
//! \return true if the keyboard has been hit, false otherwise
/* virtual */ bool RawConsole::PrivKeyboardHit()
{
    int ready = 0;
    timeval tv;
    fd_set files;
    FD_ZERO(&files);
    // Stream 0 is stdin
    FD_SET(0, &files);

    tv.tv_sec = 0;
    tv.tv_usec = 0;

    ready = select(1, &files, NULL, NULL, &tv);

    return ready ? true : false;
}

bool LinuxRawConsole::DetectColorTerm()
{
    static const char* const supportedColorTerms[] =
    {
        // This list has been obtained from:
        // $ dircolors --print-database | grep TERM
        "Eterm", "ansi", "color-xterm", "con[0-9]*x[0-9]*", "cons25", "console", "cygwin",
        "dtterm", "eterm-color", "gnome", "gnome-256color", "hurd", "jfbterm", "konsole", "kterm",
        "linux", "linux-c", "mach-color", "mach-gnu-color", "mlterm", "putty", "putty-256color",
        "rxvt.*", "screen.*", "st", "st-256color", "terminator", "tmux.*", "vt100", "xterm.*"
    };

    const string term = Xp::GetElw("TERM");

    for (const char* regexStr : supportedColorTerms)
    {
        RegExp termRe;
        if ((termRe.Set(regexStr, RegExp::FULL) == RC::OK) && termRe.Matches(term))
        {
            return true;
        }
    }

    return false;
}

/* virtual */ RC LinuxRawConsole::Enable(ConsoleBuffer* pConsoleBuffer)
{
    m_Enabled = true;
    return OK;
}

/* virtual */ RC LinuxRawConsole::Disable()
{
    m_Enabled = false;
    return OK;
}

/* virtual */ bool LinuxRawConsole::IsEnabled() const
{
    return m_Enabled;
}

/* virtual */  void LinuxRawConsole::PrintStringToScreen
(
    const char*           str,
    size_t                strLen,
    Tee::ScreenPrintState state
)
{
    if (!Platform::IsUserInterfaceDisabled() &&
        (state != Tee::SPS_NORMAL))
    {
        const char*      colorEscape    = nullptr;
        constexpr size_t colorEscapeLen = 10;
        const char*      colorOff       = nullptr;
        size_t           colorOffLen    = 0;
        const char*      colorOffEol    = "\n";
        size_t           colorOffEolLen = 1;
        if (IsColorTerm())
        {
            switch (state)
            {
                case Tee::SPS_WARNING:
                    colorEscape = "\033[0;30;43m"; break; // black on orange
                case Tee::SPS_FAIL:
                    colorEscape = "\033[1;39;41m"; break; // white on red
                case Tee::SPS_PASS:
                    colorEscape = "\033[1;39;42m"; break; // white on green
                case Tee::SPS_HIGHLIGHT:
                    colorEscape = "\033[0;30;46m"; break; // black on cyan
                case Tee::SPS_HIGH:
                    colorEscape = "\033[0;30;43m"; break; // black on orange
                case Tee::SPS_BOTH:
                    colorEscape = "\033[0;30;46m"; break; // black on cyan
                case Tee::SPS_LOW:
                    colorEscape = "\033[1;39;44m"; break; // white on blue
                default:
                    colorEscape = "\033[1;39;45m"; break; // white on magenta
            }
            if (colorEscape)
            {
                colorOff       = "\033[0m";
                colorOffLen    = 4;
                colorOffEol    = "\033[0m\n";
                colorOffEolLen = 5;
            }
        }
        const char* const end = str + strLen;
        while (str < end)
        {
            if (colorEscape)
            {
                if (fwrite(colorEscape, 1, colorEscapeLen, stdout) != colorEscapeLen)
                    return;
            }
            const char* const eol = static_cast<const char*>(memchr(str, '\n', end - str));
            if (eol)
            {
                const size_t len = eol - str;
                if (fwrite(str, 1, len, stdout) != len)
                    return;
                str = eol + 1;
                if (fwrite(colorOffEol, 1, colorOffEolLen, stdout) != colorOffEolLen)
                    return;
            }
            else
            {
                const size_t len = end - str;
                if (fwrite(str, 1, len, stdout) != len)
                    return;
                str = end;
                if (colorOff)
                {
                    if (fwrite(colorOff, 1, colorOffLen, stdout) != colorOffLen)
                        return;
                }
            }
        }
    }
    else
    {
        if (fwrite(str, 1, strLen, stdout) != strLen)
            return;
    }
    fflush(stdout);
}

//-----------------------------------------------------------------------------
//! Create a RawConsole and register it with ConsoleManager.
//! ConsoleManager owns the pointer so the console stays around during static
//! destruction
static Console *CreateLinuxRawConsole() { return new LinuxRawConsole; }
static ConsoleFactory s_RawConsole(CreateLinuxRawConsole, "raw", true);
