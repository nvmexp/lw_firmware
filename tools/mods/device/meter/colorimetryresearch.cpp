/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "colorimetryresearch.h"
#include "colorimetryresearch/flickerapi.h"
#include "core/include/serial.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/xp.h"

bool ColorScienceAPI1LibraryInitialized = false;

typedef int32_t (*fn_cs_flicker_filter)
(
    cs_flicker_filter_t* filter,
    double sampling_rate,
    double *data,
    uint32_t count,
    double *filtered_data
);
typedef flicker_t* (*fn_cs_flicker_create)
(
    double sampling_rate,
    double *data,
    uint32_t count,
    double maximumSearchFrequency
);
typedef void (*fn_cs_flicker_free)(flicker_t *ctx);
typedef int32_t (*fn_cs_flicker_data)
(
    flicker_t *ctx,
    cs_flicker_data_t *flicker_data
);

fn_cs_flicker_filter dll_cs_flicker_filter = nullptr;
fn_cs_flicker_create dll_cs_flicker_create = nullptr;
fn_cs_flicker_data dll_cs_flicker_data = nullptr;
fn_cs_flicker_free dll_cs_flicker_free = nullptr;

#define GET_CS_FUNCTION(function_name)                                        \
do {                                                                          \
    dll_##function_name =                                                     \
        (fn_##function_name) Xp::GetDLLProc(cs_lib_handle, #function_name);   \
    if (!dll_##function_name)                                                 \
    {                                                                         \
        Printf(Tee::PriHigh, "Cannot load ColorScienceAPI1 function (%s)\n",  \
               #function_name);                                               \
        return RC::DLL_LOAD_FAILED;                                           \
    }                                                                         \
} while (0)

static RC InitializeCSLibrary()
{
    RC rc;

    if (ColorScienceAPI1LibraryInitialized)
        return OK;

    void *cs_lib_handle;
    string cs_lib_name = "libColorScienceAPI.so";
    if (Xp::GetOperatingSystem() == Xp::OS_WINMFG)
        cs_lib_name = "ColorScienceAPI1.dll";
    rc = Xp::LoadDLL(cs_lib_name, &cs_lib_handle, Xp::UnloadDLLOnExit);
    if (OK != rc)
    {
        Printf(Tee::PriHigh,
            "Cannot load ColorScienceAPI1 library %s, rc = %d\n",
            cs_lib_name.c_str(), (UINT32)rc);
        return rc;
    }

    GET_CS_FUNCTION(cs_flicker_filter);
    GET_CS_FUNCTION(cs_flicker_create);
    GET_CS_FUNCTION(cs_flicker_data);
    GET_CS_FUNCTION(cs_flicker_free);

    ColorScienceAPI1LibraryInitialized = true;

    return rc;
}

static bool ParseResponse(const string *response, string *value)
{
    vector<string> tokens;
    if (OK != Utility::Tokenizer(*response, ":\n\r", &tokens))
        return false;

    if (tokens.size() == 0)
        return false;
    if (tokens[0] != "OK")
        return false;

    if ((value != nullptr) &&
        (tokens.size() >= 4))
        *value = tokens[3];

    return true;
}

enum SendComandMode
{
    SENDCOMMAND_ONLY_WRITE = 1,
    SENDCOMMAND_ONLY_READ = 2,
    SENDCOMMAND_WRITE_AND_READ = SENDCOMMAND_ONLY_WRITE | SENDCOMMAND_ONLY_READ,
};

static RC SendCommand
(
    Serial *pCom,
    const char *cmd,
    SendComandMode mode,
    UINT32 allowExtraWaitms,
    string *value
)
{
    Printf(Tee::PriLow, "CRI: Sending command %s", cmd);

    string response;
    UINT32 cmdAttempts = 0;
    do
    {
        cmdAttempts++;
        if (mode & SENDCOMMAND_ONLY_WRITE)
        {
            pCom->ClearBuffers(SerialConst::CLIENT_METER);
            RC cmdRC = pCom->PutString(SerialConst::CLIENT_METER, cmd);
            if (cmdRC != OK)
            {
                Tasker::Sleep(100);
                continue;
            }
        }
        if (mode & SENDCOMMAND_ONLY_READ)
        {
            response.erase();
            UINT32 totalreadWaitms = 0;
            do
            {
                if (pCom->ReadBufCount() == 0)
                {
                    Tasker::Sleep(1);
                    totalreadWaitms += 1;
                    continue;
                }
                string partialResponse;
                RC cmdRC = pCom->GetString(SerialConst::CLIENT_METER,
                    &partialResponse);
                if (cmdRC != OK)
                {
                    Printf(Tee::PriNormal, "CRI: GetString rc = %d\n",
                        (UINT32)cmdRC);
                    Tasker::Sleep(10);
                    totalreadWaitms += 10;
                    continue;
                }
                response += partialResponse;

                while (response.size() && response[0] == '\n')
                {
                    response.erase(0, 1);
                }
            } while ((response.find("\n") == string::npos) &&
                     (totalreadWaitms < (20+allowExtraWaitms)));

            Printf(Tee::PriLow, "CRI: totalreadWaitms = %d\n", totalreadWaitms);

            if (ParseResponse(&response, value))
            {
                return OK;
            }
            Printf(Tee::PriLow, "CRI: Parsing of response %s has failed.\n",
                response.c_str());
        }
        else
        {
            return OK;
        }
    } while (cmdAttempts<3);

    return RC::SERIAL_COMMUNICATION_ERROR;
}

ColorimetryResearchInstruments::~ColorimetryResearchInstruments()
{
    for (UINT32 instrIdx = 0; instrIdx < m_Instruments.size(); instrIdx++)
    {
        m_Instruments[instrIdx].pCom->Uninitialize(SerialConst::CLIENT_METER);
    }
}

static RC OneTimeConfiguration
(
    Serial *pCom
)
{
    RC rc;

    // Select "Auto" range:
    CHECK_RC(SendCommand(pCom, "SM RangeMode 0\n", SENDCOMMAND_WRITE_AND_READ, 0, nullptr));
    // Select "Auto" exposure:
    CHECK_RC(SendCommand(pCom, "SM ExposureMode 0\n", SENDCOMMAND_WRITE_AND_READ, 0, nullptr));
    // Select sampling frequency for flicker measurements:
    CHECK_RC(SendCommand(pCom, "SM SamplingRate 1600\n", SENDCOMMAND_WRITE_AND_READ, 0, nullptr));

    return OK;
}

RC ColorimetryResearchInstruments::FindInstruments(UINT32 *numInstruments)
{
    RC rc;

    *numInstruments = 0;

    CHECK_RC(InitializeCSLibrary());

    for (UINT32 portIdx = 1; portIdx <= Serial::GetHighestComPort(); portIdx++)
    {
        Serial *pCom = GetSerialObj::GetCom(portIdx);

        RC iniRC = pCom->Initialize(SerialConst::CLIENT_METER, true);

        if (iniRC != OK)
        {
            pCom->Uninitialize(SerialConst::CLIENT_METER);
            continue;
        }

        string modelName;
        RC cmdRC = SendCommand(pCom, "RC Model\n", SENDCOMMAND_WRITE_AND_READ,
            0, &modelName);
        if (cmdRC != OK)
        {
            pCom->Uninitialize(SerialConst::CLIENT_METER);
            continue;
        }

        if (modelName.size())
        {
            Instrument newInstrument = {pCom, modelName, false};

            CHECK_RC(OneTimeConfiguration(pCom));

            m_Instruments.push_back(newInstrument);
        }
        else
        {
            CHECK_RC(pCom->Uninitialize(SerialConst::CLIENT_METER));
        }
    }

    *numInstruments = (UINT32)m_Instruments.size();

    return OK;
}

static RC StartTemporalMeasurement
(
    Serial *pCom
)
{
    RC rc;

    string id;
    CHECK_RC(SendCommand(pCom, "RC ID\n", SENDCOMMAND_WRITE_AND_READ, 0, &id));
    CHECK_RC(SendCommand(pCom, "SM Mode 1\n", SENDCOMMAND_WRITE_AND_READ, 0, nullptr));
    CHECK_RC(SendCommand(pCom, "M\n",  SENDCOMMAND_ONLY_WRITE, 0, nullptr));

    return OK;
}

static RC GetTemporalValues
(
    Serial *pCom,
    vector<FLOAT64> *values,
    FLOAT64 *samplingFrequency
)
{
    RC rc;

    values->clear();

    CHECK_RC(SendCommand(pCom, "M\n",  SENDCOMMAND_ONLY_READ, 2000, nullptr));

    // This has to be done separately as the measurement
    // points will come right after the response and we could loose them.
    CHECK_RC(pCom->PutString(SerialConst::CLIENT_METER, "RM Temporal\n"));

    UINT32 readAttempts = 0;
    string temporalResponse;
    string partialResponse;
    do
    {
        readAttempts++;
        partialResponse.erase();
        CHECK_RC(pCom->GetString(SerialConst::CLIENT_METER, &partialResponse));
        if (partialResponse.size())
        {
            temporalResponse += partialResponse;
        }
        else
        {
            Tasker::Sleep(1);
        }
    } while ((temporalResponse.find("\n") == string::npos) &&
        (readAttempts < 250));

    vector<string> temporalResponseTokens;
    CHECK_RC(Utility::Tokenizer(temporalResponse, ":",
        &temporalResponseTokens));

    if (temporalResponseTokens.size() < 4)
    {
        Printf(Tee::PriNormal,
            "Temporal command error - not complete response\n");
        return RC::SERIAL_COMMUNICATION_ERROR;
    }
    if (temporalResponseTokens[0] != "OK")
    {
        Printf(Tee::PriNormal,
            "Temporal command error - not successful response\n");
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    UINT32 numValues = 0;
    if (2 != sscanf(temporalResponseTokens[3].c_str(), "%lf,%u",
        samplingFrequency, &numValues))
    {
        Printf(Tee::PriNormal,
            "Temporal command error - parameter parsing error\n");
        return RC::SERIAL_COMMUNICATION_ERROR;
    }
    temporalResponse.erase(0, temporalResponse.find("\n")+1);

    while (values->size() < numValues)
    {
        do
        {
            while (temporalResponse.size() &&
                   temporalResponse[0] == '\n')
            {
                temporalResponse.erase(temporalResponse.begin());
            }
            if (temporalResponse.find("\n") == string::npos)
                break;

            UINT32 value = 0;
            sscanf(temporalResponse.c_str(), "%d", &value);
            values->push_back(value);
            temporalResponse.erase(0, temporalResponse.find("\n")+1);
        } while (true);

        if (values->size() >= numValues)
            break;

        partialResponse.erase();
        CHECK_RC(pCom->GetString(SerialConst::CLIENT_METER, &partialResponse));
        if (partialResponse.size())
        {
            temporalResponse += partialResponse;
        }
        else
        {
            Tasker::Sleep(1);
        }
    }

    if (values->size() != numValues)
    {
        Printf(Tee::PriNormal,
            "Temporal command error - unexpected number of values = %d "
            "(expected %d)\n", (UINT32)values->size(), numValues);
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    return OK;
}

RC ColorimetryResearchInstruments::MeasureFlickerStart
(
    UINT32 instrumentIdx
)
{
    RC rc;

    if (instrumentIdx >= m_Instruments.size())
        return RC::SOFTWARE_ERROR;

    Serial *pCom = m_Instruments[instrumentIdx].pCom;

    CHECK_RC(StartTemporalMeasurement(pCom));

    m_Instruments[instrumentIdx].MeasureFlickerStarted = true;

    return OK;
}

RC ColorimetryResearchInstruments::MeasureFlicker
(
    UINT32 instrumentIdx,
    FLOAT64 maximumSearchFrequency,
    FLOAT64 lowPassFilterFrequency,
    UINT32 filterOrder,
    FLOAT64 *flickerFrequency,
    FLOAT64 *flickerLevel,
    FLOAT64 *flickerModulationAmpliture
)
{
    RC rc;

    if (instrumentIdx >= m_Instruments.size())
        return RC::SOFTWARE_ERROR;

    *flickerFrequency = 0;
    *flickerLevel = 0;

    Serial *pCom = m_Instruments[instrumentIdx].pCom;

    if (m_Instruments[instrumentIdx].MeasureFlickerStarted)
    {
        m_Instruments[instrumentIdx].MeasureFlickerStarted = false;
    }
    else
    {
        CHECK_RC(StartTemporalMeasurement(pCom));
    }

    vector<FLOAT64> temportalValues;
    FLOAT64 samplingFrequency = 0;
    CHECK_RC(GetTemporalValues(pCom, &temportalValues, &samplingFrequency));
    if (temportalValues.size() == 0)
    {
        Printf(Tee::PriNormal,
            "Error: Temporal measurement didn't return any values\n");
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    vector<FLOAT64> filteredValues;
    filteredValues.resize(temportalValues.size());

    cs_flicker_filter_t filterData = {0};
    filterData.filter_type = FILTER_TYPE_NONE;
    filterData.filter_family = FILTER_FAMILY_NONE;
    filterData.order = filterOrder;
    filterData.frequency = 800; // __CR_FILTER_FREQUENCY_MAX
    filterData.bandwidth = 800; // __CR_FILTER_BANDWIDTH_MAX

    if (lowPassFilterFrequency > 0)
    {
        filterData.filter_type = FILTER_TYPE_LOWPASS;
        filterData.frequency = lowPassFilterFrequency;
    }

    dll_cs_flicker_filter(&filterData, samplingFrequency,
        &temportalValues[0], (uint32_t)temportalValues.size(),
        &filteredValues[0]);
    flicker_t *flickerContext = dll_cs_flicker_create(samplingFrequency,
        &filteredValues[0], (uint32_t)filteredValues.size(),
        maximumSearchFrequency);

    cs_flicker_data_t flickerData  = { 0 };
    if (flickerContext != 0)
        dll_cs_flicker_data(flickerContext, &flickerData);
    // It lwrrently crashes, to be fixed:
    // dll_cs_flicker_free(flickerContext);
    flickerContext = nullptr;

    *flickerFrequency = flickerData.flicker_frequency;
    *flickerLevel = flickerData.flicker_level;
    *flickerModulationAmpliture = flickerData.flicker_modulation_amplitude;

    return OK;
}

RC ColorimetryResearchInstruments::PerformColorimeterMeasurement
(
    UINT32 instrumentIdx,
    FLOAT64 *manualSyncFrequencyHz
)
{
    RC rc;

    if (instrumentIdx >= m_Instruments.size())
        return RC::SOFTWARE_ERROR;

    bool useAutosync = (manualSyncFrequencyHz == nullptr) ||
        (*manualSyncFrequencyHz == 0.0);

    Serial *pCom = m_Instruments[instrumentIdx].pCom;

    string id;
    CHECK_RC(SendCommand(pCom, "RC ID\n", SENDCOMMAND_WRITE_AND_READ, 0, &id));
    CHECK_RC(SendCommand(pCom, "SM Mode 0\n", SENDCOMMAND_WRITE_AND_READ, 0,
        nullptr)); // Colorimeter
    if (useAutosync)
    {
        CHECK_RC(SendCommand(pCom, "SM SyncMode 1\n",
            SENDCOMMAND_WRITE_AND_READ, 0, nullptr)); // Auto
    }
    else
    {
        CHECK_RC(SendCommand(pCom, "SM SyncMode 2\n",
            SENDCOMMAND_WRITE_AND_READ, 0, nullptr)); // Manual
        string syncFreqCmd = Utility::StrPrintf("SM SyncFreq %.2f\n", *manualSyncFrequencyHz);
        CHECK_RC(SendCommand(pCom, syncFreqCmd.c_str(), SENDCOMMAND_WRITE_AND_READ, 0, nullptr));
    }

    // Autosync takes some time - timeout 5000 covers the worst case with 100%+ margin:
    CHECK_RC(SendCommand(pCom, "M\n", SENDCOMMAND_WRITE_AND_READ, 5000, nullptr));

    return OK;
}

RC ColorimetryResearchInstruments::CaptureSyncFrequency
(
    UINT32 instrumentIdx,
    FLOAT64 *manualSyncFrequencyHz
)
{
    RC rc;

    if (instrumentIdx >= m_Instruments.size())
        return RC::SOFTWARE_ERROR;

    Serial *pCom = m_Instruments[instrumentIdx].pCom;

    if ((manualSyncFrequencyHz != nullptr) && (*manualSyncFrequencyHz == 0.0))
    {
        string syncFreq;
        CHECK_RC(SendCommand(pCom, "RM SyncFreq\n", SENDCOMMAND_WRITE_AND_READ, 0, &syncFreq));
        *manualSyncFrequencyHz = atof(syncFreq.c_str());
    }

    return OK;
}

RC ColorimetryResearchInstruments::MeasureLuminance
(
    UINT32 instrumentIdx,
    FLOAT64 *manualSyncFrequencyHz, // If null or 0.0 the auto sync will be used.
                                    // The auto determined freq will be returned
                                    // here if not null.
    FLOAT64 *luminanceCdM2 // CdM2 = candela per square meter (also called nits)
)
{
    RC rc;

    *luminanceCdM2 = 0.0;

    CHECK_RC(PerformColorimeterMeasurement(instrumentIdx, manualSyncFrequencyHz));

    Serial *pCom = m_Instruments[instrumentIdx].pCom;
    string y;
    CHECK_RC(SendCommand(pCom, "RM Y\n", SENDCOMMAND_WRITE_AND_READ, 0, &y));

    *luminanceCdM2 = atof(y.c_str());

    CHECK_RC(CaptureSyncFrequency(instrumentIdx, manualSyncFrequencyHz));

    return OK;
}

RC ColorimetryResearchInstruments::MeasureCIE1931xy
(
    UINT32 instrumentIdx,
    FLOAT64 *manualSyncFrequencyHz, // See ::MeasureLuminance for description
    FLOAT32 *x,
    FLOAT32 *y
)
{
    RC rc;

    *x = 0.0;
    *y = 0.0;

    CHECK_RC(PerformColorimeterMeasurement(instrumentIdx, manualSyncFrequencyHz));

    Serial *pCom = m_Instruments[instrumentIdx].pCom;
    string xy;
    CHECK_RC(SendCommand(pCom, "RM xy\n", SENDCOMMAND_WRITE_AND_READ, 0, &xy));

    if (sscanf(xy.c_str(), "%f,%f", x, y) != 2)
    {
        Printf(Tee::PriNormal,
            "Error: Unable to parse instument response for xy.\n");
        return RC::SERIAL_COMMUNICATION_ERROR;
    }

    CHECK_RC(CaptureSyncFrequency(instrumentIdx, manualSyncFrequencyHz));

    return OK;
}
