/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file mqdatainterface.cpp
//! \brief A message queue (MQ) interface to a data source (such as libFuzzer)
//!

#include <mqueue.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "mqdatainterface.h"
#include "mqmsgfmt.h"

RC MqDataInterface::Initialize(LwU32 maxLen)
{
    struct mq_attr attr;
    char           pathbuf[64];
    UcodeFuzzMsg   msg;

    m_LogVerbose = (getelw("UCODEFUZZ_MODS_MQ_LOG_VERBOSE") != nullptr);

    snprintf(pathbuf, sizeof(pathbuf), UCODEFUZZ_FUZZ_QUEUE_PREFIX "%d", getpid());
    mq_unlink(pathbuf);

    attr.mq_maxmsg = UCODEFUZZ_FUZZ_QUEUE_LENGTH;
    attr.mq_msgsize = sizeof(UcodeFuzzMsg);

    m_QdRecv = mq_open(pathbuf, O_CREAT | O_RDONLY, 660, &attr);
    if (m_QdRecv < 0)
    {
        Printf(Tee::PriError,
               "MqDataInterface: mq_open failed to open fuzz queue (for receiving): %s\n",
               strerror(errno));
        return RC::LWRM_ERROR;
    }
    m_Path = pathbuf;

    m_QdSend = mq_open(UCODEFUZZ_SERVER_QUEUE_NAME, O_WRONLY);
    if (m_QdSend < 0)
    {
        Printf(Tee::PriError,
               "MqDataInterface: mq_open failed to open server queue (for sending): %s\n",
               strerror(errno));
        return RC::LWRM_ERROR;
    }

    // initialize connection with libfuzzer (i.e. hello message)
    msg.msgType = UCODEFUZZ_MSG_PARAMS_FROM_MODS;
    msg.paramsFromMods.maxLen = maxLen;
    msg.paramsFromMods.modsPid = getpid();
    if (mq_send(m_QdSend, (char *) &msg, sizeof(msg), 0) < 0)
    {
        Printf(Tee::PriError,
               "MqDataInterface: mq_send failed to send initialization message: %s\n",
               strerror(errno));
        return RC::LWRM_ERROR;
    }

    Printf(Tee::PriNormal, "MqDataInterface: successfully sent initialization message\n");
    return RC::OK;
}

MqDataInterface::~MqDataInterface()
{
    mq_close(m_QdSend);
    mq_close(m_QdRecv);
    mq_unlink(m_Path.c_str());
}

RC MqDataInterface::NextBytes(std::vector<LwU8>& bytes)
{
    UcodeFuzzMsg  msg;

    if (mq_receive(m_QdRecv, (char *) &msg, sizeof(msg), NULL) < 0)
    {
        Printf(Tee::PriError,
               "MqDataInterface: mq_receive failed to receive fuzz data: %s\n",
               strerror(errno));
        return RC::LWRM_ERROR;
    }

    if (msg.msgType == UCODEFUZZ_MSG_TESTCASE_DATA)
    {
        LwU32 msgToReceive = msg.testcaseData.fuzzDataMsgCount;

        if (m_LogVerbose)
        {
            Printf(Tee::PriNormal,
                    "MqDataInterface: receiving %d fuzz data messages\n", msgToReceive);
        }

        // clear stale data
        bytes.clear();

        for (LwU32 i = 0; i < msgToReceive; i++)
        {
            memset(&msg, 0, sizeof(msg));

            if (mq_receive(m_QdRecv, (char *) &msg, sizeof(msg), NULL) < 0)
            {
                Printf(Tee::PriError,
                       "MqDataInterface: mq_receive failed to receive fuzz data message (index %d): %s\n",
                       i, strerror(errno));
                return RC::LWRM_ERROR;
            }

            if (msg.msgType == UCODEFUZZ_MSG_FUZZ_DATA)
            {
                if (m_LogVerbose)
                {
                    Printf(Tee::PriNormal,
                           "MqDataInterface: received fuzz data message (size %u)\n",
                           msg.fuzzData.size);
                }
                bytes.insert(bytes.end(), msg.fuzzData.data, msg.fuzzData.data + msg.fuzzData.size);
            }
            else
            {
                Printf(Tee::PriError,
                       "MqDataInterface: got unexpected message type while receiving fuzz data (THIS IS A BUG)\n");
                return RC::LWRM_ERROR;
            }
        }

        if (m_LogVerbose)
        {
            Printf(Tee::PriNormal,
                   "MqDataInterface: received all fuzz data (size %zu)\n", bytes.size());
        }
        return RC::OK;
    }
    else if (msg.msgType == UCODEFUZZ_MSG_STOP)
    {
        Printf(Tee::PriNormal, "MqDataInterface: received stop message\n");
        return RC::EXIT_OK;
    }
    else
    {
        Printf(Tee::PriError,
               "MqDataInterface: got unexpected message type while receiving fuzz data (THIS IS A BUG)\n");
        return RC::LWRM_ERROR;
    }
}

RC MqDataInterface::SubmitResult(const TestcaseResult& result)
{
    UcodeFuzzMsg      msg;
    UcodeFuzzCovType  covType = result.covType;
    LwU32             covMsgToSend;

    struct {
        struct {
            LwU32  pos;
            LwU32  elementsPerMsg;
        } sanitizerCoverage;
    } covState;

    msg.msgType = UCODEFUZZ_MSG_TESTCASE_RESULT;

    msg.testcaseResult.rc      = result.rc.Get();
    msg.testcaseResult.covType = result.covType;

    // initialize covState for this covType (if needed)
    // and determine number of coverage messages needed
    switch (covType)
    {
        case UCODEFUZZ_MSG_COV_SANITIZER_COVERAGE:
        {
            covState.sanitizerCoverage.pos = 0;
            // determine number of coverage messages needed based on number of elements per message
            covState.sanitizerCoverage.elementsPerMsg = \
                sizeof(msg.covSanitizerCoverage.data) / sizeof(msg.covSanitizerCoverage.data[0]);
            covMsgToSend = (result.covData.sanitizerCoverage.size() +
                            (covState.sanitizerCoverage.elementsPerMsg - 1)) /
                           covState.sanitizerCoverage.elementsPerMsg;
            break;
        }

        case UCODEFUZZ_MSG_COV_NONE:
        default:
            // fallback to no coverage
            covMsgToSend = 0;
            break;
    }
    msg.testcaseResult.covMsgCount = covMsgToSend;

    if (mq_send(m_QdSend, (char *) &msg, sizeof(msg), 0) < 0)
    {
        Printf(Tee::PriError,
                "MqDataInterface: mq_send failed to send testcase results: %s\n",
                strerror(errno));
        return RC::LWRM_ERROR;
    }

    while (covMsgToSend > 0)
    {
        switch (covType)
        {
            case UCODEFUZZ_MSG_COV_SANITIZER_COVERAGE:
            {
                msg.msgType = UCODEFUZZ_MSG_COV_SANITIZER_COVERAGE;
                msg.covSanitizerCoverage.numElements = \
                    std::min((LwU32) result.covData.sanitizerCoverage.size() - covState.sanitizerCoverage.pos,
                             covState.sanitizerCoverage.elementsPerMsg);
                std::copy(result.covData.sanitizerCoverage.begin() + covState.sanitizerCoverage.pos,
                          result.covData.sanitizerCoverage.begin() + covState.sanitizerCoverage.pos +
                            msg.covSanitizerCoverage.numElements,
                          msg.covSanitizerCoverage.data);
                covState.sanitizerCoverage.pos += msg.covSanitizerCoverage.numElements;
                break;
            }

            case UCODEFUZZ_MSG_COV_NONE:
            default:
                // impossible
                Printf(Tee::PriError,
                        "MqDataInterface: covType changed between sending testcase results and sending coverage data!");
                return RC::LWRM_ERROR;
        }

        if (mq_send(m_QdSend, (char *) &msg, sizeof(msg), 0) < 0)
        {
            Printf(Tee::PriError,
                    "MqDataInterface: mq_send failed to send coverage data: %s\n",
                    strerror(errno));
            return RC::LWRM_ERROR;
        }

        covMsgToSend--;
    }

    return RC::OK;
}
