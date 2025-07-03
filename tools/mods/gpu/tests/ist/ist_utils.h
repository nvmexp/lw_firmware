/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once

// An improvement on setget.h's SETGET_PROP which requires specifying type
#define SETGET_PROP2(propName)                                             \
    decltype(m_##propName) Get##propName() const { return m_##propName; }  \
    void Set##propName(const decltype(m_##propName)& val) { m_##propName = val; }

// Factor out some very common boilerplate code
#define SW_ERROR(test, message)                                \
    do                                                         \
    {                                                          \
        if ((test))                                            \
        {                                                      \
            Printf(Tee::PriError, "SOFTWARE ERROR: " message); \
            return RC::SOFTWARE_ERROR;                         \
        }                                                      \
    } while(0)

#define FILE_EXISTS(file)                                                               \
    do                                                                                   \
    {                                                                                    \
        if (!Xp::DoesFileExist(file))                                                    \
        {                                                                                \
            Printf(Tee::PriError, "File %s not found\n", file.c_str());     \
            return RC::FILE_DOES_NOT_EXIST;                                 \
        }                                                                   \
    } while(0)                                                              \

    
