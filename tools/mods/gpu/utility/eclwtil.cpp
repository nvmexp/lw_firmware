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

#include "gpu/utility/eclwtil.h"

#include "core/include/script.h"
#include "core/include/massert.h"
#include "core/include/tee.h"
#include "gpu/include/gpusbdev.h"
#include <numeric>

namespace
{
    using namespace Ecc;

    //!
    //! \brief Map MODS ECC units to RM's LW2080_ECC_UNITS.
    //!
    static const std::map<Unit, LW2080_ECC_UNITS> s_EclwnitToLw2080Eclwnit =
    {
        { Unit::LRF,             ECC_UNIT_LRF             },
        { Unit::CBU,             ECC_UNIT_CBU             },
        { Unit::L1,              ECC_UNIT_L1              },
        { Unit::L1DATA,          ECC_UNIT_L1DATA          },
        { Unit::L1TAG,           ECC_UNIT_L1TAG           },
        { Unit::SHM,             ECC_UNIT_SHM             },
        { Unit::TEX,             ECC_UNIT_TEX             },
        { Unit::L2,              ECC_UNIT_LTC             },
        { Unit::DRAM,            ECC_UNIT_DRAM            },
        { Unit::SM_ICACHE,       ECC_UNIT_SM_ICACHE       },
        { Unit::GCC_L15,         ECC_UNIT_GCC_L15         },
        { Unit::GPCMMU,          ECC_UNIT_GPCMMU          },
        { Unit::HUBMMU_L2TLB,    ECC_UNIT_HUBMMU_L2TLB    },
        { Unit::HUBMMU_HUBTLB,   ECC_UNIT_HUBMMU_HUBTLB   },
        { Unit::HUBMMU_FILLUNIT, ECC_UNIT_HUBMMU_FILLUNIT },
        { Unit::GPCCS,           ECC_UNIT_GPCCS           },
        { Unit::FECS,            ECC_UNIT_FECS            },
        { Unit::PMU,             ECC_UNIT_PMU             },
        { Unit::SM_RAMS,         ECC_UNIT_SM_RAMS         },
        { Unit::HSHUB,           ECC_UNIT_HSHUB           },
        { Unit::PCIE_REORDER,    ECC_UNIT_PCIE_REORDER    },
        { Unit::PCIE_P2PREQ,     ECC_UNIT_PCIE_P2PREQ     }
    };

    //!
    //! \brief Maps MOD ECC units to RM's LW2080_CTRL_GPU_ECC_UNIT_*
    //! values.
    //!
    static const std::map<Unit, RmGpuEclwnit> s_EclwnitToRmGpuEclwnit =
    {
        { Unit::LRF,             LW2080_CTRL_GPU_ECC_UNIT_SM              },
        { Unit::CBU,             LW2080_CTRL_GPU_ECC_UNIT_SM_CBU          },
        { Unit::L1,              LW2080_CTRL_GPU_ECC_UNIT_L1              },
        { Unit::L1DATA,          LW2080_CTRL_GPU_ECC_UNIT_SM_L1_DATA      },
        { Unit::L1TAG,           LW2080_CTRL_GPU_ECC_UNIT_SM_L1_TAG       },
        { Unit::SHM,             LW2080_CTRL_GPU_ECC_UNIT_SHM             },
        { Unit::TEX,             LW2080_CTRL_GPU_ECC_UNIT_TEX             },
        { Unit::L2,              LW2080_CTRL_GPU_ECC_UNIT_L2              },
        { Unit::DRAM,            LW2080_CTRL_GPU_ECC_UNIT_FBPA            },
        { Unit::SM_ICACHE,       LW2080_CTRL_GPU_ECC_UNIT_SM_ICACHE       },
        { Unit::GCC_L15,         LW2080_CTRL_GPU_ECC_UNIT_GCC             },
        { Unit::GPCMMU,          LW2080_CTRL_GPU_ECC_UNIT_GPCMMU          },
        { Unit::HUBMMU_L2TLB,    LW2080_CTRL_GPU_ECC_UNIT_HUBMMU_L2TLB    },
        { Unit::HUBMMU_HUBTLB,   LW2080_CTRL_GPU_ECC_UNIT_HUBMMU_HUBTLB   },
        { Unit::HUBMMU_FILLUNIT, LW2080_CTRL_GPU_ECC_UNIT_HUBMMU_FILLUNIT },
        { Unit::GPCCS,           LW2080_CTRL_GPU_ECC_UNIT_GPCCS           },
        { Unit::FECS,            LW2080_CTRL_GPU_ECC_UNIT_FECS            },
        { Unit::PMU,             LW2080_CTRL_GPU_ECC_UNIT_PMU             },
        { Unit::SM_RAMS,         LW2080_CTRL_GPU_ECC_UNIT_SM_RAMS         },
        { Unit::HSHUB,           LW2080_CTRL_GPU_ECC_UNIT_HSHUB           },
        { Unit::PCIE_REORDER,    LW2080_CTRL_GPU_ECC_UNIT_PCIE_REORDER    },
        { Unit::PCIE_P2PREQ,     LW2080_CTRL_GPU_ECC_UNIT_PCIE_P2PREQ     }
    };

    //!
    //! \brief Maps RM's LW2080_CTRL_GPU_ECC_UNIT_* to MOD ECC units
    //!
    static const std::map<RmGpuEclwnit, Unit> s_RmGpuEclwnitToEclwnit =
    {
        { LW2080_CTRL_GPU_ECC_UNIT_SM,              Unit::LRF             },
        { LW2080_CTRL_GPU_ECC_UNIT_SM_CBU,          Unit::CBU             },
        { LW2080_CTRL_GPU_ECC_UNIT_L1,              Unit::L1              },
        { LW2080_CTRL_GPU_ECC_UNIT_SM_L1_DATA,      Unit::L1DATA          },
        { LW2080_CTRL_GPU_ECC_UNIT_SM_L1_TAG,       Unit::L1TAG           },
        { LW2080_CTRL_GPU_ECC_UNIT_SHM,             Unit::SHM             },
        { LW2080_CTRL_GPU_ECC_UNIT_TEX,             Unit::TEX             },
        { LW2080_CTRL_GPU_ECC_UNIT_L2,              Unit::L2              },
        { LW2080_CTRL_GPU_ECC_UNIT_FBPA,            Unit::DRAM            },
        { LW2080_CTRL_GPU_ECC_UNIT_SM_ICACHE,       Unit::SM_ICACHE       },
        { LW2080_CTRL_GPU_ECC_UNIT_GCC,             Unit::GCC_L15         },
        { LW2080_CTRL_GPU_ECC_UNIT_GPCMMU,          Unit::GPCMMU          },
        { LW2080_CTRL_GPU_ECC_UNIT_HUBMMU_L2TLB,    Unit::HUBMMU_L2TLB    },
        { LW2080_CTRL_GPU_ECC_UNIT_HUBMMU_HUBTLB,   Unit::HUBMMU_HUBTLB   },
        { LW2080_CTRL_GPU_ECC_UNIT_HUBMMU_FILLUNIT, Unit::HUBMMU_FILLUNIT },
        { LW2080_CTRL_GPU_ECC_UNIT_GPCCS,           Unit::GPCCS           },
        { LW2080_CTRL_GPU_ECC_UNIT_FECS,            Unit::FECS            },
        { LW2080_CTRL_GPU_ECC_UNIT_PMU,             Unit::PMU             },
        { LW2080_CTRL_GPU_ECC_UNIT_SM_RAMS,         Unit::SM_RAMS         },
        { LW2080_CTRL_GPU_ECC_UNIT_HSHUB,           Unit::HSHUB           },
        { LW2080_CTRL_GPU_ECC_UNIT_PCIE_REORDER,    Unit::PCIE_REORDER    },
        { LW2080_CTRL_GPU_ECC_UNIT_PCIE_P2PREQ,     Unit::PCIE_P2PREQ     }
    };

    //!
    //! \brief Maps RM's ECC_UNIT_* from ctrl2080ecc.h to MOD ECC units
    //!
    static const std::map<RmGpuEclwnit, Unit> s_RmEclwnitToEclwnit =
    {
        { ECC_UNIT_LRF,             Unit::LRF             },
        { ECC_UNIT_CBU,             Unit::CBU             },
        { ECC_UNIT_L1,              Unit::L1              },
        { ECC_UNIT_L1DATA,          Unit::L1DATA          },
        { ECC_UNIT_L1TAG,           Unit::L1TAG           },
        { ECC_UNIT_SHM,             Unit::SHM             },
        { ECC_UNIT_TEX,             Unit::TEX             },
        { ECC_UNIT_LTC,             Unit::L2              },
        { ECC_UNIT_DRAM,            Unit::DRAM            },
        { ECC_UNIT_SM_ICACHE,       Unit::SM_ICACHE       },
        { ECC_UNIT_GCC_L15,         Unit::GCC_L15         },
        { ECC_UNIT_GPCMMU,          Unit::GPCMMU          },
        { ECC_UNIT_HUBMMU_L2TLB,    Unit::HUBMMU_L2TLB    },
        { ECC_UNIT_HUBMMU_HUBTLB,   Unit::HUBMMU_HUBTLB   },
        { ECC_UNIT_HUBMMU_FILLUNIT, Unit::HUBMMU_FILLUNIT },
        { ECC_UNIT_GPCCS,           Unit::GPCCS           },
        { ECC_UNIT_FECS,            Unit::FECS            },
        { ECC_UNIT_PMU,             Unit::PMU             },
        { ECC_UNIT_SM_RAMS,         Unit::SM_RAMS         },
        { ECC_UNIT_HSHUB,           Unit::HSHUB           },
        { ECC_UNIT_PCIE_REORDER,    Unit::PCIE_REORDER    },
        { ECC_UNIT_PCIE_P2PREQ,     Unit::PCIE_P2PREQ     }
    };

    //!
    //! \brief Maps MODS ECC units to RM's
    //! LW2080_CTRL_GR_ECC_INJECT_ERROR_UNIT_* values.
    //!
    //! This map only contains RM supported HW injectable ECC units. This will
    //! be a subset of the units listed in Ecc::Unit and a subset of RM's
    //! LW2080_CTRL_GR_ECC_INJECT_ERROR_UNIT_* values.
    //!
    static const std::map<Unit, RmGrInjectLoc> s_EclwnitToRmGrInjectLoc =
    {
        { Unit::LRF,       DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_UNIT, _LRF)     },
        { Unit::CBU,       DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_UNIT, _CBU)     },
        { Unit::L1DATA,    DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_UNIT, _L1DATA)  },
        { Unit::L1TAG,     DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_UNIT, _L1TAG)   },
        { Unit::SHM,       DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_UNIT, _SHM)     },
        { Unit::SM_ICACHE, DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_UNIT, _ICACHE)  },
        { Unit::GCC_L15,   DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_UNIT, _GCC_L15) },
        { Unit::GPCMMU,    DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_UNIT, _GPCMMU)  },
        { Unit::SM_RAMS,   DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_UNIT, _SM_RAMS) }
    };

    //!
    //! \brief Maps ErrorType to the RM control call Gr injection types listed as
    //! LW208F_CTRL_CMD_GR_ECC_INJECT_ERROR_TYPE_*.
    //!
    //! NOTE: RM API shows the types of Gr injectable errors as SBE/DBE, but this
    //! actually maps to CORR/UNCORR.
    //!
    //! NOTE: Injecting a correctable error into a parity unit is a nop. See
    //! section 5.1.6.1 of the Volta/Turing ECC FD.
    //!   Ref: https://p4viewer/get/hw/doc/gpu/volta/volta/design/Functional_Descriptions/Resiliency/Volta_gpu_resiliency_ECC.doc
    //!
    static const std::map<ErrorType, RmGrInjectType> s_ErrorTypeToRmGrInjectType =
    {
        { ErrorType::CORR,   DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_TYPE, _SBE) },
        { ErrorType::UNCORR, DRF_DEF(208F, _CTRL_GR, _ECC_INJECT_ERROR_TYPE, _DBE) }
    };

    //!
    //! \brief Maps MODS ECC units to RM's
    //! LW2080_CTRL_MMU_ECC_INJECT_ERROR_UNIT_* values.
    //!
    //! This map only contains RM supported HW injectable ECC units. This will
    //! be a subset of the units listed in Ecc::Unit and a subset of RM's
    //! LW2080_CTRL_MMU_ECC_INJECT_ERROR_UNIT_* values.
    //!
    static const std::map<Unit, RmMmuInjectLoc> s_EclwnitToRmMmuInjectUnit =
    {
        { Unit::HUBMMU_L2TLB,    DRF_DEF(208F, _CTRL_MMU, _ECC_INJECT_ERROR_UNIT, _L2TLB)    },
        { Unit::HUBMMU_HUBTLB,   DRF_DEF(208F, _CTRL_MMU, _ECC_INJECT_ERROR_UNIT, _HUBTLB)   },
        { Unit::HUBMMU_FILLUNIT, DRF_DEF(208F, _CTRL_MMU, _ECC_INJECT_ERROR_UNIT, _FILLUNIT) },
        { Unit::HSHUB,           DRF_DEF(208F, _CTRL_MMU, _ECC_INJECT_ERROR_UNIT, _HSHUB)    }
    };

    //!
    //! \brief Maps MODS ECC units to RM's
    //! LW2080_CTRL_FB_ECC_INJECT_ERROR_LOC_* values.
    //!
    //! This map only contains RM supported HW injectable ECC units. This will
    //! be a subset of the units listed in Ecc::Unit and a subset of RM's
    //! LW2080_CTRL_FB_ECC_INJECT_ERROR_LOC_* values.
    //!
    static const std::map<Unit, RmMmuInjectLoc> s_EclwnitToRmFbInjectLoc =
    {
        { Unit::L2,    DRF_DEF(208F, _CTRL_FB, _ECC_INJECTION_SUPPORTED_LOC, _LTC)    }
    };

    //!
    //! \brief Maps MODS ECC units to RM's
    //! LW208F_CTRL_BUS_UNIT_TYPE_* values.
    //!
    //! This map only contains RM supported HW injectable ECC units. This will
    //! be a subset of the units listed in Ecc::Unit and a subset of RM's
    //! LW208F_CTRL_BUS_UNIT_TYPE_* values.
    //!
    static const std::map<Unit, LW208F_CTRL_BUS_UNIT_TYPE> s_EclwnitToRmBusInjectUnit =
    {
        { Unit::PCIE_REORDER,    LW208F_CTRL_BUS_PCIE_REORDER_BUFFER },
        { Unit::PCIE_P2PREQ,     LW208F_CTRL_BUS_PCIE_P2PREQ_BUFFER  }
    };


    //!
    //! \brief Maps MODS ECC units to RM's
    //! LW2080_CTRL_MMU_ECC_INJECT_ERROR_TYPE_* values.
    //!
    //! This map only contains RM supported HW injectable ECC units. This will
    //! be a subset of the units listed in Ecc::Unit and a subset of RM's
    //! LW2080_CTRL_MMU_ECC_INJECT_ERROR_TYPE_* values.
    //!
    static const std::map<ErrorType, RmMmuInjectType> s_ErrorTypeToRmMmuInjectType =
    {
        { ErrorType::CORR,   DRF_DEF(208F, _CTRL_MMU, _ECC_INJECT_ERROR_TYPE, _CORRECTABLE)   },
        { ErrorType::UNCORR, DRF_DEF(208F, _CTRL_MMU, _ECC_INJECT_ERROR_TYPE, _UNCORRECTABLE) }
    };

    //!
    //! \brief Maps ErrorType to the RM control call FB injection types
    //! (LW208F_CTRL_FB_ERROR_TYPE).
    //!
    //! NOTE: Injecting a correctable error into a parity unit is a nop. See
    //! section 5.1.6.1 of the Volta/Turing ECC FD.
    //!   Ref: https://p4viewer/get/hw/doc/gpu/volta/volta/design/Functional_Descriptions/Resiliency/Volta_gpu_resiliency_ECC.doc
    //!
    static const std::map<ErrorType, LW208F_CTRL_FB_ERROR_TYPE> s_ErrorTypeToRmFbInjectType =
    {

        { ErrorType::CORR,   LW208F_CTRL_FB_ERROR_TYPE_CORRECTABLE   },
        { ErrorType::UNCORR, LW208F_CTRL_FB_ERROR_TYPE_UNCORRECTABLE }
    };

    //!
    //! \brief Maps ErrorType to the RM control call Bus injection types
    //! (LW208F_CTRL_BUS_ERROR_TYPE).
    //!
    static const std::map<ErrorType, LW208F_CTRL_BUS_ERROR_TYPE> s_ErrorTypeToRmBusInjectType =
    {

        { ErrorType::CORR,   LW208F_CTRL_BUS_ERROR_TYPE_CORRECTABLE   },
        { ErrorType::UNCORR, LW208F_CTRL_BUS_ERROR_TYPE_UNCORRECTABLE }
    };

};

namespace Ecc
{
    ErrCounts ErrCounts::operator+(const ErrCounts& o) const
    {
        ErrCounts counts = *this;
        counts += o;
        return counts;
    }

    ErrCounts& ErrCounts::operator+=(const ErrCounts& o)
    {
        this->corr += o.corr;
        this->uncorr += o.uncorr;
        return *this;
    }

    ErrCounts ErrCounts::operator-(const ErrCounts& o) const
    {
        ErrCounts counts = *this;
        counts -= o;
        return counts;
    }

    ErrCounts& ErrCounts::operator-=(const ErrCounts& o)
    {
        this->corr -= o.corr;
        this->uncorr -= o.uncorr;
        return *this;
    }

    //--------------------------------------------------------------------------

    const char* ToString(Unit eclwnit)
    {
        switch (eclwnit)
        {
            case Unit::LRF:
                return "LRF";
            case Unit::CBU:
                return "CBU";
            case Unit::L1:
                return "L1";
            case Unit::L1DATA:
                return "L1DATA";
            case Unit::L1TAG:
                return "L1TAG";
            case Unit::SHM:
                return "SHM";
            case Unit::TEX:
                return "TEX";
            case Unit::L2:
                return "L2";
            case Unit::DRAM:
                return "FB";
            case Unit::SM_ICACHE:
                return "SM_ICACHE";
            case Unit::GCC_L15:
                return "GCC_L15";
            case Unit::GPCMMU:
                return "GPCMMU";
            case Unit::HUBMMU_L2TLB:
                return "HUBMMU_L2TLB";
            case Unit::HUBMMU_HUBTLB:
                return "HUBMMU_HUBTLB";
            case Unit::HUBMMU_FILLUNIT:
                return "HUBMMU_FILLUNIT";
            case Unit::GPCCS:
                return "GPCCS";
            case Unit::FECS:
                return "FECS";
            case Unit::PMU:
                return "PMU";
            case Unit::SM_RAMS:
                return "SM_RAMS";
            case Unit::HSHUB:
                return "HSHUB";
            case Unit::PCIE_REORDER:
                return "PCIE_REORDER";
            case Unit::PCIE_P2PREQ:
                return "PCIE_P2PREQ";
            default:
                Printf(Tee::PriError, "Unknown ECC unit (LW2080_ECC_UNITS %u)\n",
                       static_cast<UINT32>(eclwnit));
                MASSERT(!"Unknown ECC unit");
                return "unknown";
        }
    }

    const char* ToString(ErrorType errType)
    {
        switch (errType)
        {
            case ErrorType::CORR:
                return "CORR";
            case ErrorType::UNCORR:
                return "UNCORR";
            default:
                Printf(Tee::PriError, "Unknown ECC error type (%u)\n",
                       static_cast<UINT32>(errType));
                MASSERT(!"Unknown ECC error type");
                return "unknown";
        }
    }

    const char* ToString(Source eccSource)
    {
        switch (eccSource)
        {
            case Source::UNDEFINED:
                return "UNDEFINED";
            case Source::DRAM:
                return "DRAM";
            case Source::GPU:
                return "GPU";
            case Source::DRAM_AND_GPU:
                return "DRAM AND GPU";
            default:
                Printf(Tee::PriError, "Unknown ECC source (%u)\n",
                       static_cast<UINT32>(eccSource));
                MASSERT(!"Unknown ECC source");
                return "unknown";
        }
    }

    const char* HsHubSublocationString(UINT32 sublocIdx)
    {
        switch (sublocIdx)
        {
            case LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_HSCE:
                return "HSCE";
            case LW208F_CTRL_MMU_ECC_INJECT_ERROR_HSHUB_SUBLOCATION_LINK:
                return "Link";
            default:
                MASSERT(!"Unknown HSHUB ECC sublocation\n");
                break;
        }
        return "unknown";
    }

    //--------------------------------------------------------------------------

    RmGpuEclwnit ToRmGpuEclwnit(Unit eclwnit)
    {
        auto gpuEclwnitIter = s_EclwnitToRmGpuEclwnit.find(eclwnit);
        MASSERT(gpuEclwnitIter != s_EclwnitToRmGpuEclwnit.end());
        return gpuEclwnitIter->second;
    }

    UINT32 ToRmNotifierEvent(ErrorType errorType)
    {
        switch (errorType)
        {
            case ErrorType::CORR:
                return LW2080_NOTIFIERS_ECC_CORR;
            case ErrorType::UNCORR:
                return LW2080_NOTIFIERS_ECC_UNCORR;
            default:
                return LW2080_NOTIFIERS_MAXCOUNT; // Return invalid notifier index
        }
    }

    //--------------------------------------------------------------------------
    Unit ToEclwnitFromRmGpuEclwnit(RmGpuEclwnit rmGpuEclwnit)
    {
        auto eclwnitIter = s_RmGpuEclwnitToEclwnit.find(rmGpuEclwnit);
        MASSERT(eclwnitIter != s_RmGpuEclwnitToEclwnit.end());
        return eclwnitIter->second;
    }

    Unit ToEclwnitFromRmEclwnit(RmEclwnit rmEclwnit)
    {
        auto eclwnitIter = s_RmEclwnitToEclwnit.find(rmEclwnit);
        MASSERT(eclwnitIter != s_RmEclwnitToEclwnit.end());
        return eclwnitIter->second;
    }

    UINT32 ToEclwnitBitFromRmGpuEclwnit(RmGpuEclwnit rmGpuEclwnit)
    {
        return 1U << static_cast<UINT32>(ToEclwnitFromRmGpuEclwnit(rmGpuEclwnit));
    }

    bool IsUnitEnabled(Unit eclwnit, UINT32 unitBitMask)
    {
        using IntType = typename underlying_type<Unit>::type;
        return (unitBitMask & (1 << static_cast<IntType>(eclwnit)));
    }

    Unit GetNextUnit(Unit eclwnit)
    {
        using IntType = typename underlying_type<Unit>::type;
        return static_cast<Unit>(static_cast<IntType>(eclwnit) + 1);
    }

    UnitType GetUnitType(Unit eclwnit)
    {
        switch (eclwnit)
        {
            case Unit::LRF:
            case Unit::CBU:
            case Unit::L1:
            case Unit::L1DATA:
            case Unit::L1TAG:
            case Unit::SHM:
            case Unit::TEX:
            case Unit::SM_ICACHE:
            case Unit::GCC_L15:
            case Unit::GPCMMU:
            case Unit::HUBMMU_L2TLB:
            case Unit::HUBMMU_HUBTLB:
            case Unit::HUBMMU_FILLUNIT:
            case Unit::GPCCS:
            case Unit::FECS:
            case Unit::SM_RAMS:
                return UnitType::GPC;
            case Unit::L2:
                return UnitType::L2;
            case Unit::DRAM:
                return UnitType::FB;
            case Unit::PMU:
                return UnitType::FALCON;
            case Unit::HSHUB:
                return UnitType::HSHUB;
            case Unit::PCIE_REORDER:
            case Unit::PCIE_P2PREQ:
                return UnitType::PCIE;
            default:
                MASSERT(!"Unknown ECC Unit");
        }

        return UnitType::MAX;
    }
    //--------------------------------------------------------------------------

    bool IsInjectLocSupported(Unit injectLoc)
    {
        if (injectLoc == Ecc::Unit::L2) { return true; }

        return s_EclwnitToRmGrInjectLoc.count(injectLoc) != 0
            || s_EclwnitToRmMmuInjectUnit.count(injectLoc) != 0
            || s_EclwnitToRmBusInjectUnit.count(injectLoc) != 0;
    }

    bool IsAddressReportingSupported(Unit addressLoc)
    {
        switch (addressLoc)
        {
            case Unit::LRF:
            case Unit::CBU:
            case Unit::L1DATA:
            case Unit::L1TAG:
            case Unit::L2:
            case Unit::DRAM:
            case Unit::SM_ICACHE:
            case Unit::GCC_L15:
            case Unit::GPCMMU:
            case Unit::SM_RAMS:
            case Unit::HUBMMU_L2TLB:
            case Unit::HUBMMU_HUBTLB:
            case Unit::HUBMMU_FILLUNIT:
                return true;
            default:
                break;
        }
        return false;
    }

    RmGrInjectLoc ToRmGrInjectLoc(Unit eclwnit)
    {
        auto injectLocIter = s_EclwnitToRmGrInjectLoc.find(eclwnit);
        MASSERT(injectLocIter != s_EclwnitToRmGrInjectLoc.end());
        return injectLocIter->second;
    }

    //!
    //! \brief Return the injection type value used by RM in the LW208F
    //! injections API.
    //!
    //! The value is already shifted to the appropriate bit position.
    //!
    RmGrInjectType ToRmGrInjectType(ErrorType errorType)
    {
        auto injectTypeSearch = s_ErrorTypeToRmGrInjectType.find(errorType);
        MASSERT(injectTypeSearch != s_ErrorTypeToRmGrInjectType.end());
        return injectTypeSearch->second;
    }


    RmGrInjectLoc ToRmMmuInjectUnit(Unit eclwnit)
    {
        auto injectLocIter = s_EclwnitToRmMmuInjectUnit.find(eclwnit);
        MASSERT(injectLocIter != s_EclwnitToRmMmuInjectUnit.end());
        return injectLocIter->second;
    }

    //!
    //! \brief Return the injection type value used by RM in the LW208F
    //! injections API.
    //!
    //! The value is already shifted to the appropriate bit position.
    //!
    RmMmuInjectType ToRmMmuInjectType(ErrorType errorType)
    {
        auto injectTypeSearch = s_ErrorTypeToRmMmuInjectType.find(errorType);
        MASSERT(injectTypeSearch != s_ErrorTypeToRmMmuInjectType.end());
        return injectTypeSearch->second;
    }

    //!
    //! \brief Return the injection type value used by RM in the LW208F
    //! injections API.
    //!
    //! The value is already shifted to the appropriate bit position.
    //!
    LW208F_CTRL_FB_ERROR_TYPE ToRmFbInjectType(ErrorType errorType)
    {
        auto injectTypeSearch = s_ErrorTypeToRmFbInjectType.find(errorType);
        MASSERT(injectTypeSearch != s_ErrorTypeToRmFbInjectType.end());
        return injectTypeSearch->second;
    }

    RmFbInjectLoc ToRmFbInjectLoc(Unit eclwnit)
    {
        auto injectLocIter = s_EclwnitToRmFbInjectLoc.find(eclwnit);
        MASSERT(injectLocIter != s_EclwnitToRmFbInjectLoc.end());
        return injectLocIter->second;
    }


    //!
    //! \brief Return the injection type value used by RM in the LW208F
    //! injections API.
    //!
    //! The value is already shifted to the appropriate bit position.
    //!
    LW208F_CTRL_BUS_ERROR_TYPE ToRmBusInjectType(ErrorType errorType)
    {
        auto injectTypeSearch = s_ErrorTypeToRmBusInjectType.find(errorType);
        MASSERT(injectTypeSearch != s_ErrorTypeToRmBusInjectType.end());
        return injectTypeSearch->second;
    }

    LW208F_CTRL_BUS_UNIT_TYPE ToRmBusInjectUnit(Unit eclwnit)
    {
        auto injectLocIter = s_EclwnitToRmBusInjectUnit.find(eclwnit);
        MASSERT(injectLocIter != s_EclwnitToRmBusInjectUnit.end());
        return injectLocIter->second;
    }

    namespace
    {
        RC UnknownEccErrorType(Ecc::ErrorType errType)
        {
            Printf(Tee::PriError, "Unknown ECC error type (%u)\n",
                   static_cast<UINT32>(errType));
            MASSERT(!"Unknown ECC error type");
            return RC::SOFTWARE_ERROR;
        }
    };

    //!
    //! \brief Return an appropriate RC for the given ECC unit error.
    //!
    //! \param eclwnit ECC unit to check for errors. If value is
    //! Ecc::Unit::MAX, then the given error type will be checked
    //! across all ECC units. In this case a generic error will be returned.
    //! \param errType Error type to check.
    //!
    RC GetRC(Unit eclwnit, ErrorType errType)
    {
        // User wants a generic error
        if (eclwnit == Unit::MAX)
        {
            switch (errType)
            {
                case ErrorType::CORR:
                    return RC::ECC_CORRECTABLE_ERROR;
                case ErrorType::UNCORR:
                    return RC::ECC_UNCORRECTABLE_ERROR;
                default:
                    return UnknownEccErrorType(errType);
            }
        }

        switch (eclwnit)
        {
            case Unit::DRAM:
                switch (errType)
                {
                    case ErrorType::CORR:
                        return RC::FB_ECC_CORRECTABLE_ERROR;
                    case ErrorType::UNCORR:
                        return RC::FB_ECC_UNCORRECTABLE_ERROR;
                    default:
                        return UnknownEccErrorType(errType);
                }
                break;

            case Unit::L2:
                switch (errType)
                {
                    case ErrorType::CORR:
                        return RC::L2_ECC_CORRECTABLE_ERROR;
                    case ErrorType::UNCORR:
                        return RC::L2_ECC_UNCORRECTABLE_ERROR;
                    default:
                        return UnknownEccErrorType(errType);
                }
                break;

            case Unit::LRF:
            case Unit::CBU:
            case Unit::L1:
            case Unit::L1DATA:
            case Unit::L1TAG:
            case Unit::SHM:
            case Unit::TEX:
            case Unit::SM_ICACHE:
            case Unit::SM_RAMS:
                switch (errType)
                {
                    case ErrorType::CORR:
                        return RC::SM_ECC_CORRECTABLE_ERROR;
                    case ErrorType::UNCORR:
                        return RC::SM_ECC_UNCORRECTABLE_ERROR;
                    default:
                        return UnknownEccErrorType(errType);
                }
                break;

            case Unit::GCC_L15:
                switch (errType)
                {
                    case ErrorType::CORR:
                        return RC::GCC_ECC_CORRECTABLE_ERROR;
                    case ErrorType::UNCORR:
                        return RC::GCC_ECC_UNCORRECTABLE_ERROR;
                    default:
                        return UnknownEccErrorType(errType);
                }
                break;

            case Unit::GPCMMU:
                switch (errType)
                {
                    case ErrorType::CORR:
                        return RC::GPCMMU_ECC_CORRECTABLE_ERROR;
                    case ErrorType::UNCORR:
                        return RC::GPCMMU_ECC_UNCORRECTABLE_ERROR;
                    default:
                        return UnknownEccErrorType(errType);
                }
                break;

            case Unit::HUBMMU_L2TLB:
            case Unit::HUBMMU_HUBTLB:
            case Unit::HUBMMU_FILLUNIT:
                switch (errType)
                {
                    case ErrorType::CORR:
                        return RC::HUBMMU_ECC_CORRECTABLE_ERROR;
                    case ErrorType::UNCORR:
                        return RC::HUBMMU_ECC_UNCORRECTABLE_ERROR;
                    default:
                        return UnknownEccErrorType(errType);
                }
                break;

            case Unit::GPCCS:
            case Unit::FECS:
                switch (errType)
                {
                    case ErrorType::CORR:
                        return RC::CTXSW_ECC_CORRECTABLE_ERROR;
                    case ErrorType::UNCORR:
                        return RC::CTXSW_ECC_UNCORRECTABLE_ERROR;
                    default:
                        return UnknownEccErrorType(errType);
                }
                break;

            case Unit::PMU:
                switch (errType)
                {
                    case ErrorType::CORR:
                        return RC::FALCON_ECC_CORRECTABLE_ERROR;
                    case ErrorType::UNCORR:
                        return RC::FALCON_ECC_UNCORRECTABLE_ERROR;
                    default:
                        return UnknownEccErrorType(errType);
                }
                break;

            case Unit::HSHUB:
                switch (errType)
                {
                    case ErrorType::CORR:
                        return RC::HSHUB_ECC_CORRECTABLE_ERROR;
                    case ErrorType::UNCORR:
                        return RC::HSHUB_ECC_UNCORRECTABLE_ERROR;
                    default:
                        return UnknownEccErrorType(errType);
                }
                break;

            case Unit::PCIE_REORDER:
            case Unit::PCIE_P2PREQ:
                switch (errType)
                {
                    case ErrorType::CORR:
                        return RC::PCIE_ECC_CORRECTABLE_ERROR;
                    case ErrorType::UNCORR:
                        return RC::PCIE_ECC_UNCORRECTABLE_ERROR;
                    default:
                        return UnknownEccErrorType(errType);
                }
                break;

            default:
                Printf(Tee::PriError, "Unknown ECC unit (Unit %u)\n",
                       static_cast<UINT32>(eclwnit));
                MASSERT(!"Unknown ECC unit");
                return RC::SOFTWARE_ERROR;
        }

        return OK;
    }

    vector<UINT32> GetValidVirtualGpcs(GpuSubdevice* pSubdev)
    {
        MASSERT(pSubdev);
        vector<UINT32> validGpcs;

        // Populate collection of virtual GPCs that are valid to collect ECC counts
        // from.
        if (pSubdev->IsSMCEnabled())
        {
            // In SMC mode, ECC counts can only be gathered from active GPCs
            // (ie. GPCs assigned to an engine in an SMC partition).
            validGpcs = pSubdev->GetActiveSmcVirtualGpcs();
        }
        else
        {
            const UINT32 gpcCount = pSubdev->GetGpcCount();
            validGpcs.resize(gpcCount);
            std::iota(validGpcs.begin(), validGpcs.end(), 0);
        }

        return validGpcs;
    }
};

Ecc::DetailedErrCounts& Ecc::DetailedErrCounts::operator+=(const DetailedErrCounts& other)
{
    if (eccCounts.size() < other.eccCounts.size())
    {
        eccCounts.resize(other.eccCounts.size());
    }
    for (UINT32 idx0 = 0; idx0 < other.eccCounts.size(); idx0++)
    {
        if (eccCounts[idx0].size() < other.eccCounts[idx0].size())
        {
            const Ecc::ErrCounts zero = { 0, 0 };
            eccCounts[idx0].resize(other.eccCounts[idx0].size(), zero);
        }
        for (UINT32 idx1 = 0; idx1 < other.eccCounts[idx0].size(); idx1++)
        {
            eccCounts[idx0][idx1].corr += other.eccCounts[idx0][idx1].corr;
            eccCounts[idx0][idx1].uncorr += other.eccCounts[idx0][idx1].uncorr;
        }
    }
    return *this;
}

Ecc::DetailedErrCounts& Ecc::DetailedErrCounts::operator-=(const DetailedErrCounts& other)
{
    if (eccCounts.size() < other.eccCounts.size())
    {
        eccCounts.resize(other.eccCounts.size());
    }
    for (UINT32 idx0 = 0; idx0 < other.eccCounts.size(); idx0++)
    {
        if (eccCounts[idx0].size() < other.eccCounts[idx0].size())
        {
            const Ecc::ErrCounts zero = { 0, 0 };
            eccCounts[idx0].resize(other.eccCounts[idx0].size(), zero);
        }
        for (UINT32 idx1 = 0; idx1 < other.eccCounts[idx0].size(); idx1++)
        {
            eccCounts[idx0][idx1].corr -= other.eccCounts[idx0][idx1].corr;
            eccCounts[idx0][idx1].uncorr -= other.eccCounts[idx0][idx1].uncorr;
        }
    }
    return *this;
}
//--------------------------------------------------------------------------
JS_CLASS(EccConst);
static SObject EccConst_Object
(
    "EccConst",
    EccConstClass,
    0,
    0,
    "EccConst JS Object"
);

// Expose ECC units
PROP_CONST(EccConst, UNIT_DRAM,            static_cast<UINT32>(Ecc::Unit::DRAM));
PROP_CONST(EccConst, UNIT_L2,              static_cast<UINT32>(Ecc::Unit::L2));
PROP_CONST(EccConst, UNIT_L1,              static_cast<UINT32>(Ecc::Unit::L1));
PROP_CONST(EccConst, UNIT_LRF,             static_cast<UINT32>(Ecc::Unit::LRF));
PROP_CONST(EccConst, UNIT_TEX,             static_cast<UINT32>(Ecc::Unit::TEX));
PROP_CONST(EccConst, UNIT_CBU,             static_cast<UINT32>(Ecc::Unit::CBU));
PROP_CONST(EccConst, UNIT_L1DATA,          static_cast<UINT32>(Ecc::Unit::L1DATA));
PROP_CONST(EccConst, UNIT_L1TAG,           static_cast<UINT32>(Ecc::Unit::L1TAG));
PROP_CONST(EccConst, UNIT_SHM,             static_cast<UINT32>(Ecc::Unit::SHM));
PROP_CONST(EccConst, UNIT_SM_ICACHE,       static_cast<UINT32>(Ecc::Unit::SM_ICACHE));
PROP_CONST(EccConst, UNIT_GCC_L15,         static_cast<UINT32>(Ecc::Unit::GCC_L15));
PROP_CONST(EccConst, UNIT_GPCMMU,          static_cast<UINT32>(Ecc::Unit::GPCMMU));
PROP_CONST(EccConst, UNIT_HUBMMU_L2TLB,    static_cast<UINT32>(Ecc::Unit::HUBMMU_L2TLB));
PROP_CONST(EccConst, UNIT_HUBMMU_HUBTLB,   static_cast<UINT32>(Ecc::Unit::HUBMMU_HUBTLB));
PROP_CONST(EccConst, UNIT_HUBMMU_FILLUNIT, static_cast<UINT32>(Ecc::Unit::HUBMMU_FILLUNIT));
PROP_CONST(EccConst, UNIT_GPCCS,           static_cast<UINT32>(Ecc::Unit::GPCCS));
PROP_CONST(EccConst, UNIT_FECS,            static_cast<UINT32>(Ecc::Unit::FECS));
PROP_CONST(EccConst, UNIT_PMU,             static_cast<UINT32>(Ecc::Unit::PMU));
PROP_CONST(EccConst, UNIT_SM_RAMS,         static_cast<UINT32>(Ecc::Unit::SM_RAMS));
PROP_CONST(EccConst, UNIT_HSHUB,           static_cast<UINT32>(Ecc::Unit::HSHUB));
PROP_CONST(EccConst, UNIT_PCIE_REORDER,    static_cast<UINT32>(Ecc::Unit::PCIE_REORDER));
PROP_CONST(EccConst, UNIT_PCIE_P2PREQ,     static_cast<UINT32>(Ecc::Unit::PCIE_P2PREQ));
PROP_CONST(EccConst, UNIT_MAX,             static_cast<UINT32>(Ecc::Unit::MAX));

// Expose ECC error types
PROP_CONST(EccConst, ERR_CORR,   static_cast<UINT32>(Ecc::ErrorType::CORR));
PROP_CONST(EccConst, ERR_UNCORR, static_cast<UINT32>(Ecc::ErrorType::UNCORR));
PROP_CONST(EccConst, ERR_MAX,    static_cast<UINT32>(Ecc::ErrorType::MAX));
