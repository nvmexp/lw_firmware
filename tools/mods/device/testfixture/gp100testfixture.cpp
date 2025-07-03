/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <string>
#include <memory>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/end.hpp>
#include <boost/xpressive/xpressive.hpp>

#include "core/include/jscript.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "core/include/utility.h"
#include "core/include/xp.h"

#include "device/include/gp100testfixture.h"
#include "core/include/fileholder.h"

namespace fs = boost::filesystem;
using namespace boost::xpressive;

using Utility::StrPrintf;

namespace
{
    const unsigned int s_E2998USBVendorId = 403;
    const unsigned int s_E2998USBProductId = 6001;

    const size_t s_OuttakeFans[2] = { 1, 2 };
    const size_t s_IntakeFans[2] = { 3, 4 };

    const UINT64 s_Timeout = 2000;

    const UINT32 availableClocks[] = { 0, 100, 133, 156 };

    const char *s_Prompt = "uart> ";
    const char *s_TemperatureCmd = "temp";
    const char *s_TachCmd = "tach -a";
}

GP100TestFixture::GP100TestFixture()
  : m_Ser(nullptr)
{
    bool found = false;
    string ttyName;
    // This directory lists all UARTs emulated by USB connected devices.
    fs::path usbSerialDevices = "/sys/bus/usb-serial/devices";
    if (fs::exists(usbSerialDevices))
    {
        fs::directory_iterator end;
        for (fs::directory_iterator it(usbSerialDevices); end != it; ++it)
        {
            if (fs::is_symlink(*it))
            {
                // Each symbolic link in this directory points to the correspondent device
                // description. For example it can be
                // /sys/devices/pci0000:00/0000:00:14.0/usb1/1-10/1-10.4/1-10.4:1.0/ttyUSB0
                fs::path devPath = usbSerialDevices / fs::read_symlink(*it);

                // Go one level up and read uevent file. For example
                // /sys/devices/pci0000:00/0000:00:14.0/usb1/1-10/1-10.4/1-10.4:1.0/uevent
                fs::path ueventPath = devPath.parent_path() / "uevent";
                string uevent(fs::file_size(ueventPath), 0);
                FileHolder ueventFileHolder(ueventPath.string().c_str(), "r");
                FILE* ueventFile = ueventFileHolder.GetFile();
                size_t nRead = fread(&uevent[0], 1, uevent.size(), ueventFile);
                uevent.resize(nRead);

                // Search PRODUCT=403/6001/600 in the uevent file. It contains vendor and
                // product IDs.
                sregex prodRe = "PRODUCT" >> *_s >> '=' >> *_s >> (s1 = +_d) >> '/' >> (s2 = +_d);
                smatch what;
                if (regex_search(uevent.begin(), uevent.end(), what, prodRe))
                {
                    unsigned int prodId;
                    unsigned int vendorId;

                    string num;

                    num.assign(what[1].first, what[1].second);
                    vendorId = atoi(num.c_str());

                    num.assign(what[2].first, what[2].second);
                    prodId = atoi(num.c_str());

                    if (s_E2998USBVendorId == vendorId && s_E2998USBProductId == prodId)
                    {
                        found = true;
                        ttyName = ("/dev" / it->path().filename()).string();
                        break;
                    }
                }
            }
        }
    }
    if (found)
    {
        m_Ser = GetSerialObj::GetCom(ttyName.c_str());
        StickyRC rc;
        rc = m_Ser->Initialize(SerialConst::CLIENT_NOT_IN_USE, true);
        rc = m_Ser->SetBaud(SerialConst::CLIENT_NOT_IN_USE, 115200);

        string response;
        rc = ExecCommand("\n", &response);

        if (OK != rc)
        {
            m_Ser = nullptr;
        }
    }
}

RC GP100TestFixture::ExecCommand(const string &cmd, string *response)
{
    MASSERT(nullptr != response);

    RC rc;

    for (string::const_iterator it = cmd.begin(); cmd.end() != it; ++it)
    {
        CHECK_RC(m_Ser->Put(SerialConst::CLIENT_NOT_IN_USE, *it));
        Tasker::Sleep(10);
    }

    response->clear();

    UINT64 start = Xp::QueryPerformanceCounter();
    UINT64 freq = Xp::QueryPerformanceFrequency();
    double elapsedTime(0);
    bool hasPrompt = false;

    do
    {
        UINT32 c;
        CHECK_RC(m_Ser->Get(SerialConst::CLIENT_NOT_IN_USE, &c));
        response->append(1, static_cast<char>(c));
        elapsedTime = double(Xp::QueryPerformanceCounter() - start) / freq;
        hasPrompt = boost::algorithm::ends_with(*response, s_Prompt);
    } while (s_Timeout > elapsedTime && !hasPrompt);

    if (!hasPrompt)
    {
        Printf(Tee::PriHigh, "Timeout on waiting for \"%s\" prompt\n", s_Prompt);
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC GP100TestFixture::GetTemperature(UINT32 *pTempCelsius)
{
    MASSERT(nullptr != pTempCelsius);
    RC rc;

    *pTempCelsius = 0;

    if (IsInited())
    {
        string result;
        CHECK_RC(ExecCommand(string(s_TemperatureCmd) + '\n', &result));

        smatch what;
        sregex temperatureCmdResult = as_xpr(s_TemperatureCmd) >> +_ln >> (s1 = +_d) >> 'C';
        if (regex_search(result.begin(), result.end(), what, temperatureCmdResult))
        {
            string num;

            num.assign(what[1].first, what[1].second);
            *pTempCelsius = atoi(num.c_str());
        }
        else
        {
            return RC::SERIAL_COMMUNICATION_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "No USB device to communicate with the board.\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC GP100TestFixture::GetTachometer(map<size_t, UINT32> *pFansRPM)
{
    MASSERT(nullptr != pFansRPM);
    RC rc;

    pFansRPM->clear();

    if (IsInited())
    {
        string result;
        CHECK_RC(ExecCommand(string(s_TachCmd) + '\n', &result));

        smatch what;
        sregex tachCmdResult = (s1 = +_d) >> *_s >> ':' >> *_s >> (s2 = +_d);
        string::const_iterator lwr = result.begin();
        while (regex_search(lwr, result.end(), what, tachCmdResult))
        {
            string num;
            size_t fanNum;

            num.assign(what[1].first, what[1].second);
            fanNum = atoi(num.c_str());

            num.assign(what[2].first, what[2].second);
            (*pFansRPM)[fanNum] = atoi(num.c_str());

            lwr = what[0].second;
        }

        if (pFansRPM->empty())
        {
            return RC::SERIAL_COMMUNICATION_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "No USB device to communicate with the board.\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC GP100TestFixture::GetIntakeFans(vector<size_t> *pIntakeIdx) const
{
    MASSERT(nullptr != pIntakeIdx);
    RC rc;

    pIntakeIdx->clear();
    copy(
        boost::begin(s_IntakeFans),
        boost::end(s_IntakeFans),
        back_inserter(*pIntakeIdx)
    );

    return rc;
}

bool GP100TestFixture::IsIntake(size_t fanNum) const
{
    return boost::end(s_IntakeFans) !=
        find(boost::begin(s_IntakeFans), boost::end(s_IntakeFans), fanNum);
}

RC GP100TestFixture::GetOuttakeFans(vector<size_t> *pOuttakeIdx) const
{
    RC rc;

    MASSERT(nullptr != pOuttakeIdx);

    pOuttakeIdx->clear();
    copy(
        boost::begin(s_OuttakeFans),
        boost::end(s_OuttakeFans),
        back_inserter(*pOuttakeIdx)
    );

    return rc;
}

bool GP100TestFixture::IsOuttake(size_t fanNum) const
{
    return boost::end(s_OuttakeFans) !=
        find(boost::begin(s_OuttakeFans), boost::end(s_OuttakeFans), fanNum);
}

RC GP100TestFixture::SetPWM(UINT32 pwm)
{
    using namespace boost::algorithm;

    RC rc;

    if (IsInited())
    {
        string cmd = StrPrintf("pwm %u\n", pwm);
        string result;

        // The HW interface with the board is very volatile and flaky. Sometimes it
        // reads garbage characters and then complains that the command wasn't
        // found.
        do
        {
            CHECK_RC(ExecCommand(cmd, &result));
        } while (contains(result, "Command not found:"));

        sregex okResponse = *_ >> (icase("OK") | icase("Okay")) >> +_ln >> s_Prompt;
        if (!regex_match(result, okResponse))
        {
            return RC::SERIAL_COMMUNICATION_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "No USB device to communicate with the board.\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC GP100TestFixture::SetClock(UINT32 clockMHz)
{
    using namespace boost::algorithm;

    RC rc;

    if (IsInited())
    {
        string cmd = StrPrintf("clock %u\n", clockMHz);
        string result;

        // The HW interface with the board is very volatile and flaky. Sometimes it
        // reads garbage characters and then complains that the command wasn't
        // found.
        do
        {
            CHECK_RC(ExecCommand(cmd, &result));
        } while (contains(result, "Command not found:"));

        sregex okResponse = *_ >> (icase("OK") | icase("Okay")) >> +_ln >> s_Prompt;
        sregex clockDisabledResponse = *_ >> icase("disabled") >> +_ln >> s_Prompt;
        if (
            (0 != clockMHz && !regex_match(result, okResponse)) ||
            (0 == clockMHz && !regex_match(result, clockDisabledResponse))
        )
        {
            return RC::SERIAL_COMMUNICATION_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "No USB device to communicate with the board.\n");
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

JS_CLASS(GP100TestFixture);

SObject GP100TestFixture_Object(
    "GP100TestFixture",
    GP100TestFixtureClass,
    0,
    0,
    ""
);

namespace
{
    GP100TestFixture& GetGP100TestFixture()
    {
        static GP100TestFixture gp100TestFixture;
        return gp100TestFixture;
    }
}

C_(GP100TestFixture_SetPWM);
static STest GP100TestFixture_SetPWM
(
    GP100TestFixture_Object,
    "SetPWM",
    C_GP100TestFixture_SetPWM,
    1,
    "Sets fans PWM in percents for GP100 test fixture board."
);

C_(GP100TestFixture_SetPWM)
{
    JavaScriptPtr js;
    UINT32 arg;
    if ((NumArguments != 1)  || (OK != js->FromJsval(pArguments[0], &arg)))
    {
        JS_ReportError(pContext, "Usage: GP100TestFixture.SetPWM(UINT32)");
        return JS_FALSE;
    }

    RC rc = GetGP100TestFixture().SetPWM(arg);
    if (OK != rc)
        Printf(Tee::PriNormal, "Failed to set PWM\n");

    RETURN_RC(rc);
}

C_(GP100TestFixture_SetClock);
static STest GP100TestFixture_SetClock
(
    GP100TestFixture_Object,
    "SetClock",
    C_GP100TestFixture_SetClock,
    1,
    "Sets LWLINK reference clock to 0, 100, 133 or 156 MHz."
);

C_(GP100TestFixture_SetClock)
{
    JavaScriptPtr js;
    UINT32 arg;
    if ((NumArguments != 1) || (OK != js->FromJsval(pArguments[0], &arg)))
    {
        JS_ReportError(pContext, "Usage: GP100TestFixture.SetClock(UINT32)");
        return JS_FALSE;
    }

    bool isAvailableFreq =
        boost::end(availableClocks) !=
        find(
            boost::begin(availableClocks),
            boost::end(availableClocks),
            arg
        );
    if (!isAvailableFreq)
    {
        string frqStr;
        for (const UINT32 *p = boost::begin(availableClocks); boost::end(availableClocks) != p; ++p)
        {
            if (boost::begin(availableClocks) != p)
            {
                frqStr += ", ";
            }
            frqStr += StrPrintf("%u", *p);
        }

        Printf(Tee::PriNormal, "Clocks can only be set to the following values: %s\n", frqStr.c_str());
        RETURN_RC(RC::BAD_PARAMETER);
    }

    RC rc = GetGP100TestFixture().SetClock(arg);
    if (OK != rc)
        Printf(Tee::PriNormal, "Failed to set LWLINK reference clock\n");

    RETURN_RC(rc);
}
