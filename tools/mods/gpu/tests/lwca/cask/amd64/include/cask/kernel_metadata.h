#ifndef INCLUDE_GUARD_CASK_KERNEL_METADATA_H
#define INCLUDE_GUARD_CASK_KERNEL_METADATA_H

// Enum values match SW's lwblasOperation_t, with CONJUGATE_NON_TRANSPOSED added
ENUMCLASS(MatrixLayoutType,
    {
        N                           = 0,
        NON_TRANSPOSED              = 0,
        T                           = 1,
        TRANSPOSED                  = 1,
        C                           = 2,
        CONJUGATE_TRANSPOSED        = 2,
        // CONJUGATE_NON_TRANSPOSED = 3,   // Not lwrrently used, no assigned letter
    }
);

ENUMCLASS(GenericLayoutType,
    {
        AFFINE = 0
    }
);


ENUMCLASS(ActivationLayoutType,
    {
        NCHW,
        NHWC,
    }
);


ENUMCLASS(WeightLayoutType,
    {
        KCRS,
        KRSC,
        CRSK,
    }
);


ENUMCLASS(ActivationFuncType,
    {
        RELU           = 0x0001,
        CLIP_RELU      = 0x0002,
        LEAKY_RELU     = 0x0004,
        GELU           = 0x0008,
        GELU_ERF       = 0x0010,
        SWISH          = 0x0020,
    }
);


ENUMCLASS(MmaInstrSpRatio,
    {
        INSTR_R_1_1,
        INSTR_R_2_1,
        INSTR_R_4_2,
    }
);


ENUMCLASS(MmaInstrShape,
    {
        SHAPE_111, // FMA
        SHAPE_884,
        SHAPE_8816,
        SHAPE_8832,
        SHAPE_1684,
        SHAPE_1688,
        SHAPE_16816,
        SHAPE_16832,
        SHAPE_16864,
        SHAPE_646416,
        SHAPE_646464,
        SHAPE_6412864,
        SHAPE_6425664,
        SHAPE_646432,
        SHAPE_6412816,
        SHAPE_6412832,
        SHAPE_6425616,
        SHAPE_6425632,
        SHAPE_64648,
        SHAPE_641288,
        SHAPE_642568
    }
);


ENUMCLASS(Algorithm,
    {
        GEMM             = 0x0001,
        IMPLICIT_GEMM    = 0x0002,
        SPARSE           = 0x0004,
        PRECOMPUTED      = 0x0008,
        INDEXED          = 0x0010,
        WITHOUT_SMEM     = 0x0020,
        WARP_SPECIALIZED = 0x0040,
        WINOGRAD         = 0x0080,
        TWOD_TILING      = 0x0100,
    }
);


ENUMCLASS(SplitKMode,
    {
        ONE_KERNEL = 0x0001,
        TWO_KERNEL = 0x0002,
    }
);


ENUMCLASS(TriangularKind,
    {
        LOWER,
        UPPER,
        RECT,
    }
);


ENUMCLASS(EdgeType,
    {
        NONE,
        INTERIOR,
        SMALL,
        MEDIUM,
        LARGE,
    }
);

ENUMCLASS(MultiplicationAlgorithm,
    {
        Add,
        MultiplyAdd,
        MultiplyAddSaturate,
        MultiplyAddFastBF16,
        MultiplyAddFastF16,
        MultiplyAddFastF32,
        MultiplyAddComplex,
        MultiplyAddGaussianComplex,
        XorPopc,
        AndPopc,
    }
);

// ---------------------------------------------------------
// Type colwersion : old metadata type => new metadata type
// ---------------------------------------------------------


inline MatrixLayoutType::Label ColwertToMatrixLayoutType(sass_private::layoutType_t type)
{
    return static_cast<MatrixLayoutType::Label>(static_cast<uint32_t>(type));
}


inline WeightLayoutType::Label ColwertToWeightLayoutType(sass_private::lwDnnLayout_t type)
{
    switch(type) {
        case sass_private::NCHW:
            return WeightLayoutType::KCRS;
        case sass_private::NHWC:
            return WeightLayoutType::KRSC;
        default:
            return WeightLayoutType::KCRS;
    }
}


inline ActivationLayoutType::Label ColwertToActivationLayoutType(sass_private::lwDnnLayout_t type)
{
    switch(type) {
        case sass_private::NCHW:
            return ActivationLayoutType::NCHW;
        case sass_private::NHWC:
            return ActivationLayoutType::NHWC;
        default:
            return ActivationLayoutType::NCHW;
    }
}


inline TriangularKind::Label ColwertToTriangularKind(sass_private::shapeTypeC_t type)
{
    return static_cast<TriangularKind::Label>(static_cast<uint32_t>(type));
}


inline EdgeType::Label ColwertToEdgeType(sass_private::edgeType_t type)
{
    return static_cast<EdgeType::Label>(static_cast<uint32_t>(type));
}

// ---------------------------------------------------------
// Type colwersion : new metadata type => old metadata type
// ---------------------------------------------------------


inline sass_private::layoutType_t ColwertFromMatrixLayoutType(MatrixLayoutType type)
{
    return static_cast<sass_private::layoutType_t>(MatrixLayoutType::Label(type));
}


inline sass_private::lwDnnLayout_t ColwertToLwDnnLayout(WeightLayoutType type)
{
    switch(WeightLayoutType::Label(type)) {
        case WeightLayoutType::KCRS:
            return sass_private::NCHW;
        case WeightLayoutType::KRSC:
            return sass_private::NHWC;
        default:
            return sass_private::NCHW;
    }
}


inline sass_private::lwDnnLayout_t ColwertToLwDnnLayout(ActivationLayoutType type)
{
    switch(ActivationLayoutType::Label(type)) {
        case ActivationLayoutType::NCHW:
            return sass_private::NCHW;
        case ActivationLayoutType::NHWC:
            return sass_private::NHWC;
        default:
            return sass_private::NCHW;
    }
}


inline sass_private::shapeTypeC_t ColwertFromTriangularKind(TriangularKind type)
{
    return static_cast<sass_private::shapeTypeC_t>(static_cast<uint32_t>(type));
}


inline sass_private::edgeType_t ColwertFromEdgeType(EdgeType type)
{
    return static_cast<sass_private::edgeType_t>(static_cast<uint32_t>(type));
}


// ---------------------------------------------------------
// Type colwersion : string -> metadata type
// ---------------------------------------------------------


inline MmaInstrSpRatio::Label MmaInstrSpRatioFromName(const std::string& str) {
    static const std::map<const std::string, MmaInstrSpRatio> lut = {
        { "INSTR_R_1_1",   MmaInstrSpRatio::INSTR_R_1_1   },
        { "INSTR_R_2_1",   MmaInstrSpRatio::INSTR_R_2_1   },
        { "INSTR_R_4_2",   MmaInstrSpRatio::INSTR_R_4_2   }
    };

    auto shape = lut.find(str);
    if (shape != lut.end()) {
        return shape->second;
    } else {
        fprintf(stderr, "coverted from an unknown string!\n");
        abort();
    }
}


inline MmaInstrShape::Label MmaInstrShapeFromName(const std::string& str) {
    static const std::map<const std::string, MmaInstrShape> lut = {
        { "111",       MmaInstrShape::SHAPE_111       },
        { "884",       MmaInstrShape::SHAPE_884       },
        { "8816",      MmaInstrShape::SHAPE_8816      },
        { "8832",      MmaInstrShape::SHAPE_8832      },
        { "1684",      MmaInstrShape::SHAPE_1684      },
        { "1688",      MmaInstrShape::SHAPE_1688      },
        { "16816",     MmaInstrShape::SHAPE_16816     },
        { "16832",     MmaInstrShape::SHAPE_16832     },
        { "16864",     MmaInstrShape::SHAPE_16864     },
        { "646416",    MmaInstrShape::SHAPE_646416    },
        { "646432",    MmaInstrShape::SHAPE_646432    },
        { "646464",    MmaInstrShape::SHAPE_646464    },
        { "6412864",   MmaInstrShape::SHAPE_6412864   },
        { "6425664",   MmaInstrShape::SHAPE_6425664   },
        { "6412816",   MmaInstrShape::SHAPE_6412816   },
        { "6412832",   MmaInstrShape::SHAPE_6412832   },
        { "6425616",   MmaInstrShape::SHAPE_6425616   },
        { "6425632",   MmaInstrShape::SHAPE_6425632   },
        { "64648",     MmaInstrShape::SHAPE_64648     },
        { "641288",    MmaInstrShape::SHAPE_641288    },
        { "642568",    MmaInstrShape::SHAPE_642568    }
    };

    auto shape = lut.find(str);
    if (shape != lut.end()) {
        return shape->second;
    } else {
        fprintf(stderr, "coverted from an unknown string!\n");
        abort();
    }
}

#endif // INCLUDE_GUARD_CASK_KERNEL_METADATA_H
