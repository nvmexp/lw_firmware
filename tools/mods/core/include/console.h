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

//! @file    console.h
//! @brief   Declare Console -- Abstract Console Class.

#pragma once

#include "rc.h"
#include <string>

class GpuDevice;
class ConsoleBuffer;

//!
//! @class(Console)  Console.
//!
//! This abstract class defines the necessary interfaces for a
//! console object.
//!
//! All consoles should self register with the ConsoleManager
//! when created
class Console
{
public:

    //! Enumeration representing any key that may be received from console
    //! input.
    enum VirtualKey
    {
        // normal or control keys
        VKC_NONE               = 0x000
        ,VKC_CONTROL_A          = 0x001
        ,VKC_CONTROL_B          = 0x002
        ,VKC_CONTROL_C          = 0x003
        ,VKC_CONTROL_D          = 0x004
        ,VKC_CONTROL_E          = 0x005
        ,VKC_CONTROL_F          = 0x006
        ,VKC_CONTROL_G          = 0x007
        ,VKC_BACKSPACE          = 0x008
        //    ,VKC_CONTROL_H          = 0x008
        ,VKC_TAB                = 0x009
        //    ,VKC_CONTROL_I          = 0x009
        ,VKC_LINEFEED           = 0x00a
        //    ,VKC_CONTROL_J          = 0x00a
        ,VKC_CONTROL_K          = 0x00b
        ,VKC_CONTROL_L          = 0x00c
        ,VKC_RETURN             = 0x00d
        //    ,VKC_CONTROL_M          = 0x00d
        ,VKC_CONTROL_N          = 0x00e
        ,VKC_CONTROL_O          = 0x00f
        ,VKC_CONTROL_P          = 0x010
        ,VKC_CONTROL_Q          = 0x011
        ,VKC_CONTROL_R          = 0x012
        ,VKC_CONTROL_S          = 0x013
        ,VKC_CONTROL_T          = 0x014
        ,VKC_CONTROL_U          = 0x015
        ,VKC_CONTROL_V          = 0x016
        ,VKC_CONTROL_W          = 0x017
        ,VKC_CONTROL_X          = 0x018
        ,VKC_CONTROL_Y          = 0x019
        ,VKC_CONTROL_Z          = 0x01a
        //    ,VKC_CONTROL_LBRACKET   = 0x01b
        ,VKC_ESCAPE             = 0x01b
        ,VKC_CONTROL_BACKSLASH  = 0x01c
        ,VKC_CONTROL_RBRACKET   = 0x01d
        ,VKC_CONTROL_CARET      = 0x01e
        ,VKC_CONTROL_UNDERSCORE = 0x01f
        ,VKC_SPACE              = 0x020
        ,VKC_EXCLAMATIONPOINT   = 0x021
        ,VKC_DOUBLEQUOTE        = 0x022
        ,VKC_HASH               = 0x023
        ,VKC_DOLLAR             = 0x024
        ,VKC_PERCENT            = 0x025
        ,VKC_AMPERSAND          = 0x026
        ,VKC_QUOTE              = 0x027
        ,VKC_LPAREN             = 0x028
        ,VKC_RPAREN             = 0x029
        ,VKC_STAR               = 0x02a
        ,VKC_PLUS               = 0x02b
        ,VKC_COMMA              = 0x02c
        ,VKC_DASH               = 0x02d
        ,VKC_PERIOD             = 0x02e
        ,VKC_SLASH              = 0x02f
        ,VKC_0                  = 0x030
        ,VKC_1                  = 0x031
        ,VKC_2                  = 0x032
        ,VKC_3                  = 0x033
        ,VKC_4                  = 0x034
        ,VKC_5                  = 0x035
        ,VKC_6                  = 0x036
        ,VKC_7                  = 0x037
        ,VKC_8                  = 0x038
        ,VKC_9                  = 0x039
        ,VKC_COLON              = 0x03a
        ,VKC_SEMICOLON          = 0x03b
        ,VKC_LANGLE             = 0x03c
        ,VKC_EQUALS             = 0x03d
        ,VKC_RANGLE             = 0x03e
        ,VKC_QUESTIONMARK       = 0x03f
        ,VKC_AT                 = 0x040
        ,VKC_CAPITAL_A          = 0x041
        ,VKC_CAPITAL_B          = 0x042
        ,VKC_CAPITAL_C          = 0x043
        ,VKC_CAPITAL_D          = 0x044
        ,VKC_CAPITAL_E          = 0x045
        ,VKC_CAPITAL_F          = 0x046
        ,VKC_CAPITAL_G          = 0x047
        ,VKC_CAPITAL_H          = 0x048
        ,VKC_CAPITAL_I          = 0x049
        ,VKC_CAPITAL_J          = 0x04a
        ,VKC_CAPITAL_K          = 0x04b
        ,VKC_CAPITAL_L          = 0x04c
        ,VKC_CAPITAL_M          = 0x04d
        ,VKC_CAPITAL_N          = 0x04e
        ,VKC_CAPITAL_O          = 0x04f
        ,VKC_CAPITAL_P          = 0x050
        ,VKC_CAPITAL_Q          = 0x051
        ,VKC_CAPITAL_R          = 0x052
        ,VKC_CAPITAL_S          = 0x053
        ,VKC_CAPITAL_T          = 0x054
        ,VKC_CAPITAL_U          = 0x055
        ,VKC_CAPITAL_V          = 0x056
        ,VKC_CAPITAL_W          = 0x057
        ,VKC_CAPITAL_X          = 0x058
        ,VKC_CAPITAL_Y          = 0x059
        ,VKC_CAPITAL_Z          = 0x05a
        ,VKC_LBRACKET           = 0x05b
        ,VKC_BACKSLASH          = 0x05c
        ,VKC_RBRACKET           = 0x05d
        ,VKC_CARET              = 0x05e
        ,VKC_UNDERSCORE         = 0x05f
        ,VKC_BACKQUOTE          = 0x060
        ,VKC_LOWER_A            = 0x061
        ,VKC_LOWER_B            = 0x062
        ,VKC_LOWER_C            = 0x063
        ,VKC_LOWER_D            = 0x064
        ,VKC_LOWER_E            = 0x065
        ,VKC_LOWER_F            = 0x066
        ,VKC_LOWER_G            = 0x067
        ,VKC_LOWER_H            = 0x068
        ,VKC_LOWER_I            = 0x069
        ,VKC_LOWER_J            = 0x06a
        ,VKC_LOWER_K            = 0x06b
        ,VKC_LOWER_L            = 0x06c
        ,VKC_LOWER_M            = 0x06d
        ,VKC_LOWER_N            = 0x06e
        ,VKC_LOWER_O            = 0x06f
        ,VKC_LOWER_P            = 0x070
        ,VKC_LOWER_Q            = 0x071
        ,VKC_LOWER_R            = 0x072
        ,VKC_LOWER_S            = 0x073
        ,VKC_LOWER_T            = 0x074
        ,VKC_LOWER_U            = 0x075
        ,VKC_LOWER_V            = 0x076
        ,VKC_LOWER_W            = 0x077
        ,VKC_LOWER_X            = 0x078
        ,VKC_LOWER_Y            = 0x079
        ,VKC_LOWER_Z            = 0x07a
        ,VKC_LBRACE             = 0x07b
        ,VKC_PIPE               = 0x07c
        ,VKC_RBRACE             = 0x07d
        ,VKC_TILDE              = 0x07e
        ,VKC_CONTROL_BACKSPACE  = 0x07f
        // alt keys
        ,VKC_ALT_ESCAPE         = 0x101
        ,VKC_CONTROL_AT         = 0x103
        ,VKC_ALT_BACKSPACE      = 0x10e
        ,VKC_BACKTAB            = 0x10f
        ,VKC_ALT_Q              = 0x110
        ,VKC_ALT_W              = 0x111
        ,VKC_ALT_E              = 0x112
        ,VKC_ALT_R              = 0x113
        ,VKC_ALT_T              = 0x114
        ,VKC_ALT_Y              = 0x115
        ,VKC_ALT_U              = 0x116
        ,VKC_ALT_I              = 0x117
        ,VKC_ALT_O              = 0x118
        ,VKC_ALT_P              = 0x119
        ,VKC_ALT_LBRACKET       = 0x11a
        ,VKC_ALT_RBRACKET       = 0x11b
        ,VKC_ALT_RETURN         = 0x11c
        ,VKC_ALT_A              = 0x11e
        ,VKC_ALT_S              = 0x11f
        ,VKC_ALT_D              = 0x120
        ,VKC_ALT_F              = 0x121
        ,VKC_ALT_G              = 0x122
        ,VKC_ALT_H              = 0x123
        ,VKC_ALT_J              = 0x124
        ,VKC_ALT_K              = 0x125
        ,VKC_ALT_L              = 0x126
        ,VKC_ALT_SEMICOLON      = 0x127
        ,VKC_ALT_QUOTE          = 0x128
        ,VKC_ALT_BACKQUOTE      = 0x129
        ,VKC_ALT_BACKSLASH      = 0x12b
        ,VKC_ALT_Z              = 0x12c
        ,VKC_ALT_X              = 0x12d
        ,VKC_ALT_C              = 0x12e
        ,VKC_ALT_V              = 0x12f
        ,VKC_ALT_B              = 0x130
        ,VKC_ALT_N              = 0x131
        ,VKC_ALT_M              = 0x132
        ,VKC_ALT_COMMA          = 0x133
        ,VKC_ALT_PERIOD         = 0x134
        ,VKC_ALT_SLASH          = 0x135
        ,VKC_ALT_KPSTAR         = 0x137
        ,VKC_F1                 = 0x13b
        ,VKC_F2                 = 0x13c
        ,VKC_F3                 = 0x13d
        ,VKC_F4                 = 0x13e
        ,VKC_F5                 = 0x13f
        ,VKC_F6                 = 0x140
        ,VKC_F7                 = 0x141
        ,VKC_F8                 = 0x142
        ,VKC_F9                 = 0x143
        ,VKC_F10                = 0x144
        ,VKC_HOME               = 0x147
        ,VKC_UP                 = 0x148
        ,VKC_PAGEUP             = 0x149
        ,VKC_ALT_KPMINUS        = 0x14a
        ,VKC_LEFT               = 0x14b
        ,VKC_CENTER             = 0x14c
        ,VKC_RIGHT              = 0x14d
        ,VKC_ALT_KPPLUS         = 0x14e
        ,VKC_END                = 0x14f
        ,VKC_DOWN               = 0x150
        ,VKC_PAGEDOWN           = 0x151
        ,VKC_INSERT             = 0x152
        ,VKC_DELETE             = 0x153
        ,VKC_SHIFT_F1           = 0x154
        ,VKC_SHIFT_F2           = 0x155
        ,VKC_SHIFT_F3           = 0x156
        ,VKC_SHIFT_F4           = 0x157
        ,VKC_SHIFT_F5           = 0x158
        ,VKC_SHIFT_F6           = 0x159
        ,VKC_SHIFT_F7           = 0x15a
        ,VKC_SHIFT_F8           = 0x15b
        ,VKC_SHIFT_F9           = 0x15c
        ,VKC_SHIFT_F10          = 0x15d
        ,VKC_CONTROL_F1         = 0x15e
        ,VKC_CONTROL_F2         = 0x15f
        ,VKC_CONTROL_F3         = 0x160
        ,VKC_CONTROL_F4         = 0x161
        ,VKC_CONTROL_F5         = 0x162
        ,VKC_CONTROL_F6         = 0x163
        ,VKC_CONTROL_F7         = 0x164
        ,VKC_CONTROL_F8         = 0x165
        ,VKC_CONTROL_F9         = 0x166
        ,VKC_CONTROL_F10        = 0x167
        ,VKC_ALT_F1             = 0x168
        ,VKC_ALT_F2             = 0x169
        ,VKC_ALT_F3             = 0x16a
        ,VKC_ALT_F4             = 0x16b
        ,VKC_ALT_F5             = 0x16c
        ,VKC_ALT_F6             = 0x16d
        ,VKC_ALT_F7             = 0x16e
        ,VKC_ALT_F8             = 0x16f
        ,VKC_ALT_F9             = 0x170
        ,VKC_ALT_F10            = 0x171
        ,VKC_CONTROL_PRINT      = 0x172
        ,VKC_CONTROL_LEFT       = 0x173
        ,VKC_CONTROL_RIGHT      = 0x174
        ,VKC_CONTROL_END        = 0x175
        ,VKC_CONTROL_PAGEDOWN   = 0x176
        ,VKC_CONTROL_HOME       = 0x177
        ,VKC_ALT_1              = 0x178
        ,VKC_ALT_2              = 0x179
        ,VKC_ALT_3              = 0x17a
        ,VKC_ALT_4              = 0x17b
        ,VKC_ALT_5              = 0x17c
        ,VKC_ALT_6              = 0x17d
        ,VKC_ALT_7              = 0x17e
        ,VKC_ALT_8              = 0x17f
        ,VKC_ALT_9              = 0x180
        ,VKC_ALT_0              = 0x181
        ,VKC_ALT_DASH           = 0x182
        ,VKC_ALT_EQUALS         = 0x183
        ,VKC_CONTROL_PAGEUP     = 0x184
        ,VKC_F11                = 0x185
        ,VKC_F12                = 0x186
        ,VKC_SHIFT_F11          = 0x187
        ,VKC_SHIFT_F12          = 0x188
        ,VKC_CONTROL_F11        = 0x189
        ,VKC_CONTROL_F12        = 0x18a
        ,VKC_ALT_F11            = 0x18b
        ,VKC_ALT_F12            = 0x18c
        ,VKC_CONTROL_UP         = 0x18d
        ,VKC_CONTROL_KPDASH     = 0x18e
        ,VKC_CONTROL_CENTER     = 0x18f
        ,VKC_CONTROL_KPPLUS     = 0x190
        ,VKC_CONTROL_DOWN       = 0x191
        ,VKC_CONTROL_INSERT     = 0x192
        ,VKC_CONTROL_DELETE     = 0x193
        ,VKC_CONTROL_KPSLASH    = 0x195
        ,VKC_CONTROL_KPSTAR     = 0x196
        ,VKC_ALT_EHOME          = 0x197
        ,VKC_ALT_EUP            = 0x198
        ,VKC_ALT_EPAGEUP        = 0x199
        ,VKC_ALT_ELEFT          = 0x19b
        ,VKC_ALT_ERIGHT         = 0x19d
        ,VKC_ALT_EEND           = 0x19f
        ,VKC_ALT_EDOWN          = 0x1a0
        ,VKC_ALT_EPAGEDOWN      = 0x1a1
        ,VKC_ALT_EINSERT        = 0x1a2
        ,VKC_ALT_EDELETE        = 0x1a3
        ,VKC_ALT_KPSLASH        = 0x1a4
        ,VKC_ALT_TAB            = 0x1a5
        ,VKC_ALT_ENTER          = 0x1a6
        // extended keys
        ,VKC_EHOME              = 0x247
        ,VKC_EUP                = 0x248
        ,VKC_EPAGEUP            = 0x249
        ,VKC_ELEFT              = 0x24b
        ,VKC_ERIGHT             = 0x24d
        ,VKC_EEND               = 0x24f
        ,VKC_EDOWN              = 0x250
        ,VKC_EPAGEDOWN          = 0x251
        ,VKC_EINSERT            = 0x252
        ,VKC_EDELETE            = 0x253
        ,VKC_CONTROL_ELEFT      = 0x273
        ,VKC_CONTROL_ERIGHT     = 0x274
        ,VKC_CONTROL_EEND       = 0x275
        ,VKC_CONTROL_EPAGEDOWN  = 0x276
        ,VKC_CONTROL_EHOME      = 0x277
        ,VKC_CONTROL_EPAGEUP    = 0x284
        ,VKC_CONTROL_EUP        = 0x28d
        ,VKC_CONTROL_EDOWN      = 0x291
        ,VKC_CONTROL_EINSERT    = 0x292
        ,VKC_CONTROL_EDELETE    = 0x293
    };

    //! Enumeration representing the different cursor types that may be applied
    //! via Console::SetLwrsor.  Not all console types support changing the
    //! cursor
    enum Cursor
    {
        LWRSOR_NONE
        ,LWRSOR_BLOCK
        ,LWRSOR_NORMAL
    };

    //-------------------------------------------------------------------------
    //! \brief Destructor - a virtual destructor is required to avoid warnings
    //!        with some compilers even though all functions are abstract
    //!
    virtual ~Console() { }

    //-------------------------------------------------------------------------
    //! \brief Enable the console
    //!
    //! \param pConsoleBuffer is a pointer to a console buffer for buffering
    //!        output writes
    //!
    //! \return OK if successful, not OK otherwise
    virtual RC Enable(ConsoleBuffer *pConsoleBuffer) = 0;

    //-------------------------------------------------------------------------
    //! \brief Disable the console
    //!
    //! \return OK if successful, not OK otherwise
    virtual RC Disable() = 0;

    //-------------------------------------------------------------------------
    //! \brief Check whether the console is enabled
    //!
    //! \return true if the console is enabled, false otherwise
    virtual bool IsEnabled() const = 0;

    //-------------------------------------------------------------------------
    //! \brief Suspend the console
    //!
    //! \return ok if successful, not ok otherwise
    virtual RC Suspend() = 0;

    //-------------------------------------------------------------------------
    //! \brief Pop-Up display the console (i.e. ensure that the console is
    //!        displayed
    //!
    //! \return OK if successful, not OK otherwise
    virtual RC PopUp() = 0;

    //-------------------------------------------------------------------------
    //! \brief Pop-Down a popped-up console
    //!
    //! \return OK if successful, not OK otherwise
    virtual RC PopDown() = 0;

    //-------------------------------------------------------------------------
    //! \brief Return whether the console has been popped up
    //!
    //! \return true if the console has been popped up, false otherwise
    virtual bool IsPoppedUp() const = 0;

    //-------------------------------------------------------------------------
    //! \brief Set the device that the console should be displayed on
    //!
    //! \param pDevice is the device to display the console on
    //!
    //! \return OK if display device change is successful, false otherwise
    virtual RC SetDisplayDevice(GpuDevice *pDevice) = 0;

    //-------------------------------------------------------------------------
    //! \brief Get the device that the console should be displayed on
    //!
    //! \return the display device change, NULL otherwise
    virtual GpuDevice * GetDisplayDevice() = 0;

    //-------------------------------------------------------------------------
    //! \brief Set whether the console is allowed to yield when performing
    //!        operations
    //!
    //! \param bAllowYield true if the console can yield, false otherwise
    virtual void AllowYield(bool bAllowYield) = 0;

    //-------------------------------------------------------------------------
    // The following APIs deal with cursor manipulation and console I/O not
    // all APIs are supported on all platforms.  APIs which are not supported
    // must still be implemented and provide default functionality even if
    // it is a no-op.  Unsupported APIs SHOULD NOT assert!!

    //-------------------------------------------------------------------------
    //! \brief Get the number of columns in the current console screen
    //!
    //! \return the number of columns in the current console screen
    virtual UINT32 GetScreenColumns() const = 0;

    //-------------------------------------------------------------------------
    //! \brief Get the number of rows in the current console screen
    //!
    //! \return the number of rows in the current console screen
    virtual UINT32 GetScreenRows() const = 0;

    //-------------------------------------------------------------------------
    //! \brief Get the current cursor position within the console screen.  The
    //!        upper left hand corner is Row == 0, Column == 0.
    //!
    //! \param pRow is the returned value for the current row.  If NULL no
    //!        value is returned
    //! \param pColumn is the returned value for the current column.  If NULL
    //!        no value is returned
    //!
    virtual void GetLwrsorPosition(INT32 * pRow, INT32 * pColumn) const = 0;

    //-------------------------------------------------------------------------
    //! \brief Set the current cursor position within the console screen.  The
    //!        upper left hand corner is Row == 0, Column == 0.
    //!
    //! \param row is the value to set the current row to
    //! \param column is the value to set the current column to
    //!
    virtual void SetLwrsorPosition(INT32 row, INT32 column) = 0;

    //-------------------------------------------------------------------------
    //! \brief Change the cursor appearence
    //!
    //! \param Type is the type of cursor to use
    //!
    //! \return OK if successful, not OK otherwise
    virtual RC SetLwrsor(Cursor Type) = 0;

    //-------------------------------------------------------------------------
    //! \brief Clear all text from the screen.
    //!
    virtual void ClearTextScreen() = 0;

    //-------------------------------------------------------------------------
    //! \brief Scroll the screen by the specified number of lines
    //!
    //! \param lines is the number of lines to scroll the screen.  Negative
    //!        values scroll the screen up (earlier text) positive values
    //!        scroll the screen down (later text).  A 0 value should reset
    //!        the lwrrently displayed position to the current cursor location
    //!
    //! \return OK if successful, not OK otherwise
    virtual RC ScrollScreen(INT32 lines) = 0;

    //-------------------------------------------------------------------------
    //! \brief Print a string to the screen
    //!
    //! \param str is the string to print to the screen
    //! \param strLen is the length of the string
    //! \param state is the state (color) for the string to print
    //!
    virtual void PrintStringToScreen(const char*           str,
                                     size_t                strLen,
                                     Tee::ScreenPrintState state) = 0;

    //-------------------------------------------------------------------------
    //! \brief Get a key from the console input.  This function will block
    //!        exelwtion and not return until a key is available.
    //!
    //! \return The key retrieved from the console input
    //!
    virtual VirtualKey GetKey() = 0;

    //-------------------------------------------------------------------------
    //! \brief Get a string from the console input.  This function will block
    //!        exelwtion and not return until an input string is available.
    //!
    //! \param pString is a pointer to the returned string
    //!
    //! \return OK if successful, not OK otherwise
    //!
    virtual RC GetString(string *pString) = 0;

    //-------------------------------------------------------------------------
    //! \brief Return whether the keyboard has been pressed (non-blocking)
    //!
    //! \return true if the keyboard has been pressed, false otherwise
    //!
    virtual bool KeyboardHit() = 0;

    //-------------------------------------------------------------------------
    //! \brief Return whether the console is capable of formatted output
    //!
    //! \return true if the console is capable of formatted output,
    //!         false otherwise
    //!
    virtual bool Formatting() const = 0;

    //-------------------------------------------------------------------------
    //! \brief Return whether the console requires that the input be echoed
    //!
    //! \return true if the console requires input to be echoed,
    //!         false otherwise
    //!
    virtual bool Echo() const = 0;

};
