/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "bgmonitor.h"
#include "bgmonitorfactory.h"
#include "core/include/fileholder.h"
#include "core/include/rc.h"
#include "core/include/types.h"
#include "core/include/utility.h"
#include "device/utility/genericipmi.h"
#include "lwmisc.h"

// RapidJson includes
#include "document.h"
#include "filereadstream.h"
#include "error/en.h"

using namespace rapidjson;

class IpmiLwstomMonitor final: public BgMonitor
{
    public:
        explicit IpmiLwstomMonitor(BgMonitorType type)
            : BgMonitor(type, "Ipmi Custom Sensor")
        {
        }

        vector<SampleDesc> GetSampleDescs(UINT32 devIdx) override
        {
            vector<SampleDesc> descs;
            descs.reserve(m_Sensors.size());

            for (const auto& sensor : m_Sensors)
            {
                descs.push_back({ sensor.label, sensor.units, true, INT });
            }

            return descs;
        }

        RC GatherData(UINT32 devIdx, Sample* pSample) override
        {
            StickyRC rc;
            vector<UINT08> recvData;

            {
                DEFER
                {
                    for (auto& custom : m_LwstomCleanup)
                    {
                        rc = m_IpmiDevice.RawAccess(custom.netfn, custom.cmd, custom.data,
                                                    recvData);
                    }
                };

                for (auto& custom : m_LwstomSetup)
                {
                    CHECK_RC(m_IpmiDevice.RawAccess(custom.netfn, custom.cmd, custom.data,
                                                    recvData));
                }

                for (auto& sensor : m_Sensors)
                {
                    if (sensor.hasWriteCommand)
                    {
                        CHECK_RC(m_IpmiDevice.RawAccess(sensor.writeCommand.netfn,
                                                        sensor.writeCommand.cmd,
                                                        sensor.writeCommand.data,
                                                        recvData));
                        Tasker::Sleep(sensor.delayMs);
                    }
                    CHECK_RC(m_IpmiDevice.RawAccess(sensor.readData.netfn,
                                                    sensor.readData.cmd,
                                                    sensor.readData.data,
                                                    recvData));

                    if (sensor.dataByteOffset + sensor.dataBytes > recvData.size())
                    {
                        return RC::UNEXPECTED_RESULT;
                    }

                    UINT32 value = 0;
                    for (unsigned int byte = 0; byte < sensor.dataBytes; byte++)
                    {
                        value |= recvData[byte + sensor.dataByteOffset] << (byte * 8);
                    }

                    pSample->Push(value);
                }
            }

            return rc;
        }

        bool IsSubdevBased() override
        {
            return false;
        }

        bool IsLwSwitchBased() override
        {
            return false;
        }

        // This logger does not touch any GPUs, so it does not need to
        // be paused.
        bool IsPauseable() override
        {
            return false;
        }

        UINT32 GetNumDevices() override
        {
            //only supports one ipmi device
            return 1;
        }

        bool IsMonitoring(UINT32) override
        {
            return true;
        }

    private:

        struct IpmiCommand
        {
            UINT08 netfn;
            UINT08 cmd;
            vector<UINT08> data;
        };

        string IpmiCommandToString(const IpmiCommand& command)
        {
            string s = Utility::StrPrintf("0x%02x 0x%02x", command.netfn, command.cmd);
            for (const auto& d : command.data)
            {
                s.append(Utility::StrPrintf(" 0x%02x", d));
            }

            return s;
        }

        struct SensorInfo
        {
            string label;
            string units;
            bool hasWriteCommand = false;
            IpmiCommand writeCommand;
            FLOAT64 delayMs = 0;
            IpmiCommand readData;
            UINT08 dataByteOffset = 0;
            UINT08 dataBytes = 0;
        };

        RC JsonParseUINT32(const string& context, const Value& json, UINT32* pValue)
        {
            RC rc;
            UINT32 value = 0;
            if (json.IsString())
            {
                string s(json.GetString(), json.GetStringLength());
                CHECK_RC(Utility::StringToUINT32(s, &value, 0));
            }
            else if (json.IsUint())
            {
                value = json.GetUint(); // RapidJson Uint is 32 bits
            }
            else
            {
                Printf(Tee::PriError, "%s: Expecting unsigned integer or string.\n",
                       context.c_str());
                return RC::CANNOT_PARSE_FILE;
            }

            *pValue = value;

            return rc;
        }

        RC JsonParseUINT08(const string& context, const Value& json, UINT08* pValue)
        {
            RC rc;
            UINT32 value = 0;

            CHECK_RC(JsonParseUINT32(context, json, &value));

            if (value > 255)
            {
                Printf(Tee::PriError, "%s: Expecting 8-bit integer: %d.\n",
                       context.c_str(), value);
                return RC::CANNOT_PARSE_FILE;
            }

            *pValue = value;

            return rc;
        }

        RC JsonParseIpmiCommand(const string& context, const Value& command, IpmiCommand* pLwstom)
        {
            RC rc;

            if (!command.IsArray())
            {
                Printf(Tee::PriError, "%s: Expecting array.\n", context.c_str());
                return RC::CANNOT_PARSE_FILE;
            }

            SizeType size = command.Size();
            if (size < 2)
            {
                Printf(Tee::PriError, "%s: Only %d elements, needs at least 2.\n",
                       context.c_str(), size);
                return RC::CANNOT_PARSE_FILE;
            }

            for (SizeType i = 0; i < size; i++)
            {
                UINT08 value = 0;
                CHECK_RC(JsonParseUINT08(context + "[]", command[i], &value));

                if (i == 0)
                {
                    pLwstom->netfn = value;
                }
                else if (i == 1)
                {
                    pLwstom->cmd = value;
                }
                else
                {
                    pLwstom->data.push_back(value);
                }
            }

            return rc;
        }

        RC ParseConfig(const string& configFile)
        {
            RC rc;
            Document doc;
            vector<char> JsonBuffer;
            CHECK_RC(Utility::ReadPossiblyEncryptedFile(configFile, &JsonBuffer, nullptr));

            // This is necessary to avoid RapidJson accessing beyond JsonBuffer's bounds
            JsonBuffer.push_back('\0');

            doc.Parse(&JsonBuffer[0]);

            if (doc.HasParseError())
            {
                size_t line, column;

                CHECK_RC(Utility::GetLineAndColumnFromFileOffset(
                    JsonBuffer, doc.GetErrorOffset(), &line, &column));

                Printf(Tee::PriError,
                       "JSON syntax error in %s at line %zd column %zd\n",
                       configFile.c_str(), line, column);
                Printf(Tee::PriError, "%s\n", GetParseError_En(doc.GetParseError()));

                return RC::CANNOT_PARSE_FILE;
            }

            if (doc.HasMember("setup"))
            {
                const Value& cmds = doc["setup"];
                if (!cmds.IsArray())
                {
                    Printf(Tee::PriError, "%s:setup: Expecting an array.\n", configFile.c_str());
                    return RC::CANNOT_PARSE_FILE;
                }

                for (SizeType i = 0; i < cmds.Size(); i++)
                {
                    string context = Utility::StrPrintf("%s:setup[%d]", configFile.c_str(), i);
                    IpmiCommand cmd;
                    CHECK_RC(JsonParseIpmiCommand(context, cmds[i], &cmd));

                    Printf(Tee::PriLow, "Adding Ipmi Custom Sensor setup: %s\n",
                           IpmiCommandToString(cmd).c_str());

                    m_LwstomSetup.push_back(cmd);
                }

            }

            if (doc.HasMember("cleanup"))
            {
                const Value& cmds = doc["cleanup"];
                if (!cmds.IsArray())
                {
                    Printf(Tee::PriError, "%s:cleanup: Expecting an array.\n", configFile.c_str());
                    return RC::CANNOT_PARSE_FILE;
                }

                for (SizeType i = 0; i < cmds.Size(); i++)
                {
                    string context = Utility::StrPrintf("%s:cleanup[%d]", configFile.c_str(), i);
                    IpmiCommand cmd;
                    CHECK_RC(JsonParseIpmiCommand(context, cmds[i], &cmd));

                    Printf(Tee::PriLow, "Adding Ipmi Custom Sensor cleanup: %s\n",
                           IpmiCommandToString(cmd).c_str());

                    m_LwstomCleanup.push_back(cmd);
                }
            }

            if (!doc.HasMember("sensors"))
            {
                Printf(Tee::PriError, "%s: Missing required section: sensors\n",
                       configFile.c_str());
                return RC::CANNOT_PARSE_FILE;
            }

            const Value& sensors = doc["sensors"];
            if (!sensors.IsArray())
            {
                Printf(Tee::PriError, "%s:sensors: Expecting an array.\n", configFile.c_str());
                return RC::CANNOT_PARSE_FILE;
            }

            for (SizeType i = 0; i < sensors.Size(); i++)
            {
                const Value& sensor = sensors[i];
                SensorInfo info;
                string context = Utility::StrPrintf("%s:sensors[%d]", configFile.c_str(), i);

                if (!sensor.IsObject())
                {
                    Printf(Tee::PriError, "%s: Expecting an object.\n", context.c_str());
                    return RC::CANNOT_PARSE_FILE;
                }

                // parse label. required string
                if (!sensor.HasMember("label"))
                {
                    Printf(Tee::PriError, "%s: Missing required section: label.\n",
                           context.c_str());
                    return RC::CANNOT_PARSE_FILE;
                }
                if (!sensor["label"].IsString())
                {
                    Printf(Tee::PriError, "%s:label: Expecting a string.\n", context.c_str());
                    return RC::CANNOT_PARSE_FILE;
                }
                info.label.assign(sensor["label"].GetString(), sensor["label"].GetStringLength());

                // parse units. required string
                if (!sensor.HasMember("units"))
                {
                    Printf(Tee::PriError, "%s: Missing required section: units.\n",
                           context.c_str());
                    return RC::CANNOT_PARSE_FILE;
                }
                if (!sensor["units"].IsString())
                {
                    Printf(Tee::PriError, "%s:units: Expecting a string.\n", context.c_str());
                    return RC::CANNOT_PARSE_FILE;
                }
                info.units.assign(sensor["units"].GetString(), sensor["units"].GetStringLength());

                // parse data. optional ipmi command
                if (sensor.HasMember("command"))
                {
                    CHECK_RC(JsonParseIpmiCommand(context + ":command",
                                                  sensor["command"],
                                                  &info.writeCommand));
                    info.hasWriteCommand = true;
                }

                // parse data. required ipmi command
                if (!sensor.HasMember("data"))
                {
                    Printf(Tee::PriError, "%s: Missing required section: data.\n",
                           context.c_str());
                    return RC::CANNOT_PARSE_FILE;
                }
                CHECK_RC(JsonParseIpmiCommand(context + ":data", sensor["data"], &info.readData));

                // parse delay. optional double in seconds
                if (sensor.HasMember("delay"))
                {
                    if (!sensor["delay"].IsDouble())
                    {
                        Printf(Tee::PriError, "%s:delay: Expecting a floating point number.\n",
                               context.c_str());
                        return RC::CANNOT_PARSE_FILE;
                    }
                    info.delayMs = sensor["delay"].GetDouble() * 1000.0;
                }

                // parse offset. required uint
                if (!sensor.HasMember("offset"))
                {
                    Printf(Tee::PriError, "%s: Missing required section: offset.\n",
                           context.c_str());
                    return RC::CANNOT_PARSE_FILE;
                }
                CHECK_RC(JsonParseUINT08(context + ":offset", sensor["offset"],
                                         &info.dataByteOffset));

                // parse size. required uint
                if (!sensor.HasMember("size"))
                {
                    Printf(Tee::PriError, "%s: Missing required section: size.\n",
                           context.c_str());
                    return RC::CANNOT_PARSE_FILE;
                }
                CHECK_RC(JsonParseUINT08(context + ":size", sensor["size"], &info.dataBytes));
                if ((info.dataBytes > 4) || (info.dataBytes == 0))
                {
                    Printf(Tee::PriError, "%s:size: %d is outside the valid range of 1 to 4.\n",
                           context.c_str(), info.dataBytes);
                    return RC::CANNOT_PARSE_FILE;
                }

                Printf(Tee::PriLow, "Adding Ipmi custom sensor: %s\n", info.label.c_str());
                Printf(Tee::PriLow, "    units:   %s\n", info.units.c_str());
                if (info.hasWriteCommand)
                {
                    Printf(Tee::PriLow, "    cmd:     %s\n",
                           IpmiCommandToString(info.writeCommand).c_str());
                }
                Printf(Tee::PriLow, "    delayMs: %f\n", info.delayMs);
                Printf(Tee::PriLow, "    data:    %s\n",
                       IpmiCommandToString(info.readData).c_str());
                Printf(Tee::PriLow, "    offset:  %d\n", info.dataByteOffset);
                Printf(Tee::PriLow, "    size:    %d\n", info.dataBytes);

                m_Sensors.push_back(info);
            }

            return rc;
        }

        RC InitFromJsImpl(const JsArray&     params,
                          const set<UINT32>& monitorDevices)
        {
            RC rc;
            JavaScriptPtr js;

            string configFile;
            CHECK_RC(js->FromJsval(params[0], &configFile));

            CHECK_RC(ParseConfig(configFile));

            return RC::OK;
        }

        IpmiDevice     m_IpmiDevice;
        vector<SensorInfo> m_Sensors;
        vector<IpmiCommand> m_LwstomSetup;
        vector<IpmiCommand> m_LwstomCleanup;
};

static BgMonFactoryRegistrator<IpmiLwstomMonitor> registerIpmiLwstomMonitor
(
    BgMonitorFactories::GetInstance(),
    BgMonitorType::IPMI_LWSTOM_SENSOR
);
