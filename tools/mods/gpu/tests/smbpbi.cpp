/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2016,2018-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <functional>
#include <map>
#include <utility>

#include "lwtypes.h"
#include "oob/smbpbi.h"

#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "gputestc.h"
#include "random.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "device/utility/genericipmi.h"

#include "device/smb/smbdev.h"
#include "device/smb/smbmgr.h"
#include "device/smb/smbport.h"

#include "gpu/include/boarddef.h"

#define DRFT(bits) DRF<1 ? bits, 0 ? bits>

//! SMBPBI stands for SMBus Post Box Interface. SMBPBI allows to read
//! temperatures, power, ECC statistics, etc using a couple of registers. CPU
//! writes an opcode to a command register and reads the result from a data
//! register. The correspondent namespace unites constants and classes that
//! abstract access to SMBPBI hardware.
namespace SMBPBI
{
    bool useIPMI = false;

    namespace OpCode
    {
        enum OpCodeEnum
        {
            NOP                   = 0x00, // No action
            GET_CAPABILTIES       = 0x01, // Get capabilities
            GET_SP_TEMP           = 0x02, // Get single precision temperature
            GET_EP_TEMP           = 0x03, // Get extended precision temperature
            GET_POWER             = 0x04, // Get power
            GET_GPU_INFO          = 0x05, // Get GPU information
            QUERY_ECC_STAT2       = 0x07, // Get ECC statistics, format V2
            QUERY_ECC_STAT3       = 0x0C, // Get ECC statistics, format V3
            READ_SCRATCH_MEM      = 0x0D, // Read from the scratch memory
            WRITE_SCRATCH_MEM     = 0x0E, // Write to the scratch memory
            COPY_SCRATCH_BLOCK    = 0x0F, // Copy block to the scratch memory
            ASYNC_REQUEST         = 0x10, // Submit/poll asynchronous request to the GPU driver
            STATE_REGS            = 0x11, // Access internal state registers
            EXT_POWER_CHECK       = 0x12, // Check external power
            PAGE_STAT             = 0x13  // Read dynamic page retirement statistics
        };
    }
    using OpCode::OpCodeEnum;

    namespace Status
    {
        enum StatusEnum
        {
            NULL_STATUS       = 0x00,
            ERR_REQUEST       = 0x01,
            ERR_OPCODE        = 0x02,
            ERR_ARG1          = 0x03,
            ERR_ARG2          = 0x04,
            ERR_DATA          = 0x05,
            ERR_MISC          = 0x06,
            ERR_I2C_ACCESS    = 0x07,
            ERR_NOT_SUPPORTED = 0x08,
            ERR_NOT_AVAILABLE = 0x09,
            ERR_BUSY          = 0x0A,
            ERR_AGAIN         = 0x0B,
            ACCEPTED          = 0x1C,
            INACTIVE          = 0x1D,
            READY             = 0x1E,
            SUCCESS           = 0x1F
        };
    }
    using Status::StatusEnum;

    namespace TempSrc
    {
        enum TempSrcEnum
        {
            GPU_0  = 0,
            GPU_1  = 1,
            BOARD  = 4,
            MEMORY = 5
        };
    }
    using TempSrc::TempSrcEnum;

    namespace GpuInfo
    {
        enum GpuInfoEnum
        {
            BOARD_PART_NUMBER  = 0x00,
            OEM_INFORMATION    = 0x01,
            SERIAL_NUMBER      = 0x02,
            MARKETING_NAME     = 0x03,
            GPU_PART_NUMBER    = 0x04,
            MEMORY_VENDOR      = 0x05,
            MEMORY_PART_NUMBER = 0x06,
            BUILD_DATE         = 0x07,
            FIRMWARE_VERSION   = 0x08,
            PCI_VID            = 0x09,
            PCI_DID            = 0x0A,
            PCI_SSVID          = 0x0B,
            PCI_SSDID          = 0x0C
        };
    }
    using GpuInfo::GpuInfoEnum;

    struct GpuInfoIdxToLengthInit
    {
        GpuInfoEnum m_gpuInfoIdx;
        size_t      m_length;

        operator pair<const GpuInfoEnum, size_t>() const
        {
            return make_pair(m_gpuInfoIdx, m_length);
        }
    } gpuInfoIdxToLengthInit[] =
    {
        {GpuInfo::BOARD_PART_NUMBER,  LW_MSGBOX_SYSID_DATA_SIZE_BOARD_PART_NUM_V1  },
        {GpuInfo::OEM_INFORMATION,    LW_MSGBOX_SYSID_DATA_SIZE_OEM_INFO_V1        },
        {GpuInfo::SERIAL_NUMBER,      LW_MSGBOX_SYSID_DATA_SIZE_SERIAL_NUM_V1      },
        {GpuInfo::MARKETING_NAME,     LW_MSGBOX_SYSID_DATA_SIZE_MARKETING_NAME_V1  },
        {GpuInfo::GPU_PART_NUMBER,    LW_MSGBOX_SYSID_DATA_SIZE_GPU_PART_NUM_V1    },
        {GpuInfo::MEMORY_VENDOR,      LW_MSGBOX_SYSID_DATA_SIZE_MEMORY_VENDOR_V1   },
        {GpuInfo::MEMORY_PART_NUMBER, LW_MSGBOX_SYSID_DATA_SIZE_MEMORY_PART_NUM_V1 },
        {GpuInfo::BUILD_DATE,         LW_MSGBOX_SYSID_DATA_SIZE_BUILD_DATE_V1      },
        {GpuInfo::FIRMWARE_VERSION,   LW_MSGBOX_SYSID_DATA_SIZE_FIRMWARE_VER_V1    },
        {GpuInfo::PCI_VID,            LW_MSGBOX_SYSID_DATA_SIZE_VENDOR_ID_V1       },
        {GpuInfo::PCI_DID,            LW_MSGBOX_SYSID_DATA_SIZE_DEV_ID_V1          },
        {GpuInfo::PCI_SSVID,          LW_MSGBOX_SYSID_DATA_SIZE_SUB_VENDOR_ID_V1   },
        {GpuInfo::PCI_SSDID,          LW_MSGBOX_SYSID_DATA_SIZE_SUB_ID_V1          },
    };

    typedef map<GpuInfoEnum, size_t> GpuInfoIdxToLength;
    const GpuInfoIdxToLength gpuInfoIdxToLength(
        &gpuInfoIdxToLengthInit[0],
        &gpuInfoIdxToLengthInit[0] + NUMELEMS(gpuInfoIdxToLengthInit));

    namespace EccStat
    {
        enum EccStatEnum
        {
            AGGREGATE_COUNTERS = 0,
            INDIVIDUAL_ADDRESS = 1
        };
    }
    using EccStat::EccStatEnum;

    namespace EccStatDevice
    {
        enum EccStatDeviceEnum
        {
            FRAME_BUFFER   = 0,
            GRAPHIC_DEVICE = 1
        };
    }
    using EccStatDevice::EccStatDeviceEnum;

    namespace EccStatFBSrc
    {
        enum EccStatFBSrcEnum
        {
            LEVEL2_CACHE = 0,
            FRAME_BUFFER = 1
        };
    }
    using EccStatFBSrc::EccStatFBSrcEnum;

    namespace EccStatGRSrc
    {
        enum EccStatGRSrcEnum
        {
            LEVEL1_CACHE = 0,
            SM_REG_FILE  = 1,
            TEX_UNITS    = 2
        };
    }
    using EccStatGRSrc::EccStatGRSrcEnum;

    namespace EccStatType
    {
        enum EccStatTypeEnum
        {
            SINGLE_BIT = 0,
            DOUBLE_BIT = 1
        };
    }
    using EccStatType::EccStatTypeEnum;

    namespace EccAddrStat
    {
        enum EccAddrStatEnum
        {
            COUNTERS = 0,
            ADDRESS  = 1
        };
    }
    using EccAddrStat::EccAddrStatEnum;

    namespace StateRegRW
    {
        enum StateRegRWEnum
        {
            WRITE = 0,
            READ  = 1
        };
    }
    using StateRegRW::StateRegRWEnum;

    template <unsigned int M, unsigned int N, class IntegralType = UINT32>
    class DRFSetter;

    template <unsigned int M, unsigned int N, class IntegralType = UINT32>
    struct DRF
    {
        static const unsigned int start  = N;
        static const unsigned int finish = M;

        static
        DRFSetter<M, N, IntegralType> Set(IntegralType &v);

        static
        IntegralType Get(IntegralType v)
        {
            return (v >> Shift()) & Mask();
        }

        static
        unsigned int Base()
        {
            return start;
        }

        static
        unsigned int Extent()
        {
            return finish;
        }

        static
        unsigned int Shift()
        {
            return Base();
        }

        static
        UINT32 Mask()
        {
            return static_cast<IntegralType>(-1) >> (sizeof(IntegralType) * 8 - Size());
        }

        static
        UINT32 ShiftMask()
        {
            return Mask() << Shift();
        }

        static
        size_t Size()
        {
            return Extent() - Base() + 1;
        }
    };

    template <unsigned int M, unsigned int N, class IntegralType>
    class DRFSetter
    {
    public:
        DRFSetter(IntegralType &lhs)
          : m_lhs(lhs)
        {}

        IntegralType& operator=(IntegralType rhs)
        {
            typedef DRF<M, N> ThisDRF;

            m_lhs = (m_lhs & ~ThisDRF::ShiftMask()) | (rhs << ThisDRF::Shift());

            return m_lhs;
        }

    private:
        IntegralType &m_lhs;
    };

    template <unsigned int M, unsigned int N, class IntegralType>
    DRFSetter<M, N, IntegralType> DRF<M, N, IntegralType>::Set(IntegralType &v)
    {
        return DRFSetter<M, N, IntegralType>(v);
    }

    class Arg1Setter;
    class Arg2Setter;

    class CommandDefs
    {
    protected:
        typedef DRFT(31:31)                ExelwteBit;
        typedef DRFT(LW_MSGBOX_CMD_ARG2)   Arg2Bits;
        typedef DRFT(LW_MSGBOX_CMD_ARG1)   Arg1Bits;
        typedef DRFT(LW_MSGBOX_CMD_OPCODE) OpCodeBits;

        friend class Arg1Setter;
        friend class Arg2Setter;
    };

    template <OpCodeEnum opCode>
    class Command : public CommandDefs
    {
    public:
        Command()
          : m_arg1(0)
          , m_arg2(0)
        {}

        Command(UINT08 arg1)
          : m_arg1(arg1)
          , m_arg2(0)
        {}

        Command(UINT08 arg1, UINT08 arg2)
          : m_arg1(arg1)
          , m_arg2(arg2)
        {}

        operator UINT32 () const
        {
            UINT32 cmd = 0;

            OpCodeBits::Set(cmd) = GetOpCode();
            Arg1Bits::Set(cmd) = GetArg1();
            Arg2Bits::Set(cmd) = GetArg2();
            ExelwteBit::Set(cmd) = 1;

            return cmd;
        }

        static
        OpCodeEnum GetOpCode()
        {
            return opCode;
        }

        UINT08 GetArg1() const
        {
            return m_arg1;
        }

        UINT08 GetArg2() const
        {
            return m_arg2;
        }

    private:
        UINT08 m_arg1;
        UINT08 m_arg2;
    };

    //! Constructs command 32-bit word by placing the opcode and the arguments
    //! to the proper bits.
    class CommandBuilder
    {
        typedef DRF<7, 6, UINT08> EccStatBits;
        typedef DRF<5, 4, UINT08> EccDeviceBits;
        typedef DRF<3, 2, UINT08> EccSourceBits;
        typedef DRF<1, 0, UINT08> EccTypeBits;
        typedef DRF<7, 4, UINT08> EccPartitionBits;
        typedef DRF<3, 0, UINT08> EccSliceBits;
        typedef DRF<3, 0, UINT08> EccSubpartBits;
        typedef DRF<7, 6, UINT08> EccTexBits;
        typedef DRF<7, 4, UINT08> EccGpcBits2;
        typedef DRF<3, 0, UINT08> EccTpcBits2;
        typedef DRF<5, 3, UINT08> EccGpcBits3;
        typedef DRF<2, 0, UINT08> EccTpcBits3;
        typedef DRF<4, 4, UINT08> EccAddrOrCntBit;
        typedef DRF<3, 0, UINT08> EccAddrIdxBits;

    public:
        static
        Arg1Setter Arg1(UINT32 &cmd);

        static
        Arg2Setter Arg2(UINT32 &cmd);

        static
        UINT32 Nop()
        {
            return Command<OpCode::NOP>();
        }

        static
        UINT32 GetCapabilities(UINT08 capSet)
        {
            return Command<OpCode::GET_CAPABILTIES>(capSet);
        }

        static
        UINT32 GetSinglePrecisionTemperature(TempSrcEnum tempSrc)
        {
            return Command<OpCode::GET_SP_TEMP>(tempSrc);
        }

        static
        UINT32 GetExtendedPrecisionTemperature(TempSrcEnum tempSrc)
        {
            return Command<OpCode::GET_EP_TEMP>(tempSrc);
        }

        static
        UINT32 GetPower()
        {
            return Command<OpCode::GET_POWER>();
        }

        static
        UINT32 GetGPUInformation(GpuInfoEnum gpuInfoIdx, UINT08 dataOffset)
        {
            return Command<OpCode::GET_GPU_INFO>(static_cast<UINT08>(gpuInfoIdx), dataOffset);
        }

        static
        UINT32 GetL1CacheEccStat2(EccStatTypeEnum statType, UINT08 gpcNum, UINT08 tpcNum)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::AGGREGATE_COUNTERS;
            EccDeviceBits::Set(arg1) = EccStatDevice::GRAPHIC_DEVICE;
            EccSourceBits::Set(arg1) = EccStatGRSrc::LEVEL1_CACHE;
            EccTypeBits::Set(arg1) = statType;

            UINT08 arg2 = 0;
            EccGpcBits2::Set(arg2) = gpcNum;
            EccTpcBits2::Set(arg2) = tpcNum;

            return Command<OpCode::QUERY_ECC_STAT2>(arg1, arg2);
        }

        static
        UINT32 GetSMRegFileEccStat2(EccStatTypeEnum statType, UINT08 gpcNum, UINT08 tpcNum)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::AGGREGATE_COUNTERS;
            EccDeviceBits::Set(arg1) = EccStatDevice::GRAPHIC_DEVICE;
            EccSourceBits::Set(arg1) = EccStatGRSrc::SM_REG_FILE;
            EccTypeBits::Set(arg1) = statType;

            UINT08 arg2 = 0;
            EccGpcBits2::Set(arg2) = gpcNum;
            EccTpcBits2::Set(arg2) = tpcNum;

            return Command<OpCode::QUERY_ECC_STAT2>(arg1, arg2);
        }

        static
        UINT32 GetL2CacheEccStat2(EccStatTypeEnum statType, UINT08 partNum, UINT08 sliceNum)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::AGGREGATE_COUNTERS;
            EccDeviceBits::Set(arg1) = EccStatDevice::FRAME_BUFFER;
            EccSourceBits::Set(arg1) = EccStatFBSrc::LEVEL2_CACHE;
            EccTypeBits::Set(arg1) = statType;

            UINT08 arg2 = 0;
            EccPartitionBits::Set(arg2) = partNum;
            EccSliceBits::Set(arg2) = sliceNum;

            return Command<OpCode::QUERY_ECC_STAT2>(arg1, arg2);
        }

        static
        UINT32 GetFrameBufEccStat2(EccStatTypeEnum statType, UINT08 partNum, UINT08 subPartNum)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::AGGREGATE_COUNTERS;
            EccDeviceBits::Set(arg1) = EccStatDevice::FRAME_BUFFER;
            EccSourceBits::Set(arg1) = EccStatFBSrc::FRAME_BUFFER;
            EccTypeBits::Set(arg1) = statType;

            UINT08 arg2 = 0;
            EccPartitionBits::Set(arg2) = partNum;
            EccSubpartBits::Set(arg2) = subPartNum;

            return Command<OpCode::QUERY_ECC_STAT2>(arg1, arg2);
        }

        static
        UINT32 GetL1CacheEccStat3(EccStatTypeEnum statType, UINT08 gpcNum, UINT08 tpcNum)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::AGGREGATE_COUNTERS;
            EccDeviceBits::Set(arg1) = EccStatDevice::GRAPHIC_DEVICE;
            EccSourceBits::Set(arg1) = EccStatGRSrc::LEVEL1_CACHE;
            EccTypeBits::Set(arg1) = statType;

            UINT08 arg2 = 0;
            EccGpcBits3::Set(arg2) = gpcNum;
            EccTpcBits3::Set(arg2) = tpcNum;

            return Command<OpCode::QUERY_ECC_STAT3>(arg1, arg2);
        }

        static
        UINT32 GetSMRegFileEccStat3(EccStatTypeEnum statType, UINT08 gpcNum, UINT08 tpcNum)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::AGGREGATE_COUNTERS;
            EccDeviceBits::Set(arg1) = EccStatDevice::GRAPHIC_DEVICE;
            EccSourceBits::Set(arg1) = EccStatGRSrc::SM_REG_FILE;
            EccTypeBits::Set(arg1) = statType;

            UINT08 arg2 = 0;
            EccGpcBits3::Set(arg2) = gpcNum;
            EccTpcBits3::Set(arg2) = tpcNum;

            return Command<OpCode::QUERY_ECC_STAT3>(arg1, arg2);
        }

        static
        UINT32 GetTexUnitEccStat3(EccStatTypeEnum statType, UINT08 gpcNum, UINT08 tpcNum)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::AGGREGATE_COUNTERS;
            EccDeviceBits::Set(arg1) = EccStatDevice::GRAPHIC_DEVICE;
            EccSourceBits::Set(arg1) = EccStatGRSrc::TEX_UNITS;
            EccTypeBits::Set(arg1) = statType;

            UINT08 arg2 = 0;
            EccGpcBits3::Set(arg2) = gpcNum;
            EccTpcBits3::Set(arg2) = tpcNum;

            return Command<OpCode::QUERY_ECC_STAT3>(arg1, arg2);
        }

        static
        UINT32 GetL2CacheEccStat3(EccStatTypeEnum statType, UINT08 partNum, UINT08 sliceNum)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::AGGREGATE_COUNTERS;
            EccDeviceBits::Set(arg1) = EccStatDevice::FRAME_BUFFER;
            EccSourceBits::Set(arg1) = EccStatFBSrc::LEVEL2_CACHE;
            EccTypeBits::Set(arg1) = statType;

            UINT08 arg2 = 0;
            EccPartitionBits::Set(arg2) = partNum;
            EccSliceBits::Set(arg2) = sliceNum;

            return Command<OpCode::QUERY_ECC_STAT3>(arg1, arg2);
        }

        static
        UINT32 GetFrameBufEccStat3(EccStatTypeEnum statType, UINT08 partNum, UINT08 subPartNum)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::AGGREGATE_COUNTERS;
            EccDeviceBits::Set(arg1) = EccStatDevice::FRAME_BUFFER;
            EccSourceBits::Set(arg1) = EccStatFBSrc::FRAME_BUFFER;
            EccTypeBits::Set(arg1) = statType;

            UINT08 arg2 = 0;
            EccPartitionBits::Set(arg2) = partNum;
            EccSubpartBits::Set(arg2) = subPartNum;

            return Command<OpCode::QUERY_ECC_STAT3>(arg1, arg2);
        }

        static
        UINT32 GetEccAddressStatAddress(size_t partNum, UINT08 subPartNum, UINT08 idx)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::INDIVIDUAL_ADDRESS;
            EccAddrOrCntBit::Set(arg1) = EccAddrStat::ADDRESS;
            EccAddrIdxBits::Set(arg1) = idx;

            UINT08 arg2 = 0;
            EccPartitionBits::Set(arg2) = partNum;
            EccSubpartBits::Set(arg2) = subPartNum;

            return Command<OpCode::QUERY_ECC_STAT3>(arg1, arg2);
        }

        static
        UINT32 GetEccAddressStatCounters(size_t partNum, UINT08 subPartNum, UINT08 idx)
        {
            UINT08 arg1 = 0;
            EccStatBits::Set(arg1) = EccStat::INDIVIDUAL_ADDRESS;
            EccAddrOrCntBit::Set(arg1) = EccAddrStat::COUNTERS;
            EccAddrIdxBits::Set(arg1) = idx;

            UINT08 arg2 = 0;
            EccPartitionBits::Set(arg2) = partNum;
            EccSubpartBits::Set(arg2) = subPartNum;

            return Command<OpCode::QUERY_ECC_STAT3>(arg1, arg2);
        }

        static
        UINT32 ReadScratch(UINT08 whereOffset)
        {
            return Command<OpCode::READ_SCRATCH_MEM>(whereOffset);
        }

        static
        UINT32 WriteScratch(UINT08 whereOffset, UINT08 numWords)
        {
            MASSERT(0 < numWords);
            return Command<OpCode::WRITE_SCRATCH_MEM>(whereOffset, numWords - 1);
        }

        static
        UINT32 CopyScratchBlock(UINT08 whereOffset, UINT08 numWords)
        {
            MASSERT(0 < numWords);
            return Command<OpCode::COPY_SCRATCH_BLOCK>(whereOffset, numWords - 1);
        }

        static
        UINT32 ReadStateReg(UINT08 regIdx)
        {
            return Command<OpCode::STATE_REGS>(StateRegRW::READ, regIdx);
        }

        static
        UINT32 WriteStateReg(UINT08 regIdx)
        {
            return Command<OpCode::STATE_REGS>(StateRegRW::WRITE, regIdx);
        }

        static
        UINT32 CheckPower()
        {
            return Command<OpCode::EXT_POWER_CHECK>();
        }

        static
        UINT32 ReadRetiredPagesStat()
        {
            return Command<OpCode::PAGE_STAT>();
        }
    };

    class Arg1Setter
    {
    public:
        Arg1Setter(UINT32 &cmd)
          : m_cmd(cmd)
        {}

        UINT32 & operator=(UINT08 arg1)
        {
            CommandDefs::Arg1Bits::Set(m_cmd) = arg1;

            return m_cmd;
        }

        operator UINT08() const
        {
            return static_cast<UINT08>(CommandDefs::Arg1Bits::Get(m_cmd));
        }

    private:
        UINT32 &m_cmd;
    };

    class Arg2Setter
    {
    public:
        Arg2Setter(UINT32 &cmd)
          : m_cmd(cmd)
        {}

        UINT32 & operator=(UINT08 arg1)
        {
            CommandDefs::Arg2Bits::Set(m_cmd) = arg1;

            return m_cmd;
        }

        operator UINT08() const
        {
            return static_cast<UINT08>(CommandDefs::Arg2Bits::Get(m_cmd));
        }

    private:
        UINT32 &m_cmd;
    };

    Arg1Setter CommandBuilder::Arg1(UINT32 &cmd)
    {
        return Arg1Setter(cmd);
    }

    Arg2Setter CommandBuilder::Arg2(UINT32 &cmd)
    {
        return Arg2Setter(cmd);
    }

    class Device
    {
        typedef DRF<31, 24> Byte3Bits;
        typedef DRF<23, 16> Byte2Bits;
        typedef DRF<15,  8> Byte1Bits;
        typedef DRF< 7,  0> Byte0Bits;

        static const UINT08 commandRegCmd = 0x5c;
        static const UINT08 dataInRegCmd = 0x5d;

        struct SafeBoolHelper
        {
            int x;
        };
        typedef int SafeBoolHelper::* SafeBool;

    public:
        Device()
            : m_isInited(false)
            , m_smbPort(nullptr)
            , m_boardAddress(0)
        {}

        RC Init(UINT08 boardAddr)
        {
            RC rc;

            if (!m_isInited)
            {
                m_boardAddress = boardAddr;

                // Set m_isInited to true while initializing,
                // otherwise ReadStatus will automatically fail
                m_isInited = true;
                bool found = false;

                if (SMBPBI::useIPMI)
                {
                    UINT08 status = 0;
                    if (OK == ReadStatus(&status))
                    {
                        found = true;
                    }
                }
                else
                {
                    SmbManager *smbManager = SmbMgr::Mgr();
                    CHECK_RC(smbManager->Find());
                    CHECK_RC(smbManager->InitializeAll());
                    UINT32 numDev = 0;
                    CHECK_RC(smbManager->GetNumDevices(&numDev));
                    for (UINT32 i = 0; !found && numDev > i; ++i)
                    {
                        SmbDevice *smbDevice = nullptr;
                        CHECK_RC(smbManager->GetByIndex(i, reinterpret_cast<McpDev**>(&smbDevice)));
                        UINT32 numSubDev = 0;
                        CHECK_RC(smbDevice->GetNumSubDev(&numSubDev));
                        for (UINT32 j = 0; numSubDev > j; ++j)
                        {
                            CHECK_RC(smbDevice->GetSubDev(j, reinterpret_cast<SmbPort**>(&m_smbPort)));
                            UINT08 status = 0;
                            if (OK == ReadStatus(&status))
                            {
                                found = true;
                                break;
                            }
                        }
                    }
                }

                if (!found)
                {
                    m_isInited = false;
                    m_smbPort = nullptr;
                }
            }
            return rc;
        }

        RC Cleanup()
        {
            RC rc;

            if (!SMBPBI::useIPMI)
            {
                SmbManager *smbManager = SmbMgr::Mgr();
                CHECK_RC(smbManager->Forget());
            }

            return rc;
        }

        bool IsInited() const
        {
            return m_isInited;
        }

        operator SafeBool() const
        {
            return IsInited() ? &SafeBoolHelper::x : 0;
        }

        RC WriteCommand(UINT32 cmd)
        {
            return Write(commandRegCmd, cmd);
        }

        RC WriteDataIn(UINT32 inData)
        {
            return Write(dataInRegCmd, inData);
        }

        RC ReadStatus(UINT08 *status)
        {
            typedef DRF<28, 24> StatusBits;

            MASSERT(nullptr != status);
            *status = 0;

            RC rc;
            UINT32 cmd = 0;

            CHECK_RC(Read(commandRegCmd, &cmd));
            *status = static_cast<UINT08>(StatusBits::Get(cmd));

            return rc;
        }

        RC ReadDataIn(UINT32 *inData)
        {
            return Read(dataInRegCmd, inData);
        }

        template <class Predicate>
        RC WaitForStatus(Predicate pred)
        {
            RC rc;
            const size_t timeOut = 100;
            UINT08 status = 0;
            size_t count = 0;

            do
            {
                if (0 != count)
                {
                    Tasker::Sleep(100);
                }

                CHECK_RC(ReadStatus(&status));
                ++count;
            } while (timeOut > count && !pred(static_cast<StatusEnum>(status)));

            if (timeOut == count)
            {
                return RC::TIMEOUT_ERROR;
            }

            return rc;
        }

    private:
        RC Write(UINT08 where, UINT32 what)
        {
            vector<UINT08> data(4);

            RC rc;

            if (IsInited())
            {
                data[0] = Byte0Bits::Get(what);
                data[1] = Byte1Bits::Get(what);
                data[2] = Byte2Bits::Get(what);
                data[3] = Byte3Bits::Get(what);
                if (SMBPBI::useIPMI)
                {
                    CHECK_RC(m_IpmiDevice.WriteSMBPBI(m_boardAddress, where, data));
                }
                else
                {
                    CHECK_RC(m_smbPort->WrBlkCmd(m_boardAddress, where, &data));
                }
            }
            else
            {
                return RC::WAS_NOT_INITIALIZED;
            }

            return rc;
        }

        RC Read(UINT08 where, UINT32 *what)
        {
            MASSERT(nullptr != what);
            *what = 0;

            vector<UINT08> data(4);

            RC rc;

            if (IsInited())
            {
                if (SMBPBI::useIPMI)
                {
                    CHECK_RC(m_IpmiDevice.ReadSMBPBI(m_boardAddress, where, data));
                }
                else
                {
                    CHECK_RC(m_smbPort->RdBlkCmd(m_boardAddress, where, &data));
                }

                Byte0Bits::Set(*what) = data[0];
                Byte1Bits::Set(*what) = data[1];
                Byte2Bits::Set(*what) = data[2];
                Byte3Bits::Set(*what) = data[3];
            }
            else
            {
                return RC::WAS_NOT_INITIALIZED;
            }

            return rc;
        }

        bool       m_isInited;
        SmbPort   *m_smbPort;
        UINT08     m_boardAddress;
        IpmiDevice m_IpmiDevice;
    };

    class Exelwtor
    {
    public:
        Exelwtor()
          : m_lastStatus(Status::NULL_STATUS)
        {}

        RC Init(UINT08 boardAddr)
        {
            return m_dev.Init(boardAddr);
        }

        RC Cleanup()
        {
            return m_dev.Cleanup();
        }

        bool IsInited() const
        {
            return m_dev.IsInited();
        }

        RC operator()(UINT32 cmd)
        {
            return Execute(cmd, false, 0);
        }

        RC operator()(UINT32 cmd, UINT32 dataIn)
        {
            return Execute(cmd, true, dataIn);
        }

        RC GetResult(UINT32 *data)
        {
            MASSERT(nullptr != data);
            *data = 0;

            RC rc;

            CHECK_RC(m_dev.ReadDataIn(data));

            return rc;
        }

        UINT08 GetLastStatus() const
        {
            return m_lastStatus;
        }

    private:
        RC Execute(UINT32 cmd, bool needDataIn, UINT32 dataIn)
        {
            RC rc;

            if (!m_dev)
            {
                return RC::WAS_NOT_INITIALIZED;
            }

            if (m_firstCommand)
            {
                CHECK_RC(m_dev.WaitForStatus(bind1st(equal_to<StatusEnum>(), Status::READY)));
                m_firstCommand = false;
            }

            // according to the specification if the status is equal to READY,
            // we have to repeat the command as a part of initialization process
            for (size_t i = 0; 2 > i; ++i)
            {
                if (needDataIn)
                {
                    CHECK_RC(m_dev.WriteDataIn(dataIn));
                }

                CHECK_RC(m_dev.WriteCommand(cmd));
                CHECK_RC(m_dev.WaitForStatus(
                    bind1st(not_equal_to<StatusEnum>(),
                    Status::NULL_STATUS)));
                CHECK_RC(m_dev.ReadStatus(&m_lastStatus));
                if (Status::READY != m_lastStatus)
                {
                    break;
                }
            }

            return rc;
        }

        static bool m_firstCommand;
        UINT08 m_lastStatus;
        Device m_dev;
    };
    bool Exelwtor::m_firstCommand = true;

    class SMBPBInterface
    {
    private:
        typedef DRF<31, 24> Byte3Bits;
        typedef DRF<23, 16> Byte2Bits;
        typedef DRF<15,  8> Byte1Bits;
        typedef DRF< 7,  0> Byte0Bits;

        enum StateRegs
        {
            SCRATCH_MEMORY_REG = 0
        };

    public:
        RC Init(UINT08 boardAddr);
        RC Cleanup();
        bool IsInited() const;

        bool GetCapTempPrimaryGPU() const
        {
            return m_capTempPrimaryGPU;
        }

        bool GetCapTempSecondaryGPU() const
        {
            return m_capTempSecondaryGPU;
        }

        bool GetCapTempBoard() const
        {
            return m_capTempBoard;
        }

        bool GetCapTempBoardMemory() const
        {
            return m_capTempBoardMemory;
        }

        UINT08 GetCapTempFracBits() const
        {
            return m_capTempFracBits;
        }

        bool GetCapPowerConsumption() const
        {
            return m_capPowerConsumption;
        }

        bool GetCapGPUPerfControl() const
        {
            return m_capGPUPerfControl;
        }

        bool GetCapGPUSysControl() const
        {
            return m_capGPUSysControl;
        }

        bool GetCapBoardPartNum() const
        {
            return m_capBoardPartNum;
        }

        bool GetCapOEMInfo() const
        {
            return m_capOEMInfo;
        }

        bool GetCapSerialNum() const
        {
            return m_capSerialNum;
        }

        bool GetCapMarketingName() const
        {
            return m_capMarketingName;
        }

        bool GetCapSiliconRev() const
        {
            return m_capSiliconRev;
        }

        bool GetCapMemVendor() const
        {
            return m_capMemVendor;
        }

        bool GetCapMemPartNum() const
        {
            return m_capMemPartNum;
        }

        bool GetCapBuildDate() const
        {
            return m_capBuildDate;
        }

        bool GetCapFirmwareVersion() const
        {
            return m_capFirmwareVersion;
        }

        bool GetCapVID() const
        {
            return m_capVID;
        }

        bool GetCapDID() const
        {
            return m_capDID;
        }

        bool GetCapSSVID() const
        {
            return m_capSubsysVID;
        }

        bool GetCapSSDID() const
        {
            return m_capSubsysDID;
        }

        bool GetCapECCStatV1() const
        {
            return m_capECCStatV1;
        }

        bool GetCapECCStatV2() const
        {
            return m_capECCStatV2;
        }

        bool GetCapECCStatV3() const
        {
            return m_capECCStatV3;
        }

        bool GetCapRetiredPageCount() const
        {
            return m_capRetiredPageCount;
        }

        bool GetCapGPUDrvIsNotLoaded() const
        {
            return m_capGPUDrvIsNotLoaded;
        }

        bool GetCapGPURequest() const
        {
            return m_capGPURequest;
        }

        UINT32 GetCapScratchSpaceSize() const
        {
            return m_capScratchSpaceSize;
        }

        bool GetCapFanQuery() const
        {
            return m_capFanQuery;
        }

        bool GetCapCheckExtPower() const
        {
            return m_capCheckExtPower;
        }

        RC GetTemperature(TempSrcEnum src, UINT32 *temp);
        RC GetTemperature(TempSrcEnum src, double *temp);
        RC GetPower(UINT32 *pwr);
        RC GetBoardPartNumber(string *boardPartNumber);
        RC GetOEMInfo(string *oemInfo);
        RC GetSerialNumber(string *serialNum);
        RC GetMarketingName(string *marketName);
        RC GetGPUPartNumber(string *gpuPartNumber);
        RC GetMemoryVendor(string *memVendor);
        RC GetMemoryPartNumber(string *memPartNum);
        RC GetBuildDate(string *buildDate);
        RC GetFirmwareVersion(string *firmware);
        RC GetVID(UINT16 *vid);
        RC GetDID(UINT16 *did);
        RC GetSSVID(UINT16 *ssvid);
        RC GetSSDID(UINT16 *ssdid);

        RC GetEccL1SBErrorsV2(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccL1DBErrorsV2(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccSMRegFileSBErrorsV2(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccSMRegFileDBErrorsV2(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccL2SBErrorsV2(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt);
        RC GetEccL2DBErrorsV2(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt);
        RC GetEccFrameBufSBErrorsV2(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt);
        RC GetEccFrameBufDBErrorsV2(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt);

        RC GetEccL1SBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccL1DBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccSMRegFileSBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccSMRegFileDBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccTexUnitSBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccTexUnitDBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccL2SBErrorsV3(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt);
        RC GetEccL2DBErrorsV3(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt);
        RC GetEccFrameBufSBErrorsV3(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt);
        RC GetEccFrameBufDBErrorsV3(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt);

        RC GetEccL1SBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccL1DBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccSMRegFileSBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccSMRegFileDBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccTexUnitSBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccTexUnitDBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt);
        RC GetEccL2SBErrors(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt);
        RC GetEccL2DBErrors(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt);
        RC GetEccFrameBufSBErrors(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt);
        RC GetEccFrameBufDBErrors(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt);

        RC ReadScratchMemory(UINT32 dwordAddress, UINT32 *value);
        RC WriteScratchMemory(UINT32 dwordAddress, UINT32 value);

        RC CheckExternalPower(bool *isEnough);

    private:
        RC GetGpuInfo(GpuInfoEnum gpuInfo, const char *name, bool truncate, string *outStr);
        RC GetString(
            UINT32 cmd,
            size_t length,
            bool truncate,
            string *outStr,
            StatusEnum* status,
            size_t *lstIdx
        );

        RC ReadStateRegister(StateRegs reg, UINT32 *value);
        RC WriteStateRegister(StateRegs reg, UINT32 value);

        Exelwtor m_exelwtor;

        bool   m_capTempPrimaryGPU;
        bool   m_capTempSecondaryGPU;
        bool   m_capTempBoard;
        bool   m_capTempBoardMemory;
        UINT08 m_capTempFracBits;
        bool   m_capPowerConsumption;
        bool   m_capGPUPerfControl;
        bool   m_capGPUSysControl;
        bool   m_capBoardPartNum;
        bool   m_capOEMInfo;
        bool   m_capSerialNum;
        bool   m_capMarketingName;
        bool   m_capSiliconRev;
        bool   m_capMemVendor;
        bool   m_capMemPartNum;
        bool   m_capBuildDate;
        bool   m_capFirmwareVersion;
        bool   m_capVID;
        bool   m_capDID;
        bool   m_capSubsysVID;
        bool   m_capSubsysDID;
        bool   m_capECCStatV1;
        bool   m_capECCStatV2;
        bool   m_capECCStatV3;
        bool   m_capRetiredPageCount;
        bool   m_capGPUDrvIsNotLoaded;
        bool   m_capGPURequest;
        UINT32 m_capScratchSpaceSize;
        bool   m_capFanQuery;
        bool   m_capCheckExtPower;
    };

    RC SMBPBInterface::Init(UINT08 boardAddr)
    {
        typedef DRF<0,  0>  CapTempPrimaryGPUBits;
        typedef DRF<1,  1>  CapTempSecondaryGPUBits;
        typedef DRF<4,  4>  CapTempBoardBits;
        typedef DRF<5,  5>  CapTempBoardMemoryBits;
        typedef DRF<11, 8>  CapTempFracBitsBits;
        typedef DRF<16, 16> CapPowerConsumptionBits;
        typedef DRF<17, 17> CapGPUPerfControlBits;
        typedef DRF<18, 18> CapGPUSysControlBits;

        typedef DRF<0,  0>  CapBoardPartNumBits;
        typedef DRF<1,  1>  CapOEMInfoBits;
        typedef DRF<2,  2>  CapSerialNumBits;
        typedef DRF<3,  3>  CapMarketingNameBits;
        typedef DRF<4,  4>  CapSiliconRevBits;
        typedef DRF<5,  5>  CapMemVendorBits;
        typedef DRF<6,  6>  CapMemPartNumBits;
        typedef DRF<7,  7>  CapBuildDateBits;
        typedef DRF<8,  8>  CapFirmwareVersionBits;
        typedef DRF<9,  9>  CapVIDBits;
        typedef DRF<10, 10> CapDIDBits;
        typedef DRF<11, 11> CapSubsysVIDBits;
        typedef DRF<12, 12> CapSubsysDIDBits;
        typedef DRF<16, 16> CapECCStatV1Bits;
        typedef DRF<17, 17> CapECCStatV2Bits;
        typedef DRF<18, 18> CapECCStatV3Bits;
        typedef DRF<19, 19> CapRetiredPageCountBits;

        typedef DRF<0,  0>  CapGPUDrvIsNotLoadedBits;
        typedef DRF<1,  1>  CapGPURequestBits;
        typedef DRF<4,  2>  CapScratchSpaceSizeBits;
        typedef DRF<5,  5>  CapFanQueryBits;

        RC rc;

        CHECK_RC(m_exelwtor.Init(boardAddr));

        UINT32 cap;
        CHECK_RC(m_exelwtor(CommandBuilder::GetCapabilities(0)));
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            return RC::HW_STATUS_ERROR;
        }
        CHECK_RC(m_exelwtor.GetResult(&cap));
        m_capTempPrimaryGPU   = (0 != CapTempPrimaryGPUBits::Get(cap));
        m_capTempSecondaryGPU = (0 != CapTempSecondaryGPUBits::Get(cap));
        m_capTempBoard        = (0 != CapTempBoardBits::Get(cap));
        m_capTempBoardMemory  = (0 != CapTempBoardMemoryBits::Get(cap));
        m_capTempFracBits     = static_cast<UINT08>(CapTempFracBitsBits::Get(cap));
        m_capPowerConsumption = (0 != CapPowerConsumptionBits::Get(cap));
        m_capGPUPerfControl   = (0 != CapGPUPerfControlBits::Get(cap));
        m_capGPUSysControl    = (0 != CapGPUSysControlBits::Get(cap));

        CHECK_RC(m_exelwtor(CommandBuilder::GetCapabilities(1)));
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            return RC::HW_STATUS_ERROR;
        }
        CHECK_RC(m_exelwtor.GetResult(&cap));
        m_capBoardPartNum     = (0 != CapBoardPartNumBits::Get(cap));
        m_capOEMInfo          = (0 != CapOEMInfoBits::Get(cap));
        m_capSerialNum        = (0 != CapSerialNumBits::Get(cap));
        m_capMarketingName    = (0 != CapMarketingNameBits::Get(cap));
        m_capSiliconRev       = (0 != CapSiliconRevBits::Get(cap));
        m_capMemVendor        = (0 != CapMemVendorBits::Get(cap));
        m_capMemPartNum       = (0 != CapMemPartNumBits::Get(cap));
        m_capBuildDate        = (0 != CapBuildDateBits::Get(cap));
        m_capFirmwareVersion  = (0 != CapFirmwareVersionBits::Get(cap));
        m_capVID              = (0 != CapVIDBits::Get(cap));
        m_capDID              = (0 != CapDIDBits::Get(cap));
        m_capSubsysVID        = (0 != CapSubsysVIDBits::Get(cap));
        m_capSubsysDID        = (0 != CapSubsysDIDBits::Get(cap));
        m_capECCStatV1        = (0 != CapECCStatV1Bits::Get(cap));
        m_capECCStatV2        = (0 != CapECCStatV2Bits::Get(cap));
        m_capECCStatV3        = (0 != CapECCStatV3Bits::Get(cap));
        m_capRetiredPageCount = (0 != CapRetiredPageCountBits::Get(cap));

        CHECK_RC(m_exelwtor(CommandBuilder::GetCapabilities(2)));
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            return RC::HW_STATUS_ERROR;
        }
        CHECK_RC(m_exelwtor.GetResult(&cap));
        m_capGPUDrvIsNotLoaded = (0 != CapGPUDrvIsNotLoadedBits::Get(cap));
        m_capGPURequest        = (0 != CapGPURequestBits::Get(cap));

        // See Table 3.6 of LWPU SMBus Post-Box Interface (SMBPBI)
        // Tesla/Lwdqro Software Design Guide
        UINT32 scratchSizePwr2 = CapScratchSpaceSizeBits::Get(cap);
        if (0 == scratchSizePwr2)
        {
            m_capScratchSpaceSize = 0;
        }
        else
        {
            // scratchSizePwr2 equal 1 corresponds to 4 kB, 2 to 8 kB,... 7 to 256 kB
            m_capScratchSpaceSize = (1 << (scratchSizePwr2 + 1)) * 1024;
        }

        m_capFanQuery          = (0 != CapFanQueryBits::Get(cap));

        CHECK_RC(m_exelwtor(CommandBuilder::CheckPower()));
        m_capCheckExtPower = Status::ERR_OPCODE != m_exelwtor.GetLastStatus();

        return rc;
    }

    RC SMBPBInterface::Cleanup()
    {
        return m_exelwtor.Cleanup();
    }

    bool SMBPBInterface::IsInited() const
    {
        return m_exelwtor.IsInited();
    }

    RC SMBPBInterface::GetTemperature(TempSrcEnum src, UINT32 *temp)
    {
        typedef DRF<31, 8> IntegerBits;

        MASSERT(nullptr != temp);
        *temp = 0;

        RC rc;

        m_exelwtor(CommandBuilder::GetSinglePrecisionTemperature(src));
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            if (Status::ERR_ARG1 == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: %d is invalid sensor ID\n",
                    static_cast<int>(src));
            }

            if (Status::ERR_NOT_SUPPORTED == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: requested sensor %d is not supported\n",
                    static_cast<int>(src));
            }

            return RC::HW_STATUS_ERROR;
        }
        UINT32 res = 0;
        CHECK_RC(m_exelwtor.GetResult(&res));
        *temp = IntegerBits::Get(res);

        return rc;
    }

    RC SMBPBInterface::GetTemperature(TempSrcEnum src, double *temp)
    {
        typedef DRF<31, 31> SignBit;
        typedef DRF<30,  8> IntegerBits;
        typedef DRF<7,   0> FractionalBits;

        MASSERT(nullptr != temp);
        *temp = 0;

        RC rc;

        m_exelwtor(CommandBuilder::GetExtendedPrecisionTemperature(src));
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            if (Status::ERR_ARG1 == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: %d is invalid sensor ID\n",
                    static_cast<int>(src));
            }

            if (Status::ERR_NOT_SUPPORTED == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: requested sensor %d is not supported\n",
                    static_cast<int>(src));
            }

            return RC::HW_STATUS_ERROR;
        }
        UINT32 res = 0;
        CHECK_RC(m_exelwtor.GetResult(&res));
        *temp = IntegerBits::Get(res) + FractionalBits::Get(res) / 256.0;
        *temp *= SignBit::Get(res) ? -1 : 1;

        return rc;
    }

    RC SMBPBInterface::GetPower(UINT32 *pwr)
    {
        MASSERT(nullptr != pwr);
        *pwr = 0;

        RC rc;

        m_exelwtor(CommandBuilder::GetPower());
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            if (Status::ERR_ARG1 == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: invalid power source\n");
            }

            if (Status::ERR_NOT_SUPPORTED == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: requested power source is not supported\n");
            }

            return RC::HW_STATUS_ERROR;
        }
        CHECK_RC(m_exelwtor.GetResult(pwr));

        return rc;
    }

    RC SMBPBInterface::GetBoardPartNumber(string *boardPartNumber)
    {
        MASSERT(nullptr != boardPartNumber);

        return GetGpuInfo(
            GpuInfo::BOARD_PART_NUMBER, "board part number", true, boardPartNumber);
    }

    RC SMBPBInterface::GetOEMInfo(string *oemInfo)
    {
        MASSERT(nullptr != oemInfo);

        return GetGpuInfo(GpuInfo::OEM_INFORMATION, "OEM information", true, oemInfo);
    }

    RC SMBPBInterface::GetSerialNumber(string *serialNum)
    {
        MASSERT(nullptr != serialNum);

        return GetGpuInfo(GpuInfo::SERIAL_NUMBER, "serial number", true, serialNum);
    }

    RC SMBPBInterface::GetMarketingName(string *marketName)
    {
        MASSERT(nullptr != marketName);

        return GetGpuInfo(GpuInfo::MARKETING_NAME, "marketing name", true, marketName);
    }

    RC SMBPBInterface::GetGPUPartNumber(string *gpuPartNumber)
    {
        MASSERT(nullptr != gpuPartNumber);

        return GetGpuInfo(GpuInfo::GPU_PART_NUMBER, "GPU part number", true, gpuPartNumber);
    }

    RC SMBPBInterface::GetMemoryVendor(string *memVendor)
    {
        MASSERT(nullptr != memVendor);

        return GetGpuInfo(GpuInfo::MEMORY_VENDOR, "memory vendor", true, memVendor);
    }

    RC SMBPBInterface::GetMemoryPartNumber(string *memPartNum)
    {
        MASSERT(nullptr != memPartNum);

        return GetGpuInfo(GpuInfo::MEMORY_PART_NUMBER, "memory part number", true, memPartNum);
    }

    RC SMBPBInterface::GetBuildDate(string *buildDate)
    {
        MASSERT(nullptr != buildDate);

        return GetGpuInfo(GpuInfo::BUILD_DATE, "build date", false, buildDate);
    }

    RC SMBPBInterface::GetFirmwareVersion(string *firmware)
    {
        MASSERT(nullptr != firmware);

        return GetGpuInfo(GpuInfo::FIRMWARE_VERSION, "firmware version", true, firmware);
    }

    RC SMBPBInterface::GetVID(UINT16 *vid)
    {
        typedef DRF<15, 8, UINT16> Byte1Bits;
        typedef DRF< 7, 0, UINT16> Byte0Bits;

        MASSERT(nullptr != vid);
        *vid = 0;

        RC rc;

        string vidStr;
        CHECK_RC(GetGpuInfo(GpuInfo::PCI_VID, "PCI configuration vendor ID", false, &vidStr));

        Byte0Bits::Set(*vid) = vidStr[0];
        Byte1Bits::Set(*vid) = vidStr[1];

        return rc;
    }

    RC SMBPBInterface::GetDID(UINT16 *did)
    {
        typedef DRF<15, 8, UINT16> Byte1Bits;
        typedef DRF< 7, 0, UINT16> Byte0Bits;

        MASSERT(nullptr != did);
        *did = 0;

        RC rc;

        string didStr;
        CHECK_RC(GetGpuInfo(GpuInfo::PCI_DID, "PCI configuration device ID", false, &didStr));

        Byte0Bits::Set(*did) = didStr[0];
        Byte1Bits::Set(*did) = didStr[1];

        return rc;
    }

    RC SMBPBInterface::GetSSVID(UINT16 *ssvid)
    {
        typedef DRF<15, 8, UINT16> Byte1Bits;
        typedef DRF< 7, 0, UINT16> Byte0Bits;

        MASSERT(nullptr != ssvid);
        *ssvid = 0;

        RC rc;

        string ssvidStr;
        CHECK_RC(GetGpuInfo(
            GpuInfo::PCI_SSVID, "PCI configuration subsystem vendor ID", false, &ssvidStr));

        Byte0Bits::Set(*ssvid) = ssvidStr[0];
        Byte1Bits::Set(*ssvid) = ssvidStr[1];

        return rc;
    }

    RC SMBPBInterface::GetSSDID(UINT16 *ssdid)
    {
        typedef DRF<15, 8, UINT16> Byte1Bits;
        typedef DRF< 7, 0, UINT16> Byte0Bits;

        MASSERT(nullptr != ssdid);
        *ssdid = 0;

        RC rc;

        string ssdidStr;
        CHECK_RC(GetGpuInfo(
            GpuInfo::PCI_SSDID, "PCI configuration subsystem device ID", false, &ssdidStr));

        Byte0Bits::Set(*ssdid) = ssdidStr[0];
        Byte1Bits::Set(*ssdid) = ssdidStr[1];

        return rc;
    }

    RC SMBPBInterface::GetEccL1SBErrorsV2(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetL1CacheEccStat2(EccStatType::SINGLE_BIT, gpcNum, tpcNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccL1DBErrorsV2(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetL1CacheEccStat2(EccStatType::DOUBLE_BIT, gpcNum, tpcNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccSMRegFileSBErrorsV2(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetSMRegFileEccStat2(EccStatType::SINGLE_BIT, gpcNum, tpcNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccSMRegFileDBErrorsV2(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetSMRegFileEccStat2(EccStatType::DOUBLE_BIT, gpcNum, tpcNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccL2SBErrorsV2(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetL2CacheEccStat2(EccStatType::SINGLE_BIT, partNum, sliceNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccL2DBErrorsV2(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetL2CacheEccStat2(EccStatType::DOUBLE_BIT, partNum, sliceNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccFrameBufSBErrorsV2(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetFrameBufEccStat2(EccStatType::SINGLE_BIT, partNum, subPartNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccFrameBufDBErrorsV2(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetFrameBufEccStat2(EccStatType::DOUBLE_BIT, partNum, subPartNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccL1SBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetL1CacheEccStat3(EccStatType::SINGLE_BIT, gpcNum, tpcNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccL1DBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetL1CacheEccStat3(EccStatType::DOUBLE_BIT, gpcNum, tpcNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccSMRegFileSBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetSMRegFileEccStat3(EccStatType::SINGLE_BIT, gpcNum, tpcNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccSMRegFileDBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetSMRegFileEccStat3(EccStatType::DOUBLE_BIT, gpcNum, tpcNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccTexUnitSBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetTexUnitEccStat3(EccStatType::SINGLE_BIT, gpcNum, tpcNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccTexUnitDBErrorsV3(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetTexUnitEccStat3(EccStatType::DOUBLE_BIT, gpcNum, tpcNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccL2SBErrorsV3(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetL2CacheEccStat3(EccStatType::SINGLE_BIT, partNum, sliceNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccL2DBErrorsV3(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetL2CacheEccStat3(EccStatType::DOUBLE_BIT, partNum, sliceNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccFrameBufSBErrorsV3(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetFrameBufEccStat3(EccStatType::SINGLE_BIT, partNum, subPartNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccFrameBufDBErrorsV3(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt)
    {
        MASSERT(nullptr != cnt);
        *cnt = 0;

        RC rc;

        CHECK_RC(m_exelwtor(
            CommandBuilder::GetFrameBufEccStat3(EccStatType::DOUBLE_BIT, partNum, subPartNum)));
        CHECK_RC(m_exelwtor.GetResult(cnt));

        return rc;
    }

    RC SMBPBInterface::GetEccL1SBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        if (GetCapECCStatV2())
        {
            return GetEccL1SBErrorsV2(gpcNum, tpcNum, cnt);
        }

        if (GetCapECCStatV3())
        {
            return GetEccL1SBErrorsV3(gpcNum, tpcNum, cnt);
        }

        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SMBPBInterface::GetEccL1DBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        if (GetCapECCStatV2())
        {
            return GetEccL1DBErrorsV2(gpcNum, tpcNum, cnt);
        }

        if (GetCapECCStatV3())
        {
            return GetEccL1DBErrorsV3(gpcNum, tpcNum, cnt);
        }

        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SMBPBInterface::GetEccSMRegFileSBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        if (GetCapECCStatV2())
        {
            return GetEccSMRegFileSBErrorsV2(gpcNum, tpcNum, cnt);
        }

        if (GetCapECCStatV3())
        {
            return GetEccSMRegFileSBErrorsV3(gpcNum, tpcNum, cnt);
        }

        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SMBPBInterface::GetEccSMRegFileDBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        if (GetCapECCStatV2())
        {
            return GetEccSMRegFileDBErrorsV2(gpcNum, tpcNum, cnt);
        }

        if (GetCapECCStatV3())
        {
            return GetEccSMRegFileDBErrorsV3(gpcNum, tpcNum, cnt);
        }

        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SMBPBInterface::GetEccTexUnitSBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        if (GetCapECCStatV3())
        {
            return GetEccTexUnitSBErrorsV3(gpcNum, tpcNum, cnt);
        }

        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SMBPBInterface::GetEccTexUnitDBErrors(UINT08 gpcNum, UINT08 tpcNum, UINT32 *cnt)
    {
        if (GetCapECCStatV3())
        {
            return GetEccTexUnitDBErrorsV3(gpcNum, tpcNum, cnt);
        }

        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SMBPBInterface::GetEccL2SBErrors(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt)
    {
        if (GetCapECCStatV2())
        {
            return GetEccL2SBErrorsV2(partNum, sliceNum, cnt);
        }

        if (GetCapECCStatV3())
        {
            return GetEccL2SBErrorsV3(partNum, sliceNum, cnt);
        }

        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SMBPBInterface::GetEccL2DBErrors(UINT08 partNum, UINT08 sliceNum, UINT32 *cnt)
    {
        if (GetCapECCStatV2())
        {
            return GetEccL2DBErrorsV2(partNum, sliceNum, cnt);
        }

        if (GetCapECCStatV3())
        {
            return GetEccL2DBErrorsV3(partNum, sliceNum, cnt);
        }

        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SMBPBInterface::GetEccFrameBufSBErrors(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt)
    {
        if (GetCapECCStatV2())
        {
            return GetEccFrameBufSBErrorsV2(partNum, subPartNum, cnt);
        }

        if (GetCapECCStatV3())
        {
            return GetEccFrameBufSBErrorsV3(partNum, subPartNum, cnt);
        }

        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SMBPBInterface::GetEccFrameBufDBErrors(UINT08 partNum, UINT08 subPartNum, UINT32 *cnt)
    {
        if (GetCapECCStatV2())
        {
            return GetEccFrameBufDBErrorsV2(partNum, subPartNum, cnt);
        }

        if (GetCapECCStatV3())
        {
            return GetEccFrameBufDBErrorsV3(partNum, subPartNum, cnt);
        }

        return RC::UNSUPPORTED_FUNCTION;
    }

    RC SMBPBInterface::ReadScratchMemory(UINT32 dwordAddress, UINT32 *value)
    {
        typedef DRF<15, 8> ReadBankBits;

        MASSERT(0 != value);
        *value = 0;

        RC rc;

        // a bank size is 1 kB, but the address is the address of a dword,
        // therefore we divide by 0x100
        UINT32 bankNum = dwordAddress / 0x100;

        UINT32 scratchMemState;
        CHECK_RC(ReadStateRegister(SCRATCH_MEMORY_REG, &scratchMemState));
        UINT32 lwrReadBank = ReadBankBits::Get(scratchMemState);
        if (lwrReadBank != bankNum)
        {
            CHECK_RC(WriteStateRegister(
                SCRATCH_MEMORY_REG,
                ReadBankBits::Set(scratchMemState) = bankNum));
        }
        UINT08 dwordOffset = static_cast<UINT08>(dwordAddress % 0x100);
        m_exelwtor(CommandBuilder::ReadScratch(dwordOffset));
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            if (Status::ERR_NOT_SUPPORTED == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: reading scratch memory is not supported\n");
            }
            if (Status::ERR_MISC == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: DMA controller error\n");
            }

            return RC::HW_STATUS_ERROR;
        }
        CHECK_RC(m_exelwtor.GetResult(value));

        return rc;
    }

    RC SMBPBInterface::WriteScratchMemory(UINT32 dwordAddress, UINT32 value)
    {
        typedef DRF<7, 0> WriteBankBits;

        RC rc;

        // a bank size is 1 kB, but the address is the address of a dword,
        // therefore we divide by 0x100
        UINT32 bankNum = dwordAddress / 0x100;

        UINT32 scratchMemState;
        CHECK_RC(ReadStateRegister(SCRATCH_MEMORY_REG, &scratchMemState));
        UINT32 lwrReadBank = WriteBankBits::Get(scratchMemState);
        if (lwrReadBank != bankNum)
        {
            CHECK_RC(WriteStateRegister(
                SCRATCH_MEMORY_REG,
                WriteBankBits::Set(scratchMemState) = bankNum));
        }
        UINT08 dwordOffset = static_cast<UINT08>(dwordAddress % 0x100);
        m_exelwtor(CommandBuilder::WriteScratch(dwordOffset, 1), value);
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            if (Status::ERR_NOT_SUPPORTED == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: reading scratch memory is not supported\n");
            }
            if (Status::ERR_MISC == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: DMA controller error\n");
            }

            return RC::HW_STATUS_ERROR;
        }

        return rc;
    }

    RC SMBPBInterface::CheckExternalPower(bool *isEnough)
    {
        MASSERT(nullptr != isEnough);
        *isEnough = false;

        RC rc;

        m_exelwtor(CommandBuilder::CheckPower());
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            if (Status::ERR_NOT_SUPPORTED == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: external power check is not supported\n");
            }

            return RC::HW_STATUS_ERROR;
        }
        UINT32 res;
        CHECK_RC(m_exelwtor.GetResult(&res));

        *isEnough = (0 == res);

        return rc;
    }

    RC SMBPBInterface::GetGpuInfo(
        GpuInfoEnum gpuInfo, const char *name, bool truncate, string *outStr
    )
    {
        MASSERT(nullptr != outStr);

        RC rc;

        GpuInfoIdxToLength::const_iterator it;
        it = gpuInfoIdxToLength.find(gpuInfo);
        MASSERT(gpuInfoIdxToLength.end() != it);

        StatusEnum status = Status::NULL_STATUS;
        size_t lastIdx = 0;
        UINT32 cmd = CommandBuilder::GetGPUInformation(gpuInfo, 0);
        GetString(cmd, it->second, truncate, outStr, &status, &lastIdx);

        if (Status::SUCCESS != status)
        {
            if (Status::ERR_ARG1 == status)
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: %s is not supported\n", name);
            }

            if (Status::ERR_ARG2 == status)
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: wrong offset %d\n", static_cast<int>(lastIdx));
            }

            return RC::HW_STATUS_ERROR;
        }

        return rc;
    }

    RC SMBPBInterface::GetString(
        UINT32 cmd,
        size_t length,
        bool truncate,
        string *outStr,
        StatusEnum* status,
        size_t *lstIdx)
    {
        MASSERT(nullptr != status);
        MASSERT(nullptr != outStr);
        MASSERT(nullptr != lstIdx);
        *status = Status::NULL_STATUS;
        *lstIdx = 0;

        RC rc;

        *outStr = string(length, 0);

        string::iterator it = outStr->begin();
        UINT32 (*getb[4])(UINT32) =
        {
            Byte0Bits::Get, Byte1Bits::Get, Byte2Bits::Get, Byte3Bits::Get
        };
        UINT32 res = 0;
        for (size_t i = 0; length > i; ++i)
        {
            if (0 == i % 4)
            {
                CommandBuilder::Arg2(cmd) = static_cast<UINT08>(i / 4);
                CHECK_RC(m_exelwtor(cmd));
                if (Status::SUCCESS != m_exelwtor.GetLastStatus())
                {
                    *status = static_cast<StatusEnum>(m_exelwtor.GetLastStatus());
                    *lstIdx = i / 4;
                    return RC::HW_STATUS_ERROR;
                }
                CHECK_RC(m_exelwtor.GetResult(&res));
            }

            *it++ = static_cast<string::value_type>(getb[i % 4](res));
        }

        if (truncate)
        {
            string::size_type end = outStr->find('\0');
            if (end != string::npos) outStr->erase(end);
        }

        *status = static_cast<StatusEnum>(m_exelwtor.GetLastStatus());

        return rc;
    }

    RC SMBPBInterface::ReadStateRegister(StateRegs reg, UINT32 *value)
    {
        MASSERT(nullptr != value);
        *value = 0;

        RC rc;

        m_exelwtor(CommandBuilder::ReadStateReg(reg));
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            if (Status::ERR_NOT_SUPPORTED == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: state registers access is not supported\n");
            }
            if (Status::ERR_ARG2 == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: %d is invalid state register address\n",
                    static_cast<int>(reg));
            }
            if (Status::ERR_MISC == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: DMA controller error\n");
            }

            return RC::HW_STATUS_ERROR;
        }
        CHECK_RC(m_exelwtor.GetResult(value));

        return rc;
    }

    RC SMBPBInterface::WriteStateRegister(StateRegs reg, UINT32 value)
    {
        RC rc;

        m_exelwtor(CommandBuilder::WriteStateReg(reg), value);
        if (Status::SUCCESS != m_exelwtor.GetLastStatus())
        {
            if (Status::ERR_NOT_SUPPORTED == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: state registers access is not supported\n");
            }
            if (Status::ERR_ARG2 == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: %d is invalid state register address\n",
                    static_cast<int>(reg));
            }
            if (Status::ERR_MISC == m_exelwtor.GetLastStatus())
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: DMA controller error\n");
            }

            return RC::HW_STATUS_ERROR;
        }

        return rc;
    }
}

using SMBPBI::SMBPBInterface;

class SMBPBITest : public GpuTest
{
public:
    SMBPBITest();

    RC Setup();
    RC Run();
    RC Cleanup();

    bool IsSupported();

    // Javascript properties
    SETGET_PROP(UseIPMI, bool);
    SETGET_PROP(VerifySku, bool);

private:
    // noncopyable
    SMBPBITest(const SMBPBITest&);
    const SMBPBITest& operator=(const SMBPBITest&);

    RC ValidateFields(string smbpbiStr, string dbStr);
    RC ValidateFields(string smbpbiStr, string dbStr, string desc, Tee::Priority printPri);

    GpuTestConfiguration *m_testConfig;
    SMBPBInterface        m_SMBPBI;
    bool m_UseIPMI;
    bool m_VerifySku;
};
JS_CLASS_INHERIT(SMBPBITest, GpuTest, "SMBus Post Box Interface test");
CLASS_PROP_READWRITE(SMBPBITest, UseIPMI, bool, "If true, route SMBus commands over IPMI interface");
CLASS_PROP_READWRITE(SMBPBITest, VerifySku, bool, "If true, compare SMBPBI board info with that in boards.db");

SMBPBITest::SMBPBITest()
    : m_testConfig(GetTestConfiguration())
    , m_UseIPMI(false)
    , m_VerifySku(false)
{
    SetName("SMBPBITest");
}

RC SMBPBITest::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    GpuSubdevice *subDev = GetBoundGpuSubdevice();

    SMBPBI::useIPMI = m_UseIPMI;
    CHECK_RC(m_SMBPBI.Init(subDev->GetSmbusAddress()));

    return rc;
}

RC SMBPBITest::Run()
{
    RC rc;

    Tee::Priority printPri = GetVerbosePrintPri();

    GpuSubdevice *gpuSubdevice = GetBoundGpuSubdevice();

    if (m_SMBPBI.GetCapCheckExtPower())
    {
        bool isEnoughPower;
        CHECK_RC(m_SMBPBI.CheckExternalPower(&isEnoughPower));
        if (isEnoughPower)
        {
            Printf(printPri, "The board has enough power.\n");
        }
        else
        {
            Printf(printPri, "The board doesn't have enough power.\n");
        }
    }

    if (m_SMBPBI.GetCapTempPrimaryGPU())
    {
        double temp = 0;
        CHECK_RC(m_SMBPBI.GetTemperature(SMBPBI::TempSrc::GPU_0, &temp));
        Printf(printPri, "Primary GPU temperature = %f\n", temp);
    }

    if (m_SMBPBI.GetCapPowerConsumption())
    {
        UINT32 pwr = 0;
        CHECK_RC(m_SMBPBI.GetPower(&pwr));
        Printf(printPri, "Total board power consumption = %f W\n", pwr / 1000.0);
    }

    if (m_SMBPBI.GetCapBoardPartNum())
    {
        string boardPartNum;
        CHECK_RC(m_SMBPBI.GetBoardPartNumber(&boardPartNum));
        Printf(printPri, "Board part number = \"%s\"\n", boardPartNum.c_str());
    }

    if (m_SMBPBI.GetCapOEMInfo())
    {
        string oemInfo;
        CHECK_RC(m_SMBPBI.GetOEMInfo(&oemInfo));
        Printf(printPri, "OEM string = \"%s\"\n", oemInfo.c_str());
    }

    if (m_SMBPBI.GetCapMarketingName())
    {
        string marketName;
        CHECK_RC(m_SMBPBI.GetMarketingName(&marketName));
        Printf(printPri, "Marketing name = \"%s\"\n", marketName.c_str());
    }

    {
        string gpuPartNumber;
        CHECK_RC(m_SMBPBI.GetGPUPartNumber(&gpuPartNumber));
        Printf(printPri, "GPU part number = \"%s\"\n", gpuPartNumber.c_str());
    }

    if (m_SMBPBI.GetCapMemVendor())
    {
        string memVendor;
        CHECK_RC(m_SMBPBI.GetMemoryVendor(&memVendor));
        Printf(printPri, "Memory vendor = \"%s\"\n", memVendor.c_str());
    }

    if (m_SMBPBI.GetCapMemPartNum())
    {
        string memPartNum;
        CHECK_RC(m_SMBPBI.GetMemoryPartNumber(&memPartNum));
        Printf(printPri, "Memory part number = \"%s\"\n", memPartNum.c_str());
    }

    if (m_SMBPBI.GetCapBuildDate())
    {
        string buildDate;
        CHECK_RC(m_SMBPBI.GetBuildDate(&buildDate));
        Printf(
            printPri,
            "Build date = %02x%02x/%02x/%02x\n",
            buildDate[3],
            buildDate[2],
            buildDate[1],
            buildDate[0]
        );
    }

    if (m_SMBPBI.GetCapFirmwareVersion())
    {
        string firmware;
        CHECK_RC(m_SMBPBI.GetFirmwareVersion(&firmware));
        Printf(printPri, "Firmware version = \"%s\"\n", firmware.c_str());
    }

    if (m_SMBPBI.GetCapVID())
    {
        UINT16 vid;
        CHECK_RC(m_SMBPBI.GetVID(&vid));
        Printf(printPri, "VID = 0x%04X\n", static_cast<int>(vid));
    }

    if (m_SMBPBI.GetCapDID())
    {
        UINT16 did;
        CHECK_RC(m_SMBPBI.GetDID(&did));
        Printf(printPri, "DID = 0x%04X\n", static_cast<int>(did));
    }

    if (m_SMBPBI.GetCapSSVID())
    {
        UINT16 ssvid;
        CHECK_RC(m_SMBPBI.GetSSVID(&ssvid));
        Printf(printPri, "SSVID = 0x%04X\n", static_cast<int>(ssvid));
    }

    if (m_SMBPBI.GetCapSSDID())
    {
        UINT16 ssdid;
        CHECK_RC(m_SMBPBI.GetSSDID(&ssdid));
        Printf(printPri, "SSDID = 0x%04X\n", static_cast<int>(ssdid));
    }

    if (m_SMBPBI.GetCapECCStatV2() || m_SMBPBI.GetCapECCStatV3())
    {
        Printf(printPri, "L1 Cache Error Counts:\n");
        Printf(printPri,
                "  GPC   TPC                    SBE                    DBE\n"
                "  -------------------------------------------------------\n");
        const UINT32 gpcs = gpuSubdevice->GetGpcCount();
        for (UINT32 gpc = 0; gpcs > gpc; ++gpc)
        {
            const UINT32 tpcs = gpuSubdevice->GetTpcCountOnGpc(gpc);
            for (UINT32 tpc = 0; tpcs > tpc; ++tpc)
            {
                UINT32 sbe;
                UINT32 dbe;
                CHECK_RC(m_SMBPBI.GetEccL1SBErrors(gpc, tpc, &sbe));
                CHECK_RC(m_SMBPBI.GetEccL1DBErrors(gpc, tpc, &dbe));
                Printf(
                    printPri,
                    "  %3d   %3d   %20d   %20d\n",
                    static_cast<int>(gpc),
                    static_cast<int>(tpc),
                    static_cast<int>(sbe),
                    static_cast<int>(dbe));
            }
        }
        Printf(printPri, "Register File Error Counts:\n");
        Printf(printPri,
                "  GPC   TPC                    SBE                    DBE\n"
                "  -------------------------------------------------------\n");
        for (UINT32 gpc = 0; gpcs > gpc; ++gpc)
        {
            const UINT32 tpcs = gpuSubdevice->GetTpcCountOnGpc(gpc);
            for (UINT32 tpc = 0; tpcs > tpc; ++tpc)
            {
                UINT32 sbe;
                UINT32 dbe;
                CHECK_RC(m_SMBPBI.GetEccSMRegFileSBErrors(gpc, tpc, &sbe));
                CHECK_RC(m_SMBPBI.GetEccSMRegFileDBErrors(gpc, tpc, &dbe));
                Printf(
                    printPri,
                    "  %3d   %3d   %20d   %20d\n",
                    static_cast<int>(gpc),
                    static_cast<int>(tpc),
                    static_cast<int>(sbe),
                    static_cast<int>(dbe));
            }
        }
        if (m_SMBPBI.GetCapECCStatV3())
        {
            Printf(printPri, "Texture Cache Retry Counts:\n");
            Printf(printPri,
                    "  GPC   TPC                    SBE                    DBE\n"
                    "  -------------------------------------------------------\n");
            for (UINT32 gpc = 0; gpcs > gpc; ++gpc)
            {
                const UINT32 tpcs = gpuSubdevice->GetTpcCountOnGpc(gpc);
                for (UINT32 tpc = 0; tpcs > tpc; ++tpc)
                {
                    UINT32 sbe;
                    UINT32 dbe;
                    CHECK_RC(m_SMBPBI.GetEccTexUnitSBErrors(gpc, tpc, &sbe));
                    CHECK_RC(m_SMBPBI.GetEccTexUnitDBErrors(gpc, tpc, &dbe));
                    Printf(
                        printPri,
                        "  %3d   %3d   %20d   %20d\n",
                        static_cast<int>(gpc),
                        static_cast<int>(tpc),
                        static_cast<int>(sbe),
                        static_cast<int>(dbe));
                }
            }
        }

        const UINT32 numPartitions = gpuSubdevice->GetFrameBufferUnitCount();
        const UINT32 numSubpartitions = gpuSubdevice->GetFB()->GetSubpartitions();
        Printf(printPri, "FB Error Counts:\n");
        Printf(printPri,
                "  Part   Subpartition             SBE                    DBE\n"
                "  ----------------------------------------------------------\n");
        for (UINT32 i = 0; numPartitions > i; ++i)
        {
            for (UINT32 j = 0; numSubpartitions > j; ++j)
            {
                UINT32 sbe;
                UINT32 dbe;
                CHECK_RC(m_SMBPBI.GetEccFrameBufSBErrors(i, j, &sbe));
                CHECK_RC(m_SMBPBI.GetEccFrameBufDBErrors(i, j, &dbe));
                Printf(
                    printPri,
                    "  %4d   %5d   %20d   %20d\n",
                    static_cast<int>(i),
                    static_cast<int>(j),
                    static_cast<int>(sbe),
                    static_cast<int>(dbe));
            }
        }

        Printf(printPri, "L2 Cache Error Counts:\n");
        Printf(printPri,
                "  Part   Slice                    SBE                    DBE\n"
                "  ----------------------------------------------------------\n");
        for (UINT32 i = 0; numPartitions > i; ++i)
        {
            const UINT32 numSlices = gpuSubdevice->GetFB()->GetL2SlicesPerFbp(i);
            for (UINT32 j = 0; numSlices > j; ++j)
            {
                UINT32 sbe;
                UINT32 dbe;
                CHECK_RC(m_SMBPBI.GetEccFrameBufSBErrors(i, j, &sbe));
                CHECK_RC(m_SMBPBI.GetEccFrameBufDBErrors(i, j, &dbe));
                Printf(
                    printPri,
                    "  %4d   %5d   %20d   %20d\n",
                    static_cast<int>(i),
                    static_cast<int>(j),
                    static_cast<int>(sbe),
                    static_cast<int>(dbe));
            }
        }
    }

    if (0 < m_SMBPBI.GetCapScratchSpaceSize())
    {
        Random rnd;
        rnd.SeedRandom(0x08ca75beU);

        const UINT32 scratchNumWords = m_SMBPBI.GetCapScratchSpaceSize();

        vector<UINT32> scratchValues(scratchNumWords);
        for (UINT32 i = 0; scratchValues.size() > i; ++i)
        {
            scratchValues[i] = rnd.GetRandom();
            CHECK_RC(m_SMBPBI.WriteScratchMemory(i, scratchValues[i]));
        }
        for (UINT32 i = 0; scratchValues.size() > i; ++i)
        {
            UINT32 v;
            CHECK_RC(m_SMBPBI.ReadScratchMemory(i, &v));
            if (v != scratchValues[i])
            {
                Printf(
                    Tee::PriHigh,
                    "SMBPBI error: scratch memory miscompare\n");
                return RC::BAD_MEMORY;
            }
        }
    }

    if (m_VerifySku)
    {
        string boardName;
        UINT32 index;
        const BoardDB & boards = BoardDB::Get();

        // Get board entry from boards.db if present
        CHECK_RC(gpuSubdevice->GetBoardInfoFromBoardDB(&boardName, &index, false));
        const BoardDB::BoardEntry *pGoldEntry = boards.GetBoardEntry(gpuSubdevice->DeviceId(), boardName, index);

        // Long explanation so that users understand the purpose of the test
        Printf(printPri, "\n"
                         "Comparing board info from SMBPBI with info from boards.db\n"
                         "Test passes if all SMBPBI and boards.db fields match (or either field is empty)\n"
                         "This test should only be used for basic verification of SMBus and SMBPBI\n"
                         "Use test 17 or 217 for full SKU verification\n"
                         "\n");
        if (pGoldEntry)
        {
            StickyRC firstRc;

            Printf(printPri, "Found boards.db entry: %s[%d]\n", boardName.c_str(), index);
            Printf(printPri, "-----------------------------------\n"
                                   "Field              SMBPBI boards.db\n");
            vector<string> fields;
            if (m_SMBPBI.GetCapBoardPartNum())
            {
                // Fetch board info via SMBPBI
                string boardPartNumber;
                CHECK_RC(m_SMBPBI.GetBoardPartNumber(&boardPartNumber));
                if (boardPartNumber == "")
                {
                    Printf(Tee::PriWarn, "SMBPBI - field 'BoardPartNumber' empty\n");
                }
                fields.clear();
                Utility::Tokenizer(boardPartNumber, "-", &fields);

                fields.resize(4, "");
                //     boardTopLevel = fields[0]; (unused)
                string boardPCB      = fields[1];
                // First character of boardPCB represents type of board (production, etc.),
                // and is not used in the check
                if (boardPCB.length() > 0)
                {
                    boardPCB = boardPCB.substr(1, string::npos);
                }
                string boardSku      = fields[2];
                string boardRevision = fields[3];

                // Fetch board info from boards.db
                string dbBoardProjectAndSku = pGoldEntry->m_Signature.m_BoardProjectAndSku;
                if (dbBoardProjectAndSku == "")
                {
                    Printf(Tee::PriWarn, "boards.db - field 'BoardProjectAndSku' empty\n");
                }
                fields.clear();
                Utility::Tokenizer(dbBoardProjectAndSku, "-", &fields);

                fields.resize(3, "");
                string dbBoardProject   = fields[0];
                string dbBoardSku       = fields[1];
                string dbBoardRevision  = fields[2];

                // Compare
                firstRc = ValidateFields(boardPCB, dbBoardProject, "Board Project", printPri);
                firstRc = ValidateFields(boardSku, dbBoardSku, "Board SKU", printPri);
                firstRc = ValidateFields(boardRevision, dbBoardRevision, "Board Revision", printPri);
            }
            {
                // Fetch GPU info via SMBPBI
                string gpuPartNumber;
                CHECK_RC(m_SMBPBI.GetGPUPartNumber(&gpuPartNumber));
                if (gpuPartNumber == "")
                {
                    Printf(Tee::PriError, "SMBPBI - field 'GPUPartNumber' empty\n");
                    firstRc = RC::UNEXPECTED_RESULT;
                }
                fields.clear();
                Utility::Tokenizer(gpuPartNumber, "-", &fields);

                fields.resize(3, "");
                //     gpuDeviceId = fields[0]; (unused)
                string gpuSku      = fields[1];
                string gpuRevision = fields[2];

                //
                // Fetch Chip SKU from boards.db
                //
                // There can be multiple chip SKUs per board, we iterate through the SKUs
                // until we find one that matches.
                // If no SKU matches, we fail on the SKU that was read last.
                //
                vector<string> dbChipSkuVec = pGoldEntry->m_OtherInfo.m_OfficialChipSKUs;
                if (dbChipSkuVec.size() == 0)
                {
                    Printf(Tee::PriError, "boards.db - field 'OfficialChipSku' empty\n");
                    firstRc = RC::UNEXPECTED_RESULT;
                }

                string dbOfficialChipSku;
                string dbChipName;
                string dbGpuSku;
                string dbGpuRevision;
                for (size_t i = 0; i < dbChipSkuVec.size(); i++)
                {
                    dbOfficialChipSku = dbChipSkuVec[i];
                    fields.clear();
                    Utility::Tokenizer(dbOfficialChipSku, "-", &fields);

                    fields.resize(3, "");
                    dbChipName    = fields[0];
                    dbGpuSku      = fields[1];
                    dbGpuRevision = fields[2];

                    // If we found the SKU, stop searching
                    if (OK == ValidateFields(gpuSku, dbGpuSku) &&
                        OK == ValidateFields(gpuRevision, dbGpuRevision))
                    {
                        break;
                    }
                }
                // Compare using the last read SKU
                firstRc = ValidateFields(gpuSku, dbGpuSku, "GPU SKU", printPri);
                firstRc = ValidateFields(gpuRevision, dbGpuRevision, "GPU Revision", printPri);
            }
            if (m_SMBPBI.GetCapDID())
            {
                // Fetch PCI Device ID from SMBPBI
                UINT16 deviceId;
                CHECK_RC(m_SMBPBI.GetDID(&deviceId));
                string gpuDeviceId = Utility::StrPrintf("0x%04X", deviceId);

                // Fetch Device ID from boards.db
                string dbDeviceId = Utility::StrPrintf("0x%04X", pGoldEntry->m_Signature.m_PciDevId);

                // Compare
                firstRc = ValidateFields(gpuDeviceId, dbDeviceId, "GPU PciDevId", printPri);
            }
            if (m_SMBPBI.GetCapSSVID())
            {
                // Fetch SSVID from SMBPBI
                UINT16 ssvid;
                CHECK_RC(m_SMBPBI.GetSSVID(&ssvid));
                string boardSSVID = Utility::StrPrintf("0x%04X", ssvid);

                // Fetch SSVID from boards.db
                string dbSSVID = Utility::StrPrintf("0x%04X", pGoldEntry->m_Signature.m_SVID);

                // Compare
                firstRc = ValidateFields(boardSSVID, dbSSVID, "SSVID", printPri);
            }
            if (m_SMBPBI.GetCapSSDID())
            {
                // Fetch SSDID from SMBPBI
                UINT16 ssdid;
                CHECK_RC(m_SMBPBI.GetSSDID(&ssdid));
                string boardSSDID = Utility::StrPrintf("0x%04X", ssdid);

                // Fetch SSDID from boards.db
                string dbSSDID = Utility::StrPrintf("0x%04X", pGoldEntry->m_Signature.m_SSID);

                // Compare
                firstRc = ValidateFields(boardSSDID, dbSSDID, "SSDID", printPri);
            }
            Printf(printPri, "-----------------------------------\n");
            rc = firstRc;
        }
        else
        {
            Printf(printPri,
                "BOARD MATCH NOT FOUND\n"
                "The probable cause of this is that you have an outdated\n"
                "boards.db file.\n");
            rc = RC::UNEXPECTED_RESULT;
        }
        Printf(printPri, "\n");
    }

    return rc;
}

RC SMBPBITest::Cleanup()
{
    RC rc;

    CHECK_RC(m_SMBPBI.Cleanup());

    CHECK_RC(GpuTest::Cleanup());

    return rc;
}

bool SMBPBITest::IsSupported()
{
    return true;
}

//! \brief   Checks whether the SMBPBI and boards.db board info contradict one another
//!
//! Oftentimes boards.db or SMBPBI will return empty fields. We don't want to fail
//! in such common cases, so we return OK if either field is empty.
//!
//! \returns OK if either string is empty or the strings compare correctly
//!          RC::UNEXPECTED_RESULT if the both strings are not empty and miscompare
RC SMBPBITest::ValidateFields(string smbpbiStr, string dbStr)
{
    if (smbpbiStr == "" || dbStr == "")
    {
        return OK;
    }

    if (smbpbiStr == dbStr)
    {
        return OK;
    }
    else
    {
        return RC::UNEXPECTED_RESULT;
    }
}

//! \brief   Checks whether the SMBPBI and boards.db board info contradict one another,
//!          then prints the argument 'desc' followed by the contents of the two strings
//!
//! Oftentimes boards.db or SMBPBI will return empty fields. We don't want to fail
//! in such common cases, so we return OK if either field is empty.
//!
//! \returns OK if either string is empty or the strings compare correctly
//!          RC::UNEXPECTED_RESULT if the both strings are not empty and miscompare
RC SMBPBITest::ValidateFields(string smbpbiStr, string dbStr, string desc, Tee::Priority printPri)
{
    RC rc = ValidateFields(smbpbiStr, dbStr);
    if (rc == OK)
    {
        Printf(printPri, "%-15s%10s%10s\n", desc.c_str(), smbpbiStr.c_str(), dbStr.c_str());
    }
    else
    {
        Printf(Tee::PriError, "%s - %s (SMBPBI) doesn't match %s (boards.db)\n", desc.c_str(), smbpbiStr.c_str(), dbStr.c_str());
    }
    return rc;
}

