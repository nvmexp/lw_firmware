/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2011,2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file mcpdev.h
//! \brief Header file with base class for MCP devices
//!
//! Contains class and constant definitions for MCP devices.
//!

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_MCPDEV_H
#define INCLUDED_MCPDEV_H

#ifndef INCLUDED_RC_H
    #include "core/include/rc.h"
#endif
#ifndef INCLUDED_TYPES_H
    #include "core/include/types.h"
#endif
#ifndef INCLUDED_TEE_H
    #include "core/include/tee.h"
#endif
#ifndef INCLUDED_CHIPSET_H
    #include "chipset.h"
#endif
#ifndef INCLUDED_TASKER_H
    #include "core/include/tasker.h"
#endif

#ifndef INCLUDED_STL_VECTOR
   #include <vector>
   #define INCLUDED_STL_VECTOR
#endif
#ifndef INCLUDED_CHIPSET_H
    #include "chipset.h"
#endif
#ifndef INCLUDED_MEMORYIF_H
    #include "core/include/memoryif.h"
#endif

#include <string>

class SmbPort;

//! \brief Base class for MCP devices.
//!
//! This class has very little built-in functionality--basically just
//! register reads and writes and PCI access. If you are creating a device
//! that does not have a associated domain, bus, device, and function numbers
//! (and the associated PCI config space), your device is probably different
//! enough from the standard MCP device that you should not be inheriting
//! from this class. On the other hand, if you are creating a bus mastering
//! "toy" device, you should use BusMasterDevice instead to take advantage
//! of that class' built-in functionality.
//!
//! I'd prefer for this class to be named McpDevice, but that name is already
//! take by an older class (that this one is meant to replace). Eventually
//! we may be able to rename this class to McpDevice.
//------------------------------------------------------------------------------
class McpDev
{
public:

    //! \brief args ised when call PollReg()
    struct PollRegArgs
    {
        McpDev* pDev;
        UINT32 regCode;
        UINT32 exp;
        UINT32 unexp;
    };

    //! \brief args ised when call PollMem()
    struct PollMemArgs
    {
        union {
            UINT32 memAddr;
            void   *pMemAddr;
        };
        UINT32 exp;
        UINT32 unexp;
    };

    //! \brief args ised when call PollMemSetAny()
    struct PollMemSetAnyArgs
    {
        union {
            UINT32 memAddr;
            void   *pMemAddr;
        };
        UINT32 exp;
    };

    //! \brief args ised when call PollMemClearAny()
    struct PollMemClearAnyArgs
    {
        union {
            UINT32 memAddr;
            void   *pMemAddr;
        };
        UINT32 unexp;
    };

    //! \brief args used when calling PollIo()
    struct PollIoArgs
    {
        UINT32 ioAddr;
        UINT08 exp;
        UINT08 unexp;
    };

    //! \brief Identifiers for the different stages of initialization.
    //!
    //! For the most part, this will only be used for debugging purposes,
    //! and for internal code-checking. Note that devices do not need to
    //! use all of these levels--many devices that inherit directly from
    //! McpDev will probably just use INIT_CREATED and INIT_DONE and skip
    //! the intermediary steps.
    //--------------------------------------------------------------------------
    typedef enum
    {
        INIT_CREATED = 0,
        INIT_PCI = 1,
        INIT_BAR = 2,
        INIT_REG = 3,
        INIT_DEV = 4,
        INIT_DONE = 5
    } InitStates;

    //! \brief Constructs a basic device.
    //!
    //! Merely creates the basic software object. Does not do any sort
    //! of initialization, and does not touch hardware at all. Calling
    //! McpDev::Initialize is necessary before any other functions will
    //! work.
    //! \param Domain Device's domain number
    //! \param Bus Device's bus number.
    //! \param Device Devices's device number.
    //! \param Func Device's function number.
    //! \param ChipVersion Device's chipset version number.
    //! \sa Initialize()
    //--------------------------------------------------------------------------
    McpDev(UINT32 Domain, UINT32 Bus, UINT32 Device, UINT32 Func, UINT32 ChipVersion=Chipset::INVALID);

    //! \brief Clean up internal software variables.
    //!
    //! Does not touch hardware. NOTE: Subclass destructors should call
    //! Uninitialize() to make sure that we do not leak resources!
    //! (Uninitialize references functions defined by the subclasses,
    //! so it cannot be called from this destructor.)
    //! \sa Uninitialize()
    //--------------------------------------------------------------------------
    virtual ~McpDev();

    //! \brief validate the device is desired device
    //! Validate is called in mgr class right after device creation.
    //! If device fail validation, device should be deleted, withought failure.
    //! *** Note: Make it virtual for now, will be changed to pure virtual
    //! to enforce implimentation of every sub class.
    //--------------------------------------------------------------------------
    RC virtual Validate();

    //! \brief Search the external connectivity's of current device/subdevices.
    //--------------------------------------------------------------------------
    virtual RC Search();

    //! \brief user force the page's range.
    //--------------------------------------------------------------------------
    RC ForcePageRange(UINT32 ForceAddrBits);

    //! \brief Get m_AddrBits.
    //--------------------------------------------------------------------------
    RC GetAddrBits(UINT32 *pDmaBits);

    //! \brief Prepare the device for use.
    //!
    //! This will need to be defined by each subclass. Possible steps in
    //! initialization might include setting values in the PCI config space,
    //! reading BAR values, setting up interrupts, reseting hardware,
    //! initializing hardware registers, and other per-device operations. Not
    //! all devices will need to do all of these steps. After a call to
    //! Initialize with Target = INIT_DONE, all other device APIs should work.
    //! \param Target Level to initialize to.
    //! \sa Uninitialize()
    //--------------------------------------------------------------------------
    virtual RC Initialize(InitStates Target) = 0;

    //! \brief Clean up the device.
    //!
    //! The reverse operation of Initialize(). Will need to be defined by
    //! each subclass.
    //! \param Target Level to uninitialize to.
    //! \sa Initialize()
    //--------------------------------------------------------------------------
    virtual RC Uninitialize(InitStates Target) = 0;

    //! \brief Read in the capabilities of the device
    //!
    //! The code implementation will read off the capabilities from the table
    //! based on the chipset version
    virtual RC ReadInCap(){return OK;}

    //! \brief Override the device capabilities of the current device with ChipVersion
    //!
    //! This code sets the m_ChipVersion of the current device.
    //! New capabilities are read in based on the new chip version
    RC OverrideCap(UINT32 ChipVersion);

    //! \brief Print the capabilities of the current device
    virtual RC PrintCap () {return OK;}

    //! \brief Print the capability lists for all devices
    virtual RC PrintCapTable () {return OK;}

    //! \brief Get the Capability table base address
    virtual RC GetCap (void ** CapTable) {return OK;}

    //! \brief Gets the vendor ID.
    //--------------------------------------------------------------------------
    RC GetVendorId(UINT16* pId = NULL);
    //! \brief Gets the device ID.
    //--------------------------------------------------------------------------
    RC GetDeviceId(UINT16* pId = NULL);
    //! \brief Gets the class code.
    //--------------------------------------------------------------------------
    RC GetClassCode(UINT32* pId = NULL);
    //! \brief Gets the domain, bus, dev, func
    //--------------------------------------------------------------------------
    RC GetDomainBusDevFunc(UINT32* pDomain, UINT32* pBus, UINT32* pDev, UINT32* pFunc);

    //! \brief Return true if it's LWpu Device.
    //--------------------------------------------------------------------------
    bool IsLwDevice();
    //--------------------------------------------------------------------------
    //! \brief Read a device register.
    //!
    //! Calling with isDebug = false should only allow access to "known"
    //! registers and is the normal mode of using this function. Calling
    //! with isDebug = true will allow unrestricted access to any register,
    //! which is useful when doing interactive mode debugging (but is not
    //! a long-term solution for accessing a register).
    //! \param Offset The register Offset address.
    //! \param pData On success, contains the Data read. The the register
    //!        is smaller than 32 bits, the upper bits will be 0.
    //! \param IsDebug Setting to true will allow unresticted register
    //!        access for interactive debugging purposes.
    //! \sa RegWr()
    //--------------------------------------------------------------------------
    RC RegRd(UINT32 Offset, UINT32* pData, bool IsDebug=false);

    //! \brief Write a device register
    //!
    //! This call will succeed even if the register is read-only. The client
    //! must call RegRd to verify the register's new value. See RegRd() for
    //! more on the isDebug parameter.
    //! \param Offset The register Offset address.
    //! \param Data The Data to write.
    //! \param IsDebug Setting to true will allow unrestricted register
    //!        access for interactive mode debugging.
    //! \sa RegRd()
    //--------------------------------------------------------------------------
    RC RegWr(UINT32 Offset, UINT32 Data, bool IsDebug=false);

    //! \brief Read a 32-bit PCI config space register.
    //--------------------------------------------------------------------------
    virtual RC PciRd32(UINT32 Offset, UINT32* pData);
    //! \brief Write a 32-bit PCI config space register.
    //--------------------------------------------------------------------------
    virtual RC PciWr32(UINT32 Offset, UINT32 Data);
    //! \brief Read a 16-bit PCI config space register.
    //--------------------------------------------------------------------------
    virtual RC PciRd16(UINT32 Offset, UINT16* pData);
    //! \brief Write a 16-bit PCI config space register.
    //--------------------------------------------------------------------------
    virtual RC PciWr16(UINT32 Offset, UINT16 Data);
    //! \brief Read an 8-bit PCI config space register.
    //--------------------------------------------------------------------------
    virtual RC PciRd08(UINT32 Offset, UINT08* pData);
    //! \brief Write an 8-bit PCI config space register.
    //--------------------------------------------------------------------------
    virtual RC PciWr08(UINT32 Offset, UINT08 Data);

    //! \brief Print information about the device.
    //!
    //! The boolean parameters control what information will be printed.
    //! "Basic information" is the information common to all devices: device
    //! id, interrupt status, etc. "Controller information" is specific to
    //! the type of device, and might consist of register values, bar values,
    //! and the like. "External information" pertains to external devices
    //! connected to the controller--PHYs, codecs, drives, etc.
    //! \param Pri The Pri to print at.
    //! \param printBasic Whether to print basic information.
    //! \param printController Whether to print controller information.
    //! \param printExternal Whether to print external information.
    //--------------------------------------------------------------------------
    RC PrintInfo(Tee::Priority Pri = Tee::PriNormal,
                 bool IsBasic = true,
                 bool IsController =true,
                 bool IsExternal=true);

    //! \brief Print the controller's registers.
    //! \param Pri Priority to print register content at.
    //--------------------------------------------------------------------------
    virtual RC PrintReg(Tee::Priority Pri = Tee::PriNormal) = 0;

    //! \brief Dump the PCI config space and interpreted info.
    //--------------------------------------------------------------------------
    RC DumpPci(Tee::Priority Pri = Tee::PriNormal);

    //! \brief Prints information about PCI config register reads.
    #define PRINTMASK_PCIRD         0x00000001
    //! \brief Prints information about PCI config register writes.
    #define PRINTMASK_PCIWR         0x00000002
    //! \brief Prints information about device register reads.
    #define PRINTMASK_REGRD         0x00000004
    //! \brief Prints information about device register writes.
    #define PRINTMASK_REGWR         0x00000008
    //! \brief Controls special printing capabilities for MCP devices.
    //!
    //! The print mask is used to selectively enabled certain types
    //! of printing at run time. Each bit in the mask can enable
    //! printing of certain kinds of information. Not all of the
    //! bits are lwrrently specified; those that are specified
    //! are defined as the PRINTMASK_* constants above. Bear in
    //! mind that not all of the constants will be valid for all
    //! devices. In general, this function should only be used
    //! for debugging, never in actual code!
    static RC SetPrintMask(UINT32 Mask);
    //! \brief Retrieves the settings for specialized printing.
    //!
    //! See comments on SetPrintMask for more information.
    static RC GetPrintMask(UINT32* pMask);

    //! \brief Associate a test object with this device.
    //!
    //! Generally, we'll want just one test at a time to have access to
    //! a given device. These APIs should allow tests to make sure that
    //! the device is free. In addition, they are expandable to allow
    //! multiple tests to access the device conlwrently, if that's
    //! ever a scenario we want to support. These APIs probably need to
    //! be thread-safe, unless we want to commit to having single-thread
    //! test setup. THEY ARE NOT LWRRENTLY THREAD-SAFE!!!
    //! \param pTest Pointer to the test object to associate with this device.
    //! \sa DeAssociateTest(), HasAssociatedTest()
    //--------------------------------------------------------------------------
    RC AssociateTest();

    //! \brief Break the association between a test and device object.
    //!
    //! \param pTest Pointer to the test object to disassociate from this device.
    //! \sa See AssociateTest() for an explanation of this API.
    //--------------------------------------------------------------------------
    RC DeAssociateTest();

    //! \brief Check whether this device has an associated test object.
    //!
    //! \sa See AssociateTest() for an explanation of this API.
    //--------------------------------------------------------------------------
    bool HasAssociatedTest();

    //! \brief Poll for status.
    //!
    //! This function return true if all the expected bits are set and, all un-
    //! expected bits are cleared
    //! pPollArgs = PollMemArgs (UINT32 memAddr, UINT32 exp, UINT32 unexp)
    //--------------------------------------------------------------------------
    static bool PollMem(void * pPollArgs);

    //! \brief Poll for status.
    //!
    //! This function return true if the argument value is not equal to the
    //! value in the memory location
    //! pArgs = PollMemSetAnyArgs(UINT32 memAddr, UINT32 exp)
    //--------------------------------------------------------------------------
    static bool PollMemNotEqual(void * pArgs);

    //! \brief Poll for status.
    //!
    //! This function return true if any of un-expected bits is cleared
    //! pPollArgs = PollMemClearAnyArgs (UINT32 memAddr, UINT32 unexp)
    //--------------------------------------------------------------------------
    static bool PollMemClearAny(void * pPollArgs);

    //! \brief Poll for status.
    //!
    //! This function return true if any of Expected bits is set
    //! pPollArgs = PollMemSetAnyArgs (UINT32 memAddr, UINT32 exp)
    //--------------------------------------------------------------------------
    static bool PollMemSetAny(void *pPollArgs);

    //! \brief Poll registers for status.
    //!
    //! This function return true if all the expected bits are set and, all un-
    //! expected bits are cleared
    //! pPollArgs = PollRegArgs::(McpDev*, UINT32 regcode, UINT32 exp, UINT32 unexp)
    //--------------------------------------------------------------------------
    static bool PollReg(void* pPollArgs);

    //! \brief Poll i/o space for status
    //! This function returns true if all the expected bits are set and
    //! all unexpected bits are cleared
    //! pPollArgs = PollIoArgs::(UINT32 ioAddr, UINT08 exp, UINT08 unexp)
    //--------------------------------------------------------------------
    static bool PollIo(void *pPollArgs);

    //! \brief Compare strings irrespective of case
    //!
    //! This function is case insensitive and compares two strings
    //! It returns true if the strings are not identical
    //! Otherwise it returns false
    //! \param s1 First string
    //! \param s2 Second string
    static bool NoCaseStrEq(const string S1, const string S2);

    //!brief Retrieves the sku of the current chipset.
    // wrapper around Chipset function to maintain sync
    static RC SetTimeoutScaler(UINT32 TimeoutScaler);
    static RC GetTimeoutScaler(UINT32* pTimeoutScaler);

    static RC WaitRegMem(UINT32 VirtMemBase, UINT32 RegCode, UINT32 Exp, UINT32 UnExp, UINT32 TimeoutMs);
    static RC WaitRegMem(void * pVirtMemBase, UINT32 RegCode, UINT32 Exp, UINT32 UnExp, UINT32 TimeoutMs);
    static RC WaitRegDev(McpDev* pDev, UINT32 RegCode, UINT32 Exp, UINT32 UnExp, UINT32 TimeoutMs);
    static RC WaitRegBits(McpDev* pDev, UINT32 RegCode, UINT32 MsBit, UINT32 LsBit, UINT32 Exp, UINT32 TimeoutMs);
    static RC WaitRegBitClearAnyMem(UINT32 VirtMemBase, UINT32 RegCode, UINT32 UnExp, UINT32 TimeoutMs);
    static RC WaitRegBitClearAnyMem(void *pVirtMemBase, UINT32 RegCode, UINT32 UnExp, UINT32 TimeoutMs);
    static RC WaitRegBitSetAnyMem(void *pVirtMemBase, UINT32 RegCode, UINT32 Exp, UINT32 TimeoutMs);

    //! \brief Gets the number of sub devices
    //!
    //! \param nNumSubDev On return, will be set to number of sub devices
    RC GetNumSubDev(UINT32 * pNumSubDev);

    //! \brief Gets the index of the current sub device.
    //!
    //! \param index On return, will be set to the index of the current sub device.
    RC GetLwrrentSubDev(UINT32* pSubDevIndex);

    //! \brief Gets a pointer to the current sub device.
    //!
    //! \param pSubDev On return, will point to the current sub device.
    RC GetLwrrentSubDev(SmbPort** ppSubDev);

    //! \brief Set the index of the current sub device.
    //!
    //! \param index The index to set as the current sub device.
    RC SetLwrrentSubDev(UINT32 SubDevIndex);

    //! \brief Gets a pointer to the next valid sub device.
    //!
    //! \param the pointer to the index of sub device list.
    //         (*pIndex) should be 0xff to start from the head of the sub-device list
    //         On return, (*pIndex) will contain the index of the valid sub-device returned
    //! \param ppSubDev On return, will be set to point to the valid sub device pointed to by (*pIndex).
    RC GetNextSubDev(UINT08 *pIndex, SmbPort** ppSubDev);

    //! \brief Gets a pointer to the sub device at the specified index.
    //!
    //! \param index The index of interest.
    //! \param ppSubDev On return, will be set to point to the sub device of interest.
    RC GetSubDev(UINT32 SubDevIndex, SmbPort** ppSubDev);

    RC ValidateSubDev(UINT32 SubDevIndex);

    //! \brief Retrieves an indexed capability from the Capability List
    //! \brief CapIndex the index to the capability array
    //! \brief CapValue the value of the capability which is to be returned
    RC GetCap(UINT32 CapIndex, UINT32* CapValue);

    //! \brief Retrieves a pointer to the capability array
    //! \parqam CapList UINT32 pointer to the CapList array
    //! \brief CapLength the length of the CapList array
    RC GetCapList (const UINT32 ** CapList, UINT32 * CapLength);

    //! \brief Retrieves the chipversion of the current device
    UINT32 GetChipVersion ();

    //! \brief establish external link
    virtual RC  LinkExtDev ();

    //!brief Retrieves the sku of the current chipset.
    // wrapper around Chipset function to maintain sync
    Chipset::ChipSku GetChipSku();

protected:

    //! \brief Do not call directly!
    //!
    //! This is the device-specific method to read a register. In general,
    //! it should never be called directly--RegRd(), which checks the
    //! device's init state, should be called instead. This function should
    //! keep track of the "known" registers and their sizes, and read them
    //! appropriately.
    //--------------------------------------------------------------------------
    virtual RC DeviceRegRd(UINT32 Offset, UINT32* pData) = 0;

    //! \brief Do not call directly!
    //!
    //! This is the device-specific method to write a register. In general,
    //! it should never be called directly--RegWr(), which checks the
    //! device's init state, should be called instead. This function should
    //! keep track of the "known" registers and their sizes, and write them
    //! appropriately.
    //--------------------------------------------------------------------------
    virtual RC DeviceRegWr(UINT32 Offset, UINT32 Data) = 0;

    //! \brief Print basic information.
    //!
    //! This might include device ID, interrupt tables, bus/dev/fun, etc.
    //! In general, prefer PrintInfo() to calling this directly.
    //! \sa PrintInfo()
    //--------------------------------------------------------------------------
    virtual RC PrintInfoBasic(Tee::Priority Pri) { return OK;}

    //! \brief Print controller information.
    //!
    //! This includes BAR addresses and register values. In general, prefer
    //! PrintInfo() to calling this method directly. Each subclass will
    //! need to implement this. For standarization purposes, implementation
    //! should probably indent each line by 4 spaces.
    //! \sa PrintInfo()
    //--------------------------------------------------------------------------
    virtual RC PrintInfoController(Tee::Priority Pri) = 0;

    //! \brief Print information about external devices.
    //!
    //! This includes PHYs, codecs, disk drives, etc. In general, prefer
    //! PrintInfo() to calling this method directly. Each subclass will
    //! need to implement this. For standarization purposes, implementation
    //! should probably indent each line by 4 spaces.
    //! \sa PrintInfo()
    //--------------------------------------------------------------------------
    virtual RC PrintInfoExternal(Tee::Priority Pri) = 0;

    //! \brief Reads and stores basic device info, such as device and vendor ID.
    //!
    //! This is a helper function that should be called (if appropriate) by
    //! the subclasses of McpDev as part of initialization.
    virtual RC ReadBasicInfo();

    //! \brief Enables bus mastering.
    //!
    //! This is a helper function that should be called (if appropriate) by
    //! the subclasses of McpDev as part of initialization.
    //! Non-busmastering devices should ignore this function.
    virtual RC EnableBusMastering();

    //! \brief Initialize all sub-devices
    RC InitializeAllSubDev();

    //! \brief Un-Initialize all sub-devices
    RC UninitializeAllSubDev();

    UINT32 m_Domain;
    UINT32 m_Bus;
    UINT32 m_Dev;
    UINT32 m_Func;
    UINT16 m_VendorId;
    UINT16 m_DeviceId;
    UINT32 m_ClassCode;
    UINT32 m_ChipVersion;
    UINT32 m_CapLength;

    UINT32 m_AddrBits;
    const UINT32 * m_pCap;

    //! \brief Tracks the software initialization state.
    //!
    //! This is useful for making sure that a device is in a state that
    //! supports certain operations. For example, reading device registers
    //! when the BAR has not yet been read is not supported. This should
    //! probably only be changed from within Initialize(), although it
    //! can be read from anywhere.
    //! \sa Initialize(), Uinitialize()
    //--------------------------------------------------------------------------
    InitStates m_InitState;

    //! \brief Controls specialized printing functions.
    static UINT32 m_PrintMask;

    //! \brief The scaler value to use for timeouts
    //!
    //! This defaults to 1, unless the user sets it
    static UINT32 m_TimeoutScaler;

    //! \brief Stores pointers to known sub devices.
    //!
    //! It stores all the Sub Device pointer. m_vpSubDev.size() would give the
    //! number of sub devices found.
    vector<SmbPort*> m_vpSubDev;

    //! \brief The index of the sub device lwrrently being pointed to.
    UINT32 m_LwrSubDevIndex;
private:

    //! \brief The number of tests lwrrently associate with this device.
    //!
    //! For the moment, this should never be more than 1. If we decide to
    //! allow multiple tests to run conlwrrently on the same piece of
    //! hardware some time in the future, it might be more. Perhaps we'd
    //! be better served by a pointer to the test instead?
    //--------------------------------------------------------------------------
    UINT32 m_NAssociatedTests;
};

//UINT32 McpDev::m_TimeoutScaler = 1;

//McpDev::m_TimeoutScaler = 1;

#endif // INCLUDED_MCPDEV_H

