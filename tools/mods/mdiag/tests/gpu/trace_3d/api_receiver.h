/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef __API_RECEIVER_H__
#define __API_RECEIVER_H__

#include "mailbox.h"

namespace T3dPlugin
{
    class ApiReceiver
    {
    public:
        virtual void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox) = 0;
    };

    template <class T1>
    class VoidApiReceiver1 : public ApiReceiver
    {
    public:
        typedef void (*pFn)(T1);

        VoidApiReceiver1(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();

            traceMailBox->Release();

            m_Api(m_T1);

            pluginMailBox->Acquire();

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }

        pFn m_Api;
        T1 m_T1;
    };

    template <class T1, class T2>
    class VoidApiReceiver2 : public ApiReceiver
    {
    public:
        typedef void (*pFn)(T1, T2);

        VoidApiReceiver2(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();
            m_T2 = traceMailBox->template Read<T2>();

            traceMailBox->Release();

            m_Api(m_T1, m_T2);

            pluginMailBox->Acquire();

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
        T1 m_T1;
        T2 m_T2;
    };

    template <class T1, class T2, class T3>
    class VoidApiReceiver3 : public ApiReceiver
    {
    public:
        typedef void (*pFn)(T1, T2, T3);

        VoidApiReceiver3(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();
            m_T2 = traceMailBox->template Read<T2>();
            m_T3 = traceMailBox->template Read<T3>();

            traceMailBox->Release();

            m_Api(m_T1, m_T2, m_T3);

            pluginMailBox->Acquire();

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
        T1 m_T1;
        T2 m_T2;
        T3 m_T3;
    };

    template <class T1, class T2, class T3, class T4>
    class VoidApiReceiver4 : public ApiReceiver
    {
    public:
        typedef void (*pFn)(T1, T2, T3, T4);

        VoidApiReceiver4(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();
            m_T2 = traceMailBox->template Read<T2>();
            m_T3 = traceMailBox->template Read<T3>();
            m_T4 = traceMailBox->template Read<T4>();

            traceMailBox->Release();

            m_Api(m_T1, m_T2, m_T3, m_T4);

            pluginMailBox->Acquire();

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
        T1 m_T1;
        T2 m_T2;
        T3 m_T3;
        T4 m_T4;
    };

    template <class T1, class T2, class T3, class T4, class T5>
    class VoidApiReceiver5 : public ApiReceiver
    {
    public:
        typedef void (*pFn)(T1, T2, T3, T4, T5);

        VoidApiReceiver5(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();
            m_T2 = traceMailBox->template Read<T2>();
            m_T3 = traceMailBox->template Read<T3>();
            m_T4 = traceMailBox->template Read<T4>();
            m_T5 = traceMailBox->template Read<T5>();

            traceMailBox->Release();

            m_Api(m_T1, m_T2, m_T3, m_T4, m_T5);

            pluginMailBox->Acquire();

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
        T1 m_T1;
        T2 m_T2;
        T3 m_T3;
        T4 m_T4;
        T5 m_T5;
    };

    template <class T1, class T2, class T3, class T4, class T5, class T6>
    class VoidApiReceiver6 : public ApiReceiver
    {
    public:
        typedef void (*pFn)(T1, T2, T3, T4, T5, T6);

        VoidApiReceiver6(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();
            m_T2 = traceMailBox->template Read<T2>();
            m_T3 = traceMailBox->template Read<T3>();
            m_T4 = traceMailBox->template Read<T4>();
            m_T5 = traceMailBox->template Read<T5>();
            m_T6 = traceMailBox->template Read<T6>();

            traceMailBox->Release();

            m_Api(m_T1, m_T2, m_T3, m_T4, m_T5, m_T6);

            pluginMailBox->Acquire();

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
        T1 m_T1;
        T2 m_T2;
        T3 m_T3;
        T4 m_T4;
        T5 m_T5;
        T6 m_T6;
    };

    template <class T1, class T2, class T3, class T4, class T5, class T6, class T7>
    class VoidApiReceiver7 : public ApiReceiver
    {
    public:
        typedef void (*pFn)(T1, T2, T3, T4, T5, T6, T7);

        VoidApiReceiver7(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();
            m_T2 = traceMailBox->template Read<T2>();
            m_T3 = traceMailBox->template Read<T3>();
            m_T4 = traceMailBox->template Read<T4>();
            m_T5 = traceMailBox->template Read<T5>();
            m_T6 = traceMailBox->template Read<T6>();
            m_T7 = traceMailBox->template Read<T7>();

            traceMailBox->Release();

            m_Api(m_T1, m_T2, m_T3, m_T4, m_T5, m_T6, m_T7);

            pluginMailBox->Acquire();

            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
        T1 m_T1;
        T2 m_T2;
        T3 m_T3;
        T4 m_T4;
        T5 m_T5;
        T6 m_T6;
        T7 m_T7;
    };

    template <class RT>
    class RTApiReceiver0 : public ApiReceiver
    {
    public:
        typedef RT (*pFn)();

        RTApiReceiver0(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            traceMailBox->Release();

            RT ret = m_Api();

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(ret);
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
    };

    template <class RT, class T1>
    class RTApiReceiver1 : public ApiReceiver
    {
    public:
        typedef RT (*pFn)(T1);

        RTApiReceiver1(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();

            traceMailBox->Release();

            RT ret = m_Api(m_T1);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(ret);
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
        T1 m_T1;
    };

    template <class RT, class T1, class T2>
    class RTApiReceiver2 : public ApiReceiver
    {
    public:
        typedef RT (*pFn)(T1, T2);

        RTApiReceiver2(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();
            m_T2 = traceMailBox->template Read<T2>();

            traceMailBox->Release();

            RT ret = m_Api(m_T1, m_T2);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(ret);
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
        T1 m_T1;
        T2 m_T2;
    };

    template <class RT, class T1, class T2, class T3>
    class RTApiReceiver3 : public ApiReceiver
    {
    public:
        typedef RT (*pFn)(T1, T2, T3);

        RTApiReceiver3(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();
            m_T2 = traceMailBox->template Read<T2>();
            m_T3 = traceMailBox->template Read<T3>();

            traceMailBox->Release();

            RT ret = m_Api(m_T1, m_T2, m_T3);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(ret);
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
        T1 m_T1;
        T2 m_T2;
        T3 m_T3;
    };

    template <class RT, class T1, class T2, class T3, class T4>
    class RTApiReceiver4 : public ApiReceiver
    {
    public:
        typedef RT (*pFn)(T1, T2, T3, T4);

        RTApiReceiver4(pFn api)
        : m_Api(api)
        {
        }

        void operator ()(MailBox *pluginMailBox, MailBox *traceMailBox)
        {
            // No reset.
            m_T1 = traceMailBox->template Read<T1>();
            m_T2 = traceMailBox->template Read<T2>();
            m_T3 = traceMailBox->template Read<T3>();
            m_T4 = traceMailBox->template Read<T4>();

            traceMailBox->Release();

            RT ret = m_Api(m_T1, m_T2, m_T3, m_T4);

            pluginMailBox->Acquire();

            pluginMailBox->ResetDataOffset();
            pluginMailBox->Write(ret);
            pluginMailBox->WriteCommand(MailBox::CMD_T3DAPI_ACK);

            pluginMailBox->Send();
        }
    protected:
        pFn m_Api;
        T1 m_T1;
        T2 m_T2;
        T3 m_T3;
        T4 m_T4;
    };
}

#endif // __API_RECEIVER_H__
