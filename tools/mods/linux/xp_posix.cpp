/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/xp.h"
#include "core/include/massert.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <poll.h>

namespace
{
    int OpenFlag(Xp::InteractiveFile::OpenFlags flags)
    {
        switch (flags)
        {
            case Xp::InteractiveFile::ReadOnly:  return O_RDONLY;
            case Xp::InteractiveFile::WriteOnly: return O_WRONLY | O_TRUNC;
            case Xp::InteractiveFile::ReadWrite: return O_RDWR;
            case Xp::InteractiveFile::Create:    return O_RDWR | O_TRUNC | O_CREAT;
            case Xp::InteractiveFile::Append:    return O_WRONLY | O_APPEND | O_CREAT;
            default: MASSERT(0); break;
        }
        return 0;
    }
}

RC Xp::InteractiveFile::Open(const char* path, OpenFlags flags)
{
    MASSERT(m_Fd == IlwalidFd);
    if (m_Fd != IlwalidFd)
    {
        return RC::SOFTWARE_ERROR;
    }

    if (flags == Xp::InteractiveFile::Append)
    {
        m_Flags = flags;
    }
    else if (flags != Xp::InteractiveFile::Create)
    {
        struct stat buf;
        if (0 != stat(path, &buf))
        {
            Printf(Tee::PriError, "File %s does not exist\n", path);
            return RC::FILE_DOES_NOT_EXIST;
        }

        m_Flags = flags;

        int amode = 0;
        switch (flags)
        {
            case ReadOnly:  amode = R_OK; break;
            case WriteOnly: amode = W_OK; break;
            case ReadWrite: amode = R_OK | W_OK; break;
            default: return RC::SOFTWARE_ERROR;
        }

        if (0 != access(path, amode))
        {
            Printf(Tee::PriError, "Insufficient permissions to access %s\n", path);
            return RC::FILE_PERM;
        }
    }
    else
    {
        m_Flags = Xp::InteractiveFile::ReadWrite;
    }

    m_Fd = open(path, OpenFlag(flags), 0644);
    if (m_Fd == IlwalidFd)
    {
        Printf(Tee::PriError, "Failed to open %s\n", path);
        return RC::CANNOT_OPEN_FILE;
    }

    m_Path = path;
    m_bNeedSeek = false;

    return OK;
}

void Xp::InteractiveFile::Close()
{
    if (m_Fd != IlwalidFd)
    {
        close(m_Fd);
        m_Fd = IlwalidFd;
    }
}

RC Xp::InteractiveFile::Wait(FLOAT64 timeoutMs)
{
    if (m_Fd == IlwalidFd)
    {
        Printf(Tee::PriError, "Wait on closed file\n");
        return RC::SOFTWARE_ERROR;
    }

    if ((m_Flags == WriteOnly) || (m_Flags == Append))
    {
        return RC::SOFTWARE_ERROR;
    }

    Tasker::DetachThread detach;

    pollfd fds = {0};
    fds.fd = m_Fd;
    fds.events = POLLIN | POLLPRI;

    const int ms = static_cast<int>(floor(timeoutMs));

    const int ret = poll(&fds, 1, ms);
    if (ret == 0)
    {
        return RC::TIMEOUT_ERROR;
    }
    if (ret != 1)
    {
        return RC::FILE_READ_ERROR;
    }

    m_bNeedSeek = true;
    return OK;
}

RC Xp::InteractiveFile::Read(string* pValue)
{
    MASSERT(pValue);

    if (m_Fd == IlwalidFd)
    {
        Printf(Tee::PriError, "Read of closed file\n");
        return RC::SOFTWARE_ERROR;
    }

    if ((m_Flags == WriteOnly) || (m_Flags == Append))
    {
        Printf(Tee::PriError, "Read of write-only file\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_bNeedSeek)
    {
        close(m_Fd);
        m_Fd = open(m_Path.c_str(), OpenFlag(m_Flags));
        if (m_Fd == IlwalidFd)
        {
            Printf(Tee::PriError, "Failed to reopen %s for reading\n", m_Path.c_str());
            return RC::FILE_READ_ERROR;
        }
    }

    m_bNeedSeek = true;

    *pValue = "";
    const size_t maxRead = 64;
    ssize_t readSize = 0;
    char buf[maxRead];
    do
    {
        readSize = read(m_Fd, buf, maxRead);
        if (readSize > static_cast<ssize_t>(maxRead))
        {
            readSize = -1;
        }
        if (readSize > 0)
        {
            *pValue += string(buf, static_cast<size_t>(readSize));
        }
    } while (readSize > 0);

    if (readSize < 0)
    {
        *pValue = "";
        Printf(Tee::PriError, "File read failed - %s\n", m_Path.c_str());
        return RC::FILE_READ_ERROR;
    }

    return OK;
}

RC Xp::InteractiveFile::Write(const char* value, size_t size)
{
    MASSERT(value);

    if (m_Fd == IlwalidFd)
    {
        Printf(Tee::PriError, "Write of closed file\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_Flags == ReadOnly)
    {
        Printf(Tee::PriError, "Write of read-only file\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_Flags == ReadWrite)
    {
        m_bNeedSeek = true;
    }

    if (m_bNeedSeek)
    {
        close(m_Fd);
        m_Fd = open(m_Path.c_str(), OpenFlag(m_Flags) | O_TRUNC);
        if (m_Fd == IlwalidFd)
        {
            Printf(Tee::PriError, "Failed to reopen %s for writing\n", m_Path.c_str());
            return RC::FILE_WRITE_ERROR;
        }
    }

    if (m_Flags != Append)
    {
        m_bNeedSeek = true;
    }

    const ssize_t writtenSize = write(m_Fd, value, size);
    if (static_cast<size_t>(writtenSize) != size)
    {
        Printf(Tee::PriError, "File write failed - %s\n", m_Path.c_str());
        return RC::FILE_WRITE_ERROR;
    }

    return OK;
}
