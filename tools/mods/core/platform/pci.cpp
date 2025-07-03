/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012,2014,2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/pci.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/pcicfg_saver.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/xp.h"
#include "lwmisc.h"
#include "core/include/deprecat.h"
#include "hopper/gh100/dev_xtl_ep_pcfg_gpu.h"

/**
 * @file pci.cpp
 * @brief Pci object, properties and methods
 *
 *
 */

namespace
{

#define LWIDIA_HIDDEN_AER_OFFSET       0x420
#define INTEL_HIDDEN_AER_OFFSET        0x1C0

   //-----------------------------------------------------------------------------
   //! \brief Get the extended capability base for an extended capability block
   //!        that has been hidden (see bug 941194 for more information)
   //!
   //! \param BusNumber : PCI bus number of the device
   //! \param DevNumber : PCI device number of the device
   //! \param FunNumber : PCI function number of the device
   //! \param ExtCapId  : Extended cap ID to retrieve.
   //! \param BaseCapOffset : Offset for the base capabilities block.
   //! \param pExtCapOffset : Pointer to returned extended cap block offset.
   //!
   //! \return OK if the block was found, not OK otherwise
   //!
   RC GetHiddenExtendedCapBase
   (
      INT32              DomainNumber,
      INT32              BusNumber,
      INT32              DevNumber,
      INT32              FunNumber,
      Pci::ExtendedCapID ExtCapId,
      UINT08             BaseCapOffset,
      UINT16 *           pExtCapOffset
   )
   {
       MASSERT(pExtCapOffset);
       RC rc;

       *pExtCapOffset = 0;

       // Lwrrently only the AER hidden block is supported
       if (ExtCapId != Pci::AER_ECI)
       {
          Printf(Tee::PriDebug, "[%04x:%02x:%02x.%x] unsupported hidden cap id 0x%02x\n",
                 DomainNumber, BusNumber, DevNumber, FunNumber, ExtCapId);
          return RC::PCI_CAP_NOT_SUPPORTED;
       }

       UINT32 devType;
       CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber,
                                    FunNumber, 0, &devType));

       // For lwpu Gen2 or Gen3 devices the AER capability block is located
       // at a fixed address
       if ((devType & 0xFFFF) == Pci::Lwpu)
       {
          UINT32 devCap;
          CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber,
                                       FunNumber,
                                       BaseCapOffset + LW_PCI_CAP_PCIE_LINK_CAP,
                                       &devCap));
          const UINT32 maxSpeed = REF_VAL(LW_PCI_CAP_PCIE_LINK_CAP_LINK_SPEED,
                                          devCap);
          if ((maxSpeed == 2) || (maxSpeed == 3))
          {
             *pExtCapOffset = LWIDIA_HIDDEN_AER_OFFSET;
          }
       }
       // For IntelZ75 designs, the block is located at a fixed address
       else if (((devType & 0xFFFF) == Pci::Intel) &&
                (((devType >> 16) == Pci::IntelZ75) ||
                 ((devType >> 16) == Pci::IntelHSW)))
       {
          *pExtCapOffset = INTEL_HIDDEN_AER_OFFSET;
       }

       return (*pExtCapOffset) ? OK : RC::PCI_CAP_NOT_SUPPORTED;
   }
}

const char * Pci::AspmStateToString(ASPMState aspmState)
{
    switch (aspmState)
    {
        case Pci::ASPM_DISABLED:
             return "Disabled";
        case Pci::ASPM_L0S:
            return "L0s";
        case Pci::ASPM_L1:
            return "L1";
        case Pci::ASPM_L0S_L1:
            return "L0s/L1";
        case Pci::ASPM_UNKNOWN_STATE:
            return "Unknown";
        default:
            break;
    }
    return "Invalid";
}

const char * Pci::AspmL1SubstateToString(PmL1Substate aspmL1Subtate)
{
    switch (aspmL1Subtate)
    {
       case Pci::PM_SUB_DISABLED:
            return "Disabled";
        case Pci::PM_SUB_L11:
            return "L1.1";
        case Pci::PM_SUB_L12:
            return "L1.2";
        case Pci::PM_SUB_L11_L12:
            return "L1.1/L1.2";
        case Pci::PM_UNKNOWN_SUB_STATE:
            return "Unknown";
        default:
            break;
    }
    return "Invalid";
}

RC Pci::FindCapBase(INT32 DomainNumber, INT32 BusNumber, INT32 DevNumber, INT32 FunNumber,
                    UINT08 CapId, UINT08 * pCapPtr)
{
    RC rc;
    UINT08 CapPtr;
    UINT32 CapWord1;
    UINT32 CapBaseOffset = 0;
    bool failed = true;

    DEFER
    {
        if (failed)
        {
            Printf(Tee::PriLow, "[%04x:%02x:%02x.%x] failed to find cap id 0x%x\n",
                   DomainNumber, BusNumber, DevNumber, FunNumber, CapId);
        }
    };

    CHECK_RC(Platform::PciRead08(DomainNumber, BusNumber, DevNumber, FunNumber,
             PCI_CAP_PTR_OFFSET, &CapPtr));

    while (CapPtr!=0)
    {
        CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber, FunNumber,
                                     CapPtr, &CapWord1));

        // Check the CapId: for (5) for MSI, (0x11) for Msix
        if ((CapWord1&0xff) == CapId)
        {
            // Record the offset for MSI Cap
            CapBaseOffset = CapPtr;
            break;
        }

        // Update Cap Pointer for next Cap
        CapPtr = ((CapWord1&0xffff)>>8);
    }

    // If MSI Cap is not found
    if (!CapBaseOffset)
    {
        failed = false;
        Printf(Tee::PriLow, "[%04x:%02x:%02x.%x] cap id 0x%x not found\n",
               DomainNumber, BusNumber, DevNumber, FunNumber, CapId);
        return RC::PCI_CAP_NOT_SUPPORTED;
    }

    failed = false;
    Printf(Tee::PriLow, "[%04x:%02x:%02x.%x] cap id 0x%x found at offs 0x%02x\n",
           DomainNumber, BusNumber, DevNumber, FunNumber, CapId, CapBaseOffset);
    if (pCapPtr)
    {
        *pCapPtr = CapBaseOffset;
    }
    return RC::OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the extended capability info for an extended capability block
//!        (see bug 941194 for more information)
//!
//! \param BusNumber : PCI bus number of the device
//! \param DevNumber : PCI device number of the device
//! \param FunNumber : PCI function number of the device
//! \param ExtCapId  : Extended cap ID to retrieve.
//! \param BaseCapOffset : Offset for the base capabilities block.
//! \param pExtCapOffset : Pointer to returned extended cap block offset.
//! \param pExtCapSize   : Pointer to returned extended cap size.
//!
//! \return OK if the block was found, not OK otherwise
//!
RC Pci::GetExtendedCapInfo
(
   INT32         DomainNumber,
   INT32         BusNumber,
   INT32         DevNumber,
   INT32         FunNumber,
   ExtendedCapID ExtCapId,
   UINT08        BaseCapOffset,
   UINT16 *      pExtCapOffset,
   UINT16 *      pExtCapSize
)
{
    RC rc;
    UINT32 CapHeader;
    UINT32 CapOffset = PCI_FIRST_EXT_CAP_OFFSET;
    bool failed = true;

    DEFER
    {
        if (failed)
        {
            Printf(Tee::PriLow, "[%04x:%02x:%02x.%x] failed to find extended cap id 0x%x\n",
                   DomainNumber, BusNumber, DevNumber, FunNumber, ExtCapId);
        }
    };

    *pExtCapOffset = 0;
    do
    {
        CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber, FunNumber,
                                     CapOffset, &CapHeader));
        if (((CapHeader & 0xFFFF) == (UINT32)ExtCapId))
        {
            Printf(Tee::PriLow, "[%04x:%02x:%02x.%x] extended cap id 0x%x found at offs 0x%04x\n",
                   DomainNumber, BusNumber, DevNumber, FunNumber, ExtCapId, CapOffset);
            *pExtCapOffset = CapOffset;
        }
        CapOffset = CapHeader >> 20;
        if (CapOffset > 0 && CapOffset < PCI_FIRST_EXT_CAP_OFFSET)
        {
            Printf(Tee::PriWarn,
                   "Illegal next-pointer in PCIE extended capability header: 0x%03x\n",
                   CapOffset);
            break;
        }
    } while (((CapHeader & 0xFFFF) != (UINT32)ExtCapId) && (CapOffset != 0));

    failed = false;

    if (*pExtCapOffset == 0)
    {
        rc = GetHiddenExtendedCapBase(DomainNumber, BusNumber, DevNumber, FunNumber,
                                      ExtCapId, BaseCapOffset, pExtCapOffset);
        if (rc == RC::OK)
        {
            Printf(Tee::PriLow, "[%04x:%02x:%02x.%x] extended cap id 0x%x found at offs 0x%04x\n",
                   DomainNumber, BusNumber, DevNumber, FunNumber, ExtCapId, *pExtCapOffset);
        }
        else
        {
            if (ExtCapId == Pci::AER_ECI)
            {
                Printf(Tee::PriLow, "[%04x:%02x:%02x.%x] extended cap id 0x%x not found\n",
                       DomainNumber, BusNumber, DevNumber, FunNumber, ExtCapId);
            }
            return rc;
        }
    }

    UINT32 capHeader, capVersion;
    CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber, FunNumber,
                                 *pExtCapOffset, &capHeader));
    capVersion = REF_VAL(LW_PCI_EXT_CAP_HEADER_VERSION, capHeader);
    switch (ExtCapId)
    {
        case Pci::AER_ECI:
            if (capVersion == 1)
                *pExtCapSize = 0x3C;
            else
                *pExtCapSize = 0x48;
            break;
        case Pci::SRIOV:
            *pExtCapSize = 0x40;
            break;
        case Pci::SEC_PCIE_ECI:
            {

                UINT32 devCap;
                CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber, FunNumber,
                                             BaseCapOffset + LW_PCI_CAP_PCIE_LINK_CAP, &devCap));
                const UINT32 maxWidth = REF_VAL(LW_PCI_CAP_PCIE_LINK_CAP_LINK_WIDTH, devCap);
                *pExtCapSize = 0xC + maxWidth * 2;
                break;
            }
        case Pci::L1SS_ECI:
            *pExtCapSize = 0x10;
            break;
        case Pci::DPC_ECI:
            {
                UINT32 dpcCapCtrl;
                CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber, FunNumber,
                                             *pExtCapOffset + LW_PCI_DPC_CAP_CTRL, &dpcCapCtrl));
                if (FLD_TEST_DRF(_PCI, _DPC_CAP_CTRL, _RP_EXT, _ENABLED, dpcCapCtrl))
                {
                    const UINT32 pioLogDwords =
                            DRF_VAL(_PCI, _DPC_CAP_CTRL, _RP_PIO_LOG_DWORDS, dpcCapCtrl);
                    *pExtCapSize = static_cast<UINT16>(0x20 + pioLogDwords * sizeof(UINT32));
                }
                else
                {
                    *pExtCapSize = 0xC;
                }
            }
            break;
        case Pci::RESIZABLE_BAR_ECI:
            {
                UINT32 resizableBarCtrl;
                CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber, FunNumber, 
                                             *pExtCapOffset + LW_PCI_CAP_RESIZABLE_BAR_CTRL,
                                             &resizableBarCtrl));

                // The first resizable BAR control (and only the first) contains the number of
                // resizeable BARs
                const UINT32 maxResizableBars =
                    DRF_VAL(_PCI, _CAP_RESIZABLE_BAR_CTRL, _NUM_RESIZABLE_BARS, resizableBarCtrl);

                // Cap size includes the capability header plus one CAP/CTRL block for each
                // resizable BAR
                *pExtCapSize = 4 + maxResizableBars * LW_PCI_CAP_RESIZABLE_BAR_STRIDE;
            }
            break;
        case Pci::LTR_ECI:
            *pExtCapSize = 0x8;
            break;
        case VC_ECI:
        case MFVC_ECI:
        case VC9_ECI:
            {
                UINT32 cap1;
                CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber, FunNumber,
                                             *pExtCapOffset + LW_PCI_CAP_VC_CAP1, &cap1));

                const UINT32 extVcCount = DRF_VAL(_PCI, _CAP_VC_CAP1, _EVC_COUNT, cap1);
                const UINT32 lpExtVcCount = DRF_VAL(_PCI, _CAP_VC_CAP1, _LPEVC_COUNT, cap1);
                const UINT32 arbSize = 1 << DRF_VAL(_PCI, _CAP_VC_CAP1, _ARB_SIZE, cap1);

                *pExtCapSize = 0;

                // Check the Port ARB tables first, if any are present they will
                // determine the size
                if (extVcCount)
                {
                    UINT32 lastArbOffset = 0;
                    UINT32 lastResCap;

                    // The ARB tables are always in order highest VC last
                    for (INT32 lwrExtVc = static_cast<INT32>(extVcCount);
                          lwrExtVc >= 0 && (lastArbOffset == 0); lwrExtVc--)
                    {
                        CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber, FunNumber,
                            *pExtCapOffset + LW_PCI_CAP_VC_RES_CAP +
                                (lwrExtVc * LW_PCI_CAP_VC_PER_VC_STRIDE),
                            &lastResCap));
                        lastArbOffset =
                            DRF_VAL(_PCI, _CAP_VC_RES_CAP, _ARB_OFF_QWORD, lastResCap) * 16;
                    }
                    if (lastArbOffset)
                    {
                        UINT32 vcArbPhases = 0;

                        if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_CAP, _256_PHASE, 1, lastResCap))
                            vcArbPhases = 128;
                        if ((FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_CAP, _128_PHASE, 1, lastResCap)) ||
                            (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_CAP, _128_PHASE_TB, 1, lastResCap)))
                            vcArbPhases = 128;
                        else if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_CAP, _64_PHASE, 1, lastResCap))
                            vcArbPhases = 64;
                        else if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_RES_CAP, _32_PHASE, 1, lastResCap))
                            vcArbPhases = 32;
                        *pExtCapSize = lastArbOffset + (arbSize * vcArbPhases) / 8;
                    }
                }

                // Check the VC ARB table if there are no Port ARB tables as it will
                // determine the size
                if ((*pExtCapSize == 0) && lpExtVcCount)
                {
                    UINT32  vcCap2;
                    UINT32 vcArbOffset;
                    CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber, FunNumber,
                                                 *pExtCapOffset + LW_PCI_CAP_VC_CAP2, &vcCap2));
                    vcArbOffset = DRF_VAL(_PCI, _CAP_VC_CAP2, _ARB_OFF_QWORD, vcCap2) * 16;
                    if (vcArbOffset)
                    {
                        UINT32 vcArbPhases = 0;

                        if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_CAP2, _128_PHASE, 1, vcCap2))
                            vcArbPhases = 128;
                        else if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_CAP2, _64_PHASE, 1, vcCap2))
                            vcArbPhases = 64;
                        else if (FLD_TEST_DRF_NUM(_PCI, _CAP_VC_CAP2, _32_PHASE, 1, vcCap2))
                            vcArbPhases = 32;
                        // Fixed 4 bits per phase per lpevcc (plus VC0)
                        *pExtCapSize = vcArbOffset + ((lpExtVcCount + 1) * vcArbPhases * 4) / 8;
                    }
                }
                // If there are no ARB tables the number of extended VC ports determine the size
                // VC 0 is always present so subract 1 when callwlating the size
                if (*pExtCapSize == 0)
                {
                    *pExtCapSize = 0x1c + ((extVcCount - 1) * LW_PCI_CAP_VC_PER_VC_STRIDE);
                }
            }
            break;
        case PASID_ECI:
            *pExtCapSize = 0x8;
            break;
        case ACS_ECI:
            {
                UINT16 acsCap;
                CHECK_RC(Platform::PciRead16(DomainNumber, BusNumber, DevNumber, FunNumber,
                                             *pExtCapOffset + LW_PCI_CAP_ACS_CAP, &acsCap));
                UINT16 acsVectorBits = DRF_VAL(_PCI, _CAP_ACS_CAP, _EGRESS_VECTOR_BITS, acsCap);
                if (acsVectorBits == 0)
                    acsVectorBits = 256;

                *pExtCapSize = 0x8 + ((acsVectorBits + 31) / 32) * sizeof(UINT32);
            }
            break;
        case ATS_ECI:
            *pExtCapSize = 0x8;
            break;
        case PRI_ECI:
            *pExtCapSize = 0x10;
            break;
        case VSEC_ECI:
            {
                UINT32 vsecHdr = 0;
                // Read 1 DWORD passed the PCI Header to get to LWPU's GPU VSEC header.
                CHECK_RC(Platform::PciRead32(DomainNumber, BusNumber, DevNumber, FunNumber,
                                             *pExtCapOffset + 4, &vsecHdr));
                *pExtCapSize = DRF_VAL(_EP_PCFG, _GPU_VSEC_VENDOR_SPECIFIC_HEADER, _VSEC_LENGTH,
                                       vsecHdr);
                break;
            }
        default:
            Printf(Tee::PriError, "Unknown Size for PCI Extended Block ID 0x%02x\n", ExtCapId);
            return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC Pci::ReadExtendedCapBlock
(
    INT32           domain,
    INT32           bus,
    INT32           device,
    INT32           function,
    ExtendedCapID   extCapId,
    vector<UINT32>* pExtCapData
)
{
    MASSERT(pExtCapData);
    RC rc;

    UINT08 capPtr = 0;
    CHECK_RC(FindCapBase(domain,
                         bus,
                         device,
                         function,
                         PCI_CAP_ID_PCIE,
                         &capPtr));

    UINT16 capOffset = 0;
    UINT16 capSize   = 0;
    if (OK != GetExtendedCapInfo(domain,
                                 bus,
                                 device,
                                 function,
                                 extCapId,
                                 capPtr,
                                 &capOffset,
                                 &capSize))
    {
        // If unable to get cap info, just return no data
        return OK;
    }

    UINT32 data = 0;
    for (UINT16 lwrOffset = 0; lwrOffset < capSize; lwrOffset += 4)
    {
        CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                                     capOffset + lwrOffset, &data));
        pExtCapData->push_back(data);
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the BAR base address and size from the PCI Config Space Header
//!
//! \param busNumber       : PCI bus number of the device
//! \param deviceNumber    : PCI device number of the device
//! \param funnctionNumber : PCI function number of the device
//! \param barIndex        : Index of BAR to retrieve info for
//! \param pBaseAddress    : Pointer to returned base address of desired BAR
//! \param pBarSize        : Pointer to returned size of desired BAR
//!
//! \return OK if base address and size could be computed, not OK otherwise
//!
RC Pci::GetBarInfo
(
    UINT32    domainNumber,
    UINT32    busNumber,
    UINT32    deviceNumber,
    UINT32    functionNumber,
    UINT32    barIndex,
    PHYSADDR* pBaseAddress,
    UINT64*   pBarSize
)
{
    RC rc;
    CHECK_RC(GetBarInfo(domainNumber, busNumber, deviceNumber, functionNumber,
                        barIndex, pBaseAddress, pBarSize,
                        LW_EP_PCFG_GPU_BARREG0));
    return OK;
}

RC Pci::GetBarInfo
(
    UINT32    domainNumber,
    UINT32    busNumber,
    UINT32    deviceNumber,
    UINT32    functionNumber,
    UINT32    barIndex,
    PHYSADDR* pBaseAddress,
    UINT64*   pBarSize,
    UINT32    barConfigOffset
)
{
    RC rc;

    bool barIs64Bit = false;
    UINT32 baseAddressLow = 0;

    for (UINT32 ibar = 0; ibar <= barIndex; ibar++)
    {
        CHECK_RC(Platform::PciRead32(domainNumber, busNumber,
                                     deviceNumber, functionNumber,
                                     barConfigOffset,
                                     &baseAddressLow));

        barIs64Bit = FLD_TEST_DRF(_EP_PCFG_GPU, _BARREG0, _REG_ADDR_TYPE,
                                  _64BIT, baseAddressLow);

        if (ibar != barIndex)
        {
            barConfigOffset += (barIs64Bit ? 8 : 4);
        }
    }

    // Get the address
    *pBaseAddress = baseAddressLow & 0xFFFFFFF0;
    UINT32 baseAddressHigh = 0;
    if (barIs64Bit)
    {
        CHECK_RC(Platform::PciRead32(domainNumber, busNumber,
                                     deviceNumber, functionNumber,
                                     barConfigOffset+4,
                                     &baseAddressHigh));
        *pBaseAddress |= static_cast<PHYSADDR>(baseAddressHigh) << 32;
    }

    // Get the size
    UINT32 barSizeHigh = ~0U;
    if (barIs64Bit)
    {
        CHECK_RC(Platform::PciWrite32(domainNumber, busNumber,
                                      deviceNumber, functionNumber,
                                      barConfigOffset+4, ~0U));
        CHECK_RC(Platform::PciRead32(domainNumber, busNumber,
                                     deviceNumber, functionNumber,
                                     barConfigOffset+4, &barSizeHigh));
        CHECK_RC(Platform::PciWrite32(domainNumber, busNumber,
                                      deviceNumber, functionNumber,
                                      barConfigOffset+4, baseAddressHigh));
    }
    UINT32 barSizeLow = 0;
    CHECK_RC(Platform::PciWrite32(domainNumber, busNumber,
                                  deviceNumber, functionNumber,
                                  barConfigOffset, ~0U));
    CHECK_RC(Platform::PciRead32(domainNumber, busNumber,
                                 deviceNumber, functionNumber,
                                 barConfigOffset, &barSizeLow));
    CHECK_RC(Platform::PciWrite32(domainNumber, busNumber,
                                  deviceNumber, functionNumber,
                                  barConfigOffset, baseAddressLow));
    const UINT64 barSize = (static_cast<UINT64>(barSizeHigh) << 32)
                            | (barSizeLow & 0xFFFFFFF0);
    *pBarSize = 1 + ~barSize;

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Get the IRQ number from the PCI Config Space Header
//!
//! \param busNumber      : PCI bus number of the device
//! \param deviceNumber   : PCI device number of the device
//! \param functionNumber : PCI function number of the device
//! \param pIrq           : Pointer to returned IRQ number
//!
//! \return OK if IRQ number could be found, not OK otherwise
//!
RC Pci::GetIRQ
(
    UINT32  domainNumber,
    UINT32  busNumber,
    UINT32  deviceNumber,
    UINT32  functionNumber,
    UINT32* pIrq
)
{
    RC rc;
    UINT32 pciLw15;

    CHECK_RC(Platform::PciRead32(domainNumber, busNumber, deviceNumber,
                                 functionNumber, LW_EP_PCFG_GPU_MISC,
                                 &pciLw15));
    *pIrq = DRF_VAL(_EP_PCFG_GPU, _MISC, _INTR_LINE, pciLw15);

    return rc;
}

string Pci::DpcCapMaskToString(UINT32 capMask)
{
   if (capMask == DPC_CAP_NONE)
      return "None";

   string capStr = "Unknown";

   if (capMask & (DPC_CAP_BASIC | DPC_CAP_RP_EXT))
   {
      if (capMask & DPC_CAP_RP_EXT)
         capStr = "RP-Ext";
      else
         capStr = "Basic";

      string prepend = "(";
      if (capMask & DPC_CAP_POISONED_TLP)
      {
         capStr += prepend + "TLP";
         prepend = ",";
      }
      if (capMask & DPC_CAP_SW_TRIG)
      {
         capStr += prepend + "SW";
         prepend = ",";
      }

      if ((prepend == ",") || (prepend == ""))
         capStr += ")";
   }
   return capStr;
}

string Pci::DpcStateToString(DpcState state)
{
    switch (state)
    {
        case DPC_STATE_DISABLED:
           return "Disabled";
        case DPC_STATE_FATAL:
           return "Fatal";
        case DPC_STATE_NON_FATAL:
           return "NonFatal";
        default:
        case DPC_STATE_UNKNOWN:
           return "Unknown";
    }
}

//--------------------------------------------------------------------
//! \brief Colwert BDF to config address
//!
//! Note: For ARI & SRIOV support, function is allowed to overflow
//! into device, which can overflow into bus.
//!
UINT32 Pci::GetConfigAddress
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 offset
)
{
    const UINT32 offsetLo = offset & DRF_MASK(PCI_CONFIG_ADDRESS_OFFSET_LO);
    const UINT32 offsetHi = offset >> DRF_SIZE(PCI_CONFIG_ADDRESS_OFFSET_LO);
    const UINT32 shiftedBdf =
        (bus      << DRF_SHIFT(PCI_CONFIG_ADDRESS_BUS)) +
        (device   << DRF_SHIFT(PCI_CONFIG_ADDRESS_DEVICE)) +
        (function << DRF_SHIFT(PCI_CONFIG_ADDRESS_FUNCTION));

    MASSERT(domain <= DRF_MASK(PCI_CONFIG_ADDRESS_DOMAIN));
    MASSERT((shiftedBdf & ~DRF_SHIFTMASK(PCI_CONFIG_ADDRESS_BDF)) == 0);
    MASSERT(offsetHi <= DRF_MASK(PCI_CONFIG_ADDRESS_OFFSET_HI));

    return (REF_NUM(PCI_CONFIG_ADDRESS_DOMAIN,    domain)   |
            shiftedBdf                                      |
            REF_NUM(PCI_CONFIG_ADDRESS_OFFSET_HI, offsetHi) |
            REF_NUM(PCI_CONFIG_ADDRESS_OFFSET_LO, offsetLo));
}

//--------------------------------------------------------------------
//! \brief Colwert config address to BDF
//!
//! The output parameters are allowed to be nullptr, in which case
//! they're ignored.
//!
void Pci::DecodeConfigAddress
(
    UINT32  configAddress,
    UINT32* pDomain,
    UINT32* pBus,
    UINT32* pDevice,
    UINT32* pFunction,
    UINT32* pOffset
)
{
    if (pDomain)
        *pDomain = REF_VAL(PCI_CONFIG_ADDRESS_DOMAIN, configAddress);
    if (pBus)
        *pBus = REF_VAL(PCI_CONFIG_ADDRESS_BUS, configAddress);
    if (pDevice)
        *pDevice = REF_VAL(PCI_CONFIG_ADDRESS_DEVICE, configAddress);
    if (pFunction)
        *pFunction = REF_VAL(PCI_CONFIG_ADDRESS_FUNCTION, configAddress);
    if (pOffset)
    {
        *pOffset = ((REF_VAL(PCI_CONFIG_ADDRESS_OFFSET_HI, configAddress) <<
                     DRF_SIZE(PCI_CONFIG_ADDRESS_OFFSET_LO)) |
                    REF_VAL(PCI_CONFIG_ADDRESS_OFFSET_LO, configAddress));
    }
}

//--------------------------------------------------------------------
//! \brief Check if Function-Level Reset (FLR) is supported
//!
//!
bool Pci::GetFLResetSupport(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
{

    UINT08 capPtr;
    if (OK != FindCapBase(domain, bus, device, function,
                         PCI_CAP_ID_PCIE, &capPtr))
    {
        return false;
    }

    UINT32 deviceCap = 0;
    if (OK != Platform::PciRead32(domain, bus, device, function,
                         capPtr+LW_PCI_CAP_DEVICE_CAP, &deviceCap))
    {
        return false;
    }

    return FLD_TEST_DRF_NUM(_PCI, _CAP_DEVICE_CAP, _FLR, 1, deviceCap);
}

//--------------------------------------------------------------------
//! \brief Trigger a Function-Level Reset (FLR)
//!
//!
RC Pci::TriggerFLReset(UINT32 domain, UINT32 bus, UINT32 device, UINT32 function)
{
    RC rc;
    UINT08 capPtr;
    CHECK_RC(FindCapBase(domain, bus, device, function,
                         PCI_CAP_ID_PCIE, &capPtr));

    UINT16 deviceCtl = 0;
    CHECK_RC(Platform::PciRead16(domain, bus, device, function,
                                 capPtr+LW_PCI_CAP_DEVICE_CTL, &deviceCtl));
    deviceCtl = FLD_SET_DRF_NUM(_PCI, _CAP_DEVICE_CTL, _FLR, 1, deviceCtl);
    CHECK_RC(Platform::PciWrite16(domain, bus, device, function,
                                  capPtr+LW_PCI_CAP_DEVICE_CTL, deviceCtl));

    return rc;
}

RC Pci::ResizeBar
(
    UINT32 domainNumber,
    UINT32 busNumber,
    UINT32 deviceNumber,
    UINT32 functionNumber,
    UINT32 pciCapBase,
    UINT32 barNumber,
    UINT64 barSizeMb
)
{
    // Only 1 bit should be set in barSizeMb (PCI spec resizes bar in powers of 2)
    MASSERT((barSizeMb & (barSizeMb - 1)) == 0);
    RC rc;

    UINT16 extCapOffset;
    UINT16 extCapSize;
    CHECK_RC(GetExtendedCapInfo(domainNumber, busNumber, deviceNumber,
                                functionNumber, Pci::RESIZABLE_BAR_ECI, pciCapBase,
                                &extCapOffset, &extCapSize));

    UINT32 resizableBarCtrl;
    CHECK_RC(Platform::PciRead32(domainNumber, busNumber, deviceNumber,
                                 functionNumber, 
                                 extCapOffset + LW_PCI_CAP_RESIZABLE_BAR_CTRL,
                                 &resizableBarCtrl));

    // The index register is the register index of the start of the BAR.  So if
    // BAR0 were 64 bit the index of the MODS BAR1 would be 2.  Note that outside
    // of MODS in this case what MODS calls BAR1 would actually be termed BAR2/3
    // or referened in programs like lspci as "Region 2" or "Resource 2" in the
    // linux kernel
    UINT32 barIndex = 0;
    UINT32 barConfigOffset = PCI_BAR_OFFSET(0);
    for (UINT32 ibar = 0; ibar < barNumber; ibar++)
    {
        UINT32 baseAddressLow;
        CHECK_RC(Platform::PciRead32(domainNumber, busNumber,
                                     deviceNumber, functionNumber,
                                     barConfigOffset,
                                     &baseAddressLow));

        bool barIs64Bit = FLD_TEST_DRF(_EP_PCFG_GPU, _BARREG0, _REG_ADDR_TYPE,
                                       _64BIT, baseAddressLow);
        barConfigOffset += (barIs64Bit ? 8 : 4);
        barIndex += (barIs64Bit ? 2 : 1);
    }

    // The first resizable BAR control (and only the first) contains the number of resizeable
    // BARS
    const UINT32 maxResizableBars =
        DRF_VAL(_PCI, _CAP_RESIZABLE_BAR_CTRL, _NUM_RESIZABLE_BARS, resizableBarCtrl);
    UINT32 lwrBar = DRF_VAL(_PCI, _CAP_RESIZABLE_BAR_CTRL, _INDEX, resizableBarCtrl);
    UINT32 foundBarIdx = 0;
    while (lwrBar != barIndex)
    {
        foundBarIdx++;
        if (foundBarIdx == maxResizableBars)
            break;
        CHECK_RC(Platform::PciRead32(domainNumber, busNumber, deviceNumber,
                                     functionNumber, 
                                     extCapOffset + LW_PCI_CAP_RESIZABLE_BAR_CTRL +
                                          foundBarIdx * LW_PCI_CAP_RESIZABLE_BAR_STRIDE,
                                     &resizableBarCtrl));
        lwrBar = DRF_VAL(_PCI, _CAP_RESIZABLE_BAR_CTRL, _INDEX, resizableBarCtrl);
    }
    if (foundBarIdx == maxResizableBars)
    {
        Printf(Tee::PriError, "BAR %u is not resizable\n", barNumber);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    UINT32 temp;
    UINT64 resizableBarCap;
    CHECK_RC(Platform::PciRead32(domainNumber, busNumber, deviceNumber,
                                 functionNumber, 
                                 extCapOffset + LW_PCI_CAP_RESIZABLE_BAR_CAP +
                                     foundBarIdx * LW_PCI_CAP_RESIZABLE_BAR_STRIDE,
                                 &temp));
    resizableBarCap = DRF_VAL(_PCI, _CAP_RESIZABLE_BAR_CAP, _BAR_SIZE_BITS, temp);
    resizableBarCap |=
        static_cast<UINT64>(DRF_VAL(_PCI, _CAP_RESIZABLE_BAR_CTRL, _BAR_SIZE_CAP_UPPER, resizableBarCtrl)) <<
        DRF_SIZE(LW_PCI_CAP_RESIZABLE_BAR_CAP_BAR_SIZE_BITS);

    // The single bit set in barSizeMb directly maps to the cap bits
    if ((resizableBarCap & barSizeMb) == 0)
    {
        Printf(Tee::PriError, "BAR %u requested size (%llu MB) is not supported\n",
               barNumber, barSizeMb);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    // The bit index is what BAR_SIZE expects per spec (i.e. writing 8 gives you a 256MB BAR)
    const UINT32 capBit = Utility::BitScanForward(barSizeMb);
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        const UINT32 lwrBarSize =
            DRF_VAL(_PCI, _CAP_RESIZABLE_BAR_CTRL, _BAR_SIZE, resizableBarCtrl);
        if (capBit > lwrBarSize)
        {
            Printf(Tee::PriError, "Cannot expand BAR1 on Emulation/Silicon!\n");
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }
    }
    resizableBarCtrl =
        FLD_SET_DRF_NUM(_PCI, _CAP_RESIZABLE_BAR_CTRL, _BAR_SIZE, capBit, resizableBarCtrl);
    CHECK_RC(Platform::PciWrite32(domainNumber, busNumber, deviceNumber,
                                  functionNumber, 
                                  extCapOffset + LW_PCI_CAP_RESIZABLE_BAR_CTRL +
                                       foundBarIdx * LW_PCI_CAP_RESIZABLE_BAR_STRIDE,
                                  resizableBarCtrl));
    return rc;
}

UINT32 Pci::LinkSpeedToGen(PcieLinkSpeed linkSpeed)
{
    switch (linkSpeed)
    {
        case Speed32000MBPS:
            return 5;
        case Speed16000MBPS:
            return 4;
        case Speed8000MBPS:
            return 3;
        case Speed5000MBPS:
            return 2;
        case Speed2500MBPS:
            return 1;
        default:
        case SpeedUnknown:
            MASSERT(!"Unknown PCI link speed");
            break;
    }
    return 1;
}

RC Pci::ResetFunction
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function,
    UINT32 postResetDelayUs
)
{
    RC rc;
    UINT08 capPtr = 0;
    UINT08 pwrmCapPtr = 0;

    CHECK_RC(Pci::FindCapBase(domain, bus, device, function, PCI_CAP_ID_PCIE, &capPtr));
    UINT32 devCap;
    CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                                 capPtr + LW_PCI_CAP_DEVICE_CAP, &devCap));
    if (!FLD_TEST_DRF_NUM(_PCI, _CAP_DEVICE_CAP, _FLR, 1, devCap))
    {
        string domainStr;
        if (domain)
            domainStr = Utility::StrPrintf("%04x:", domain);
        Printf(Tee::PriError, "Function level reset not supported on %s%02x:%02x.%x",
               domainStr.c_str(), bus, device, function);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    // Find the Capability header for Power Management
    if (Pci::FindCapBase(domain, bus, device, function, PCI_CAP_ID_POWER_MGT, &pwrmCapPtr) == RC::OK)
    {
        // Extract Power Management Control and Status register from
        // Power Management Capability header
        UINT16 pwrMgtCtrlSts = 0;
        CHECK_RC(Platform::PciRead16(domain, bus, device, function,
                                     pwrmCapPtr + LW_PCI_CAP_POWER_MGT_CTRL_STS, &pwrMgtCtrlSts));

        pwrMgtCtrlSts =
            FLD_SET_DRF(_PCI, _CAP_POWER_MGT_CTRL_STS, _POWER_STATE, _PS_D0, pwrMgtCtrlSts);
        CHECK_RC(Platform::PciWrite16(domain, bus, device, function,
                                      pwrmCapPtr + LW_PCI_CAP_POWER_MGT_CTRL_STS,
                                      pwrMgtCtrlSts));
    }


    PciCfgSpaceSaver flrConfigSpace(domain, bus, device, function, capPtr);

    Printf(Tee::PriLow, "PCI FLR : saving config space\n");
    CHECK_RC(flrConfigSpace.Save());

    CHECK_RC(Platform::PciWrite16(domain, bus, device, function, LW_PCI_COMMAND,
                                  DRF_NUM(_PCI, _COMMAND, _INTX_DISABLE, 1)));


    Printf(Tee::PriLow, "PCI FLR : Writing FLR\n");
    UINT32 devCtrl;
    CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                                 capPtr + LW_PCI_CAP_DEVICE_CTL, &devCtrl));
    devCtrl = FLD_SET_DRF_NUM(_PCI, _CAP_DEVICE_CTL, _FLR, 1, devCtrl);
    CHECK_RC(Platform::PciWrite32(domain, bus, device, function,
                                  capPtr + LW_PCI_CAP_DEVICE_CTL, devCtrl));

    Printf(Tee::PriLow, "PCI FLR : Delaying %uus after FLR\n", postResetDelayUs);
    Platform::SleepUS(postResetDelayUs);

    RC pollRc;
    Printf(Tee::PriLow, "PCI FLR : Waiting for config space to return\n");
    CHECK_RC(Tasker::PollHw(Tasker::FixTimeout(60000), [&]()->bool
    {
        UINT16 cmd;
        pollRc = Platform::PciRead16(domain, bus, device, function, LW_PCI_COMMAND, &cmd);
        return (pollRc != RC::OK) || (cmd != _UINT16_MAX);
    }));
    CHECK_RC(pollRc);

    Printf(Tee::PriLow, "PCI FLR : Restoring config space\n");
    CHECK_RC(flrConfigSpace.Restore());
    return rc;
}

JS_CLASS( Pci );

//! Pci_Object
/**
 *
 */
static SObject Pci_Object
(
   "Pci",
   PciClass,
   0,
   0,
   "PCI interface."
);

// Properties
PROP_CONST(Pci, LOCAL_NODE, Pci::LOCAL_NODE);
PROP_CONST(Pci, HOST_NODE, Pci::HOST_NODE);
PROP_CONST(Pci, ASPM_DISABLED, Pci::ASPM_DISABLED);
PROP_CONST(Pci, ASPM_L0S, Pci::ASPM_L0S);
PROP_CONST(Pci, ASPM_L1, Pci::ASPM_L1);
PROP_CONST(Pci, ASPM_L0S_L1, Pci::ASPM_L0S_L1);
PROP_CONST(Pci, ASPM_UNKNOWN_STATE, Pci::ASPM_UNKNOWN_STATE);
PROP_CONST(Pci, PM_SUB_DISABLED, Pci::PM_SUB_DISABLED);
PROP_CONST(Pci, PM_SUB_L11, Pci::PM_SUB_L11);
PROP_CONST(Pci, PM_SUB_L12, Pci::PM_SUB_L12);
PROP_CONST(Pci, PM_SUB_L11_L12, Pci::PM_SUB_L11_L12);
PROP_CONST(Pci, PM_UNKNOWN_SUB_STATE, Pci::PM_UNKNOWN_SUB_STATE);
PROP_CONST(Pci, CLASS_CODE_VIDEO_CONTROLLER_VGA, Pci::CLASS_CODE_VIDEO_CONTROLLER_VGA);
PROP_CONST(Pci, CLASS_CODE_VIDEO_CONTROLLER_3D, Pci::CLASS_CODE_VIDEO_CONTROLLER_3D);
PROP_CONST(Pci, CLASS_CODE_USB30_XHCI_HOST, Pci::CLASS_CODE_USB30_XHCI_HOST);
PROP_CONST(Pci, CLASS_CODE_SERIAL_BUS_CONTROLLER, Pci::CLASS_CODE_SERIAL_BUS_CONTROLLER);
PROP_CONST(Pci, Speed32000MBPS, Pci::Speed32000MBPS);
PROP_CONST(Pci, Speed16000MBPS, Pci::Speed16000MBPS);
PROP_CONST(Pci, Speed8000MBPS, Pci::Speed8000MBPS);
PROP_CONST(Pci, Speed5000MBPS, Pci::Speed5000MBPS);
PROP_CONST(Pci, Speed2500MBPS, Pci::Speed2500MBPS);
PROP_CONST(Pci, SpeedUnknown, Pci::SpeedUnknown);
PROP_CONST(Pci, AERExtCapID, Pci::AER_ECI);
PROP_CONST(Pci, SecPcieExtCapID, Pci::SEC_PCIE_ECI);
PROP_CONST(Pci, DpcExtCapID, Pci::DPC_ECI);
PROP_CONST(Pci, DPC_CAP_NONE, Pci::DPC_CAP_NONE);
PROP_CONST(Pci, DPC_CAP_BASIC, Pci::DPC_CAP_BASIC);
PROP_CONST(Pci, DPC_CAP_RP_EXT, Pci::DPC_CAP_RP_EXT);
PROP_CONST(Pci, DPC_CAP_POISONED_TLP, Pci::DPC_CAP_POISONED_TLP);
PROP_CONST(Pci, DPC_CAP_SW_TRIG, Pci::DPC_CAP_SW_TRIG);
PROP_CONST(Pci, DPC_CAP_UNKNOWN, Pci::DPC_CAP_UNKNOWN);
PROP_CONST(Pci, DPC_STATE_DISABLED, Pci::DPC_STATE_DISABLED);
PROP_CONST(Pci, DPC_STATE_FATAL, Pci::DPC_STATE_FATAL);
PROP_CONST(Pci, DPC_STATE_NON_FATAL, Pci::DPC_STATE_NON_FATAL);
PROP_CONST(Pci, DPC_STATE_UNKNOWN, Pci::DPC_STATE_UNKNOWN);

static SProperty Pci_VendorIdLwidia
(
   Pci_Object,
   "VendorIdLwidia",
   0,
   Pci::Lwpu,
   0,
   0,
   JSPROP_READONLY,
   "LWPU vendor ID."
);

static SProperty Pci_VendorIdIntel
(
   Pci_Object,
   "VendorIdIntel",
   0,
   Pci::Intel,
   0,
   0,
   JSPROP_READONLY,
   "Intel vendor ID."
);

static SProperty Pci_VendorIdVia
(
   Pci_Object,
   "VendorIdVia",
   0,
   Pci::Via,
   0,
   0,
   JSPROP_READONLY,
   "VIA vendor ID."
);

static SProperty Pci_VendorIdAli
(
   Pci_Object,
   "VendorIdAli",
   0,
   Pci::Ali,
   0,
   0,
   JSPROP_READONLY,
   "ALi vendor ID."
);

// Methods and Tests

C_(Pci_Enumerate);
static SMethod Pci_Enumerate
(
   Pci_Object,
   "Enumerate",
   C_Pci_Enumerate,
   0,
   "Enumerates the devices on the PCI buses"
);

C_( Pci_FindDevice );
static STest Pci_FindDevice
(
   Pci_Object,
   "FindDevice",
   C_Pci_FindDevice,
   4,
   "Find a PCI device."
);

C_( Pci_FindClassCode );
static STest Pci_FindClassCode
(
   Pci_Object,
   "FindClassCode",
   C_Pci_FindClassCode,
   3,
   "Find a PCI class code device."
);

C_( Pci_RescanBus );
static STest Pci_RescanBus
(
   Pci_Object,
   "RescanBus",
   C_Pci_RescanBus,
   2,
   "Force a rescan of the PCI bus."
);

C_( Pci_FindMsiBase );
static SMethod Pci_FindMsiBase
(
   Pci_Object,
   "FindMsiBase",
   C_Pci_FindMsiBase,
   3,
   "Find a Msi base of given device."
);

C_( Pci_FindMsixBase );
static SMethod Pci_FindMsixBase
(
   Pci_Object,
   "FindMsixBase",
   C_Pci_FindMsixBase,
   3,
   "Find a Msi base of given device."
);

C_( Pci_Read08 );
static SMethod Pci_Read08
(
   Pci_Object,
   "Read08",
   C_Pci_Read08,
   4,
   "Read 8 bits from a PCI device."
);

C_( Pci_Read16 );
static SMethod Pci_Read16
(
   Pci_Object,
   "Read16",
   C_Pci_Read16,
   4,
   "Read 16 bits from a PCI device."
);

C_( Pci_Read32 );
static SMethod Pci_Read32
(
   Pci_Object,
   "Read32",
   C_Pci_Read32,
   4,
   "Read 32 bits from a PCI device."
);

C_( Pci_Write08 );
static STest Pci_Write08
(
   Pci_Object,
   "Write08",
   C_Pci_Write08,
   5,
   "Write 8 bits to a PCI device."
);

C_( Pci_Write16 );
static STest Pci_Write16
(
   Pci_Object,
   "Write16",
   C_Pci_Write16,
   5,
   "Write 16 bits to a PCI device."
);

C_( Pci_Write32 );
static STest Pci_Write32
(
   Pci_Object,
   "Write32",
   C_Pci_Write32,
   5,
   "Write 32 bits to a PCI device."
);

// Implementation

// STest
C_( Pci_FindDevice )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32      DeviceId    = 0;
    INT32      VendorId    = 0;
    INT32      Index       = 0;
    JSObject * pReturlwals = 0;
    if
    (
           (NumArguments != 4)
        || (OK != pJavaScript->FromJsval(pArguments[0], &DeviceId))
        || (OK != pJavaScript->FromJsval(pArguments[1], &VendorId))
        || (OK != pJavaScript->FromJsval(pArguments[2], &Index))
        || (OK != pJavaScript->FromJsval(pArguments[3], &pReturlwals))
    )
    {
        JS_ReportError(pContext,
            "Usage: Pci.FindDevice( device id, vendor id, index, "
            "PciDevice { this.domain; this.bus; this.device; this.func; } )");
        return JS_FALSE;
    }

    RC rc = OK;

    UINT32 DomainNumber   = 0;
    UINT32 BusNumber      = 0;
    UINT32 DeviceNumber   = 0;
    UINT32 FunctionNumber = 0;
    C_CHECK_RC(Platform::FindPciDevice(DeviceId, VendorId, Index,
                                       &DomainNumber, &BusNumber,
                                       &DeviceNumber, &FunctionNumber));

    if (JS_IsArrayObject(pContext, pReturlwals))
    {
        static Deprecation depr("Pci.FindDevice", "6/1/2015",
                           "Pci.FindDevice no longer takes an output array as the last argument.\n"
                           "Instead you should use a generic object. The returned object contains"
                           " data members: domain, bus, device, func.\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 0, BusNumber));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 1, DeviceNumber));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 2, FunctionNumber));
    }

    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "domain", DomainNumber));
    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "bus",    BusNumber));
    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "device", DeviceNumber));
    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "func",   FunctionNumber));

    RETURN_RC( OK );
}

// STest
C_( Pci_FindClassCode )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32      ClassCode   = 0;
    INT32      Index       = 0;
    JSObject * pReturlwals = 0;
    if
    (
           (NumArguments != 3)
        || (OK != pJavaScript->FromJsval(pArguments[0], &ClassCode))
        || (OK != pJavaScript->FromJsval(pArguments[1], &Index))
        || (OK != pJavaScript->FromJsval(pArguments[2], &pReturlwals))
    )
    {
        JS_ReportError(pContext,
            "Usage: Pci.FindClassCode( class code, index, "
            "PciDevice{ this.domain; this.bus; this.device; this.func; } )");
        return JS_FALSE;
    }

    RC rc = OK;

    UINT32 DomainNumber   = 0;
    UINT32 BusNumber      = 0;
    UINT32 DeviceNumber   = 0;
    UINT32 FunctionNumber = 0;
    C_CHECK_RC(Platform::FindPciClassCode(ClassCode, Index, &DomainNumber, &BusNumber,
                                          &DeviceNumber, &FunctionNumber));

    if (JS_IsArrayObject(pContext, pReturlwals))
    {
        static Deprecation depr("Pci.FindClassCode", "6/1/2015",
                        "Pci.FindClassCode no longer takes an output array as the last argument.\n"
                        "Instead you should use a generic object. The returned object contains"
                        " data members: domain, bus, device, func.\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 0, BusNumber));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 1, DeviceNumber));
        C_CHECK_RC(pJavaScript->SetElement(pReturlwals, 2, FunctionNumber));
    }

    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "domain", DomainNumber));
    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "bus",    BusNumber));
    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "device", DeviceNumber));
    C_CHECK_RC(pJavaScript->SetProperty(pReturlwals, "func",   FunctionNumber));

    RETURN_RC( OK );
}

// STest
C_( Pci_RescanBus )
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   // Check the arguments.
   INT32 domain = 0;
   INT32 bus = 0;

   if
   (
         (NumArguments != 2)
      || (OK != pJavaScript->FromJsval(pArguments[0], &domain))
      || (OK != pJavaScript->FromJsval(pArguments[1], &bus))
   )
   {
      JS_ReportError(pContext,
                     "Usage: Pci.RescanBus(domainNumber, busNumber)");
      return JS_FALSE;
   }

   RETURN_RC(Platform::RescanPciBus(domain, bus));
}

C_( Pci_FindMsiBase )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32  DomainNumber   = 0;
    INT32  BusNumber      = 0;
    INT32  DeviceNumber   = 0;
    INT32  FunctionNumber = 0;
    UINT08 MsiPtr = 0;

    if
    (
           ((NumArguments != 4)
        || (OK != pJavaScript->FromJsval(pArguments[0], &DomainNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &FunctionNumber)))
        && ((NumArguments != 3)
        || (OK != pJavaScript->FromJsval(pArguments[0], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &FunctionNumber)))
    )
    {
        JS_ReportError(pContext,
            "Usage: Pci.FindMsiBase(domainnumber, busnumber, devicenumber, functionnumber)");
        return JS_FALSE;
    }
    if (NumArguments == 3)
    {
        static Deprecation depr("Pci.FindMsiBase", "6/1/2015",
                              "Pci.FindMsiBase requires PCI domain number as the first argument.\n"
                              "Assuming PCI Domain = 0\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
    }

    RC rc = OK;

    if (OK != (rc = Pci::FindCapBase( DomainNumber, BusNumber,
                                      DeviceNumber, FunctionNumber,
                                      PCI_CAP_ID_MSI, &MsiPtr)))
    {
        *pReturlwalue = JSVAL_NULL;
        if (rc == RC::PCI_CAP_MSI_NOT_SUPPORTED)
            JS_ReportError(pContext, "Msi is not supported by this device.");
        else
            JS_ReportError(pContext, "Error oclwrred in Pci.FindMsiBase()");
        return JS_FALSE;
    }

    if (OK != pJavaScript->ToJsval(MsiPtr, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Pci.FindMsiBase()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

C_( Pci_FindMsixBase )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32  DomainNumber   = 0;
    INT32  BusNumber      = 0;
    INT32  DeviceNumber   = 0;
    INT32  FunctionNumber = 0;
    UINT08 MsixPtr = 0;

    if
    (
           ((NumArguments != 4)
        || (OK != pJavaScript->FromJsval(pArguments[0], &DomainNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &FunctionNumber)))
        && ((NumArguments != 3)
        || (OK != pJavaScript->FromJsval(pArguments[0], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &FunctionNumber)))
    )
    {
        JS_ReportError(pContext,
            "Usage: Pci.FindMsiBase(domainnumber, busnumber, devicenumber, functionnumber)");
        return JS_FALSE;
    }
    if (NumArguments == 3)
    {
        static Deprecation depr("Pci.FindMsixBase", "6/1/2015",
                             "Pci.FindMsixBase requires PCI domain number as the first argument.\n"
                             "Assuming PCI Domain = 0\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
    }

    RC rc = OK;

    if (OK != (rc = Pci::FindCapBase( DomainNumber, BusNumber,
                                      DeviceNumber, FunctionNumber,
                                      PCI_CAP_ID_MSIX, &MsixPtr)))
    {
        *pReturlwalue = JSVAL_NULL;
        if (rc == RC::PCI_CAP_MSI_NOT_SUPPORTED)
            JS_ReportError(pContext, "Msix is not supported by this device.");
        else
            JS_ReportError(pContext, "Error oclwrred in Pci.FindMsixBase()");
        return JS_FALSE;
    }

    if (OK != pJavaScript->ToJsval(MsixPtr, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Pci.FindMsixBase()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }
    return JS_TRUE;
}

// SMethod
C_(Pci_Enumerate)
{
   MASSERT(pContext     != 0);

   UINT16 val;
   UINT32 val32;
   INT32 domain;
   INT32 bus;
   INT32 dev;
   INT32 func;

    Printf(Tee::PriNormal,
      "DOMAIN BUS DEV FUNC VNDID DEVID STAT CMD  BAR0     BAR1     BAR2     "
      "IRQ  SSID SVID\n" );
    for (domain = 0; domain < Xp::PCI_MAX_DOMAINS; domain ++)
    {
        for (bus = 0; bus < Xp::PCI_MAX_BUSSES; bus ++)
        {
            for (dev = 0; dev < Xp::PCI_MAX_DEVICES; dev ++)
            {
                for (func = 0; func < Xp::PCI_MAX_FUNCTIONS; func ++)
                {
                    Platform::PciRead16(domain, bus, dev, func, 0, &val);
                    if (0xffff!=val)
                    {
                        Printf(Tee::PriNormal, "%04x   %02x  %02x  %02x   %04x  ",
                            domain, bus, dev, func, val );
                        Platform::PciRead16(domain, bus, dev, func, 2, &val);
                        Printf(Tee::PriNormal, "%04x  ", val );
                        Platform::PciRead16(domain, bus, dev, func, 6, &val);
                        Printf(Tee::PriNormal, "%04x ", val );
                        Platform::PciRead16(domain, bus, dev, func, 4, &val);
                        Printf(Tee::PriNormal, "%04x ", val );
                        Platform::PciRead32(domain, bus, dev, func, 16, &val32);
                        Printf(Tee::PriNormal, "%08x ", val32 );
                        Platform::PciRead32(domain, bus, dev, func, 20, &val32);
                        Printf(Tee::PriNormal, "%08x ", val32 );
                        Platform::PciRead32(domain, bus, dev, func, 24, &val32);
                        Printf(Tee::PriNormal, "%08x ", val32 );
                        Platform::PciRead16(domain, bus, dev, func, 60, &val);
                        Printf(Tee::PriNormal, "%04x ", val );
                        Platform::PciRead16(domain, bus, dev, func, 46, &val);
                        Printf(Tee::PriNormal, "%04x ", val );
                        Platform::PciRead16(domain, bus, dev, func, 44, &val);
                        Printf(Tee::PriNormal, "%04x\n", val );
                    }
                }
            }
        }
    }
    return JS_TRUE;
}

// SMethod
C_( Pci_Read08 )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    *pReturlwalue = JSVAL_NULL;

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32 DomainNumber   = 0;
    INT32 BusNumber      = 0;
    INT32 DeviceNumber   = 0;
    INT32 FunctionNumber = 0;
    INT32 Address        = 0;
    if
    (
           ((NumArguments != 5)
        || (OK != pJavaScript->FromJsval(pArguments[0], &DomainNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[4], &Address)))
        && ((NumArguments != 4)
        || (OK != pJavaScript->FromJsval(pArguments[0], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &Address)))
    )
    {
        JS_ReportError(pContext, "Usage: Pci.Read08( "
                                 "domain number, bus number, device number, "
                                 "function number, address )");
        return JS_FALSE;
    }
    if (NumArguments == 4)
    {
        static Deprecation depr("Pci.Read08", "6/1/2015",
                                "Pci.Read08 requires PCI domain number as the first argument.\n"
                                "Assuming PCI Domain = 0\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
    }

    UINT08 Data = 0;
    if (OK != Platform::PciRead08(DomainNumber, BusNumber, DeviceNumber,
                                  FunctionNumber, Address,
                                  &Data))
    {
        JS_ReportError(pContext, "Error oclwrred in Pci.Read08()");
        return JS_FALSE;
    }

    if (OK != pJavaScript->ToJsval(Data, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Pci.Read08()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_( Pci_Read16 )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    *pReturlwalue = JSVAL_NULL;

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32 DomainNumber    = 0;
    INT32 BusNumber       = 0;
    INT32 DeviceNumber    = 0;
    INT32 FunctionNumber  = 0;
    INT32 Address         = 0;
    if
    (
           ((NumArguments != 5)
        || (OK != pJavaScript->FromJsval(pArguments[0], &DomainNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[4], &Address)))
        && ((NumArguments != 4)
        || (OK != pJavaScript->FromJsval(pArguments[0], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &Address)))
    )
    {
        JS_ReportError(pContext, "Usage: Pci.Read16( "
                                 "domain number, bus number, device number, "
                                 "function number, address )");
        return JS_FALSE;
    }
    if (NumArguments == 4)
    {
        static Deprecation depr("Pci.Read16", "6/1/2015",
                                "Pci.Read16 requires PCI domain number as the first argument.\n"
                                "Assuming PCI Domain = 0\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
    }

    UINT16 Data = 0;
    if (OK != Platform::PciRead16(DomainNumber, BusNumber,
                                  DeviceNumber, FunctionNumber, Address,
                                  &Data))
    {
        JS_ReportError(pContext, "Error oclwrred in Pci.Read16()");
        return JS_FALSE;
    }

    if (OK != pJavaScript->ToJsval(Data, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Pci.Read16()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// SMethod
C_( Pci_Read32 )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    *pReturlwalue = JSVAL_NULL;

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32 DomainNumber   = 0;
    INT32 BusNumber      = 0;
    INT32 DeviceNumber   = 0;
    INT32 FunctionNumber = 0;
    INT32 Address        = 0;
    if
    (
           ((NumArguments != 5)
        || (OK != pJavaScript->FromJsval(pArguments[0], &DomainNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[4], &Address)))
        && ((NumArguments != 4)
        || (OK != pJavaScript->FromJsval(pArguments[0], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &Address)))
    )
    {
        JS_ReportError(pContext, "Usage: Pci.Read32( "
                                 "domain number, bus number, device number, "
                                 "function number, address )");
        return JS_FALSE;
    }
    if (NumArguments == 4)
    {
        static Deprecation depr("Pci.Read32", "6/1/2015",
                                "Pci.Read32 requires PCI domain number as the first argument.\n"
                                "Assuming PCI Domain = 0\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
    }

    UINT32 Data = 0;
    if (OK != Platform::PciRead32(DomainNumber, BusNumber,
                                  DeviceNumber, FunctionNumber, Address,
                                  &Data))
    {
        JS_ReportError(pContext, "Error oclwrred in Pci.Read32()");
        return JS_FALSE;
    }

    if (OK != pJavaScript->ToJsval(Data, pReturlwalue))
    {
        JS_ReportError(pContext, "Error oclwrred in Pci.Read32()");
        *pReturlwalue = JSVAL_NULL;
        return JS_FALSE;
    }

    return JS_TRUE;
}

// STest
C_( Pci_Write08 )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32  DomainNumber   = 0;
    INT32  BusNumber      = 0;
    INT32  DeviceNumber   = 0;
    INT32  FunctionNumber = 0;
    INT32  Address        = 0;
    UINT32 Data           = 0;
    if
    (
           ((NumArguments != 6)
        || (OK != pJavaScript->FromJsval(pArguments[0], &DomainNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[4], &Address))
        || (OK != pJavaScript->FromJsval(pArguments[5], &Data)))
        && ((NumArguments != 5)
        || (OK != pJavaScript->FromJsval(pArguments[0], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &Address))
        || (OK != pJavaScript->FromJsval(pArguments[4], &Data)))
    )
    {
        JS_ReportError(pContext, "Usage: Pci.Write08( domain number, "
                                 "bus number, device number, "
                                 "function number, address, data )");
        return JS_FALSE;
    }
    if (NumArguments == 5)
    {
        static Deprecation depr("Pci.Write08", "6/1/2015",
                                "Pci.Write08 requires PCI domain number as the first argument.\n"
                                "Assuming PCI Domain = 0\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
    }

    RETURN_RC( Platform::PciWrite08(DomainNumber, BusNumber,
                                    DeviceNumber, FunctionNumber, Address,
                                    static_cast<UINT08>(Data)) );
}

// STest
C_( Pci_Write16 )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32  DomainNumber   = 0;
    INT32  BusNumber      = 0;
    INT32  DeviceNumber   = 0;
    INT32  FunctionNumber = 0;
    INT32  Address        = 0;
    UINT32 Data           = 0;
    if
    (
           ((NumArguments != 6)
        || (OK != pJavaScript->FromJsval(pArguments[0], &DomainNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[4], &Address))
        || (OK != pJavaScript->FromJsval(pArguments[5], &Data)))
        && ((NumArguments != 5)
        || (OK != pJavaScript->FromJsval(pArguments[0], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &Address))
        || (OK != pJavaScript->FromJsval(pArguments[4], &Data)))
    )
    {
        JS_ReportError(pContext, "Usage: Pci.Write16( domain number, "
                                 "bus number, device number, "
                                 "function number, address, data )");
        return JS_FALSE;
    }
    if (NumArguments == 5)
    {
        static Deprecation depr("Pci.Write16", "6/1/2015",
                                "Pci.Write16 requires PCI domain number as the first argument.\n"
                                "Assuming PCI Domain = 0\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
    }

    RETURN_RC( Platform::PciWrite16(DomainNumber, BusNumber,
                                    DeviceNumber, FunctionNumber, Address,
                                    static_cast<UINT16>(Data)) );
}

// STest
C_( Pci_Write32 )
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    JavaScriptPtr pJavaScript;

    // Check the arguments.
    INT32  DomainNumber   = 0;
    INT32  BusNumber      = 0;
    INT32  DeviceNumber   = 0;
    INT32  FunctionNumber = 0;
    INT32  Address        = 0;
    UINT32 Data           = 0;
    if
    (
           ((NumArguments != 6)
        || (OK != pJavaScript->FromJsval(pArguments[0], &DomainNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[4], &Address))
        || (OK != pJavaScript->FromJsval(pArguments[5], &Data)))
        && ((NumArguments != 5)
        || (OK != pJavaScript->FromJsval(pArguments[0], &BusNumber))
        || (OK != pJavaScript->FromJsval(pArguments[1], &DeviceNumber))
        || (OK != pJavaScript->FromJsval(pArguments[2], &FunctionNumber))
        || (OK != pJavaScript->FromJsval(pArguments[3], &Address))
        || (OK != pJavaScript->FromJsval(pArguments[4], &Data)))
    )
    {
        JS_ReportError(pContext, "Usage: Pci.Write32( domain number, bus number, device number, "
            "function number, address, data )");
        return JS_FALSE;
    }
    if (NumArguments == 5)
    {
        static Deprecation depr("Pci.Write32", "6/1/2015",
                                "Pci.Write32 requires PCI domain number as the first argument.\n"
                                "Assuming PCI Domain = 0\n");
        if (!depr.IsAllowed(pContext))
            return JS_FALSE;
    }

    RETURN_RC( Platform::PciWrite32(DomainNumber, BusNumber, DeviceNumber, FunctionNumber, Address,
       Data) );
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(Pci,
                  AspmMaskToString,
                  1,
                  "Get the string associated with an ASPM mask")
{

    STEST_HEADER(1, 1, "Usage: Pci.AspmMaskToString(Pci.ASPM_L1)\n");
    STEST_ARG(0, UINT32, state);

    // If you dont explicitly cast to a string the ToJsval for boolean is chosen
    if (pJavaScript->ToJsval(Pci::AspmStateToString(static_cast<Pci::ASPMState>(state)),
                             pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_SMETHOD_LWSTOM(Pci,
                  AspmL1SSMaskToString,
                  1,
                  "Get the string associated with an ASPM substate mask")
{

    STEST_HEADER(1, 1, "Usage: Pci.AspmL1SSMaskToString(Pci.PM_SUB_L11)\n");
    STEST_ARG(0, UINT32, State);

    // If you dont explicitly cast to a string the ToJsval for boolean is chosen
    if (pJavaScript->ToJsval(Pci::AspmL1SubstateToString(static_cast<Pci::PmL1Substate>(State)),
                             pReturlwalue) == RC::OK)
    {
        return JS_TRUE;
    }
    return JS_FALSE;
}

//-----------------------------------------------------------------------------
JS_STEST_LWSTOM(Pci,
                ReadExtendedCapBlock,
                6,
                "Read the data out of the extended capability block")
{
    STEST_HEADER(6, 6, "Usage: Pci.ReadExtendedCapBlock(domain, bus, device, function, capId, retData);");
    STEST_ARG(0, UINT32, domain);
    STEST_ARG(1, UINT32, bus);
    STEST_ARG(2, UINT32, device);
    STEST_ARG(3, UINT32, function);
    STEST_ARG(4, UINT32, capId);
    STEST_ARG(5, JSObject*, retData);

    Pci::ExtendedCapID extCapId = static_cast<Pci::ExtendedCapID>(capId);

    RC rc;
    vector<UINT32> data;
    C_CHECK_RC(Pci::ReadExtendedCapBlock(domain, bus, device, function, extCapId, &data));

    for (UINT32 i = 0; i < data.size(); i++)
    {
        C_CHECK_RC(pJavaScript->SetElement(retData, i, data[i]));
    }

    RETURN_RC(rc);
}
