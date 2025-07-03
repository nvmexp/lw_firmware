/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010,2013-2015,2018 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   linuxmutex.h
 * @brief  Mutex based on pthread mutex.
 */

#ifndef INCLUDED_LINUX_MUTEX_H
#define INCLUDED_LINUX_MUTEX_H

#include <pthread.h>

namespace Tasker
{
    enum LinuxMutexType
    {
        normal_mutex,
        relwrsive_mutex
    };

    class LinuxMutex
    {
        public:
            explicit LinuxMutex(LinuxMutexType type)
            {
#if !defined(MACOS) && !defined(BIONIC) && !defined(QNX) && (__GNUC__ >= 4) // Only avail in our gcc4-based Linux toolchains
                if (type == relwrsive_mutex)
                {
                    const pthread_mutex_t init = PTHREAD_RELWRSIVE_MUTEX_INITIALIZER_NP;
                    m_Mutex = init;
                }
                else
#endif
                {
                    pthread_mutex_init(&m_Mutex, 0);
                }
            }
            ~LinuxMutex()
            {
                pthread_mutex_destroy(&m_Mutex);
            }
            operator pthread_mutex_t*()
            {
                return &m_Mutex;
            }
            void Lock()
            {
                pthread_mutex_lock(&m_Mutex);
            }
            void Unlock()
            {
                pthread_mutex_unlock(&m_Mutex);
            }
            bool TryLock()
            {
                return 0 == pthread_mutex_trylock(&m_Mutex);
            }

        private:
            // Non-copyable
            LinuxMutex(const LinuxMutex&);
            LinuxMutex& operator=(const LinuxMutex&);

            pthread_mutex_t m_Mutex;
    };

    class LinuxMutexHolder
    {
        public:
            explicit LinuxMutexHolder(LinuxMutex& Mutex) : m_Mutex(Mutex)
            {
                m_Mutex.Lock();
            }
            ~LinuxMutexHolder()
            {
                m_Mutex.Unlock();
            }

        private:
            // Non-copyable
            LinuxMutexHolder(const LinuxMutexHolder&);
            LinuxMutexHolder& operator=(const LinuxMutexHolder&);

            LinuxMutex& m_Mutex;
    };

    class LinuxTlsValue
    {
        public:
            LinuxTlsValue()
            {
                pthread_key_create(&m_Key, NULL);
            }
            ~LinuxTlsValue()
            {
                pthread_key_delete(m_Key);
            }
            void Set(void* value)
            {
                pthread_setspecific(m_Key, value);
            }
            void* Get() const
            {
                return pthread_getspecific(m_Key);
            }

        private:
            // Non-copyable
            LinuxTlsValue(const LinuxTlsValue&);
            LinuxTlsValue& operator=(const LinuxTlsValue&);

            pthread_key_t m_Key;
    };

    class NativeMutex: public LinuxMutex
    {
        public:
            NativeMutex(): LinuxMutex(relwrsive_mutex) { }
    };

    class NativeMutexHolder: public LinuxMutexHolder
    {
        public:
            explicit NativeMutexHolder(NativeMutex& Mutex): LinuxMutexHolder(Mutex) { }
    };
}

#endif // !INCLUDED_LINUX_MUTEX_H
