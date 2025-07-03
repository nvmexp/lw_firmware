/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef __MAILBOX_H__
#define __MAILBOX_H__

#include <string.h>
#include "mdiag/sysspec.h"
// For plugin use: always include this file after t3dplugin.h
// For mods use: always include this file after types.h

// 4 cores at most.
#define MAX_CORES 4

namespace T3dPlugin
{

class MailBox;

namespace MailBoxProtocol
{
    template <typename T> inline T Read(MailBox *mailBox);
    template <> inline char *Read<char *>(MailBox *mailBox);
    template <> inline const char *Read<const char *>(MailBox *mailBox);
    template <typename T> inline void Write(MailBox *mailBox, T t);
    template <> inline void Write<char *>(MailBox *mailBox, char *str);
    template <> inline void Write<const char *>(MailBox *mailBox, const char *str);
}

class MailBox
{
public:
    static MailBox *Create(UINT32 mailBoxBase, UINT32 coreId, UINT32 threadId);

    virtual ~MailBox() {}

    // Global address to inform all cores to exit.
    static const UINT32 MailBoxGlobalCtrlAddr = 0x8f5ffffc;    // 0x90000000 - (MAX_CORES + 1) * 2 * MailBoxSize - 4
    static const UINT32 MailBoxScratchBase = 0x8f600000;       // MailBoxGlobalCtrlAddr + 4
    static const UINT32 MailBoxSize = 1024 * 1024;

    // Ctrl values.
    enum
    {
        CTRL_UNKNOWN = 0,
        CTRL_READY, // Identifying the mailbox is ready to use.
        CTRL_DONE = 0xDEADBEEF, // Identifying all work is done, only used for MailBoxGlobalCtrlAddr.
    };

    static bool IsGlobalShutdown();
    static void GlobalShutdown();

    UINT32 ReadCtrl()
    {
        UINT32 ctrl = CTRL_UNKNOWN;
        MailBoxRead(CTRL_OFFSET, sizeof(ctrl), &ctrl);
        return ctrl;
    }

    void WriteCtrl(UINT32 ctrl)
    {
        MailBoxWrite(CTRL_OFFSET, sizeof(ctrl), &ctrl);
    }

    bool IsReady()
    {
        return ReadCtrl() == (UINT32)CTRL_READY;
    }

    void Ready()
    {
        WriteCtrl((UINT32)CTRL_READY);
    }

    // Lock values used in TID segement.
    // TID could also store a real thread ID.
    enum
    {
        TID_LOCK = 0xFFFFFFFF - 1, // Means no thread can acquire the mailbox.
        TID_UNLOCK = 0xFFFFFFFF, // Means any thread can acquire the mailbox.
    };

    UINT32 ReadThreadId()
    {
        UINT32 threadId = (UINT32)TID_UNLOCK;
        MailBoxRead(TID_OFFSET, sizeof(threadId), &threadId);
        return threadId;
    }

    void WriteThreadId(UINT32 threadId)
    {
        MailBoxWrite(TID_OFFSET, sizeof(threadId), &threadId);
    }

    void Lock()
    {
        WriteThreadId((UINT32)TID_LOCK);
    }

    void Unlock()
    {
        WriteThreadId((UINT32)TID_UNLOCK);
    }

    // Both real thread ID or TID_LOCK means the mailbox is being locked.
    bool IsLocked()
    {
        return ReadThreadId() != (UINT32)TID_UNLOCK;
    }

    bool HaveMail()
    {
        bool result = false;
        if (ReadThreadId() == m_ThreadId)
        {
            Command cmd = ReadCommand();
            if (CMD_NOP < cmd && cmd < CMD_END)
            {
                result = true;
            }
            else
            {
                if (CMD_NOP == cmd)
                {
                    WarnPrintf("Ignore NOP command from mailbox!/n");
                }
                else
                {
                    ErrPrintf("Ignore unkown command from mailbox!/n");
                }
                WriteThreadId((UINT32)TID_UNLOCK);
            }
        }
        return result;
    }

    void Send()
    {
        WriteThreadId(m_ThreadId);
    }

    void Acquire()
    {
        while (IsLocked())
        {
            Yield();
        }
        Lock();
    }

    void Release()
    {
        //reset command feild to detect duplicated request sent by RTL
        WriteCommand(CMD_NOP);
        Unlock();
    }

    enum Command
    {
        CMD_NOP = 0,
        CMD_STARTUP_REQ,
        CMD_STARTUP_ACK,
        CMD_INITIALIZE_REQ,
        CMD_INITIALIZE_ACK,
        CMD_SHUTDOWN_REQ,
        CMD_SHUTDOWN_ACK,
        CMD_T3DEVENT_REQ,
        CMD_T3DEVENT_ACK,
        CMD_T3DAPI_REQ,
        CMD_T3DAPI_ACK,
        CMD_PROXY_RD_REQ,
        CMD_PROXY_RD_ACK,
        CMD_PROXY_WR_REQ,
        CMD_PROXY_WR_ACK,
        CMD_PROXY_MEMSET_REQ,
        CMD_PROXY_MEMSET_ACK,
        CMD_END,
    };

    enum IRQStatus
    {
        IRQ_OK = 0,
        IRQ_TESTSUCCEEDED,
        IRQ_TESTFAILED,
    };

    // sizeof(enum) on ARM is 1, we must force a 32bit read/write.
    Command ReadCommand()
    {
        UINT32 cmd = (UINT32)CMD_NOP;
        MailBoxRead(CMD_OFFSET, sizeof(cmd), &cmd);
        return (Command)cmd;
    }

    // sizeof(enum) on ARM is 1, we must force a 32bit read/write.
    void WriteCommand(Command cmd)
    {
        UINT32 uCmd = (UINT32)cmd;
        MailBoxWrite(CMD_OFFSET, sizeof(uCmd), &uCmd);
    }

    IRQStatus ReadIRQStatus()
    {
        UINT32 irq = (UINT32)IRQ_OK;
        MailBoxRead(IRQ_OFFSET, sizeof(irq), &irq);
        return (IRQStatus)irq;
    }

    void WriteIRQStatus(IRQStatus irq)
    {
        UINT32 uIrq = (UINT32)irq;
        MailBoxWrite(IRQ_OFFSET, sizeof(uIrq), &uIrq);
    }

    void ResetDataOffset()
    {
        m_DataOffset = 0;
    }

    template<typename T>
    T Read()
    {
        return MailBoxProtocol::Read<T>(this);
    }

    template<typename T>
    void Write(T t)
    {
        MailBoxProtocol::Write(this, t);
    }

    void Read(void *content, UINT32 size)
    {
        MailBoxRead(DATA_OFFSET + m_DataOffset, size, content);
        m_DataOffset += size;
    }

    void Write(const void *content, UINT32 size)
    {
        MailBoxWrite(DATA_OFFSET + m_DataOffset, size, content);
        m_DataOffset += size;
    }

protected:
    explicit MailBox(UINT32 mailBoxBase, UINT32 coreId, UINT32 threadId)
    : m_MailBoxBase(mailBoxBase)
    , m_DataOffset(0)
    , m_CoreId(coreId)
    , m_ThreadId(threadId)
    {
        if (s_StringPool[m_CoreId] == NULL)
        {
            s_StringPool[m_CoreId] = new StringPool;
        }
    }

    virtual void Yield() = 0;

public:
    enum
    {
        CTRL_OFFSET = 0, // Mailbox ready or not, won't change once set to CTRL_READY.
        TID_OFFSET = 4, // Thread ID.
        CMD_OFFSET = 8, // Command.
        IRQ_OFFSET = 12, // IRQ status.
        DATA_OFFSET = 16, // Data.
    };

    virtual void MailBoxRead(UINT32 mailBoxOffset, UINT32 size, void *dst) = 0;

    // virtual UINT08 MailBoxRead08(UINT32 mailBoxOffset) = 0;
    // virtual UINT16 MailBoxRead16(UINT32 mailBoxOffset) = 0;
    // virtual UINT32 MailBoxRead32(UINT32 mailBoxOffset) = 0;
    // virtual UINT64 MailBoxRead64(UINT32 mailBoxOffset) = 0;

    virtual void MailBoxWrite(UINT32 mailBoxOffset, UINT32 size, const void *src) = 0;

    // virtual void MailBoxWrite08(UINT32 mailBoxOffset, UINT08 value) = 0;
    // virtual void MailBoxWrite16(UINT32 mailBoxOffset, UINT08 value) = 0;
    // virtual void MailBoxWrite32(UINT32 mailBoxOffset, UINT08 value) = 0;
    // virtual void MailBoxWrite64(UINT32 mailBoxOffset, UINT08 value) = 0;

    UINT32 m_MailBoxBase;
    UINT32 m_DataOffset;
    UINT32 m_CoreId;
    UINT32 m_ThreadId;

    // A simple string pool to handle the string parameter (char*/const char*) passing via mailbox.
    class StringPool
    {
    public:
        static const UINT32 s_PoolSize = 50;
        static const UINT32 s_PreallocLength = 256;

        StringPool()
            : m_Index(0)
        {
            // pre-allocate 256 byte for each string, to avoid frequent re-allocate.
            for (UINT32 i = 0; i < s_PoolSize; ++i)
            {
                m_Pool[i] = new char[s_PreallocLength];
                m_Length[i] = s_PreallocLength;
            }
        }

        ~StringPool()
        {
            for (UINT32 i = 0; i < s_PoolSize; ++i)
            {
                delete[] m_Pool[i];
                m_Pool[i] = 0;
            }
        }

        char *AllocStr(UINT32 length)
        {
            if (m_Index == s_PoolSize)
            {
                m_Index = 0;
            }
            if (length > m_Length[m_Index])
            {
                delete[] m_Pool[m_Index];
                m_Pool[m_Index] = new char[length];
                m_Length[m_Index] = length;
            }
            char *str = m_Pool[m_Index];
            ++m_Index;
            return str;
        }

    private:
        char *m_Pool[s_PoolSize];
        UINT32 m_Length[s_PoolSize];

        UINT32 m_Index;
    };
    // Static StringPool for each core.
    static StringPool *s_StringPool[MAX_CORES];
};

typedef MailBox::Command Command;

namespace MailBoxProtocol
{
    template<typename T>
    inline T Read(MailBox *mailBox)
    {
        T t;
        mailBox->MailBoxRead(MailBox::DATA_OFFSET + mailBox->m_DataOffset, sizeof(T), &t);
        mailBox->m_DataOffset += sizeof(T);
        return t;
    }

    template<>
    inline char *Read<char *>(MailBox *mailBox)
    {
        UINT32 length = mailBox->Read<UINT32>();
        char *content = MailBox::s_StringPool[mailBox->m_CoreId]->AllocStr(length + 1);

        mailBox->MailBoxRead(MailBox::DATA_OFFSET + mailBox->m_DataOffset, length, content);
        content[length] = 0;
        mailBox->m_DataOffset += length;
        return content;
    }

    template<>
    inline const char *Read<const char *>(MailBox *mailBox)
    {
        return Read<char *>(mailBox);
    }

    template<>
    inline bool Read<bool>(MailBox *mailBox)
    {
        UINT32 ret = mailBox->Read<UINT32>();
        return ret == 0? false: true;
    }

    template<typename T>
    inline void Write(MailBox *mailBox, T t)
    {
        mailBox->MailBoxWrite(MailBox::DATA_OFFSET + mailBox->m_DataOffset, sizeof(T), &t);
        mailBox->m_DataOffset += sizeof(T);
    }

    template<>
    inline void Write<const char *>(MailBox *mailBox, const char *str)
    {
        UINT32 length = (UINT32)strlen(str);
        mailBox->Write<UINT32>(length);
        mailBox->MailBoxWrite(MailBox::DATA_OFFSET + mailBox->m_DataOffset, length, (const void *)str);
        mailBox->m_DataOffset += length;
    }

    template<>
    inline void Write<char *>(MailBox *mailBox, char *str)
    {
        Write(mailBox, (const char *)str);
    }

    template<>
    inline void Write<bool>(MailBox *mailBox, bool boolean)
    {
        Write(mailBox, boolean ? (UINT32)1 : (UINT32)0);
    }
}

} // namespace T3dPlugin

#endif // __MAILBOX_H__
