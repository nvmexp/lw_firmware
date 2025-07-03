/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef RESTARTER_H
#define RESTARTER_H

#include <stddef.h>

#include "rc.h"

namespace ProcCtrl
{
    class Controller;
    class Performer;

    namespace ExitStatus
    {
        enum ExitStatusEnum
        {
            success, failure, needRestart, unknown
        };
    }
    using ExitStatus::ExitStatusEnum;

    /**
     * @brief Restarts the process and manages synchronization between the
     * parent and the child.
     *
     * If one wants to start testing over again, she cannot just jump again to
     * the beginning of the code. MODS process has to finish first gracefully.
     * The following simple interface allows the user to accomplish this task.
     * The usage scenario is the following. When the first process starts the
     * IsController method will return true and this process will be allowed to
     * perform special controlling tasks using the Controller interface. The
     * Controller can start a new process and when the child process calls
     * IsController, it will return false and the child will be able to use
     * Performer interface to report back to the controller whether the test was
     * successful or a restart is needed.
     *
     * Simple usage example:
     *  boost::shared_ptr<Restarter> restarter(CreateProcessRestarter());
     *
     *  bool isControler;
     *  restarter->IsController(&isControler);
     *
     *  if (isControler)
     *  {
     *      ExitStatusEnum exitStatus;
     *      int exitCode;
     *      RC childStartRes;
     *      Controller *ctrl(nullptr);
     *      restarter->GetController(&ctrl);
     *      do
     *      {
     *          childStartRes = ctrl->StartNewAndWait(argc, argv, &exitStatus, &exitCode);
     *      } while (OK == childStartRes && ExitStatus::needRestart == exitStatus);
     *
     *      return exitCode;
     *  }
     *  else
     *  {
     *      Performer *pf(nullptr);
     *      restarter->GetPerformer(&pf);
     *
     *      // test here
     *
     *      size_t count;
     *      pf->GetStartsCount(&count);
     *      if (3 > count) pf->PostNeedRestart();
     *      else pf->PostSuccess();
     *
     *      return 22;
     *  }
     */
    class Restarter
    {
    public:
        virtual ~Restarter()
        {}

        RC IsController(bool *isController)
        {
            return DoIsController(isController);
        }

        RC GetController(Controller **controller)
        {
            return DoGetController(controller);
        }

        RC GetPerformer(Performer **performer)
        {
            return DoGetPerformer(performer);
        }

    private:
        virtual RC DoIsController(bool *isController) = 0;
        virtual RC DoGetController(Controller **controller) = 0;
        virtual RC DoGetPerformer(Performer **performer) = 0;
    };

    /**
     * @brief Starts new child processes
     */
    class Controller
    {
    public:
        virtual ~Controller()
        {}

        RC StartNewAndWait(int argc, char *argv[], bool *exitedNormally, ExitStatusEnum *exitStatus,
                           int *exitCode, RC *childRC)
        {
            return DoStartNewAndWait(argc, argv, exitedNormally, exitStatus, exitCode, childRC);
        }

        RC GetStartsCount(size_t *count) const
        {
            return DoGetStartsCount(count);
        }

    private:
        virtual RC DoStartNewAndWait(int argc, char *argv[], bool *exitedNormally,
                                     ExitStatusEnum *exitStatus, int *exitCode, RC *childRC) = 0;
        virtual RC DoGetStartsCount(size_t *count) const = 0;
    };

    /**
    * @brief Posts exelwtion result to the parent process
    */
    class Performer
    {
    public:
        typedef void (*SignalType)(ProcCtrl::Performer *, INT32 rc);
        typedef void (*SlotType)(ProcCtrl::Performer *, INT32 rc);

        virtual ~Performer()
        {}

        RC ConnectResultPoster(const SlotType &slot)
        {
            return DoConnectResultPoster(slot);
        }

        RC IlwokeResultPoster(INT32 rc)
        {
            return DoIlwokeResultPoster(rc);
        }

        bool HasResultPoster() const
        {
            return DoHasResultPoster();
        }

        RC PostNeedRestart(RC childRC)
        {
            return DoPostNeedRestart(childRC);
        }

        RC PostSuccess(RC childRC)
        {
            return DoPostSuccess(childRC);
        }

        RC PostFailure(RC childRC)
        {
            return DoPostFailure(childRC);
        }

        RC GetStartsCount(size_t *count) const
        {
            return DoGetStartsCount(count);
        }

        RC GetLastPostedResult(ExitStatusEnum *res) const
        {
            return DoGetLastPostedResult(res);
        }

    private:
        virtual RC   DoConnectResultPoster(const SlotType &slot) = 0;
        virtual RC   DoIlwokeResultPoster(INT32 rc) = 0;
        virtual bool DoHasResultPoster() const = 0;
        virtual RC   DoPostNeedRestart(RC childRC) = 0;
        virtual RC   DoPostSuccess(RC childRC) = 0;
        virtual RC   DoPostFailure(RC childRC) = 0;
        virtual RC   DoGetStartsCount(size_t *count) const = 0;
        virtual RC   DoGetLastPostedResult(ExitStatusEnum *res) const = 0;
    };
}

ProcCtrl::Restarter * CreateProcessRestarter();

#endif
