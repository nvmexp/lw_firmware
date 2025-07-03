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

#include "core/include/xp.h"
#include "core/include/platform.h"
#include "core/include/pci.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "mdiag/utils/types.h"
#include "mdiag/tests/testdir.h"
#include "sysspec.h"
#include "mdiag/utils/GZFileIO.h"
#include "core/include/tee.h"
#include "core/include/pool.h"
#include "core/include/massert.h"
#include "thread.h"
#include <assert.h>
#include <list>
#include <map>
#include <algorithm>
#include "lwtypes.h"
#include "sysspec.h"

static bool s_enableThreadIdPrint = false;
static bool s_EnableThreadNamePrint = false;

DEFINE_MSG_DIR(Mdiag, false)

void Sys::SetThreadIdPrintEnable( bool tf )
{
    s_enableThreadIdPrint = tf;
}

bool Sys::GetThreadIdPrintEnable(void)
{
    return s_enableThreadIdPrint;
}

void Sys::SetThreadNamePrintEnable( bool tf )
{
    s_EnableThreadNamePrint = tf;
}

bool Sys::GetThreadNamePrintEnable(void)
{
    return s_EnableThreadNamePrint;
}

UINT08 Sys::CfgRd08(UINT32 address)
{
    UINT32 domain;
    UINT32 bus;
    UINT32 device;
    UINT32 function;
    UINT32 offset;
    UINT08 data;

    Pci::DecodeConfigAddress(address, &domain, &bus,
                             &device, &function, &offset);
    RC rc = Platform::PciRead08(domain, bus, device, function, offset, &data);
    if (rc != OK)
    {
        ErrPrintf("Platform::PciRead08 failed: %s\n",
            rc.Message());
    }
    return data;
}

UINT16 Sys::CfgRd16(UINT32 address)
{
    UINT32 domain;
    UINT32 bus;
    UINT32 device;
    UINT32 function;
    UINT32 offset;
    UINT16 data;

    Pci::DecodeConfigAddress(address, &domain, &bus,
                             &device, &function, &offset);
    RC rc = Platform::PciRead16(domain, bus, device, function, offset, &data);
    if (rc != OK)
    {
        ErrPrintf("Platform::PciRead16 failed: %s\n",
            rc.Message());
    }
    return data;
}

UINT32 Sys::CfgRd32(UINT32 address)
{
    UINT32 domain;
    UINT32 bus;
    UINT32 device;
    UINT32 function;
    UINT32 offset;
    UINT32 data;

    Pci::DecodeConfigAddress(address, &domain, &bus,
                             &device, &function, &offset);
    RC rc = Platform::PciRead32(domain, bus, device, function, offset, &data);
    if (rc != OK)
    {
        ErrPrintf("Platform::PciRead32 failed: %s\n",
            rc.Message());
    }
    return data;
}

void Sys::CfgWr08(UINT32 address, UINT08 data)
{
    UINT32 domain;
    UINT32 bus;
    UINT32 device;
    UINT32 function;
    UINT32 offset;

    Pci::DecodeConfigAddress(address, &domain, &bus,
                             &device, &function, &offset);
    RC rc = Platform::PciWrite08(domain, bus, device, function, offset, data);
    if (rc != OK)
    {
        ErrPrintf("Platform::PciWrite08 failed: %s\n",
            rc.Message());
    }
}

void Sys::CfgWr16(UINT32 address, UINT16 data)
{
    UINT32 domain;
    UINT32 bus;
    UINT32 device;
    UINT32 function;
    UINT32 offset;

    Pci::DecodeConfigAddress(address, &domain, &bus,
                             &device, &function, &offset);
    RC rc = Platform::PciWrite16(domain, bus, device, function, offset, data);
    if (rc != OK)
    {
        ErrPrintf("Platform::PciWrite16 failed: %s\n",
            rc.Message());
    }
}

void Sys::CfgWr32(UINT32 address, UINT32 data)
{
    UINT32 domain;
    UINT32 bus;
    UINT32 device;
    UINT32 function;
    UINT32 offset;

    Pci::DecodeConfigAddress(address, &domain, &bus,
                             &device, &function, &offset);
    RC rc = Platform::PciWrite32(domain, bus, device, function, offset, data);
    if (rc != OK)
    {
        ErrPrintf("Platform::PciWrite32 failed: %s\n",
            rc.Message());
    }
}

class MyFileIO : public FileIO {
private:
   FILE* m_fileHandle;

public:
   MyFileIO() {
      m_fileHandle = NULL;
   };
   virtual ~MyFileIO() {
      assert(m_fileHandle == NULL);
   };
   virtual bool FileOpen(const char* name, const char* mode) {
      DebugPrintf("Opening file '%s' in mode '%s'\n", name, mode);
      m_fileHandle = fopen(name, mode);
      if ( m_fileHandle == NULL || ferror(m_fileHandle) ) {
         return true;
      }
      return false;
   };
   virtual void FileClose() {
      if ( m_fileHandle ) fclose(m_fileHandle);
      m_fileHandle = NULL;
   };
   virtual UINT32 FileWrite(const void* buffer, UINT32 size, UINT32 count) {
      return (UINT32)fwrite(buffer, size, count, m_fileHandle);
   };
   virtual char *FileGetStr( char *astring, int n) {
      return fgets(astring, n, m_fileHandle);
   };
   virtual int FileGetLine( string& aline ) {
       // read in one line of text: keep reading until hit '\n' or eof
       char tmpbuf[1024];
       aline.erase( 0, string::npos );
       while( !feof(m_fileHandle) )
       {
           tmpbuf[0] = '\0';
           if (NULL != fgets(tmpbuf, 1024, m_fileHandle))
           {
               aline += tmpbuf;
               if( aline[aline.size()-1] == '\n' )
               {
                   return (int)aline.size();
               }
           }
       }
       return (int)aline.size();
   }

   virtual void FilePrintf(const char *format, ...) {
      va_list ap;

      va_start(ap, format);
      vfprintf(m_fileHandle, format, ap);
      va_end(ap);
   }
};

FileIO* Sys::GetFileIO(const char* name, const char* mode)
{
    FileIO*  fp;

    if( (strlen(name)>3) && !Utility::strcasecmp(name+strlen(name)-3, ".gz") )
    {
        // Only file with name '*.gz' will be treated as gzip'ed file
        fp = new GZFileIO;
    }
    else
    {
        fp = new MyFileIO;
    }

    if (!fp)
    {
        return NULL;
    }

    if (fp->FileOpen(name, mode))
    {
        delete fp;
        return NULL;
    }

    return fp;
}

void Sys::ReleaseFileIO(FileIO* fio)
{
    delete fio;
}

void Msg::PrintingObject::VAPrintf(UINT32 group, const char *format, va_list Arguments)
{
    // exit early if we're sure this msg can't get through
    if (!Tee::WillPrint(m_priority) && !Tee::IsModuleEnabled(group))
        return;

    string prefix;

    if (Sys::GetThreadNamePrintEnable() ||
        Sys::GetThreadIdPrintEnable())
    {
        Tasker::ThreadID tid = Tasker::LwrrentThread();
        const char *testName;

        if ( tid == 0 )
        {
            testName = TestDirectory::Get_test_name_being_setup();
            tid = TestDirectory::Get_test_id_being_setup();
        }
        else
        {
            testName = Thread::LwrrentThread()->GetName();
        }

        prefix = "(";
        if (Sys::GetThreadNamePrintEnable() && testName && testName[0])
        {
            prefix += testName;
            prefix += " ";
        }
        prefix += "#";
        {
            char buf[16];
            sprintf(buf, "%d", tid);
            prefix += buf;
        }
        prefix += ") ";
    }

    prefix += m_caption;

    if (Tee::IsGroup(group))
    {
        const TeeModule& module = Tee::GetModule(group);
        prefix += " [";
        prefix += module.GetModDir();
        prefix += " ";
        prefix += module.GetName();
        prefix += "] ";
    }

    prefix += ": ";
    prefix += format;

    BoostPriority(group);

    ModsExterlwAPrintf(m_priority, group, Tee::SPS_NORMAL,
                       prefix.c_str(), Arguments);
}

void Msg::PrintingObject::BoostPriority(UINT32 group)
{
    if (m_priority == Tee::PriSecret &&
        Tee::IsGroup(group) &&
        Tee::IsModuleEnabled(group))
    {
        // boost priority to show enabled
        // groups when "-d" is not present
        m_priority = Tee::PriNormal;
    }
}

void Msg::PrintingObject::operator()(const char *format, ...)
{
    va_list Arguments;
    va_start(Arguments, format);
    if (m_raw)
    {
        ModsExterlwAPrintf(m_priority, Tee::ModuleNone, Tee::SPS_NORMAL, format, Arguments);
    }
    else
    {
        PrintingObject::VAPrintf(Tee::ModuleNone, format, Arguments);
    }
    va_end(Arguments);
}

void Msg::PrintingObject::operator()(UINT32 group, const char *format, ...)
{
    va_list Arguments;
    va_start(Arguments, format);
    if (m_raw)
    {
        BoostPriority(group);
        ModsExterlwAPrintf(m_priority, group, Tee::SPS_NORMAL, format, Arguments);
    }
    else
    {
        PrintingObject::VAPrintf(group, format, Arguments);
    }
    va_end(Arguments);
}
