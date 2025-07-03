/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Cross platform (XP) interface for USB related code.
#include "core/include/tasker.h"
#include "core/include/xp.h"
#include "device/interface/portpolicyctrl.h"
#ifdef INCLUDE_GPU
#include "device/interface/xusbhostctrl.h"
#endif
#include "linux/xp_mods_kd.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <fcntl.h>
#include <sys/ioctl.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))

//------------------------------------------------------------------------------
namespace   //USB helper functions
{
    namespace fs = boost::filesystem;

    // Structures for communicating with i2c device exposed by PPC driver
    struct I2cMsg
    {
        UINT16 slaveAddr;
        UINT16 msgFlag;
        UINT16 msgLen;
        UINT08 *msgBuf;
    };

    struct I2cRdWrData
    {
        struct I2cMsg *i2cMsgs;
        UINT32 numOfMsgs;
    };

    // Values for reading power measurement
    const UINT32 I2C_RDWR = 0x0707;
    const UINT32 I2C_RD = 0x1;
    const UINT32 I2C_WR = 0x0;

    // Reference: Cypress CCG Host Processor Interface
    const UINT08 I2C_CCGX_DEVICE_ADDR   = 0x08;
    const UINT16 USB_HPI_REGISTER_ADDR = 0x0048; 
    const UINT16 USB_HPI_STATUS_REGISTER_ADDR = 0x0049; 
    const UINT16 USB_HPI_PWR_REG_ADDR = 0x004C;

    const UINT08 USB_READ_POWER_COMMAND = 0x02; 
    const UINT08 USB_STATUS_READ_BYTES = 0x1;
    const UINT08 USB_POWER_READ_BYTES = 0x4;

    // Reference: MP8858 2.8V-22V/27W, 4-Switch Integrated Buck-Boost Colwerter with I2C Interface
    // Reference bug: http://lwbugs/2093458
    const UINT32 VOLTAGE_MAJOR_INDEX = 0x1;
    const UINT32 VOLTAGE_MINOR_INDEX = 0x0;
    const UINT32 LWRRENT_INDEX = 0x2;
    const FLOAT64 READ_TO_ACTUAL_VOLTAGE = 100.0;
    const FLOAT64 READ_TO_ACTUAL_LWRRENT = 0.05;
    const UINT08 USB_BITS_FROM_MINOR_INDEX = 0x3;

    const UINT32 USB_ILWALID_SVID_VDO   = 0x0;

    const string USB_ALT_MODE_DISABLE = "no";
    const string USB_ALT_MODE_ENABLE = "yes";

    enum NodeType
    {
        NT_PCI,
        NT_PARTNER,
        NT_I2C,
        NT_SVID,
        NT_VDO,
        NT_ACTIVE,
        NT_VDM,
        NT_PCI_CONTROL,
        NT_USB_CONTROL,
        NT_USB_AUTO_SUSPEND_MS,
        NT_SCSI_CONTROL,
        NT_SCSI_AUTO_SUSPEND_MS,
        NT_PCI_DEVICES,
        NT_PPC_FW_VERSION,
        NT_PPC_DRIVER_VERSION
    };

    struct PairedDirFilePath
    {
        string dirName;
        string absFilePath;
    };

    // tuple of <SVID, VDO>
    typedef tuple<UINT32, UINT32> tupleSvidVdo;
    const map<PortPolicyCtrl::UsbAltModes, tupleSvidVdo> mapAltModeSvidVdo =
    {
        { PortPolicyCtrl::USB_ALT_MODE_0,       make_tuple(0x0955, 0x01)   }
       ,{ PortPolicyCtrl::USB_ALT_MODE_1,       make_tuple(0xFF01, 0x0405) }
       ,{ PortPolicyCtrl::USB_ALT_MODE_2,       make_tuple(0xFF01, 0x0805) }
       ,{ PortPolicyCtrl::USB_ALT_MODE_3,       make_tuple(0x0,    0x0)    }
       ,{ PortPolicyCtrl::USB_ALT_MODE_4,       make_tuple(0x28DE, 0x8085) }
       ,{ PortPolicyCtrl::USB_LW_TEST_ALT_MODE, make_tuple(0x0955, 0x03)   }
    };

    RC FindPpcAbsoluteFilePath
    (
        NodeType nodeType,
        string portDir,
        string devDir,
        string modeDir,
        vector<PairedDirFilePath> *pFileList
    )
    {
        RC rc;
        string parentDir, fileName;
        const char *rootDir = "/sys/class/typec/";
        PairedDirFilePath dirFilePath;

        MASSERT(pFileList);
        pFileList->clear();

        switch (nodeType)
        {
            case NT_PCI:
                // fileName = /sys/class/typec/<portDir>/device/pci
                parentDir = rootDir;
                fileName = "/device/pci";
                break;

            case NT_PARTNER:
                // dirName = <portDir>-partner
                if (portDir.empty())
                {
                    return RC::BAD_PARAMETER;
                }
                dirFilePath.dirName = portDir + "-partner/";
                dirFilePath.absFilePath = rootDir + dirFilePath.dirName;
                pFileList->push_back(dirFilePath);
                break;

            case NT_I2C:
                // fileName = /sys/class/typec/<portDir>/i2c
                if (portDir.empty())
                {
                    return RC::BAD_PARAMETER;
                }
                dirFilePath.absFilePath = rootDir + portDir + "/device/i2c";
                pFileList->push_back(dirFilePath);
                break;

            case NT_SVID:
                // fileName = /sys/class/typec/<portDir>-partner/<devDir>/svid
                if (portDir.empty())
                {
                    return RC::BAD_PARAMETER;
                }
                parentDir = rootDir + portDir;
                fileName = "/svid";
                break;

            case NT_VDO:
                // fileName = /sys/class/typec/<portDir>-partner/<devDir>/<mode>/vdo
                if (portDir.empty() || devDir.empty())
                {
                    return RC::BAD_PARAMETER;
                }
                parentDir = rootDir + portDir + devDir;
                fileName = "/vdo";
                break;

            case NT_ACTIVE:
                // fileName = /sys/class/typec/<portDir>-partner/<devDir>/<mode>/active
                if (portDir.empty() || devDir.empty() || modeDir.empty())
                {
                    return RC::BAD_PARAMETER;
                }
                dirFilePath.absFilePath = rootDir + portDir + devDir + modeDir + "active";
                pFileList->push_back(dirFilePath);
                break;

            case NT_VDM:
                // fileName = /sys/class/typec/<portDir>/device/vdm
                if (portDir.empty())
                {
                    return RC::BAD_PARAMETER;
                }
                dirFilePath.absFilePath = rootDir + portDir + "/device/vdm";
                pFileList->push_back(dirFilePath);
                break;

            case NT_PCI_CONTROL:
                // fileName = /sys/bus/pci/devices/<domain:bus:device.function>/power/control
                parentDir = "/sys/bus/pci/devices/";
                fileName = "/power/control";
                break;

            case NT_USB_CONTROL:
                // fileName = /sys/bus/usb/devices/<dir>/power/control
                parentDir = "/sys/bus/usb/devices/";
                fileName = "/power/control";
                break;

            case NT_USB_AUTO_SUSPEND_MS:
                // fileName = /sys/bus/usb/devices/<dir>/power/autosuspend_delay_ms
                parentDir = "/sys/bus/usb/devices/";
                fileName = "/power/autosuspend_delay_ms";
                break;

            case NT_SCSI_CONTROL:
                // fileName = /sys/bus/scsi/devices/<dir>/power/control
                parentDir = "/sys/bus/scsi/devices/";
                fileName = "/power/control";
                break;

            case NT_SCSI_AUTO_SUSPEND_MS:
                // fileName = /sys/bus/scsi/devices/<dir>/power/autosuspend_delay_ms
                parentDir = "/sys/bus/scsi/devices/";
                fileName = "/power/autosuspend_delay_ms";
                break;

            case NT_PCI_DEVICES:
                // fileName = /sys/bus/pci/devices/<domain:bus:device.function>
                parentDir = "/sys/bus/pci/devices/";
                fileName = "";
                break;

            case NT_PPC_FW_VERSION:
                // fileName = /sys/class/typec/<portDir>/device/fwver
                if (portDir.empty())
                {
                    return RC::BAD_PARAMETER;
                }
                dirFilePath.absFilePath = rootDir + portDir + "/device/fwver";
                pFileList->push_back(dirFilePath);
                break;

            case NT_PPC_DRIVER_VERSION:
                // fileName = /sys/module/i2c_lwppc/version
                dirFilePath.absFilePath = "/sys/module/i2c_lwppc/version";
                pFileList->push_back(dirFilePath);
                break;

            default:
                return RC::BAD_PARAMETER;
        }

        if (fs::exists(parentDir) && pFileList->empty())
        {
            fs::directory_iterator end;
            for (fs::directory_iterator it(parentDir); end != it; it++)
            {
                if (fs::is_directory(*it))
                {
                    dirFilePath.dirName = it->path().filename().string();
                    dirFilePath.absFilePath = parentDir + dirFilePath.dirName + fileName;
                    pFileList->push_back(dirFilePath);
                }
            }
        }
        return rc;
    }

    RC FindPortDir
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        string *pPortDir
    )
    {
        RC rc;

        MASSERT(pPortDir);
        vector<PairedDirFilePath> fileList;

        // passing empty string to 2nd, 3rd and 4th argument since
        // portDir, devDir and modeDir are not needed for NodeType = PCI
        CHECK_RC(FindPpcAbsoluteFilePath(NT_PCI, "", "", "", &fileList));

        for (const auto &itr : fileList)
        {
            string ppcPciDBDF;
            if (Xp::InteractiveFileReadSilent(itr.absFilePath.c_str(), &ppcPciDBDF) != OK)
            {
                continue;
            }

            if (!ppcPciDBDF.empty())
            {
                // Remove trailing newline character if any
                ppcPciDBDF.erase(
                        std::remove(ppcPciDBDF.begin(), ppcPciDBDF.end(), '\n'), ppcPciDBDF.end());

                string splitters(":,.");
                vector<string> pciDBDF;
                boost::split(pciDBDF, ppcPciDBDF, boost::is_any_of(splitters));
                //[Domain:Bus:Device.Function] ==> { [Domain], [Bus], [Device], [Function] }

                if (pciDBDF.size() == 4)
                {
                    UINT32 ppcDomain = strtoul(pciDBDF[0].c_str(), nullptr, 16);
                    UINT32 ppcBus = strtoul(pciDBDF[1].c_str(), nullptr, 16);
                    UINT32 ppcDevice = strtoul(pciDBDF[2].c_str(), nullptr, 16);
                    UINT32 ppcFunction = strtoul(pciDBDF[3].c_str(), nullptr, 16);

                    if (ppcDomain == domain
                        && ppcBus == bus
                        && ppcDevice == device
                        && ppcFunction == function)
                    {
                        *pPortDir = itr.dirName;
                        return rc;
                    }
                }
            }
        }
        return RC::PCI_DEVICE_NOT_FOUND;
    }

    RC ExtractDirNameFromList
    (
        vector<PairedDirFilePath> fileList,
        UINT32 compareValue,
        string *pReturnDir
    )
    {
        MASSERT(pReturnDir);

        for (const auto &itr : fileList)
        {
            string fileData;
            if (Xp::InteractiveFileReadSilent(itr.absFilePath.c_str(), &fileData) != OK)
            {
                continue;
            }
            UINT32 readValue = strtoul(fileData.c_str(), nullptr, 16);
            if (compareValue == readValue)
            {
                *pReturnDir = itr.dirName + "/";
                return OK;
            }
        }
        return RC::FILE_DOES_NOT_EXIST;
    }

    RC FindAllNodePaths
    (
        string portDir,
        std::map<PortPolicyCtrl::UsbAltModes, string> *pMapActiveVdmPath,
        string *pVdmNodePath
    )
    {
        RC rc;

        MASSERT(pMapActiveVdmPath);

        JavaScriptPtr pJs;
        UINT32 configTimeoutMs = PortPolicyCtrl::USB_PPC_CONFIG_TIMEOUT_MS;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbPpcConfigTimeOutMs", &configTimeoutMs);

        UINT32 retryWaitTimeMS = 100;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbReTryWaitTimeMs", &retryWaitTimeMS);

        bool foundPartnerNode = false;
        UINT64 startTimeMs = Xp::GetWallTimeMS();
        do
        {
            string portPartner;
            vector<PairedDirFilePath> fileList;

            // passing empty string to 3rd and 4th argument since
            // devDir and modeDir are not needed for NodeType = PARTNER
            CHECK_RC(FindPpcAbsoluteFilePath(NT_PARTNER, portDir, "", "", &fileList));
            if (!fs::exists(fileList[0].absFilePath))
            {
                // The functional test board may still be connecting back
                // Wait for 100 ms before trying again
                Tasker::Sleep(retryWaitTimeMS);
                continue;
            }
            portPartner = fileList[0].dirName;
            foundPartnerNode = true;

            vector<PairedDirFilePath> svidFileList;
            // passing empty string to 3rd and 4th argument since
            // devDir and modeDir are not needed for NodeType = SVID
            CHECK_RC(FindPpcAbsoluteFilePath(NT_SVID, portPartner, "", "", &svidFileList));

            UINT08 filesFound = 0;
            UINT08 totalFiles = 0;
            for (const auto &itr : mapAltModeSvidVdo)
            {
                UINT32 svid = get<0>(itr.second);
                UINT32 vdo = get<1>(itr.second);

                // Standard USB C mode (2 Lanes of USB 3 + 1 Lane of USB 2)
                // doesn't have associated files
                if (svid == USB_ILWALID_SVID_VDO && vdo == USB_ILWALID_SVID_VDO)
                {
                    continue;
                }
                totalFiles++;

                string devDir;
                rc = ExtractDirNameFromList(svidFileList, svid, &devDir);
                if (rc != OK)
                {
                    // This ALT mode may not be supported in the current configuration
                    // or the corresponding sysfs node may not be ready yet
                    rc.Clear();
                    continue;
                }

                vector<PairedDirFilePath> vdoFileList;
                // passing empty string to 4th argument since
                // modeDir are not needed for NodeType = VDO
                CHECK_RC(FindPpcAbsoluteFilePath(NT_VDO, portPartner, devDir, "", &vdoFileList));

                string modeDir;
                rc = ExtractDirNameFromList(vdoFileList, vdo, &modeDir);
                if (rc != OK)
                {
                    // This ALT mode may not be supported in the current configuration 
                    // or the corresponding sysfs node may not be ready yet
                    rc.Clear();
                    continue;
                }

                CHECK_RC(FindPpcAbsoluteFilePath(NT_ACTIVE, portPartner, devDir, modeDir, &fileList));
                (*pMapActiveVdmPath)[itr.first] = fileList[0].absFilePath;
                filesFound++;

                // Need to find the 'vdm' node as well for USB_LW_TEST_ALT_MODE
                if (itr.first == PortPolicyCtrl::USB_LW_TEST_ALT_MODE)
                {
                    CHECK_RC(FindPpcAbsoluteFilePath(NT_VDM, portDir, "", "", &fileList));
                    if (pVdmNodePath)
                    {
                        *pVdmNodePath = fileList[0].absFilePath;
                    }
                }

            }
            if (filesFound == totalFiles)
            {
                break;
            }
            // All sysfs nodes may not have been generated yet.
            // Wait for 100 ms before trying again
            Tasker::Sleep(retryWaitTimeMS);
        } while ((Xp::GetWallTimeMS() - startTimeMs) < configTimeoutMs);

        if (!foundPartnerNode)
        {
            Printf(Tee::PriError,
                   "Unable to find port*-partner asssociated with the functional test board, "
                   "make sure the board is connected to the GPU\n");
            return RC::FILE_DOES_NOT_EXIST;
        }

        // It is fine even if nodes for all the ALT modes are not generated even after timeout
        // It means, that particular ALT mode may not be supported at all
        // If the ALT mode is required and sysfs nodes are not generated, the error will be caught later.
        // active node with svid = 0x955 and vdo = 0x3 (USB_LW_TEST_ALT_MODE)
        // should always be present irrespective of the current configuration
        const auto &nodeItr = (*pMapActiveVdmPath).find(PortPolicyCtrl::USB_LW_TEST_ALT_MODE);
        if (nodeItr == (*pMapActiveVdmPath).end()
                || nodeItr->second.empty() 
                || !Xp::DoesFileExist(nodeItr->second))
        {
            Printf(Tee::PriError, "Unable to find active node associated with LW_TEST_ALT_MODE\n");
            return RC::FILE_DOES_NOT_EXIST;
        }

        return rc;
    }

    RC SendSysfsIoctl(string filePath, string fileContents)
    {
        RC rc;

        JavaScriptPtr pJs;
        UINT32 sysfsWaitTimeMs = PortPolicyCtrl::USB_SYSFS_NODE_WAIT_TIME_MS;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbSysfsNodeWaitTimeMs", &sysfsWaitTimeMs);

        CHECK_RC(Xp::GetMODSKernelDriver()->WriteSysfsNode(filePath, fileContents));

        // With each write operation to sysfs node the PPC (port policy controller) driver
        // needs to configure muxes and/or negotiate with the functional test board
        Tasker::Sleep(sysfsWaitTimeMs);

        return rc;
    }

    bool IsLwidiaTestModeEnabled
    (
        map<PortPolicyCtrl::UsbAltModes, string> mapActiveVdmPath
    )
    {
        RC rc;

        const auto &nodeItr = mapActiveVdmPath.find(PortPolicyCtrl::USB_LW_TEST_ALT_MODE);
        if (!nodeItr->second.empty())
        {
            string fileData;
            if (Xp::InteractiveFileReadSilent(nodeItr->second.c_str(), &fileData) != OK)
            {
                return false;
            }
            // Remove newline character
            fileData.erase(std::remove(fileData.begin(), fileData.end(), '\n'), fileData.end());
            if (fileData.compare(USB_ALT_MODE_ENABLE) == 0)
            {
                return true;
            }
        }
        return false;
    }

    // Enable Lwpu test ALT mode so that the Functional Test board can be prepared to receive a VDM
    RC EnableLwidiaTestAltMode
    (
        string portDir,
        map<PortPolicyCtrl::UsbAltModes, string> *pMapActiveVdmPath
    )
    {
        RC rc;

        bool nodeUpdated;

        JavaScriptPtr pJs;
        UINT32 configTimeoutMs = PortPolicyCtrl::USB_PPC_CONFIG_TIMEOUT_MS;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbPpcConfigTimeOutMs", &configTimeoutMs);

        UINT32 sysfsWaitTimeMs = PortPolicyCtrl::USB_SYSFS_NODE_WAIT_TIME_MS;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbSysfsNodeWaitTimeMs", &sysfsWaitTimeMs);

        UINT32 retryWaitTimeMS = 100;
        pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbReTryWaitTimeMs", &retryWaitTimeMS);

        // for each entry in pMapActiveVdmPath, there can we 3 wait calls:
        // call to SendSysfsIoctl - wait time controlled by g_UsbSysfsNodeWaitTimeMs
        // call to FindAllNodePaths - max time out controller by g_UsbPpcConfigTimeOutMs
        // 100 ms wait between conselwtive loops
        UINT64 maxTimeoutMs = (*pMapActiveVdmPath).size() *
                                    (configTimeoutMs + sysfsWaitTimeMs + retryWaitTimeMS);

        UINT64 startTimeMs = Xp::GetWallTimeMS();
        UINT64 initialStartTimeMs = startTimeMs;
        do
        {
            nodeUpdated = false;

            // Iterate through the "active" sysfs node corresponding to all ALT modes
            // Write "no" to all ALT modes except for Lwpu Test ALT mode
            // Write "yes" to "active" node corresponding to Lwpu Test ALT mode
            for (const auto &mapItr : *pMapActiveVdmPath)
            {
                string filePath = mapItr.second;
                if (filePath.empty())
                {
                    continue;
                }

                string expected = (mapItr.first == PortPolicyCtrl::USB_LW_TEST_ALT_MODE)
                                             ? USB_ALT_MODE_ENABLE : USB_ALT_MODE_DISABLE;

                string fileData;
                CHECK_RC(Xp::InteractiveFileRead(filePath.c_str(), &fileData));
                // Remove newline character
                fileData.erase(std::remove(fileData.begin(), fileData.end(), '\n'), fileData.end());

                if (fileData.compare(expected) != 0)
                {
                    nodeUpdated = true;
                    CHECK_RC(SendSysfsIoctl(filePath, expected));
                    break;
                }
            }

            if (nodeUpdated)
            {
                // With each update to "active" sysfs node, the ALT mode configuration changes
                // which means that the sysfs nodes will be regenerated by the driver after the
                // mux config and/or negotiation with the functional test board is complete
                CHECK_RC(FindAllNodePaths(portDir, pMapActiveVdmPath, nullptr));

                // Reset the start time used for timeout since PPC driver would take
                // a couple of seconds  to regenerate and update sysfs node with
                // correct information once a mode change happens
                startTimeMs = Xp::GetWallTimeMS();
            }

            if (IsLwidiaTestModeEnabled(*pMapActiveVdmPath))
            {
                break;
            }
            Tasker::Sleep(retryWaitTimeMS);
        } while (((Xp::GetWallTimeMS() - startTimeMs) < configTimeoutMs)
                 && ((Xp::GetWallTimeMS() - initialStartTimeMs) < maxTimeoutMs));
                            

        if (!IsLwidiaTestModeEnabled(*pMapActiveVdmPath))
        {
            Printf (Tee::PriError,
                    "Unable to enter lwpu test alt mode\n");
            return RC::USBTEST_CONFIG_FAIL;
        }

        return rc;
    }

    // Input
    //      32 bit VDM header 
    //      32 bit VDM body 
    // Output
    //      string (of hex) -> <header_8_char><space><body_8_char>
    //
    string GenerateMessage(UINT32 header, UINT32 body)
    {
        const UINT08 maxHeaderBodySize = 8;
        string msgHeader = Utility::ColwertHexToStr(header);
        string msgBody = Utility::ColwertHexToStr(body);

        string msg;
        for (UINT08 idx = msgHeader.size(); idx < maxHeaderBodySize; idx++)
        {
            msg += "0";
        }
        msg += msgHeader;
        msg += " ";
        for (UINT08 idx = msgBody.size(); idx < maxHeaderBodySize; idx++)
        {
            msg += "0";
        }
        msg += msgBody;

        return msg;
    }

#ifdef INCLUDE_GPU
    RC UpdateUsbScsiNode(NodeType nodeType, string fileData)
    {
        RC rc;

        // We don't care about any of the directories which
        // contain the keywords host, target or usb.
        const string skipDirs[] = { "host", "target", "usb" };

        vector<PairedDirFilePath> fileList;
        CHECK_RC(FindPpcAbsoluteFilePath(nodeType, "", "", "", &fileList));
        string absFileName;
        for (const auto &fileItr : fileList)
        {
            string absFileName = fileItr.absFilePath;
            if (!Xp::DoesFileExist(absFileName))
            {
                continue;
            }

            bool skipDir = false;
            for (const auto &skipItr : skipDirs)
            {
                if (fileItr.dirName.find(skipItr) != string::npos)
                {
                    skipDir = true;
                    break;
                }
            }
            if (skipDir)
            {
                continue;
            }

            rc = SendSysfsIoctl(absFileName, fileData);
            if (rc != OK)
            {
                // Some devices don't allow files to be updated.
                // If it is 1 of our devices in concern, the HC will
                // not enter low power mode and the calling function
                // will handle it
                Printf(Tee::PriLow,
                       "Unable to update %s with %s\n",
                       absFileName.c_str(),
                       fileData.c_str());
                rc.Clear();
            }
        }

        return rc;
    }
#endif

    RC FindPciDeviceFilePath
    (
        UINT32 domain,
        UINT32 bus,
        UINT32 device,
        UINT32 function,
        PairedDirFilePath* pFilePath
    )
    {
        RC rc;
        vector<PairedDirFilePath> fileList;
        PairedDirFilePath dirFilePath;

        MASSERT(pFilePath);

        // find all files that start with parentDir and end with fileName
        CHECK_RC(FindPpcAbsoluteFilePath(NT_PCI_DEVICES, "", "", "", &fileList));

        // find the file corresponding to the pciDBDF
        for (const auto &itr : fileList)
        {
            string splitters(":,.");
            vector<string> pciDBDF;
            boost::split(pciDBDF, itr.dirName, boost::is_any_of(splitters));
            if (pciDBDF.size() == 4)
            {
                UINT32 hcDomain = strtoul(pciDBDF[0].c_str(), nullptr, 16);
                UINT32 hcBus = strtoul(pciDBDF[1].c_str(), nullptr, 16);
                UINT32 hcDevice = strtoul(pciDBDF[2].c_str(), nullptr, 16);
                UINT32 hcFunction = strtoul(pciDBDF[3].c_str(), nullptr, 16);

                if ((domain == hcDomain)
                     && (bus == hcBus)
                     && (device == hcDevice)
                     && (function == hcFunction))
                {
                    if (Xp::DoesFileExist(itr.absFilePath))
                    {
                        *pFilePath = itr;
                        return OK;
                    }
                }
            }
        }

        return RC::FILE_DOES_NOT_EXIST;
    }

    RC I2cWrite
    (
        INT32 devHandle,
        UINT32 devAddr,
        UINT32 regOffset,
        const vector<UINT08>& writeData,
        UINT08 addrByteWidth
    )
    {
        RC rc;

        if (devHandle < 0)
        {
            Printf(Tee::PriError, "Invalid i2c device handle\n");
            return RC::USBTEST_CONFIG_FAIL;
        }

        if (!addrByteWidth)
        {
            Printf(Tee::PriError, "Size of register offset cannot be size\n");
            return RC::BAD_PARAMETER;
        }

        vector<UINT08> buffer;
        buffer.reserve(addrByteWidth + writeData.size());
        for (UINT08 idx = 0; idx < addrByteWidth; ++idx)
        {
            const UINT08 regOffsetByte = static_cast<UINT08>((regOffset >> (idx*8)) & 0xff);
            buffer.push_back(regOffsetByte);
        }
        buffer.insert(buffer.end(), writeData.begin(), writeData.end());

        struct I2cMsg msg;
        struct I2cRdWrData rwData;

        msg.slaveAddr = devAddr;
        msg.msgFlag = I2C_WR;
        msg.msgLen = buffer.size();
        msg.msgBuf = buffer.data();

        rwData.numOfMsgs = 1;
        rwData.i2cMsgs = &msg;

        INT32 ret = ioctl(devHandle, I2C_RDWR, (unsigned long)&rwData);
        if (ret == -1)
        {
            Printf(Tee::PriError,
                   "Error while sending IOCTL to read bucl boost power register\n");
            return RC::USBTEST_CONFIG_FAIL;
        }

        return rc;
    }

    RC I2cRead
    (
        INT32 devHandle,
        UINT32 devAddr,
        UINT32 regOffset,
        vector<UINT08> *pReadData,
        UINT08 addrByteWidth,
        UINT08 dataBytesToRead
    )
    {
        RC rc;
        MASSERT(pReadData);

        if (!dataBytesToRead)
        {
            Printf(Tee::PriError, "Data size to be read is 0\n");
            return RC::BAD_PARAMETER;
        }

        if (!addrByteWidth)
        {
            Printf(Tee::PriError, "Size of register offset cannot be size\n");
            return RC::BAD_PARAMETER;
        }

        if (devHandle < 0)
        {
            Printf(Tee::PriError, "Invalid i2c device handle\n");
            return RC::USBTEST_CONFIG_FAIL;
        }

        vector<UINT08> regBuffer;
        regBuffer.reserve(addrByteWidth);
        for (UINT08 idx = 0; idx < addrByteWidth; ++idx)
        {
            regBuffer.push_back(static_cast<UINT08>((regOffset >> (idx*8)) & 0xff));
        }

        struct I2cMsg msgs[2];
        struct I2cRdWrData rwData;
        UINT08 msgIdx = 0;

        if (addrByteWidth)
        {
            msgs[msgIdx].slaveAddr = devAddr;
            msgs[msgIdx].msgFlag = I2C_WR;
            msgs[msgIdx].msgLen = regBuffer.size();
            msgs[msgIdx].msgBuf = regBuffer.data();
            msgIdx++;
        }

        pReadData->clear();
        pReadData->resize(dataBytesToRead);
        msgs[msgIdx].slaveAddr = devAddr;
        msgs[msgIdx].msgFlag = I2C_RD;
        msgs[msgIdx].msgLen = dataBytesToRead;
        msgs[msgIdx].msgBuf = &pReadData->at(0);
        msgIdx++;

        rwData.numOfMsgs = msgIdx;
        rwData.i2cMsgs = msgs;

        INT32 ret = ioctl(devHandle, I2C_RDWR, (unsigned long)&rwData);
        if (ret == -1)
        {
            Printf(Tee::PriError,
                   "Expected %u messages to be exelwted, actual %d exelwted\n",
                   rwData.numOfMsgs,
                   ret);
            return RC::USBTEST_CONFIG_FAIL;
        }

        return rc;
    }
}   // anonymous namespace - USB helper functions

/*
 * On Linux Platform, as a part of MODS-PPC (port policy controller) interface,
 * MODS would need to read from/write to the following sysfs nodes:
 *    /sys/class/typec/<portDir>/pci
 *    /sys/class/typec/<portDir>-partner/<device>/<modeID>/active
 *    /sys/class/typec/<portDir>-partner/<device>/<modeId>/vdm
 *
 * Finding correct directories (portDir, device, modeId):
 *    portDir : Correct portDir can be found by iterating through
 *              all the pci nodes and matching the PCI DBDF for PPC
 *              with that of the GPU
 *    device  : Each ALT mode has an associated SVID (refer below table).
 *              Iterate through all the svid nodes under
 *              /sys/class/typec/<portDir>-partner/<device>/ and match the
 *              SVID value with that of the corresponding ALT mode
 *              (portDir directory found above).
 *    modeId  : Each ALT mode has an associated VDO (refer below table).
 *              Iterate through all the vdo nodes under
 *              /sys/class/typec/<portDir>-partner/<device>/<modeId>/
 *              and match the VDO value with that of the corresponding
 *              ALT mode (portDir and device directories found above)
 *
 * |------------------------------------------------------------------------------------------|
 * |                   ALT Mode                             |    Node       |  SVID  | VDO    |
 * |------------------------------------------------------------------------------------------|
 * | 4 Lanes of DP + 2 Lanes of USB 3 (Lwpu GPU)          |   active      | 0x0955 | 0x01   |
 * | 4 Lanes of DP + 1 Lane of USB 2                        |   active      | 0xFF01 | 0x0405 |
 * | 2 Lanes of DP + 2 Lanes of USB 3 + 1 Lane of USB 2     |   active      | 0xFF01 | 0x0805 |
 * | 2 Lanes of USB 3 + 1 Lane of USB 2                     |     -         |   -    |   -    |
 * | Lwpu Test ALT mode (to send VDM)                     |   active/vdm  | 0x0955 | 0x03   |
 * | 4 Lanes of DP + 2 Lanes of USB 3 (Valve Adapter Board) |   active      | 0x28DE | 0x8085 |
 * |------------------------------------------------------------------------------------------|
 *
*/
RC Xp::SendMessageToFTB
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    const vector<tuple<UINT32, UINT32>>& messages
)
{
    RC rc;

    // Find the 'port0' directory corresponding to current port policy controller
    string portDir;
    rc = FindPortDir(domain, bus, device, function, &portDir);
    if (rc != OK)
    {
        Printf(Tee::PriError,
               "Unable to find port* directory corresponding to current test device, "
               "make sure PPC driver is installed\n");
        return rc;
    }

    // Find all sysfs nodes correspoding to different ALT modes
    map<PortPolicyCtrl::UsbAltModes, string> mapActiveVdmPath;
    string vdmNodePath;
    CHECK_RC(FindAllNodePaths(portDir, &mapActiveVdmPath, &vdmNodePath));

    // In order to send VDMs to the functional test board, we first need to
    // activate Lwpu Test ALT mode. All other ALT modes needs to be deactivated
    // before activating this mode
    CHECK_RC(EnableLwidiaTestAltMode(portDir, &mapActiveVdmPath));

    for (const auto &vdmMsg : messages)
    {
        // Colwert VDM header and body into string of format
        // <header_8_char><space><body_8_char>
        string msg = GenerateMessage(get<0>(vdmMsg), get<1>(vdmMsg));
        // Send the VDM to functional test board
        CHECK_RC(SendSysfsIoctl(vdmNodePath, msg));
    }

    return rc;
}

RC Xp::ConfirmUsbAltModeConfig
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT08 desiredAltMode
)
{
    RC rc;

    string portDir;
    rc = FindPortDir(domain, bus, device, function, &portDir);
    if (rc != OK)
    {
        Printf(Tee::PriError,
               "Unable to find port* directory corresponding to current test device, "
               "make sure PPC driver is installed\n");
        return rc;
    }

    map<PortPolicyCtrl::UsbAltModes, string> mapActiveVdmPath;
    CHECK_RC(FindAllNodePaths(portDir, &mapActiveVdmPath, nullptr));

    // By default when USB_ALT_MODE_3 (standard USB mode is requested) the functional 
    // test board enters into USB_LW_TEST_ALT_MODE. This is expected behavior and we
    // need to explicitely exit USB_LW_TEST_ALT_MODE in order to enter USB_ALT_MODE_3
    if (desiredAltMode == PortPolicyCtrl::USB_ALT_MODE_3)
    {
        if (!IsLwidiaTestModeEnabled(mapActiveVdmPath))
        {
            Printf (Tee::PriError,
                    "ALT mode expected was Lwpu Test ALT mode but is not set\n");
            return RC::USBTEST_CONFIG_FAIL;
        }

        // Disable Lwpu Test ALT mode
        const auto &nodeItr = mapActiveVdmPath.find(PortPolicyCtrl::USB_LW_TEST_ALT_MODE);
        CHECK_RC(SendSysfsIoctl(nodeItr->second, USB_ALT_MODE_DISABLE));

        // Refind all the sysfs nodes
        mapActiveVdmPath.clear();
        CHECK_RC(FindAllNodePaths(portDir, &mapActiveVdmPath, nullptr));
    }

    for (const auto &mapItr : mapActiveVdmPath)
    {
        string fileData;
        CHECK_RC(Xp::InteractiveFileRead(mapItr.second.c_str(), &fileData));
        fileData.erase(std::remove(fileData.begin(), fileData.end(), '\n'), fileData.end());
        if (mapItr.first == desiredAltMode)
        {
            if (fileData.compare(USB_ALT_MODE_ENABLE) != 0)
            {
                Printf(Tee::PriError, "Desired ALT mode (%u) not configured\n", desiredAltMode);
                return RC::USBTEST_CONFIG_FAIL;
            }
            else
            {
                return rc;
            }
        }

        else if (fileData.compare(USB_ALT_MODE_DISABLE) != 0)
        {
            Printf(Tee::PriError,
                   "Incorrect ALT mode configured\n"
                   "    Expected  : %u\n"
                   "    Configured: %u\n",
                   desiredAltMode,
                   mapItr.first);
            return RC::USBTEST_CONFIG_FAIL;
        }
    }

    // If we have reached here, means that no other mode is active
    // and the current ALT mode is configured to be Standard USB C mode
    // (USB_ALT_MODE_3)
    if (desiredAltMode != PortPolicyCtrl::USB_ALT_MODE_3)
    {
        Printf(Tee::PriError,
               "Incorrect ALT mode configured\n"
               "    Expected  : %u\n"
               "    Configured: %u\n",
               desiredAltMode,
               PortPolicyCtrl::USB_ALT_MODE_3);
        return RC::USBTEST_CONFIG_FAIL;
    }

    return rc;
}

/*
 * Auto suspend (entering in low power mode) for USB devices (including root hub)
 * is disabled by default. It can be enabled by updating power related sysfs nodes
 * corresponding to the device
 *
 * |-----------------------------------------------------------------------------------|
 * |                  Node                                    |      Value             |
 * |-----------------------------------------------------------------------------------|
 * | For USB devices:                                                                  |
 * |-----------------------------------------------------------------------------------|
 * | /sys/bus/usb/devices/.../power/autosuspend_delay_ms      | idle time (MS) to wait | 
 * |                                                          | before autosuspend     |
 * |-----------------------------------------------------------------------------------|
 * | /sys/bus/usb/devices/.../power/control                   |         "auto"         |
 * |-----------------------------------------------------------------------------------|
 *
 * |-----------------------------------------------------------------------------------|
 * | For SCSI devices:                                                                 |
 * |-----------------------------------------------------------------------------------|
 * | /sys/bus/scsi/devices/.../power/autosuspend_delay_ms     | idle time (MS) to wait |
 * |                                                          | before autosuspend     |
 * |-----------------------------------------------------------------------------------|
 * | /sys/bus/scsi/devices/.../power/control                  |         "auto"         |
 * |-----------------------------------------------------------------------------------|
 *
 * |-----------------------------------------------------------------------------------|
 * | For root hub:                                                                     |
 * |-----------------------------------------------------------------------------------|
 * | /sys/bus/pci/devices/.../power/control                  |         "auto"          |
 * |-----------------------------------------------------------------------------------|
 *
 * Input:
 *      Host controller's PCI DBDF to which the external device is attached
 *      Time in MS for the device to be idle before entering low power mode
 */
RC Xp::EnableUsbAutoSuspend
(
    UINT32 hcPciDomain,
    UINT32 hcPciBus,
    UINT32 hcPciDevice,
    UINT32 hcPciFunction,
    UINT32 autoSuspendDelayMs
)
{
#ifdef INCLUDE_GPU
    RC rc;
    const string controlValue = "auto";
    const string delayTimeMsStr = Utility::StrPrintf("%u", autoSuspendDelayMs);

    // If USB Data Transfer Utility is called before calling this function
    // then it would take a couple of seconds for the OS to gain control
    // back of the USB devices and regenerate the sysfs nodes corressponding
    // to those devices
    JavaScriptPtr pJs;
    UINT32 timeOutMs = XusbHostCtrl::USB_REENUMERATE_DEVICE_WAIT_TIME_MS;
    pJs->GetProperty(pJs->GetGlobalObject(), "g_UsbReEnumerateDeviceWaitTimeMs", &timeOutMs);
    Tasker::Sleep(timeOutMs);

    // Enable Low power for Xusb Host Controller

    // get the list of all directories under folder
    // /sys/bus/pci/devices/<dir>/power/control
    // The <dir> name is of format <domain:bus:device.function>
    vector<PairedDirFilePath> fileList;
    CHECK_RC(FindPpcAbsoluteFilePath(NT_PCI_CONTROL, "", "", "", &fileList));
    string absFileName;
    for (const auto &itr : fileList)
    {
        string splitters(":,.");
        vector<string> pciDBDF;
        boost::split(pciDBDF, itr.dirName, boost::is_any_of(splitters));
        if (pciDBDF.size() == 4)
        {
            UINT32 domain = strtoul(pciDBDF[0].c_str(), nullptr, 16);
            UINT32 bus = strtoul(pciDBDF[1].c_str(), nullptr, 16);
            UINT32 device = strtoul(pciDBDF[2].c_str(), nullptr, 16);
            UINT32 function = strtoul(pciDBDF[3].c_str(), nullptr, 16);

            if ((hcPciDomain == domain)
                 && (hcPciBus == bus)
                 && (hcPciDevice == device)
                 && (hcPciFunction == function))
            {
                if (Xp::DoesFileExist(itr.absFilePath))
                {
                    absFileName = itr.absFilePath;
                    break;
                }
            }
        }
    }
    if (absFileName.empty())
    {
        Printf(Tee::PriError,
               "Power control nodes for host controller at %04x:%x:%02x.%x not found\n",
               hcPciDomain,
               hcPciBus,
               hcPciDevice,
               hcPciFunction);
        return RC::FILE_DOES_NOT_EXIST;
    }

    string fileData;
    if (Xp::InteractiveFileRead(absFileName.c_str(), &fileData) != OK)
    {
        Printf(Tee::PriError,
               "Error reading data from file: %s\n",
               absFileName.c_str());
        return RC::FILE_READ_ERROR;
    }
    // Remove newline character
    fileData.erase(std::remove(fileData.begin(), fileData.end(), '\n'), fileData.end());
    if (fileData.compare(controlValue) != 0)
    {
        CHECK_RC(SendSysfsIoctl(absFileName, controlValue));
    }

    // Enable Low Power for external USB and SCSI devices
    CHECK_RC(UpdateUsbScsiNode(NT_USB_CONTROL, controlValue));
    CHECK_RC(UpdateUsbScsiNode(NT_SCSI_CONTROL, controlValue));
    CHECK_RC(UpdateUsbScsiNode(NT_USB_AUTO_SUSPEND_MS, delayTimeMsStr));
    CHECK_RC(UpdateUsbScsiNode(NT_SCSI_AUTO_SUSPEND_MS, delayTimeMsStr));

    return rc;
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC Xp::DisablePciDeviceDriver
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string* pDriverName
)
{
    RC rc;

    PairedDirFilePath filePath;

    CHECK_RC(FindPciDeviceFilePath(domain, bus, device, function, &filePath));

    fs::path driverPath = filePath.absFilePath + "/driver";

    // if a driver is bound, the device will have a symlink to the driver
    if (fs::exists(driverPath))
    {
        // colwert the path's symlink before we unbind and lose it
        driverPath = fs::canonical(driverPath);
    }
    else
    {
        Printf(Tee::PriLow, "No driver to disable for device: %s\n",
                            filePath.absFilePath.c_str());
        pDriverName->clear();
        return rc;
    }

    string unbindName = driverPath.string() + "/unbind";

    if (!Xp::DoesFileExist(unbindName))
    {
        Printf(Tee::PriError, "File \"%s\" does not exist\n", unbindName.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    // write the pci <domain:bus:device.function> to unbind
    CHECK_RC(SendSysfsIoctl(unbindName, filePath.dirName));

    if (pDriverName)
        *pDriverName = driverPath.string();

    return rc;
}

RC Xp::EnablePciDeviceDriver
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    const string& driverName
)
{
    RC rc;

    PairedDirFilePath filePath;

    CHECK_RC(FindPciDeviceFilePath(domain, bus, device, function, &filePath));

    if (Xp::DoesFileExist(filePath.absFilePath + "/driver"))
    {
        Printf(Tee::PriError, "Driver already enabled for device: %s\n", filePath.absFilePath.c_str());
        return RC::BAD_PARAMETER;
    }

    string bindName = driverName + "/bind";

    if (!Xp::DoesFileExist(bindName))
    {
        Printf(Tee::PriError, "File \"%s\" does not exist\n", bindName.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    CHECK_RC(SendSysfsIoctl(bindName, filePath.dirName));

    return rc;
}

RC Xp::GetLwrrUsbAltMode
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT08 *pLwrrAltMode
)
{
    RC rc;
    MASSERT(pLwrrAltMode);

    string portDir;
    rc = FindPortDir(domain, bus, device, function, &portDir);
    if (rc != OK)
    {
        Printf(Tee::PriError,
               "Unable to find port* directory corresponding to current test device, "
               "make sure PPC driver is installed\n");
        return rc;
    }

    map<PortPolicyCtrl::UsbAltModes, string> mapActiveVdmPath;
    CHECK_RC(FindAllNodePaths(portDir, &mapActiveVdmPath, nullptr));

    *pLwrrAltMode = PortPolicyCtrl::USB_UNKNOWN_ALT_MODE;
    for (const auto &mapItr : mapActiveVdmPath)
    {
        string fileData;
        CHECK_RC(Xp::InteractiveFileRead(mapItr.second.c_str(), &fileData));
        fileData.erase(std::remove(fileData.begin(), fileData.end(), '\n'), fileData.end());
        // PortPolicyCtrl::UsbAltModes is sorted based on the priorities of the ALT mode
        // even if multiple ALT modes may be supported (multiple sysfs nodes have value "yes"),
        // only 1 mode will be active - so return the 1st mode with value "yes"
        if (fileData.compare(USB_ALT_MODE_ENABLE) == 0)
        {
            *pLwrrAltMode = mapItr.first;
            return rc;
        }
        else
        {
            // If no mode is active, that means native USB mode is active
            *pLwrrAltMode = PortPolicyCtrl::USB_ALT_MODE_3;
        }
    }

    // This condition makes sure that we have found atleast 1 sysfs node and entered
    // the above for loop
    if (*pLwrrAltMode == PortPolicyCtrl::USB_UNKNOWN_ALT_MODE)
    {
        Printf(Tee::PriAlways, "Unable to determine the lwrrrent ALT mode\n");
        return RC::USBTEST_CONFIG_FAIL;
    }

    return rc;
}

RC Xp::GetPpcFwVersion
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string *pPrimaryVersion,
    string *pSecondaryVersion
)
{
    // PPC FW version will be printed as part of MODS header
    // FW version can be read only if PPC driver is installed.
    // There may be cases where PPC driver is not installed and in that
    // case we don't want to keep printing all the messages from below function
    // So print priority is set to low for all prints
    RC rc;
    MASSERT(pPrimaryVersion);
    MASSERT(pSecondaryVersion);

    string portDir;
    rc = FindPortDir(domain, bus, device, function, &portDir);
    if (rc != OK)
    {
        Printf(Tee::PriLow,
               "Unable to find port* directory corresponding to current test device, "
               "make sure PPC driver is installed\n");
        return rc;
    }

    vector<PairedDirFilePath> fileList;
    CHECK_RC(FindPpcAbsoluteFilePath(NT_PPC_FW_VERSION, portDir, "", "", &fileList));

    string fwVersions;
    if (!Xp::DoesFileExist(fileList[0].absFilePath))
    {
        Printf(Tee::PriLow, "Unable to find %s file\n", fileList[0].absFilePath.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    rc = Xp::InteractiveFileRead(fileList[0].absFilePath.c_str(), &fwVersions);
    if (rc != OK)
    {
        Printf(Tee::PriLow, "Unable to read data from %s file\n", fileList[0].absFilePath.c_str());
        return rc;
    }

    if (fwVersions.empty())
    {
        Printf(Tee::PriLow, "File %s is empty\n", fileList[0].absFilePath.c_str());
        return RC::FILE_UNKNOWN_ERROR;
    }

    // Remove newline character ONLY from end of fwVersions
    if (fwVersions[fwVersions.length() - 1] == '\n')
    {
        fwVersions.erase(fwVersions.length() - 1);
    }

    // fwVersions will contain the data in below format.
    // BL: fd 02 00 30 62 6e 01 00
    // FW1: 2a 00 02 31 76 6e 01 00
    // FW2: 08 06 03 32 76 6e 12 31
    const UINT08 fwVerEntries = 0x3; // FW version is as shown above, has 3 entries
    const UINT08 fwSplitSize = 0x9;  // Each entry can be spilt based on spaces
    const UINT08 secFwVersionIndex = 0x1; // 1st byte (2A) is of interest from FW1
    const UINT08 minorVersionIndex = 0x7; // Last 2 bytes (12 31) are of interest from FW2
    const UINT08 majorVersionIndex = 0x8; 
    vector<string> fwVersionSplit;

    {
        // Separate FW version entries into individual entires based on new line
        string splitters("\n");
        boost::split(fwVersionSplit, fwVersions, boost::is_any_of(splitters));
        if (fwVersionSplit.size() != fwVerEntries)
        {
            Printf(Tee::PriLow,
                   "%s file should contain 3 entries, found %llu\n",
                   fileList[0].absFilePath.c_str(),
                   static_cast<UINT64>(fwVersionSplit.size()));
            return RC::UNEXPECTED_RESULT;
        }
    }

    {   // Extract secondary FW version (1st byte in FW1).
        // In example 2a is the secondary FW version
        // FW1: 2a 00 02 31 76 6e 01 00
        string splitters = " ";
        vector<string> secondaryVersionSplit;
        boost::split(secondaryVersionSplit, fwVersionSplit[1], boost::is_any_of(splitters));
        if (secondaryVersionSplit.size() != fwSplitSize)
        {
            Printf(Tee::PriLow,
                   "Expected FW1 format \"FW1: xx xx xx xx xx xx xx xx. Actual: %s\n",
                   fwVersionSplit[1].c_str());
            return RC::UNEXPECTED_RESULT;
        }
        *pSecondaryVersion = secondaryVersionSplit[secFwVersionIndex];
    }

    {   // Extract primary FW version (combination of last 2 byte in FW2).
        // In example 3.1.18 is the primary FW version
        // FW2: 08 06 03 32 76 6e 12 31
        string splitters = " ";
        vector<string> primaryVersionSplit;
        boost::split(primaryVersionSplit, fwVersionSplit[2], boost::is_any_of(splitters));
        if (primaryVersionSplit.size() != fwSplitSize)
        {
            Printf(Tee::PriLow,
                   "Expected FW2 format \"FW2: xx xx xx xx xx xx xx xx. Actual: %s\n",
                   fwVersionSplit[2].c_str());
            return RC::UNEXPECTED_RESULT;
        }
        UINT08 majorVersion = strtoul(primaryVersionSplit[majorVersionIndex].c_str(), nullptr, 16);
        UINT08 minorVersion = strtoul(primaryVersionSplit[minorVersionIndex].c_str(), nullptr, 16);
        *pPrimaryVersion = to_string((majorVersion >> 4) & 0xF) + "."
                           + to_string(majorVersion & 0xF) + "."
                           + to_string(minorVersion & 0xFF);
    }

    return rc;
}

RC Xp::GetPpcDriverVersion
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string *pDriverVersion
)
{
    // PPC driver version will be printed as part of MODS header
    // There may be cases where PPC driver is not installed and in that
    // case we don't want to keep printing all the messages from below function
    // So print priority is set to low for all prints

    RC rc;
    MASSERT(pDriverVersion);

    string portDir;
    rc = FindPortDir(domain, bus, device, function, &portDir);
    if (rc != OK)
    {
        Printf(Tee::PriLow,
               "Unable to find port* directory corresponding to current test device, "
               "make sure PPC driver is installed\n");
        return rc;
    }

    vector<PairedDirFilePath> fileList;
    CHECK_RC(FindPpcAbsoluteFilePath(NT_PPC_DRIVER_VERSION, portDir, "", "", &fileList));

    string driverVersion;
    if (!Xp::DoesFileExist(fileList[0].absFilePath))
    {
        Printf(Tee::PriLow, "Unable to find %s file\n", fileList[0].absFilePath.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    rc = Xp::InteractiveFileRead(fileList[0].absFilePath.c_str(), &driverVersion);
    if (rc != OK)
    {
        Printf(Tee::PriLow, "Unable to read data from %s file\n", fileList[0].absFilePath.c_str());
        return rc;
    }

    if (driverVersion.empty())
    {
        Printf(Tee::PriLow, "File %s is empty\n", fileList[0].absFilePath.c_str());
        return RC::FILE_UNKNOWN_ERROR;
    }

    // Remove trailing newline character if any
    driverVersion.erase(
            std::remove(driverVersion.begin(), driverVersion.end(), '\n'), driverVersion.end());

    *pDriverVersion = driverVersion;

    return rc;
}

RC Xp::GetFtbFwVersion
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    string *pFtbFwVersion
)
{
    RC rc;
    MASSERT(pFtbFwVersion);

    // Find the 'port0' directory corresponding to current port policy controller
    string portDir;
    rc = FindPortDir(domain, bus, device, function, &portDir);
    if (rc != OK)
    {
        Printf(Tee::PriError,
               "Unable to find port* directory corresponding to current test device, "
               "make sure PPC driver is installed\n");
        return rc;
    }

    vector<PairedDirFilePath> fileList;
    CHECK_RC(FindPpcAbsoluteFilePath(NT_VDM, portDir, "", "", &fileList));

    if (!Xp::DoesFileExist(fileList[0].absFilePath))
    {
        Printf(Tee::PriError, 
               "File %s does not exist. Unable to fetch FTB version\n",
               fileList[0].absFilePath.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    string fileData;
    CHECK_RC(Xp::InteractiveFileRead(fileList[0].absFilePath.c_str(), &fileData));
    if (fileList[0].absFilePath.empty())
    {
        Printf(Tee::PriError, 
               "File %s empty. Unable to fetch FTB version\n",
               fileList[0].absFilePath.c_str());
        return RC::FILE_READ_ERROR;
    }
    // Remove newline character
    fileData.erase(std::remove(fileData.begin(), fileData.end(), '\n'), fileData.end());

    // Reference bug: http://lwbugs/2108524/4
    // Data read is of the format 00002a8f 09558157 24026161,
    // we are insterested in bits 31-28(major version), 27-24(minor version)
    string splitters(" ");
    vector<string> fwVersStr;
    boost::split(fwVersStr, fileData, boost::is_any_of(splitters));
    if (fwVersStr.size() < 3)
    {
        Printf(Tee::PriError, 
               "Incorrect data read from %s\n",
               fileList[0].absFilePath.c_str());
        return RC::FILE_READ_ERROR;
    }

    const UINT32 majorVersionBit = 28;
    const UINT32 minorVersionBit = 24;
    const UINT32 fwVer = strtoul(fwVersStr[2].c_str(), nullptr, 16);

    *pFtbFwVersion = Utility::ColwertHexToStr((fwVer >> majorVersionBit) & 0xF) + "."
                       + Utility::ColwertHexToStr((fwVer >>minorVersionBit) & 0xF);

    return rc;
}

RC Xp::GetUsbPowerConsumption
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    FLOAT64 *pVoltageV,
    FLOAT64 *pLwrrentA
)
{
    RC rc;

    // Find the 'port0' directory corresponding to current port policy controller
    string portDir;
    CHECK_RC(FindPortDir(domain, bus, device, function, &portDir));

    // passing empty string to 2nd, 3rd and 4th argument since
    // portDir, devDir and modeDir are not needed for NodeType = PCI
    vector<PairedDirFilePath> fileList;
    CHECK_RC(FindPpcAbsoluteFilePath(NT_I2C, portDir, "", "", &fileList));
    string i2cNumFile = fileList[0].absFilePath;
    if (!Xp::DoesFileExist(i2cNumFile))
    {
        Printf(Tee::PriError,
               "%s: Unable to find \".../i2c\" node containing i2c device number\n",
               __FUNCTION__);
        return RC::FILE_DOES_NOT_EXIST;
    }

    string fileData;
    CHECK_RC(Xp::InteractiveFileRead(i2cNumFile.c_str(), &fileData));
    if (fileData.empty())
    {
        Printf(Tee::PriError,
               "No data read from %s file\n", i2cNumFile.c_str());
        return RC::FILE_READ_ERROR;
    }
    // Remove newline character if present
    fileData.erase(std::remove(fileData.begin(), fileData.end(), '\n'), fileData.end());

    string i2cDevName = "/dev/i2c-" + fileData;

    INT32 i2cDevHandle = open(i2cDevName.c_str(), O_RDWR);
    if (i2cDevHandle == -1)
    {
        Printf(Tee::PriError,
               "Unable to open %s for power measurement. "
               "Make sure the udev rule is updated to allow user mode access to i2c device "
               "and i2c-dev module is loaded\n",
               i2cDevName.c_str());
        return RC::DEVICE_NOT_FOUND;
    }

    // Prepare buck boost register
    vector<UINT08> writeData = { USB_READ_POWER_COMMAND };
    CHECK_RC(I2cWrite(i2cDevHandle,
                      I2C_CCGX_DEVICE_ADDR,
                      USB_HPI_REGISTER_ADDR,
                      writeData,
                      sizeof(USB_HPI_REGISTER_ADDR)));

    // Confirm buck boost is ready
    vector<UINT08> readData;
    CHECK_RC(I2cRead(i2cDevHandle,
                     I2C_CCGX_DEVICE_ADDR,
                     USB_HPI_STATUS_REGISTER_ADDR,
                     &readData,
                     sizeof(USB_HPI_STATUS_REGISTER_ADDR),
                     USB_STATUS_READ_BYTES));

    if (readData.size() != USB_STATUS_READ_BYTES)
    {
        Printf(Tee::PriError,
               "IOCTL read expected %u bytes, read %lu bytes\n",
               USB_STATUS_READ_BYTES,
               readData.size());
        return RC::USBTEST_CONFIG_FAIL;
    }

    if (readData[0] != 0x0)
    {
        Printf(Tee::PriError,
               "Buck boost returned error (%u)\n",
               readData[0]);
        return RC::USBTEST_CONFIG_FAIL;
    }

    // Read power delivered by buck boost
    readData.clear();
    CHECK_RC(I2cRead(i2cDevHandle,
                     I2C_CCGX_DEVICE_ADDR,
                     USB_HPI_PWR_REG_ADDR,
                     &readData,
                     sizeof(USB_HPI_PWR_REG_ADDR),
                     USB_POWER_READ_BYTES));

    if (readData.size() != USB_POWER_READ_BYTES)
    {
        Printf(Tee::PriError,
               "Error while reading power values from Buck Boost\n");
        return RC::USBTEST_CONFIG_FAIL;
    }

    *pVoltageV = (static_cast<UINT16>(readData[VOLTAGE_MAJOR_INDEX] << USB_BITS_FROM_MINOR_INDEX) 
                    | (readData[VOLTAGE_MINOR_INDEX] & USB_BITS_FROM_MINOR_INDEX))
                    / READ_TO_ACTUAL_VOLTAGE;

    // Reference for dividing by 2 -> http://lwbugs/2093458/17
    // The data sheet that MODS was provided for USB power callwlations
    // seems to be old and doesn't provide the correct formula. PPC driver team
    // compared the power value read from the register against Plugable power meter
    // and found that the register reported ~twice the power as compared to the meter.
    // Until we get the new data sheet, divding the Current by 2 as a WAR.
    *pLwrrentA = (readData[LWRRENT_INDEX] * READ_TO_ACTUAL_LWRRENT) / 2.0;

    return rc;
}
