/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_SYSSPEC_H
#define INCLUDED_SYSSPEC_H

#include "mdiag/utils/types.h"
#include "core/include/cmdline.h"
#include "core/include/tee.h"
#include "core/include/platform.h"
#include "core/include/cpu.h"
#include "sim/IInterrupt.h"
#include <exception>
#include <stdio.h>
#include <string>
#include <string.h>

// NOTE: most code should simply use the Printf macros implemented after
// the Msg namespace:
//
//     CritPrintf
//     ErrPrintf
//     WarnPrintf
//     InfoPrintf
//     DebugPrintf
//     RawPrintf
//     RawDbgPrintf

namespace Msg
{
    class PrintingObject
    {
    public:
        PrintingObject(Tee::Priority priority,
                       const char* caption,
                       bool raw) :
                m_priority(priority), m_caption(caption), m_raw(raw)
        {
        }

        PrintingObject(Tee::PriDebugStub,
                       const char* caption,
                       bool raw) :
            PrintingObject(Tee::PriSecret, caption, raw)
        {
        }

        void operator()(const char *format, ...);
        void operator()(UINT32 group, const char *format, ...);
        void VAPrintf(UINT32 group, const char *format, va_list Arguments);

        static PrintingObject Make(Tee::Priority priority,
                                   const char* caption,
                                   bool raw = false) {
            return PrintingObject(priority, caption, raw);
        }

        static PrintingObject Make(Tee::PriDebugStub,
                                   const char* caption,
                                   bool raw = false)
        {
            return Make(Tee::PriSecret, caption, raw);
        }
    private:
        PrintingObject(const PrintingObject&);
        PrintingObject& operator=(const PrintingObject&);

        void BoostPriority(UINT32 group);

        Tee::Priority  m_priority;
        const char*    m_caption;
        bool           m_raw;
    };

    void SetPrintThreadName(bool bPrintThreadName);
    void SetLwstomHeader(const char *pLwstomHeader);
};

#define CritPrintf   Msg::PrintingObject::Make(Tee::PriNormal,   "Critical")
#define ErrPrintf    Msg::PrintingObject::Make(Tee::PriNormal,   "Error")
#define WarnPrintf   Msg::PrintingObject::Make(Tee::PriNormal,   "Warning")
#define InfoPrintf   Msg::PrintingObject::Make(Tee::PriNormal, "Info")
#define DebugPrintf  Msg::PrintingObject::Make(Tee::PriDebug,  "Debug")

#define RawPrintf    Msg::PrintingObject::Make(Tee::PriNormal, "", true)
#define RawDbgPrintf Msg::PrintingObject::Make(Tee::PriDebug,  "", true)

DECLARE_MSG_DIR(Mdiag)

struct ParamDecl;

class FileIO
{
public:
    enum SeekType {SeekLwr, SeekEnd, SeekSet};
    virtual ~FileIO() { }
    virtual bool FileOpen(const char* name, const char* mode) = 0;
    virtual void FileClose() = 0;
    virtual UINT32 FileWrite(const void* buffer, UINT32 size, UINT32 count) = 0;
    virtual char *FileGetStr( char *string, int n) = 0;
    virtual int FileGetLine( string& line ) = 0;
    virtual void FilePrintf(const char *format, ...) = 0;
};

// Eventually, this should be merged with Platform, etc.
namespace Sys
{
    UINT08 CfgRd08(UINT32 address);
    UINT16 CfgRd16(UINT32 address);
    UINT32 CfgRd32(UINT32 address);
    void CfgWr08(UINT32 address, UINT08 data);
    void CfgWr16(UINT32 address, UINT16 data);
    void CfgWr32(UINT32 address, UINT32 data);

    FileIO* GetFileIO(const char* name, const char* mode);
    void ReleaseFileIO(FileIO* fio);

    // support for thread-id printing
    void SetThreadIdPrintEnable( bool tf );
    bool GetThreadIdPrintEnable(void);

    // support for thread-name printing
    void SetThreadNamePrintEnable( bool tf );
    bool GetThreadNamePrintEnable(void);
}

// memory, io, config access macros
#undef MEM_RD08
#undef MEM_RD16
#undef MEM_RD32
#undef MEM_RD64
#undef MEM_WR08
#undef MEM_WR16
#undef MEM_WR32
#undef MEM_WR64
inline UINT08 MEM_RD08(uintptr_t address) { return Platform::VirtualRd08((void *)address); }
inline UINT16 MEM_RD16(uintptr_t address) { return Platform::VirtualRd16((void *)address); }
inline UINT32 MEM_RD32(uintptr_t address) { return Platform::VirtualRd32((void *)address); }
inline UINT64 MEM_RD64(uintptr_t address) { return Platform::VirtualRd64((void *)address); }
inline void MEM_WR08(uintptr_t address, UINT08 data) { Platform::VirtualWr08((void *)address, data); }
inline void MEM_WR16(uintptr_t address, UINT16 data) { Platform::VirtualWr16((void *)address, data); }
inline void MEM_WR32(uintptr_t address, UINT32 data) { Platform::VirtualWr32((void *)address, data); }
inline void MEM_WR64(uintptr_t address, UINT64 data) { Platform::VirtualWr64((void *)address, data); }

#endif
