/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include <algorithm>
#include <map>
#include "core/include/utility.h"
#include "core/include/argpproc.h"
#include "core/include/argdb.h"
#include "core/include/argread.h"
#include "gpu/utility/smlwtil.h"

#include "lwmisc.h"
#include "ctrl/ctrlxxxx.h"
#include "ctrl/ctrl2080/ctrl2080gpu.h"

static const std::map<UINT32, string> s_PartFlag2String =
{
    { LW2080_CTRL_GPU_PARTITION_FLAG_FULL_GPU,         "full"    },
    { LW2080_CTRL_GPU_PARTITION_FLAG_ONE_HALF_GPU,     "half"    },
    { LW2080_CTRL_GPU_PARTITION_FLAG_ONE_QUARTER_GPU , "quarter" },
    { LW2080_CTRL_GPU_PARTITION_FLAG_ONE_EIGHTHED_GPU, "eighth"  }
};
static const std::map<string, UINT32> s_String2PartFlag =
{
    { "full",    LW2080_CTRL_GPU_PARTITION_FLAG_FULL_GPU         },
    { "half",    LW2080_CTRL_GPU_PARTITION_FLAG_ONE_HALF_GPU     },
    { "quarter", LW2080_CTRL_GPU_PARTITION_FLAG_ONE_QUARTER_GPU  },
    { "eighth" , LW2080_CTRL_GPU_PARTITION_FLAG_ONE_EIGHTHED_GPU }
};

static const ParamDecl s_SmcParams[] =
{
    { "part", "tt", ParamDecl::PARAM_MULTI_OK, 0, 0, "SMC partition configuration" },
    LAST_PARAM
};
static const ParamConstraints s_SmcParamsConstraints[] =
{
    LAST_CONSTRAINT
};

namespace Smlwtil
{
    RC ParsePartitionsStr
    (
        string config,
        vector<Partition>* pPartitionData
    )
    {
        RC rc;
        MASSERT(pPartitionData);
        if (config.empty())
        {
            Printf(Tee::PriError, "-smc_partitions <args> is configured with an empty string!\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        // Replace all ":" with spaces so that the argReader can parse the arg string
        std::replace(config.begin(), config.end(), ':', ' ');

        // Construct the argument string
        vector<string> smcArgVector;
        string::size_type copyStart = config.find_first_not_of(' ');
        string::size_type copyEnd = config.find_first_of(' ', copyStart);
        while (copyStart != copyEnd)
        {
            if (copyEnd == string::npos)
            {
                smcArgVector.push_back(config.substr(copyStart));
                copyStart = copyEnd;
            }
            else
            {
                smcArgVector.push_back(config.substr(copyStart, copyEnd - copyStart));
                copyStart = config.find_first_not_of(' ', copyEnd);
                copyEnd = config.find_first_of(' ', copyStart);
            }
        }

        // Create the arg database
        ArgPreProcessor smcPreProc;
        ArgDatabase smcArgDatabase;
        if (RC::OK != smcPreProc.PreprocessArgs(&smcArgVector))
        {
            Printf(Tee::PriError, "-smc_partitions is incorrectly formatted\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
        if (RC::OK != smcPreProc.AddArgsToDatabase(&smcArgDatabase))
        {
            Printf(Tee::PriError, "-smc_partitions is incorrectly formatted\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        ArgReader argReader(s_SmcParams, s_SmcParamsConstraints);
        if (!argReader.ParseArgs(&smcArgDatabase))
        {
            Printf(Tee::PriError, "Unable to parse -smc_partitions\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        // Handle "part" argument
        const UINT32 numPartitions = argReader.ParamPresent("part");
        for (UINT32 i = 0; i < numPartitions; i++)
        {
            const char* flagStr = argReader.ParamNStr("part", i, 0);
            const char* smcEngineConfigStr = argReader.ParamNStr("part", i, 1);

            // Parse partition config
            Partition part = {};
            if (s_String2PartFlag.count(flagStr))
            {
                part.partitionFlag = s_String2PartFlag.at(flagStr);
            }
            else
            {
                Printf(Tee::PriError, "Unsupported partition type '%s'!\n", flagStr);
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            // Parse SMC Engine config
            vector<string> tokens;
            CHECK_RC(Utility::Tokenizer(smcEngineConfigStr, "-", &tokens));
            for (const auto& token : tokens)
            {
                bool useWithSriov = false;
                UINT32 strStart = 0;
                if (token.size() > 1 && Utility::ToLowerCase(token)[0] == 's')
                {
                    // 'S' indicates that we create the SR-IOV instance on this engine
                    useWithSriov = true;
                    strStart = 1;
                }

                if (Utility::ToLowerCase(token) == "x")
                {
                    part.smcEngineData.push_back({false, useWithSriov, 0});
                }
                else if (Utility::ToLowerCase(token) == "a")
                {
                    part.smcEngineData.push_back({true, useWithSriov, 0});
                }
                else
                {
                    // Parse numeric GPC count
                    UINT32 gpcCount = 0;
                    CHECK_RC(Utility::StringToUINT32(token.substr(strStart), &gpcCount, 0));
                    part.smcEngineData.push_back({false, useWithSriov, gpcCount});
                }
            }
            pPartitionData->push_back(part);
        }

        // Dump out parsed configuration
        for (UINT32 i = 0; i < pPartitionData->size(); i++)
        {
            const auto& part = (*pPartitionData)[i];
            Printf(Tee::PriLow, "Partition %d: %s\n",
                   i, s_PartFlag2String.at(part.partitionFlag).c_str());
            for (const auto& engdata : part.smcEngineData)
            {
                Printf(Tee::PriLow, "    SMC Engine: %d%s\n",
                       engdata.gpcCount,
                       (engdata.useAllGpcs) ? " (Use all GPCs)" : "");
            }
        }
        return rc;
    }
}

