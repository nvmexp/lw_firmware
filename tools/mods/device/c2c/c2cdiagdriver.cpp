/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwmisc.h"
#include "c2cdiagdriver.h"
#include "gh100c2cdiagregs.h"             // For GH100 registers
#include "core/include/platform.h"

//-----------------------------------------------------------------------------
RC C2cDiagDriver::GetLinkStatus(LwU32 linkIdx, LinkStatus *linkStatus)
{
    LwU32 c2cStatus;
    RC status;

    if (!IsLinkIdxValid(linkIdx) || linkStatus == NULL)
    {
        return RC::BAD_PARAMETER;
    }

    if (m_bLink0Only && (linkIdx != 0))
    {
        *linkStatus = LINK_NOT_TRAINED;
        return RC::OK;
    }

    status = ReadC2CRegister(Gh100C2cDiagRegs::s_C2CLinkStatusRegs[linkIdx], &c2cStatus);
    if (status != RC::OK)
    {
        return status;
    }

    // The value of PASS bit is valid only when STATUS_IDLE is IDLE
    // while STATUS_IDLE is BUSY, this bit is not updated.
    // C2C link is trained if both IDLE and PASS fields are set.
    if ((DRF_VAL(_PC2C_C2CS0, _PL_TX_TR_STATUS, _IDLE, c2cStatus) ==
         LW_PC2C_C2CS0_PL_TX_TR_STATUS_IDLE_IDLE))
    {
        if ((DRF_VAL(_PC2C_C2CS0, _PL_TX_TR_STATUS, _PASS, c2cStatus) ==
            LW_PC2C_C2CS0_PL_TX_TR_STATUS_PASS_PASS))
        {
            *linkStatus = LINK_TRAINED;
        }
        else
        {
            *linkStatus = LINK_TRAINING_FAILED;
        }
    }
    else
    {
        *linkStatus = LINK_NOT_TRAINED;
    }

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC C2cDiagDriver::GetLineRate(LwU32 partitionIdx, LwF32 *lineRate)
{
    RC status;
    LwU32 refpllCoeff;
    LwU32 mDiv;
    LwU32 nDiv;
    LwU32 pDiv;

    if (!IsPartitionIdxValid(partitionIdx) || lineRate == NULL)
    {
        return RC::BAD_PARAMETER;
    }

    // Following table can be used to determine frequency from REFPLL register by reading Ndiv & Pdiv
    // -------------------------------------------------------------------------------------------------
    // Protocol    Baudrate    TX/RX Core  Baudperiod  FVCO    Mdiv    Ndiv    Pdiv    Fvco    CLKOUTP/N
    //             (GBps)      Interface   (pS)        (GHz)                           (GHz)   (GHz)
    //                         (# Sym)
    // -------------------------------------------------------------------------------------------------
    // 40Gbps      40.176      32          24.89       20.088  1       93      2       2.511   1.2555
    // 38Gbps      38.016      32          26.3        19.008  1       88      2       2.376   1.188
    // 36Gbps      36.288      32          27.56       18.144  1       84      2       2.268   1.134
    // 34Gbps      34.56       32          28.94       17.28   1       80      2       2.160   1.08
    // 32Gbps      31.968      32          31.28       15.984  1       74      2       1.998   0.999
    // 30Gbps      29.808      32          33.55       14.904  1       69      2       1.863   0.9315
    // 28Gbps      28.080      32          35.61       14.040  1       65      2       1.755   0.8775
    // -------------------------------------------------------------------------------------------------

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        if (m_bLink0Only)
        {
            status = ReadC2CRegister(Gh100C2cDiagRegs::s_C2CLinkFreqRegsLink0, &refpllCoeff);
        }
        else
        {
            status = ReadC2CRegister(Gh100C2cDiagRegs::s_C2CLinkFreqRegs[partitionIdx], &refpllCoeff);
        }
        
        if (status != RC::OK)
        {
            return status;
        }
        // As of now MDIV & PDIV values are same for all configs. So just read and verify.
        mDiv = DRF_VAL(_PC2C_C2CS0, _REFPLL_COEFF, _MDIV, refpllCoeff);
        nDiv = DRF_VAL(_PC2C_C2CS0, _REFPLL_COEFF, _NDIV, refpllCoeff);
        pDiv = DRF_VAL(_PC2C_C2CS0, _REFPLL_COEFF, _PDIV, refpllCoeff);
    }
    else
    {
        // Register reads 0 on fmodel/amodel, just return values that indicate 40Gbps
        mDiv = 1;
        nDiv = 93;
        pDiv = 2;
    }

    // Frequency (CLKOUTP) = 27*(n/(m*p)) MHz
    // Baudrate (Gbps) = Frequency * 32 / 1000
    *lineRate = (LwF32) ((27 * (nDiv / (mDiv * pDiv)) * 32) / 1000.0);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC C2cDiagDriver::ReadReg(LwU32 regIdx, LwU32 *regValue)
{
    if (regValue == NULL)
    {
        return RC::BAD_PARAMETER;
    }

    return ReadC2CRegister(regIdx, regValue);
}

//-----------------------------------------------------------------------------
RC C2cDiagDriver::InitInternal(LwU32 nrPartitions, LwU32 nrLinksPerPartition)
{
    if ((nrPartitions == 0 || nrPartitions > m_maxC2CPartitions)
        || (nrLinksPerPartition == 0 || nrLinksPerPartition > m_maxLinksPerPartition)
       )
    {
        return RC::BAD_PARAMETER;
    }

    m_nrPartitions = nrPartitions;
    m_nrLinksPerPartition = nrLinksPerPartition;

    return RC::OK;
}

//-----------------------------------------------------------------------------
bool C2cDiagDriver::IsLinkIdxValid(LwU32 linkIdx)
{
    LwU32 linkPartition     = linkIdx / m_maxLinksPerPartition;
    LwU32 linkIdxInPartiton = linkIdx % m_maxLinksPerPartition;

    // Check if link idx is not mapping to link partition greater than max C2C
    // partitions. Also make sure that link idx in partition is not greater than
    // allowed number of links in a C2C partition.
    if (linkPartition >= m_maxC2CPartitions ||
        linkIdxInPartiton >= m_nrLinksPerPartition)
    {
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool C2cDiagDriver::IsPartitionIdxValid(LwU32 partitionIdx)
{
    // Since, we can have only one partition active, just check if partition
    // index is not greater that max allowed partitions.
    return (partitionIdx >= m_maxC2CPartitions) ? false : true;
}
