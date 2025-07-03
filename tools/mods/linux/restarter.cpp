/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <sys/wait.h>
#include <errno.h>
#include <mqueue.h>
#include <spawn.h>
#include <string.h>

#include <boost/atomic.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/optional.hpp>
#include <boost/spirit/include/qi.hpp>

#include "linuxmutex.h"
#include "core/include/rc.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "core/include/utility.h"

#include "core/include/restarter.h"

// Purposefully is expanded to nothing. For debug purpose define it in a way
// that fits the debugging strategy. For example, it can be a simple printf. Or
// it can be saving messages in a vector, or it can be a printf of the process
// ID, wall clock and the message.
#define dprintf(format, ...)

#define ENCHECK(sysfunname)                             \
    if (0 != errno)                                     \
    {                                                   \
        Printf(                                         \
            Tee::PriError,                              \
            #sysfunname " error in %s at %s:%d - %s\n", \
            __func__,                                   \
            __FILE__,                                   \
            __LINE__,                                   \
            strerror(errno));                           \
        return RC::SOFTWARE_ERROR;                      \
    }

#define ENCHECKR(sysfunname, res)                       \
    if (0 != res)                                       \
    {                                                   \
        Printf(                                         \
            Tee::PriError,                              \
            #sysfunname " error in %s at %s:%d - %s\n", \
            __func__,                                   \
            __FILE__,                                   \
            __LINE__,                                   \
            strerror(res));                             \
        return RC::SOFTWARE_ERROR;                      \
    }

#define ENCHECKC(sysfunname, x)                         \
    if (-1 == (x))                                      \
    {                                                   \
        Printf(                                         \
            Tee::PriError,                              \
            #sysfunname " error in %s at %s:%d - %s\n", \
            __func__,                                   \
            __FILE__,                                   \
            __LINE__,                                   \
            strerror(errno));                           \
        return RC::SOFTWARE_ERROR;                      \
    }

namespace ProcCtrl
{
    using Tasker::LinuxMutex;
    using Tasker::LinuxMutexHolder;

    namespace qi = boost::spirit::qi;
    namespace ascii = boost::spirit::ascii;

    // The implementation uses POSIX message queues to sync the parent
    // controller and its children. There are two queues. The first one is
    // called a sync queue. It has two roles. The first one it serves as a mutex
    // to determine that there are no more instances of MODS and the process can
    // become a controller. The second role is to get messages and requests from
    // children. The second queue is called a response queue and is used to post
    // responses from the controller back to the children. We have to use
    // separate queues for requests and responses because otherwise the
    // requester will read its own request in a consequent pair of
    // mq_send/mq_receive.

    // We are using threads to get notification about messages in the sync
    // queue.

    using boost::optional;
    using Utility::StrPrintf;

    class LinuxRestarter;

    class LinuxController :
        public Controller
      , private boost::noncopyable
    {
        friend class LinuxRestarter;
    private:
        LinuxController(LinuxRestarter &restarter)
          : m_Restarter(restarter)
          , m_StartsCount(0)
        {}

        mqd_t GetSyncQueue() const;
        mqd_t GetRespQueue() const;

        size_t GetStartsCount() const
        {
            return m_StartsCount;
        }

        void IncrementStartsCount()
        {
            ++m_StartsCount;
        }

        RC ProcessChildMessages();
        RC EmptySyncQueue();
        RC NotifySetup();
        RC StopNotifications();

        optional<ExitStatusEnum> GetPostedResult() const
        {
            return m_PostedResult;
        }

        void SetPostedResult(ExitStatusEnum res)
        {
            m_PostedResult = res;
        }

        void ResetPostedResult()
        {
            m_PostedResult.reset();
        }

        optional<RC> GetPostedRC() const
        {
            return m_PostedRC;
        }

        void SetPostedRC(RC rc)
        {
            m_PostedRC = rc;
        }

        void ResetPostedRC()
        {
            m_PostedRC.reset();
        }

        static const string& AckMessage();
        static const string& ReadyMessage();

        virtual
        RC DoStartNewAndWait(int argc, char *argv[], bool *exitedNormally,
                             ExitStatusEnum *exitStatus, int *exitCode, RC *childRC);

        virtual
        RC DoGetStartsCount(size_t *count) const
        {
            RC rc;
            *count = m_StartsCount;
            return rc;
        }

        LinuxRestarter          &m_Restarter;
        boost::atomic_size_t     m_StartsCount;
        optional<ExitStatusEnum> m_PostedResult;
        optional<RC>             m_PostedRC;
    };

    class LinuxPerformer :
        public Performer
      , private boost::noncopyable
    {
        friend class LinuxRestarter;
    private:
        LinuxPerformer(LinuxRestarter &restarter)
          : m_Restarter(restarter)
          , m_Signal(nullptr)
          , m_LastPostedResult(ExitStatus::unknown)
        {}

        mqd_t GetSyncQueue() const;
        mqd_t GetRespQueue() const;

        static
        const string& NeedRestartMessage();
        static
        const string& SuccessMessage();
        static
        const string& FailureMessage();
        static
        const string& GetStartCountReq();

        virtual RC DoPostNeedRestart(RC childRC);

        virtual RC DoPostSuccess(RC childRC);

        virtual RC DoPostFailure(RC childRC);

        virtual RC DoGetStartsCount(size_t *count) const;

        virtual RC DoConnectResultPoster(const SlotType &slot);

        virtual RC DoIlwokeResultPoster(INT32 rc);

        virtual bool DoHasResultPoster() const;

        virtual RC DoGetLastPostedResult(ExitStatusEnum *res) const;

        LinuxRestarter &m_Restarter;
        SignalType      m_Signal;
        ExitStatusEnum  m_LastPostedResult;
    };

    class LinuxRestarter :
        public Restarter
      , private boost::noncopyable
    {
        friend class LinuxController;
        friend class LinuxPerformer;

    public:
        LinuxRestarter()
          : m_Controller(*this)
          , m_Performer(*this)
          , m_SyncQueue(-1)
          , m_RespQueue(-1)
          , m_IsController(false)
          , m_NotifySetupGuard(Tasker::relwrsive_mutex)

        {
            m_QueueName.assign("/lwpu-");
            m_QueueName.append(Utility::StrPrintf("(ppid = %d)-", getppid()));
            m_QueueName.append(s_QueueNameUUID);

            m_ResponseQueueName.assign("/lwpu-");
            m_ResponseQueueName.append(Utility::StrPrintf("(ppid = %d)-", getppid()));
            m_ResponseQueueName.append(s_ResponseQueueNameUUID);
        }

        ~LinuxRestarter()
        {
            if (-1 != m_SyncQueue)
            {
                mq_close(m_SyncQueue);
                if (m_IsController)
                {
                    mq_unlink(m_QueueName.c_str());
                }
            }
            if (-1 != m_RespQueue)
            {
                mq_close(m_RespQueue);
                if (m_IsController)
                {
                    mq_unlink(m_ResponseQueueName.c_str());
                }
            }
        }

    private:
        // This class sets up a grammar to parse child messages that look like
        // CHILD_NEED_RESTART(RC = <num>) inside ProcessChildMessages.
        class ChildNeedRestartGrammar
          : public qi::grammar<
                char *             // this class will parse using a pointer to
                                   // char
              , unsigned int()     // the grammar returns unsigned int and have
                                   // no parameters
              , ascii::space_type  // the whitespace skipper skips ASCII spaces
              >
        {
        public:
            ChildNeedRestartGrammar()
              : ChildNeedRestartGrammar::base_type(m_Start)
            {
                // Define m_Num as case insensitive "0x" followed by a
                // hexadecimal number or a decimal number. qi::lexeme unites
                // several tokens into one lexeme, meaning no white space
                // is allowed after 0x.
                m_Num = qi::lexeme[ascii::no_case["0x"] >> qi::hex] | qi::uint_;

                // This is the full grammar definition, i.e. the start symbol of
                // the grammar. It reads: our start symbol is
                // "CHILD_NEED_RESTART" text, followed by '(', followed by "RC",
                // followed by '=', followed by a number, followed by ')'.
                m_Start =
                    qi::lit(NeedRestartMessage()) >> '(' >> "RC" >> '=' >> m_Num >> ')';
            }

        private:
            qi::rule<char *, unsigned int(), ascii::space_type> m_Start;
            qi::rule<char *, unsigned int(), ascii::space_type> m_Num;
        };

        // This class sets up a grammar to parse child messages that look like
        // CHILD_RUN_SUCCESS(RC = <num>) inside ProcessChildMessages.
        class ChildSuccessGrammar
          : public qi::grammar<
                char *             // this class will parse using a pointer to
                                   // char
              , unsigned int()     // the grammar returns unsigned int and have
                                   // no parameters
              , ascii::space_type  // the whitespace skipper skips ASCII spaces
              >
        {
        public:
            ChildSuccessGrammar()
              : ChildSuccessGrammar::base_type(m_Start)
            {
                // Define m_Num as case insensitive "0x" followed by a
                // hexadecimal number or a decimal number. qi::lexeme unites
                // several tokens into one lexeme, meaning no white space
                // is allowed after 0x.
                m_Num = qi::lexeme[ascii::no_case["0x"] >> qi::hex] | qi::uint_;

                // This is the full grammar definition, i.e. the start symbol of
                // the grammar. It reads: our start symbol is
                // "CHILD_RUN_SUCCESS" text, followed by '(', followed by "RC",
                // followed by '=', followed by a number, followed by ')'.
                m_Start =
                    qi::lit(SuccessMessage()) >> '(' >> "RC" >> '=' >> m_Num >> ')';
            }

        private:
            qi::rule<char *, unsigned int(), ascii::space_type> m_Start;
            qi::rule<char *, unsigned int(), ascii::space_type> m_Num;
        };

        // This class sets up a grammar to parse child messages that look like
        // CHILD_RUN_FAILURE(RC = <num>) inside ProcessChildMessages.
        // char * - this class will parse using a pointer to char.
        // unsigned int() - the grammar returns unsigned int and have no
        // parameters.
        // ascii::space_type - the whitspace skipper skips ASCII spaces
        class ChildFailureGrammar
          : public qi::grammar<
                char *             // this class will parse using a pointer to
                                   // char
              , unsigned int()     // the grammar returns unsigned int and have
                                   // no parameters
              , ascii::space_type  // the whitespace skipper skips ASCII spaces
              >
        {
        public:
            ChildFailureGrammar()
              : ChildFailureGrammar::base_type(m_Start)
            {
                // Define m_Num as case insensitive "0x" followed by a
                // hexadecimal number or a decimal number. qi::lexeme unites
                // several tokens into one lexeme, meaning no white space
                // is allowed after 0x.
                m_Num = qi::lexeme[ascii::no_case["0x"] >> qi::hex] | qi::uint_;

                // This is the full grammar definition, i.e. the start symbol of
                // the grammar. It reads: our start symbol is
                // "CHILD_RUN_FAILURE" text, followed by '(', followed by "RC",
                // followed by '=', followed by a number, followed by ')'.
                m_Start =
                    qi::lit(FailureMessage()) >> '(' >> "RC" >> '=' >> m_Num >> ')';
            }

        private:
            qi::rule<char *, unsigned int(), ascii::space_type> m_Start;
            qi::rule<char *, unsigned int(), ascii::space_type> m_Num;
        };

        mqd_t GetSyncQueue() const
        {
            return m_SyncQueue;
        }

        mqd_t GetRespQueue() const
        {
            return m_RespQueue;
        }

        size_t GetStartsCount() const
        {
            return m_Controller.GetStartsCount();
        }

        optional<ExitStatusEnum> GetPostedResult() const
        {
            return m_Controller.GetPostedResult();
        }

        optional<RC> GetPostedRC() const
        {
            return m_Controller.GetPostedRC();
        }

        void SetPostedResult(ExitStatusEnum res, RC childRC)
        {
            m_Controller.SetPostedResult(res);
            m_Controller.SetPostedRC(childRC);
        }

        void ResetPostedResult()
        {
            m_Controller.ResetPostedResult();
            m_Controller.ResetPostedRC();
        }

        static
        const string& AckMessage()
        {
            return s_AckMessage;
        }

        static
        const string& ReadyMessage()
        {
            return s_ParentIsReady;
        }

        static
        const string& NeedRestartMessage()
        {
            return s_NeedRestartMessage;
        }

        static
        const string& SuccessMessage()
        {
            return s_SuccessMessage;
        }

        static
        const string& FailureMessage()
        {
            return s_FailureMessage;
        }

        static
        const string& GetStartCountReq()
        {
            return s_GetStartCountReq;
        }

        virtual RC DoIsController(bool *isController);

        virtual RC DoGetController(Controller **controller)
        {
            MASSERT(controller);
            RC rc;

            if (m_IsController)
            {
                *controller = &m_Controller;
            }
            else
            {
                *controller = nullptr;
            }

            return rc;
        }

        virtual RC DoGetPerformer(Performer **performer)
        {
            MASSERT(performer);
            RC rc;

            if (!m_IsController)
            {
                *performer = &m_Performer;
            }
            else
            {
                *performer = nullptr;
            }

            return rc;
        }

        RC NotifySetup();

        RC StopNotifications();

        static void NotificationThread(sigval sv);

        RC ProcessChildMessages();

        RC EmptySyncQueue();

        LinuxController      m_Controller;
        LinuxPerformer       m_Performer;

        mqd_t                m_SyncQueue;
        mqd_t                m_RespQueue;
        bool                 m_IsController;

        LinuxMutex           m_NotifySetupGuard;

        string m_QueueName;
        string m_ResponseQueueName;

        static const char *const s_QueueNameUUID;
        static const char *const s_ResponseQueueNameUUID;

        static const string s_AckMessage;
        static const string s_NeedRestartMessage;
        static const string s_SuccessMessage;
        static const string s_FailureMessage;
        static const string s_GetStartCountReq;
        static const string s_ParentIsReady;
    };

    const char *const LinuxRestarter::s_QueueNameUUID = "4d925bf6-9e0c-11e5-a37b-080027419c8c";
    const char *const LinuxRestarter::s_ResponseQueueNameUUID = "341529b6-9f00-11e5-a6dd-080027419c8c";

    const string LinuxRestarter::s_AckMessage = "CHILD_START_ACK";
    const string LinuxRestarter::s_NeedRestartMessage = "CHILD_NEED_RESTART";
    const string LinuxRestarter::s_SuccessMessage = "CHILD_RUN_SUCCESS";
    const string LinuxRestarter::s_FailureMessage = "CHILD_RUN_FAILURE";
    const string LinuxRestarter::s_GetStartCountReq = "GET_START_COUNTER";
    const string LinuxRestarter::s_ParentIsReady = "PARENT_IS_READY";

    /* virtual */
    RC LinuxRestarter::DoIsController(bool *isController)
    {
        MASSERT(nullptr != isController);

        RC rc;

        *isController = false;

        // We were initialized and the mode of operation is already known.
        if (-1 != m_SyncQueue)
        {
            *isController = m_IsController;
            return rc;
        }

        // Do initialization.

        // Try open the queue. At this moment we use the queue descriptor just
        // as a named sync kernel object to determine whether another instance
        // of us is running.
        dprintf("try open sync queue\n");
        errno = 0;
        m_SyncQueue = mq_open(m_QueueName.c_str(), O_RDWR);
        if (-1 == m_SyncQueue && ENOENT == errno)
        {
            dprintf("the sync queue doesn't exist\n");
            // The queue doesn't exist, we are the first process, we will
            // control the run of the real processes that will perform some
            // work. We will monitor the result and restart if it's needed.

            m_QueueName.assign("/lwpu-");
            m_QueueName.append(Utility::StrPrintf("(ppid = %d)-", getpid()));
            m_QueueName.append(s_QueueNameUUID);

            m_ResponseQueueName.assign("/lwpu-");
            m_ResponseQueueName.append(Utility::StrPrintf("(ppid = %d)-", getpid()));
            m_ResponseQueueName.append(s_ResponseQueueNameUUID);

            mq_attr attr;
            memset(&attr, 0, sizeof attr);
            attr.mq_msgsize = 64;
            attr.mq_maxmsg = 10;

            dprintf("create the sync queue\n");
            // Create the sync queue.
            errno = 0;
            ENCHECKC(mq_open,
                m_SyncQueue = mq_open(m_QueueName.c_str(),
                                      O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR, &attr));

            dprintf("create the response queue\n");
            // Create the response queue.
            errno = 0;
            ENCHECKC(mq_open,
                m_RespQueue = mq_open(m_ResponseQueueName.c_str(),
                                      O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR, &attr));

            *isController = m_IsController = true;

            return OK;
        }
        else
        {
            // mq_open failed because of other reasons than it doesn't exist.
            ENCHECK(mq_open);

            dprintf("successfully opened the sync queue\n");
            dprintf("open the response queue\n");
            // Open the response queue. It is used to get responses from the
            // controller.
            errno = 0;
            ENCHECKC(mq_open,
                m_RespQueue = mq_open(m_ResponseQueueName.c_str(), O_RDWR));

            // Get the queue maximum message size.
            mq_attr attr;
            mq_getattr(m_RespQueue, &attr);

            // The buffer must be the maximum size of the message
            vector<char> buf(attr.mq_msgsize);

            // To make a non-blocking call get absolute timeout equal the current time.
            timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);

            dprintf("try to get a message from the parent\n");
            // Get a message from the parent if any.
            errno = 0;
            ssize_t msgSize = mq_timedreceive(m_RespQueue, &buf[0], buf.size(), nullptr, &timeout);
            if (-1 != msgSize && ETIMEDOUT != errno)
            {
                dprintf("received a message from the parent: %s\n", &buf[0]);
                // The parent had to post its pid, this way we confirm that the
                // queue is alive, not just left after some controller failure
                // that didn't unlink the queue with a mq_unlink(m_queueName)
                // call.
                pid_t ppid = atoi(&buf[0]);
                if (getppid() == ppid)
                {
                    // Send acknowledge message to the parent.
                    dprintf("sending an acknowledge message to the parent\n");
                    errno = 0;
                    int sendRes;
                    ENCHECKC(mq_send,
                        sendRes = mq_send(m_SyncQueue, s_AckMessage.c_str(), s_AckMessage.length(), 0));

                    *isController = m_IsController = false;

                    // Wait until the parent establishes its queue notification
                    // and will be able to accept our messages.
                    errno = 0;
                    ENCHECKC(mq_receive,
                        msgSize = mq_receive(m_RespQueue, &buf[0], buf.size(), nullptr));
                    if (0 != strncmp(s_ParentIsReady.c_str(), &buf[0], msgSize))
                    {
                        dprintf("failed to receive parent is ready message\n");
                        Printf(
                            Tee::PriError,
                            "%s: failed to receive parent is ready message\n", __func__
                        );
                        return RC::SOFTWARE_ERROR;
                    }
                    dprintf("received parent is ready message\n");

                    return OK;
                }
                else
                {
                    Printf(
                        Tee::PriError,
                        "%s: Received parent pid %u doesn't correspond to the real ppid = %u.\n",
                        __func__, ppid, getppid()
                    );
                    return RC::SOFTWARE_ERROR;
                }
            }
            else
            {
                if (ETIMEDOUT == errno)
                {
                    Printf(
                        Tee::PriError,
                        "%s: the message queue exists, but there are no messages.\n", __func__
                    );
                }
                else
                {
                    ENCHECK(mq_timedreceive);
                }
                return RC::SOFTWARE_ERROR;
            }
        }

        return rc;
    }

    RC LinuxRestarter::NotifySetup()
    {
        LinuxMutexHolder lock(m_NotifySetupGuard);
        RC rc;

        Printf(Tee::PriLow, "Register queue notification\n");

        sigevent sev;
        sev.sigev_notify = SIGEV_THREAD;
        sev.sigev_notify_function = NotificationThread;
        sev.sigev_notify_attributes = nullptr;
        sev.sigev_value.sival_ptr = this;

        dprintf("registering queue notification\n");
        ENCHECKC(mq_notify,
            mq_notify(m_SyncQueue, &sev));

        return rc;
    }

    RC LinuxRestarter::StopNotifications()
    {
        LinuxMutexHolder lock(m_NotifySetupGuard);
        RC rc;

        Printf(Tee::PriLow, "Stop notifications\n");
        dprintf("stop notifications\n");
        ENCHECKC(mq_notify, mq_notify(m_SyncQueue, nullptr));

        return rc;
    }

    void LinuxRestarter::NotificationThread(sigval sv)
    {
        LinuxRestarter *This = reinterpret_cast<LinuxRestarter *>(sv.sival_ptr);
        LinuxMutexHolder lock(This->m_NotifySetupGuard);

        Printf(Tee::PriLow, "Notified about a new message\n");

        dprintf("received a message notification\n");

        // We have to register notification again
        This->NotifySetup();

        This->ProcessChildMessages();

        Printf(Tee::PriLow, "Exit notification thread\n");
        dprintf("exiting notification thread\n");
    }

    RC LinuxRestarter::ProcessChildMessages()
    {
        RC rc;

        ssize_t msgSize;
        mq_attr attr;

        dprintf("processing messages\n");
        // Get the queue maximum message size.
        mq_getattr(m_SyncQueue, &attr);

        // The buffer must be the maximum size of the message, add 1 byte for null terminator
        vector<char> buf(attr.mq_msgsize + 1);

        ChildNeedRestartGrammar restartGrammar;
        ChildSuccessGrammar     successGrammar;
        ChildFailureGrammar     failureGrammar;

        do
        {
            // To make a non-blocking call get absolute timeout equal the current time.
            timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);

            // Get a message from a child if any.
            errno = 0;
            msgSize = mq_timedreceive(m_SyncQueue, &buf[0], buf.size(), nullptr, &timeout);
            if (-1 != msgSize)
            {
                dprintf("received message \"%s\"\n", &buf[0]);
                buf[msgSize] = 0;
                Printf(Tee::PriLow, "Received message \"%s\"\n", &buf[0]);
                unsigned int childRC;
                if (0 == strncmp(s_GetStartCountReq.c_str(), &buf[0], msgSize))
                {
                    dprintf("received a request for starts counter\n");
                    Printf(Tee::PriLow, "Received a request for starts counter\n");
                    string startsCountStr =
                        StrPrintf("%lu", static_cast<long unsigned int>(GetStartsCount()));
                    dprintf("sending response %s\n", startsCountStr.c_str());
                    Printf(Tee::PriLow, "Sending response %s\n", startsCountStr.c_str());
                    errno = 0;
                    int sendRes;
                    ENCHECKC(mq_send,
                        sendRes = mq_send(m_RespQueue,
                                          startsCountStr.c_str(), startsCountStr.length(), 0));
                }
                else if (
                    qi::phrase_parse(&buf[0], &buf[0] + msgSize,
                                     restartGrammar, ascii::space, childRC)
                )
                {
                    dprintf("received need a restart\n");
                    SetPostedResult(ExitStatus::needRestart, childRC);
                }
                else if (
                    qi::phrase_parse(&buf[0], &buf[0] + msgSize,
                                     successGrammar, ascii::space, childRC)
                )
                {
                    dprintf("received success\n");
                    SetPostedResult(ExitStatus::success, childRC);
                }
                else if (
                    qi::phrase_parse(&buf[0], &buf[0] + msgSize,
                                     failureGrammar, ascii::space, childRC)
                )
                {
                    dprintf("received failure\n");
                    SetPostedResult(ExitStatus::failure, childRC);
                }
            }
        } while (-1 != msgSize);

        return rc;
    }

    /* virtual */
    RC LinuxRestarter::EmptySyncQueue()
    {
        RC rc;

        ssize_t msgSize;
        mq_attr attr;

        dprintf("clear the sync queue\n");
        // Get the queue maximum message size.
        mq_getattr(m_SyncQueue, &attr);

        // The buffer must be the maximum size of the message, add 1 byte for null terminator
        vector<char> buf(attr.mq_msgsize + 1);

        do
        {
            // To make a non-blocking call get absolute timeout equal the current time.
            timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            errno = 0;
            msgSize = mq_timedreceive(m_SyncQueue, &buf[0], buf.size(), nullptr, &timeout);
            dprintf("a message during queue cleanup: %s\n", &buf[0]);
        } while (-1 != msgSize);

        if (ETIMEDOUT != errno)
        {
            ENCHECK(mq_timedreceive);
        }

        return rc;
    }

    inline
    mqd_t LinuxController::GetSyncQueue() const
    {
        return m_Restarter.GetSyncQueue();
    }

    inline
    mqd_t LinuxController::GetRespQueue() const
    {
        return m_Restarter.GetRespQueue();
    }

    inline
    RC LinuxController::ProcessChildMessages()
    {
        return m_Restarter.ProcessChildMessages();
    }

    inline
    RC LinuxController::EmptySyncQueue()
    {
        return m_Restarter.EmptySyncQueue();
    }

    inline
    RC LinuxController::NotifySetup()
    {
        return m_Restarter.NotifySetup();
    }

    inline
    RC LinuxController::StopNotifications()
    {
        return m_Restarter.StopNotifications();
    }

    /* static */
    inline
    const string& LinuxController::AckMessage()
    {
        return LinuxRestarter::AckMessage();
    }

    /* static */
    inline
    const string& LinuxController::ReadyMessage()
    {
        return LinuxRestarter::ReadyMessage();
    }

    /* virtual */
    RC LinuxController::DoStartNewAndWait(int argc, char *argv[], bool *exitedNormally,
                                          ExitStatusEnum *exitStatus, int *exitCode, RC *childRC)
    {
        MASSERT(nullptr != exitStatus);
        MASSERT(nullptr != exitCode);
        MASSERT(nullptr != exitedNormally);

        dprintf("starting a new child\n");

        RC rc;
        ssize_t msgSize;
        mq_attr attr;

        CHECK_RC(EmptySyncQueue());
        CHECK_RC(StopNotifications());

        // Advertise the parent's PID
        std::string ppidStr = StrPrintf("%d", getpid());
        errno = 0;
        int sendRes;
        dprintf("sending our pid: %s\n", ppidStr.c_str());
        ENCHECKC(mq_send,
            sendRes = mq_send(GetRespQueue(), ppidStr.c_str(), ppidStr.length(), 0));

        // Get the queue maximum message size.
        mq_getattr(GetSyncQueue(), &attr);

        // The buffer must be the maximum size of the message, add 1 byte for null terminator
        vector<char> buf(attr.mq_msgsize + 1);

        // Reset storage for posted result
        ResetPostedResult();

        pid_t pid;
        errno = 0;
        int res = posix_spawn(&pid, argv[0], nullptr, nullptr, argv, elwiron);
        ENCHECK(posix_spawn);
        ENCHECKR(posix_spawn, res);
        // posix_spawn can return no error if the fork call was successful, but
        // execv failed. For example, if execv can't load the exelwtable file, we
        // will miss the error. The only way to check is to poll the child process
        // id with non-blocking call to waitid. Also it is unknown who parent or
        // child first has access to CPU, therefore we can end up here after fork
        // but before execv inside the child process had a chance to fail. Therefore
        // we have to wait for a while.
        timespec t;
        t.tv_sec = 0;
        t.tv_nsec = 50 * 1000 * 1000;
        nanosleep(&t, 0);
        siginfo_t sigInfo;
        int waitidRes;
        dprintf("polling the child with waitid\n");
        ENCHECKC(waitid,
            waitidRes = waitid(P_PID, pid, &sigInfo, WEXITED | WNOHANG))
        dprintf("the child is alive\n");

        dprintf("wait for the acknowledge message\n");
        // Wait for the acknowledge message
        errno = 0;
        ENCHECKC(mq_receive,
            msgSize = mq_receive(GetSyncQueue(), &buf[0], buf.size(), nullptr));
        if (0 != strncmp(AckMessage().c_str(), &buf[0], msgSize))
        {
            dprintf("failed to receive child start acknowledge message\n");
            Printf(
                Tee::PriError,
                "%s: failed to receive child start acknowledge message\n", __func__
            );
            return RC::SOFTWARE_ERROR;
        }
        dprintf("received acknowledge from the child\n");
        Printf(Tee::PriLow, "Received acknowledge from the child\n");

        // Register to get notifications about new messages
        CHECK_RC(NotifySetup());
        // Notification won't work if the queue is not empty, the child will
        // hold on with its messages until we tell it we are ready to receive.
        dprintf("sending we are ready to receive messages\n");
        errno = 0;
        ENCHECKC(mq_send,
            mq_send(GetRespQueue(), ReadyMessage().c_str(), ReadyMessage().length(), 0));

        dprintf("waiting for the child to exit\n");
        Printf(Tee::PriLow, "Waiting for the child to exit\n");
        // Wait for the child to exit
        ENCHECKC(waitid,
            waitid(P_PID, pid, &sigInfo, WEXITED));
        dprintf("child exited\n");
        CHECK_RC(StopNotifications());
        CHECK_RC(ProcessChildMessages());

        Printf(Tee::PriLow, "Child exited\n");

        IncrementStartsCount();
        *exitCode = sigInfo.si_status;
        *exitedNormally = CLD_EXITED == sigInfo.si_code;

        // Get status from the child
        if (GetPostedResult())
        {
            dprintf("child posted result %d\n", *GetPostedResult());
            Printf(Tee::PriLow, "Child posted result %d\n", *GetPostedResult());
            *exitStatus = *GetPostedResult();
        }
        else
        {
            *exitStatus = ExitStatus::unknown;
        }

        if (GetPostedRC())
        {
            dprintf("child posted RC %d\n", GetPostedRC()->Get());
            Printf(Tee::PriLow, "Child posted RC %d\n", GetPostedRC()->Get());
            *childRC = *GetPostedRC();
        }
        else
        {
            *childRC = OK;
        }

        return rc;
    }

    inline
    mqd_t LinuxPerformer::GetSyncQueue() const
    {
        return m_Restarter.GetSyncQueue();
    }

    inline
    mqd_t LinuxPerformer::GetRespQueue() const
    {
        return m_Restarter.GetRespQueue();
    }

    /* static */
    inline
    const string& LinuxPerformer::NeedRestartMessage()
    {
        return LinuxRestarter::NeedRestartMessage();
    }

    /* static */
    inline
    const string& LinuxPerformer::SuccessMessage()
    {
        return LinuxRestarter::SuccessMessage();
    }

    /* static */
    inline
    const string& LinuxPerformer::FailureMessage()
    {
        return LinuxRestarter::FailureMessage();
    }

    /* static */
    inline
    const string& LinuxPerformer::GetStartCountReq()
    {
        return LinuxRestarter::GetStartCountReq();
    }

    /* virtual */
    RC LinuxPerformer::DoConnectResultPoster(const SlotType &slot)
    {
        m_Signal = slot;

        return OK;
    }

    /* virtual */
    RC LinuxPerformer::DoIlwokeResultPoster(INT32 rc)
    {
        m_Signal(this, rc);

        return OK;
    }

    /* virtual */
    bool LinuxPerformer::DoHasResultPoster() const
    {
        return nullptr != m_Signal;
    }

    /* virtual */
    RC LinuxPerformer::DoPostNeedRestart(RC childRC)
    {
        RC rc;

        string msg = StrPrintf("%s(RC = %#x)", NeedRestartMessage().c_str(), childRC.Get());

        dprintf("posting need restart: %s\n", msg.c_str());
        errno = 0;
        ENCHECKC(mq_send,
            mq_send(GetSyncQueue(), msg.c_str(), msg.length(), 0));

        m_LastPostedResult = ExitStatus::needRestart;

        return rc;
    }

    /* virtual */
    RC LinuxPerformer::DoPostSuccess(RC childRC)
    {
        RC rc;

        string msg = StrPrintf("%s(RC = %#x)", SuccessMessage().c_str(), childRC.Get());

        dprintf("posting success: %s\n", msg.c_str());
        errno = 0;
        ENCHECKC(mq_send,
            mq_send(GetSyncQueue(), msg.c_str(), msg.length(), 0));

        m_LastPostedResult = ExitStatus::success;

        return rc;
    }

    /* virtual */
    RC LinuxPerformer::DoPostFailure(RC childRC)
    {
        RC rc;

        string msg = StrPrintf("%s(RC = %#x)", FailureMessage().c_str(), childRC.Get());

        errno = 0;
        dprintf("posting failure: %s\n", msg.c_str());
        ENCHECKC(mq_send,
            mq_send(GetSyncQueue(), msg.c_str(), msg.length(), 0));

        m_LastPostedResult = ExitStatus::failure;

        return rc;
    }

    /* virtual */
    RC LinuxPerformer::DoGetStartsCount(size_t *count) const
    {
        MASSERT(nullptr != count);

        RC rc;

        Printf(Tee::PriLow, "Sending a request %s\n", GetStartCountReq().c_str());
        dprintf("sending a request for start count: %s\n", GetStartCountReq().c_str());
        errno = 0;
        ENCHECKC(mq_send,
            mq_send(GetSyncQueue(), GetStartCountReq().c_str(), GetStartCountReq().length(), 0));

        ssize_t msgSize;
        mq_attr attr;

        // Get the queue maximum message size.
        mq_getattr(GetSyncQueue(), &attr);

        // The buffer must be the maximum size of the message, add 1 byte for null terminator
        vector<char> buf(attr.mq_msgsize + 1);

        Printf(Tee::PriLow, "Wait for a response to %s\n", GetStartCountReq().c_str());
        dprintf("waiting for a response to %s\n", GetStartCountReq().c_str());
        errno = 0;
        ENCHECKC(mq_receive,
            msgSize = mq_receive(GetRespQueue(), &buf[0], buf.size(), nullptr));
        buf[msgSize] = 0;
        dprintf("got a response to %s = %s\n", GetStartCountReq().c_str(), &buf[0]);
        Printf(Tee::PriLow, "Get a response to %s = %s\n", GetStartCountReq().c_str(), &buf[0]);
        *count = atoi(&buf[0]);

        return rc;
    }

    RC LinuxPerformer::DoGetLastPostedResult(ExitStatusEnum *res) const
    {
        MASSERT(res);

        *res = m_LastPostedResult;

        return OK;
    }
}

ProcCtrl::Restarter * CreateProcessRestarter()
{
    return new ProcCtrl::LinuxRestarter;
}
