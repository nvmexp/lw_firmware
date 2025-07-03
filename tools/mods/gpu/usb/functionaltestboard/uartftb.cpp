/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <boost/algorithm/string.hpp>

#include "core/include/regexp.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"
#include "gpu/usb/functionaltestboard/uartftb.h"

RC UartFtb::DoInitialize()
{
    RC rc;

    if (m_pCom)
    {
        // already initialized
        return rc;
    }

    m_pCom = GetSerialObj::GetCom(m_FtbDevice.c_str());

    CHECK_RC(m_pCom->Initialize(m_ClientId, false));
    CHECK_RC(m_pCom->SetBaud(m_ClientId, USB_UART_BAUD_RATE));

    JavaScriptPtr pJs;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbSerialWaitTimeMs", &m_WaitTimeoutMs);
    pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbSerialIdleTimeMs", &m_IdleTimeoutMs);

    // send a dummy message just to make sure the ftb is responsive
    string response;
    CHECK_RC(SendMessage("", &response));

    string versionStr;
    CHECK_RC(GetFtbFwVersion(&versionStr));

    UINT32 version = strtoul(versionStr.c_str(), nullptr, 16);
    if ((version >= 0x20) && (version < 0x25))
    {
        m_SupportsEnterAltModeUsbC = false;
    }
    if ((version >= 0x20) && (version < 0x27))
    {
        m_SupportsPowerReadings = false;
    }

    return rc;
}

RC UartFtb::DoSetAltMode(PortPolicyCtrl::UsbAltModes altMode)
{
    RC rc;

    if ((altMode == PortPolicyCtrl::USB_ALT_MODE_3) &&
        !m_SupportsEnterAltModeUsbC)
    {
        Printf(Tee::PriError, "FTB firmware version 2.5+ is required to enter alt mode: %u\n",
               static_cast<UINT08>(altMode));
        return RC::USBTEST_CONFIG_FAIL;
    }
    else
    {
        string command = "enteraltmode ";
        string response;
        string altModeStr;

        switch (altMode)
        {
        case PortPolicyCtrl::USB_ALT_MODE_0:
            altModeStr = "lwpu";
            break;
        case PortPolicyCtrl::USB_ALT_MODE_1:
            altModeStr = "dp4+2";
            break;
        case PortPolicyCtrl::USB_ALT_MODE_2:
            altModeStr = "dp2+2+3";
            break;
        case PortPolicyCtrl::USB_ALT_MODE_3:
            altModeStr = "usbc";
            break;
        case PortPolicyCtrl::USB_ALT_MODE_4:
            altModeStr = "valve";
            break;
        default:
            Printf(Tee::PriError, "Unable to enter unknown altmode: %u\n",
                   static_cast<UINT08>(altMode));
            return RC::BAD_PARAMETER;
        }
        command += altModeStr;

        CHECK_RC(SendMessage(command, &response));

        if (!boost::iequals(response, altModeStr + " entered."))
        {
            Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
            return RC::SERIAL_COMMUNICATION_ERROR;
        }
    }

    return rc;
}

RC UartFtb::DoSetPowerMode(PortPolicyCtrl::UsbPowerModes powerMode)
{
    RC rc;

    string command = "powermode ";
    string response;

    switch (powerMode)
    {
    case PortPolicyCtrl::USB_POWER_MODE_0: // 5V @ 3A
        command += "5";
        break;
    case PortPolicyCtrl::USB_POWER_MODE_1: // 9V @3A
        command += "9";
        break;
    case PortPolicyCtrl::USB_POWER_MODE_2: // 12V @2.25A
        command += "12";
        break;
    case PortPolicyCtrl::USB_DEFAULT_POWER_MODE:
        command += "0";
        break;
    default:
        Printf(Tee::PriError, "Unable to enter unknown powerMode: %u\n",
               static_cast<UINT08>(powerMode));
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(SendMessage(command, &response));

    if (response.length())
    {
        Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    return rc;
}

RC UartFtb::DoSetOrientation(PortPolicyCtrl::CableOrientation orient, UINT16 orientDelay)
{
    RC rc;

    PortPolicyCtrl::CableOrientation orientNew = PortPolicyCtrl::USB_UNKNOWN_ORIENTATION;

    // try toggling, if the new state is wrong, toggle back
    for (int i = 0; (i < 2) && (orient != orientNew); i++)
    {
        // note: ftb uart doesn't support the orientation delay parameter
        CHECK_RC(FlipToggle(&orientNew));
    }

    if (orient != orientNew)
    {
        Printf(Tee::PriError, "Unable to set ftb orientation: %u\n", static_cast<UINT08>(orient));
        return RC::USBTEST_CONFIG_FAIL;
    }

    return rc;
}

RC UartFtb::DoReverseOrientation()
{
    PortPolicyCtrl::CableOrientation orientNew = PortPolicyCtrl::USB_UNKNOWN_ORIENTATION;
    return FlipToggle(&orientNew);
}

RC UartFtb::DoGenerateCableAttachDetach
(
    PortPolicyCtrl::CableAttachDetach eventType,
    UINT16 detachAttachDelay
)
{
    RC rc;

    string command = "detach_attach ";
    string response;

    switch (eventType)
    {
    case PortPolicyCtrl::USB_CABLE_ATTACH:
        command += "0 ";
        break;
    case PortPolicyCtrl::USB_CABLE_DETACH:
        command += "1 ";
        break;
    default:
        Printf(Tee::PriError, "Unknown ftb cable attach/detach event type: %u\n",
               static_cast<UINT08>(eventType));
        return RC::BAD_PARAMETER;
    }

    command += to_string(detachAttachDelay);

    CHECK_RC(SendMessage(command, &response));

    if (response.length())
    {
        Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    return rc;
}

RC UartFtb::DoReset()
{
    RC rc;

    string response;

    CHECK_RC(SendMessage("reset", &response));

    m_CheckForPrompt = true;

    if (!boost::iequals(response, "Resetting."))
    {
        Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    return rc;
}

RC UartFtb::DoGenerateInBandWakeup(UINT16 inBandDelay)
{
    RC rc;

    string command = "wakeup " + to_string(inBandDelay);
    string response;

    CHECK_RC(SendMessage(command, &response));

    if (response.length())
    {
        Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    return rc;
}

RC UartFtb::DoSetDpLoopback(PortPolicyCtrl::DpLoopback dpLoopback)
{
    RC rc;

    string response;

    PortPolicyCtrl::DpLoopback dpLoopbackNew = PortPolicyCtrl::USB_UNKNOWN_DP_LOOPBACK_STATUS;

    // try toggling, if the new state is wrong, try toggling it back
    for (int i = 0; (i < 2) && (dpLoopback != dpLoopbackNew); i++)
    {

        CHECK_RC(SendMessage("loopback-toggle", &response));

        // expected responses:
        //      Enable FTB To DP loopback.
        //      Disable FTB To DP loopback.

        RegExp re;
        re.Set("(\\w+)\\s+FTB", RegExp::SUB | RegExp::ICASE);
        if (!re.Matches(response))
        {
            Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
            return RC::SERIAL_COMMUNICATION_ERROR;
        }

        string dpLoopbackStr = re.GetSubstring(1);

        if (boost::iequals(dpLoopbackStr, "Enable"))
        {
            dpLoopbackNew = PortPolicyCtrl::USB_ENABLE_DP_LOOPBACK;
        }
        else if(boost::iequals(dpLoopbackStr, "Disable"))
        {
            dpLoopbackNew = PortPolicyCtrl::USB_DISABLE_DP_LOOPBACK;
        }
        else
        {
            Printf(Tee::PriError, "Unknown ftb dploopback state: %s\n", dpLoopbackStr.c_str());
            return RC::USBTEST_CONFIG_FAIL;
        }
    }

    if (dpLoopback != dpLoopbackNew)
    {
        Printf(Tee::PriError, "Unable to set ftb dploopback state: %u\n",
               static_cast<UINT08>(dpLoopback));
        return RC::USBTEST_CONFIG_FAIL;
    }

    return rc;
}

RC UartFtb::DoSetIsocDevice(PortPolicyCtrl::IsocDevice enableIsocDev)
{
    RC rc;

    string response;

    PortPolicyCtrl::IsocDevice enableIsocDevNew = PortPolicyCtrl::USB_UNKNOWN_ISOC_DEVICE_STATUS;

    // try toggling, if the new state is wrong, try toggling it back
    for (int i = 0; (i < 2) && (enableIsocDev != enableIsocDevNew); i++)
    {
        CHECK_RC(SendMessage("isoc-toggle", &response));

        // expected responses:
        //      Disable isoc.
        //      Enable isoc.

        RegExp re;
        re.Set("(\\w+)\\s+isoc", RegExp::SUB | RegExp::ICASE);
        if (!re.Matches(response))
        {
            Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
            return RC::SERIAL_COMMUNICATION_ERROR;
        }

        string enableIsocDevStr = re.GetSubstring(1);

        if (boost::iequals(enableIsocDevStr, "Enable"))
        {
            enableIsocDevNew = PortPolicyCtrl::USB_ISOC_DEVICE_ENABLE;
        }
        else if(boost::iequals(enableIsocDevStr, "Disable"))
        {
            enableIsocDevNew = PortPolicyCtrl::USB_ISOC_DEVICE_DISABLE;
        }
        else
        {
            Printf(Tee::PriError, "Unknown ftb isoc state: %s\n", enableIsocDevStr.c_str());
            return RC::USBTEST_CONFIG_FAIL;
        }
    }

    if (enableIsocDev != enableIsocDevNew)
    {
        Printf(Tee::PriError, "Unable to set ftb isoc state: %u\n",
               static_cast<UINT08>(enableIsocDev));
        return RC::USBTEST_CONFIG_FAIL;
    }

    return rc;
}

RC UartFtb::DoGetLwrrUsbAltMode(PortPolicyCtrl::UsbAltModes* pAltMode)
{
    RC rc;

    string response;

    CHECK_RC(SendMessage("getaltmode", &response));

    RegExp re;
    re.Set(":\\s*(.+)$", RegExp::SUB);
    if (!re.Matches(response))
    {
        Printf(Tee::PriError, "Invalid ftb response: %s\n", response.c_str());
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    string altModeStr = re.GetSubstring(1);

    if (boost::iequals(altModeStr, "Lwpu DP4+3"))
    {
        *pAltMode = PortPolicyCtrl::USB_ALT_MODE_0;
    }
    else if(boost::iequals(altModeStr, "Standard DP4+2"))
    {
        *pAltMode = PortPolicyCtrl::USB_ALT_MODE_1;
    }
    else if(boost::iequals(altModeStr, "Standard DP2+2+3"))
    {
        *pAltMode = PortPolicyCtrl::USB_ALT_MODE_2;
    }
    else if (boost::iequals(altModeStr, "USB-C"))
    {
        *pAltMode = PortPolicyCtrl::USB_ALT_MODE_3;
    }
    else if(boost::iequals(altModeStr, "Valve DP4+3"))
    {
        *pAltMode = PortPolicyCtrl::USB_ALT_MODE_4;
    }
    else if(boost::iequals(altModeStr, "Lwpu Test Mode"))
    {
        *pAltMode = PortPolicyCtrl::USB_LW_TEST_ALT_MODE;
    }
    else
    {
        Printf(Tee::PriError, "Unknown alt mode: %s\n", altModeStr.c_str());
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    return rc;
}

RC UartFtb::DoConfirmUsbAltModeConfig(PortPolicyCtrl::UsbAltModes altMode)
{
    RC rc;

    PortPolicyCtrl::UsbAltModes altmodeTest;
    CHECK_RC(GetLwrrUsbAltMode(&altmodeTest));

    if (altmodeTest != altMode)
    {
        Printf(Tee::PriError,
               "Incorrect ALT mode configured\n"
               "    Expected  : %u\n"
               "    Configured: %u\n",
               static_cast<UINT08>(altMode),
               static_cast<UINT08>(altmodeTest));

        return RC::USBTEST_CONFIG_FAIL;
    }

    return rc;
}

RC UartFtb::DoGetFtbFwVersion(string* pFtbFwVersion)
{
    RC rc;

    string response;
    CHECK_RC(SendMessage("version", &response));

    RegExp re;
    re.Set("FTB CCG3 FW Ver\\s+(.+)$", RegExp::SUB | RegExp::ICASE);
    if (!re.Matches(response))
    {
        Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

     *pFtbFwVersion = re.GetSubstring(1);

    return rc;
}

RC UartFtb::DoGetPowerDrawn(FLOAT64 *pVoltageV, FLOAT64 *pLwrrentA)
{
    RC rc;

    if (!m_SupportsPowerReadings)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (pVoltageV)
    {
        string response;
        CHECK_RC(SendMessage("getvbus", &response));

        RegExp re;
        re.Set("Lwrr voltage.+:\\s+(.+)$", RegExp::SUB | RegExp::ICASE);
        if (!re.Matches(response))
        {
            Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
            return RC::SERIAL_COMMUNICATION_ERROR;
        }

        UINT32 mv = strtoul(re.GetSubstring(1).c_str(), nullptr, 10);
         *pVoltageV = mv / 1000.0;
    }

    if (pLwrrentA)
    {
        string response;
        CHECK_RC(SendMessage("getlwrrent", &response));

        RegExp re;
        re.Set("Lwrr current.+:\\s+(.+)$", RegExp::SUB | RegExp::ICASE);
        if (!re.Matches(response))
        {
            Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
            return RC::SERIAL_COMMUNICATION_ERROR;
        }

        UINT32 ma = strtoul(re.GetSubstring(1).c_str(), nullptr, 10);
         *pLwrrentA = ma / 1000.0;
    }

    return rc;
}

bool UartFtb::WaitForCharacter(UINT32 timeoutMs)
{
    return (OK == Tasker::Poll(timeoutMs, [&]()->bool
        {
            return m_pCom->ReadBufCount();
        }));
}

RC UartFtb::WaitForPrompt(string* pResponse)
{
    RC rc;
    RegExp promptRe;
    promptRe.Set("\\s*>\\s*$", RegExp::SUB);

    MASSERT(pResponse);
    pResponse->clear();

    // wait until the ftb prints a prompt
    CHECK_RC(Tasker::Poll(m_WaitTimeoutMs, [&]()->bool
        {
            if (m_pCom->ReadBufCount())
            {
                string r;
                if (OK == m_pCom->GetString(m_ClientId, &r))
                {
                    pResponse->append(r);
                    return promptRe.Matches(*pResponse);
                }
            }
            return false;
        }));

    // in certain cases, the ftb will print the prompt multiple times.
    // keep waiting until the ftb is idle (and we still have a prompt)
    while (!promptRe.Matches(*pResponse) ||
          WaitForCharacter(m_IdleTimeoutMs))
    {
        string r;
        CHECK_RC(m_pCom->GetString(m_ClientId, &r));
        pResponse->append(r);
    }

    // strip prompt(s) and trailing whitespace from response
    while (promptRe.Matches(*pResponse))
    {
        string e;
        e = promptRe.GetSubstring(0);
        pResponse->erase(pResponse->size() - e.size(), string::npos);
    }

    return rc;
}

RC UartFtb::SendCharacter(const char c)
{
    RC rc;
    const int maxRetries = 3;
    bool success = false;
    bool sendBackSpace = false;
    for (int retries = 0; !success && (retries <= maxRetries); retries++)
    {
        if (sendBackSpace)
        {
            sendBackSpace = false;
            CHECK_RC(SendCharacter('\b'));
        }

        CHECK_RC(m_pCom->Put(m_ClientId, c));

        // wait until the ftb echoes back a character, resend if the ftb doesn't respond
        if (!WaitForCharacter(m_IdleTimeoutMs))
        {
            continue;
        }

        UINT32 echoedChar;
        CHECK_RC(m_pCom->Get(m_ClientId, &echoedChar));

        // if the echoed character isn't what we sent, send a backspace and resend the character.
        if (echoedChar != static_cast<UINT32>(c))
        {
            // give up if we were already trying to send a backspace
            if (c == '\b')
            {
                return RC::SERIAL_COMMUNICATION_ERROR;
            }
            sendBackSpace = true;
            continue;
        }

        success = true;
    }

    if (!success)
    {
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    return rc;
}


RC UartFtb::SendMessage(const string& command, string* pResponse)
{
    RC rc;
    MASSERT(pResponse);

    if (!m_pCom)
    {
        Printf(Tee::PriError, "Usb FTB uart interface not initialized\n");
        return RC::USB_PORT_NOT_CONNECTED;
    }

    // after a cold boot or reset, the ftb will print a list of commands in response to any char.
    // send a newline to trigger this before we send our command.
    if (m_CheckForPrompt)
    {
        string t;
        CHECK_RC(m_pCom->Put(m_ClientId, '\n'));
        CHECK_RC(WaitForPrompt(&t));
        m_CheckForPrompt = false;
    }

    // on failure, try checking for the prompt before sending the next command
    DEFER
    {
        if (rc != OK)
            m_CheckForPrompt = true;
    };

    // the ftb uart implementation is flaky, send one character at a time.
    for (auto iter = command.begin(); iter != command.end(); ++iter)
    {
        CHECK_RC(SendCharacter(*iter));
    }

    CHECK_RC(m_pCom->Put(m_ClientId, '\n'));

    CHECK_RC(WaitForPrompt(pResponse));

    boost::trim(*pResponse);

    return rc;
}

RC UartFtb::FlipToggle(PortPolicyCtrl::CableOrientation *pNewOrientation)
{
    RC rc;
    MASSERT(pNewOrientation);

    string response;
    CHECK_RC(SendMessage("flip-toggle", &response));

    // expected responses:
    //      Set FTB To Flip Status.
    //      Reset FTB To normal Status.

    RegExp re;
    re.Set("(\\w+)\\s+Status\\.\\s*$", RegExp::SUB | RegExp::ICASE);
    if (!re.Matches(response))
    {
        Printf(Tee::PriError, "Unknown ftb response: %s\n", response.c_str());
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    string orientStr = re.GetSubstring(1);

    if (boost::iequals(orientStr, "Flip"))
    {
        *pNewOrientation = PortPolicyCtrl::USB_CABLE_ORIENT_FLIPPED;
    }
    else if(boost::iequals(orientStr, "normal"))
    {
        *pNewOrientation = PortPolicyCtrl::USB_CABLE_ORIENT_ORIGINAL;
    }
    else
    {
        Printf(Tee::PriError, "Unknown ftb orientation: %s\n", orientStr.c_str());
        return RC::USBTEST_CONFIG_FAIL;
    }

    return rc;
}
