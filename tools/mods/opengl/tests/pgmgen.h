/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
/**
 * @file   pgmgen.h
 * Gpu program generation for glrandom.
 */
#include "glrandom.h"  // declaration of our namespace
using namespace GLRandom;

#include <stack>

//------------------------------------------------------------------------------
//! @brief container for GLRandom::Program objects.
//------------------------------------------------------------------------------
class Programs
{
public:
    Programs ();
    ~Programs ();

    void AddProg (pgmSource src, const Program & prog);
    void RemoveProg (pgmSource src, const int idx);
    void UnloadProgs (pgmSource src);
    void ClearProgs (pgmSource src);
    void SetName (const char * nameForProgNames);
    void SetSeed (UINT32 seedForProgNames);
    void SetEnableReplacement(bool enable);

    int NumProgs() const;
    int NumProgs (pgmSource src) const;

    Program * Get (pgmSource src, UINT32 idx);

    void      SetLwr (pgmSource src, UINT32 idx);
    void      SetLwr (UINT32 combinedIdx);
    void      ClearLwr ();

    Program * Lwr() const;
    UINT32    LwrCombinedIdx() const;

    void PickLwr (FancyPicker * pPicker, Random * pRand);
    void PickNext ();
    const char *      PgmSrcToStr (pgmSource ps) const;

private:
    vector<Program> * PgmSrcToVec (pgmSource ps);
    const vector<Program> * PgmSrcToVec (pgmSource ps) const;

    const char * m_Name;
    UINT32 m_Seed;

    vector<Program> m_FixedProgs;
    vector<Program> m_RandomProgs;
    vector<Program> m_UserProgs;
    vector<Program> m_SpecialProgs; // Special internal programs not available to
                                    // the picking logic.

    UINT32    m_LwrCombinedIdx;
    Program*  m_LwrProg;

    bool m_EnableReplacement;
};

//------------------------------------------------------------------------------
// Some const lookup tables, indexed by program-kind or TxAttr
//------------------------------------------------------------------------------
namespace PgmGen
{
    extern const char * const pkNicknames [PgmKind_NUM];    // "vx"
    extern const char * const pkNicknamesUC [PgmKind_NUM];  // "VX"
    extern const char * const pkNames [PgmKind_NUM];        // "Vertex"
    extern const GLenum       pkTargets [PgmKind_NUM];      // GL_VERTEX_PROGRAM_LW
    extern const GLRandomTest::ExtensionId
                              pkExtensions [PgmKind_NUM];   // ExtLW_gpu_program4
    extern const UINT32       pkPickEnable [PgmKind_NUM];   // RND_GPU_PROG_VX_ENABLE
    extern const UINT32       pkPickPgmIdx [PgmKind_NUM];   // RND_GPU_PROG_VX_INDEX
    extern const UINT32       pkPickPgmIdxDebug [PgmKind_NUM];// RND_GPU_PROG_VX_INDEX_DEBUG
    extern const GLenum       PaBOTarget [PgmKind_NUM];     // ParameterBufferObject targets

    pgmKind TargetToPgmKind(GLenum target);
};

//------------------------------------------------------------------------------
//! Backus-Naur Form program generator for creating program strings.
//!
//! This class implements the LW_gpu_program4 grammer which adheres to the
//! Backus-Nuar Form (BNF) program grammar syntax. It also contains the core logic
//! for creating random programs that are sematically correct. There are a number
//! of abstract methods that must be implemented by the subclass. These methods
//! are typically to access program specific random picker variables, or to
//! implement program specific grammar rules.
//!
//! When creating random programs there are several rules that must be followed to
//! get consistent programs, They are:
//! 1) All result registers must be written before ending the program
//! 2) When using indirect addressing you must insure that the index values don't
//!    exceed the size of the program's pgmElw[]
//! 3) When reading an input register you must insure that it has been previously
//!    written or you will get undefined results.
//! 4) Temporary variables must be declared and initialized prior to use.
//! 5) There are 32 Texture fetchers (Texture Image Units) that can be shared by
//!    all three programs. When a fetcher is used we must insure that the target
//!    dimension remains consistent. ie. If fetcher 1 access's a 2D target in the
//!    vertex program, it can not be used to access a 1D target in a fragment
//!    program.
//! 6) There are some instructions that don't actually write to all 4 register
//!    components, even if the write mask specifies all 4. In addition there are
//!    some registers where all 4 components are valid. You must make sure that
//!    the instruction's internal write mask (OpData.ResultMask) and the result
//!    register's writeableMask (RegUse.WriteableMask) are not used in a way
//!    that would cause zero components to be written.
//! 7) When creating programs that access registers using data types other than
//!    floats you must insure that all bound programs are accessing the registers
//!    using the same data type.
//------------------------------------------------------------------------------
class GpuPgmGen
{
public:
    // Public data types:
    enum {
        // Layout of common-program const regs:
        //
        ModelViewProjReg  = 0,      // VX only, tracks model-view-projection matrix
        ModelViewIlwReg   = 4,      // VX only, traces model-view matrix
        AmbientColorReg   = 8,      // global ambient color (.3, .2, .2, 0)
        Constants1Reg     = 9,      // constants (3, 4, 5, 1)
        Constants2Reg     = 10,     // constants (0, 2, .5 1)
        LightIndexReg     = 11,     // light indices, pt sz (L0, L1, psz, 64)
        RelativeTxReg     = 12,     // random tx index (0..7, 0..7, 0..7, 0..7)
        RelativeTxHndlReg = 13,     // random texture object handle index (0..NumTxHandles-1,...)
        RelativePgmElwReg = 14,     // random pgmElw index (0..128, etc et)
        SboWindowRect     = 15,     // stores the r/w SBOs rect & position
        SubIdxReg         = 16,     // random subroutine number (index to Subs for CALI)
        TmpArrayIdxReg    = 17,     // random tmpArray index (0..1)
        AtomicMask1       = 18,     // Bit masks for 32 bit atomic instructions (And,Or,Xor,MinMax).
        AtomicMask2       = 19,     // Bit masks for 32 bit atomic instructions(Add,Wrap,Xchng,CSwap).
        AtomicMask3       = 20,     // Bit masks for 64 bit atomic instrs(And, Or)
        AtomicMask4       = 21,     // Bit masks for 64 bit atomic instrs(Xor, MinMax)
        AtomicMask5       = 22,     // Bit masks for 64 bit atomic instrs(Add, Xchng/CSwap)
        LayerAndVportIDReg= 23,     // X component stores the layerID, Y component stores the viewport Idx
        FirstLightReg     = 24,     // Rest contain random spotlight setups
        // a+0    light direction
        // a+1    light-eye-half angle
        // a+2    light color
        // a+3    light spelwlar power (x), next light index (y), light num%4 (z)

        // Local program constants (in fragment shaders)
        SboRwAddrs  = 0, // gpu virtual address of the R/W SBOs
                         //(rdLow, rdHigh, wrLow, wrHigh)
        SboTxAddrReg = 1,// store the gpu address of the Tx SBO.
        NumLocalRegs = 2
    };

    // enums used to build the OpData tables
    enum
    {
        instrIlwalid = 0,       //!< instruction is not valid
        instrValid = 1,         //!< instruction is valid

        rndNoPick = 0,          //!< Don't make calls to random pickers
        rndPick = 1,            //!< Make calls to random pickers
        rndPickRequired = 2,    //!< Requires either CC0 or CC1 to be picked

        declPass0 = 0,          //!< initial pass in register declaration
        declPass1 = 1,          //!< second pass in register declaration

        ccMaskAll = 0,          //!< allow all forms of <ccMask>
        ccMaskComp = 1,         //!< allow only component form of <ccMask>

        MaxIn = 3,              //!< Max number of input registers for any given Op.

        TempArrayLen = 4,

        MaxSubs = 3,            //!< Max number of subroutines in random programs.

        // "Swizzle Index" values used access swizzle arrays.
        siX = 0,              //!< index for the X component
        siY = 1,              //!< index for the Y component
        siZ = 2,              //!< index for the Z component
        siW = 3,              //!< index for the W component

        // "Swizzle Component" values used to populate the swizzle arrays
        scX = 0,              //!< X component of .xyzw masks for swizzle
        scY = 1,              //!< Y component of .xyzw masks for swizzle
        scZ = 2,              //!< Z component of .xyzw masks for swizzle
        scW = 3,              //!< W component of .xyzw masks for swizzle

        //! Operation Instruction Types
        oitVector   = 0,        //!< ALUInstructions
        oitScalar   ,           //
        oitBinSC    ,           //
        oitBin      ,           //
        oitVecSca   ,           //
        oitTri      ,           //
        oitSwz      ,           //
        oitTex      ,           //!< TexInstructions
        oitTex2     ,           //!< Two source register variant tex instruction
        oitTexD     ,           //
        oitTexMs    ,           //!< Special processing for multisample texture fetch
        oitTxq      ,           //!< TXQ is a special case
        oitBra      ,           //!< FlowInstructions
        oitFlowCC   ,           //
        oitIf       ,           //
        oitRep      ,           //
        oitEndFlow  ,           //
        oitSpecial  ,           //!< SpecialInstructions
        oitBitFld   ,           //
        oitTxgo     ,           //!< Special range check processing for TXGO
        oitCvt      ,           //!< Special processing for CVT
        oitPk64     ,           //!< Special processing for PK64
        oitUp64     ,           //!< Special processing for UP64
        oitMem      ,           //!< Memory transaction instructions
        oitMembar   ,           //!< Memory barrier
        oitCali     ,           //!< CALI
        oitImage    ,           //!< ATOMIM,LOADIM,STOREIM
        oitUnknown  ,           //!< Unknown instruction type (error condition)
        //! Stand-alone instruction modifiers
        smNone      =      0x00,   //!< opcode has no stand-alone modifiers
        smNA        =      smNone,
        smSS0       =      0x40, //!< clamping [0,1] (saturation) modifier
        smSS1       =      0x80, //!< signed clamping [-1,1] (saturation) modifier
        smCC0       =     0x100, //!< condition code0 update modifier
        smCC1       =     0x200, //!< condition code1 update modifier
        smCEIL      =     0x400, //!< rounding modifier ceiling
        smFLR       =     0x800, //!< rounding modifier floor
        smTRUNC     =    0x1000, //!< rounding mofifier truncate
        smROUND     =    0x2000, //!< rounding modifier round
        //! Combinations of the above for readability
        smC         =   (smCC0 + smCC1),
        smSS        =   (smSS0 + smSS1),
        smCS        =   (smC | smSS),
        smR         =   (smCEIL | smFLR | smTRUNC | smROUND),
        atVector    =   0x01000, //!< opcode arg or result is a 4-component vector (float/signed/unsigned)
        atVectF     =   0x02000, //!< opcode arg or result is a 4-component floating point vector
        atVectS     =   0x04000, //!< opcode arg or result is a 4-component signed integer vector
        atVectU     =   0x08000, //!< opcode arg or result is a 4-component unsigned integer vector
        atVectNS    =   0x10000, //!< opcode arg or result is a 4-component vector with "no suffix"
        atScalar    =   0x20000, //!< opcode arg or result is a single scalar, replicated if written to a
                                 //!< vector destination. Data type is inherited from operation.
        atCC        =   0x40000, //!< opcode arg is a condition code test result. ie "(EQ)", "(GT1.x)", "(LE2.z)"
        atLabel     =   0x80000, //!< opcode arg is a jump or call label
        atNone      =         0, //!< opcode has no arg  or no result

        regTemp     =     0x001, //!< reg is a temp reg
        regIn       =     0x002, //!< reg is a vertex,geometry or fragment attribute (i.e. a program input)
        regOut      =     0x004, //!< reg is a vertex,geometry or fragment result (i.e. a program output)
        regBuffer   =     0x010, //!
        regRqd      =     0x020, //!< reg is a required output, program is incorrect if reg is not written
        regUsed     =     0x040, //!< reg has been used, and may need a declaration later (temps are not initially declared)
        regIndex    =     0x080, //!< reg is integer index reg that can be used for relative index addressing
        regCentroid =     0x100, //!< reg is attrib that should be CENTROID sampled.
        regOOB      =     0x400, //!< reg may be out of bounds for relative indexing
        regLiteral  =     0x800, //!< reg is being used as a literal vector or scalar
        regSBO      =    0x1000, //!< reg is special purpose for storing the SBO gpu address's
        regGarbage  =    0x2000  //!< reg is special purpose that doesn't affect scoring or random
                                 //! sequence you can't randomly pick this register.
    };
    enum IndexRegType
    {
        tmpA128      //!< arg to GetIndexReg() -- select tmpA128
        ,tmpA3       //!< arg to GetIndexReg() -- select tmpA3
        ,tmpA1       //!< arg to GetIndexReg() -- select tmpA1
        ,tmpASubs    //!< arg to GetIndexReg() -- select tmpASubs
    };

    struct PassthruRegs
    {
        UINT32 XfbProp; // from RndGpuPrograms::XfbStdAttrBits
        INT32 Prop;     // from GLRandom::ProgramProperty
        INT32 InReg;
        INT32 OutReg;
    };

    struct OpData
    {
        const char * Name;      //!< Operation mnemonic
        int   OpInstType;       //!< Operation Instrution Type
        int   ExtNeeded[2];     //!< the OpenGL extension this instr requires
                                //! [0] = vertex shaders, [1] = all other shaders
        int   NumArgs;          //!< num args (not counting tex target)
        int   ArgType[3];       //!< type of args (see above)
        int   ResultType;       //!< type of result (see above)
        int   ResultMask;       //!< components that can actually be written with this operation.
        int   OpModMask;        //!< allowed stand-alone opcode modifiers (see above)
        GLRandom::opDataType TypeMask; //!< allowed operation type and precision modifiers.
        GLRandom::opDataType DefaultType; //!< default data type if no type/precision suffix supplied
        bool  IsTx;             //!< true for vp3 vertex texture-lookups ?
        int   ScoreMult;        //!< "interestingness" of result is A + M*(sum of arg interestingness)/10
        int   ScoreAdd;         //!< "interestingness" of result is A + M*(sum of arg interestingness)/10
        enum
        {
            ScoreMultDivisor = 10 // value by which ScoreMult has to be divided to obtain the actual weight
        };
    };

    struct RegState
    {
        int     Id;              //!< see glr_comm.h
        int     ReadableMask;    //!< which .xyzw components are readable
        int     WriteableMask;   //!< which .xyzw components are writeable
        int     WrittenMask;     //!< which .xyzw components have been written so far
        int     Score[4];        //!< .xyzw component "interestingness" score so far
        int     Attrs;           //!< see "reg" enums above
        int     Bits;            //!< i.e. 16, 32, or 64 bits per component
        int     ArrayLen;        //!< indicate array length for indexed "regs",
                                 //!< using 0 for a single 4-vec

        //! Components modified in this Block (updates to WrittenMask).
        //! This is normally via writes that set bits in WrittenMask.
        //! Used in Block::IncorporateXX for merging RegState updates.
        int     BlockMask;

        enum { IlwalidId = -1 };

        RegState(int id = 0)
        :   Id(id)
           ,ReadableMask(0)
           ,WriteableMask(0)
           ,WrittenMask(0)
           ,Attrs(0)
           ,Bits(32)
           ,ArrayLen(0)
           ,BlockMask(0)
        {
            Score[0] = Score[1] = Score[2] = Score[3] = 0;
        }

        void InitReadOnlyReg (int score, int mask);
        void InitIndexReg();

        int  UnWrittenMask () const
        {
            return GLRandom::compXYZW & (WriteableMask & ~WrittenMask);
        }
    };
    class Literal
    {
        public:
        enum Type
        {
            s32 = GLRandom::dt32 | GLRandom::dtS,
            u32 = GLRandom::dt32 | GLRandom::dtU,
            s64 = GLRandom::dt64 | GLRandom::dtS,
            u64 = GLRandom::dt64 | GLRandom::dtU,
            f32 = GLRandom::dt32 | GLRandom::dtF,
            f64 = GLRandom::dt64 | GLRandom::dtF
        } m_Type;

        int m_Count;  //!< 0 if unused, 1-4 if used.

        union {
            UINT32 m_u32[4];
            INT32  m_s32[4];
            UINT64 m_u64[4];
            INT64  m_s64[4];
            float  m_f32[4];
            double m_f64[4];
        } m_Data;

        Literal() { Reset();}
        void Reset()
        {   m_Type = u64;
            m_Count = 0;
            m_Data.m_u64[0] = 0;
            m_Data.m_u64[1] = 0;
            m_Data.m_u64[2] = 0;
            m_Data.m_u64[3] = 0;
        }
        bool operator<(const Literal& rhs) const
        {
            if(m_Type != rhs.m_Type)
                return (m_Type < rhs.m_Type);
            if(m_Count != rhs.m_Count)
                return (m_Count < rhs.m_Count);
            if(m_Data.m_u64[0] != rhs.m_Data.m_u64[0])
                return (m_Data.m_u64[0] < rhs.m_Data.m_u64[0]);
            if(m_Data.m_u64[1] != rhs.m_Data.m_u64[1])
                return (m_Data.m_u64[1] < rhs.m_Data.m_u64[1]);
            if(m_Data.m_u64[2] != rhs.m_Data.m_u64[2])
                return (m_Data.m_u64[2] < rhs.m_Data.m_u64[2]);
            return (m_Data.m_u64[3] < rhs.m_Data.m_u64[3]);
        }
    };

    //! Each nested "block" (basically each StatementSequence) has its own
    //! state to track whether variables were written, etc.
    //!
    //! For looking up variable state, inner blocks cascade up to their
    //! enclosing block(s) for variables not written in the inner block.
    //!
    //! Changes to variable state affect only the innermost block (at first).
    //!
    //! Later, an outer block will incorporate variable updates from an inner
    //! block into its own state, at the point of the CAL/IF/REP that ties
    //! the inner to outer blocks.
    //!
    class Block
    {
    public:
        Block(const Block * parent = 0);
        ~Block();
        const Block & operator=(const Block& rhs);
        Block& operator=(Block&& rhs);

        void SetTargetStatementCount (int tgtStatementCount);

        void    AddStatement (const string & line, bool bCountStatement = true);
        void    AddStatements (const Block & child);
        int     StatementCount() const;
        int     StatementsLeft() const;

        const string & ProgStr() const;

        int              NumVars() const;
        const RegState & GetVar (int id) const;

        int CCValidMask() const;
        void UpdateCCValidMask (int newBits);

        bool UsesTXD() const;
        void SetUsesTXD();

        void AddVar (const RegState & v);
        void UpdateVar (const RegState & v);
        void UpdateVarMaybe (const RegState & v);

        enum Called { CalledMaybe, CalledForSure };

        void IncorporateInnerBlock (const Block & child, Called c);
        void IncorporateIfElseBlocks (const Block & childA, const Block & childB);
        void MarkAllOutputsUnwritten();

        int NumRqdUnwritten() const;

    private:
        //! Outer block, var-lookup cascades to here on miss.
        const Block * m_Parent;

        //! Local overrides of Parent's Vars.
        map<int, RegState> m_Vars;
        typedef map<int, RegState>::iterator         VarIter;
        typedef map<int, RegState>::const_iterator   ConstVarIter;

        //! Indent level for pretty-printing.
        //! Indent is 0 only in main outside of all IF and REP blocks.
        const int m_Indent;
        //! Original target statement-count.
        //! Nothing terrible will happen if we go over or under this,
        //! it's just a coarse control of program length.
        int m_TargetStatementCount;

        //! Flags to indicate what CC components have been updated:
        //! bits 0-3 (CC0), 4-7 (CC1).
        int m_CCValidMask;

        //! The generated shader program so far for this block.
        string m_ProgStr;

        //! Number of calls to AddStatement so far.
        int m_StatementCount;

        //! For bug 489105 workaround.
        bool m_UsesTXD;

        static const RegState s_OutOfRangeVar;

    };

    //! A subroutine is just a block with a name.
    struct Subroutine
    {
        Block           m_Block;
        const string    m_Name;       //!< Function name, i.e. "Sub1"
        bool            m_IsCalled;   //!< True if main contains a CAL to this.

        Subroutine
        (
            const Block * outer
            ,int subNum
        );
        Subroutine
        (
            const Block * outer
            ,const char * name
        );

    };

    struct RegUse
    {
        void     Init();
        int      Reg;           //!< RND_GEOM_VXPGM_REG_vOPOS to RND_GEOM_VXPGM_REG_A1
        int      IndexReg;      //!< 0 if not used for relative-addressing
        int      IndexOffset;   //!< -256..255 for vp2, -64..63 for others
        int      IndexRegComp;  //!< 0..3 for .x through .w
        UINT08   Swiz[4];       //!< input swizzle (ignored for result reg writes)
        int      SwizCount;     //!< 0(none), 1(component), or 4(all components)
        bool     Abs;           //!< input absolute-value (ignored for result reg writes)
        int      Neg;           //!< input negate (ignored for result reg writes)
        Literal  LiteralVal;    //!< value for RegLiteral or RegConstVect
    };

    struct GenState
    {
        //! Represents the main statements & master RegState map.
        Block MainBlock;

        //! Holds the Subroutines that are called by MainBlock.
        vector<Subroutine*> Subroutines;

        //! Holds a pushdown stack of the pointers to the current Block.
        //! When Blocks.top() == &MainBlock, we are in main.
        //! StatementSequence adds instructions to the Block at Blocks.top().
        stack<Block*> Blocks;

        int      PgmStyle;          //!< ie. Simple, Call, Branch, etc.
        bool     UsingRelAddr;      //!< if true we are using relative addressing somewhere in the pgm.
        int      NumOps;            //!< Size of the Ops array.
        OpData * Ops;               //!< Describe opcodes for current vx/gm/fr extension.
        GLRandom::Program  Prog;    //!< The program we are creating
        int      LwrSub;            //!< the current subroutine we are building instrs for.
        INT32    BranchOpsLeft;     //!< number of branch calls we still need to make to call all of the sub-routines
        INT32    MsOpsLeft;         //!< number of multisample instr. we still need to make (5 ops/instr)
        UINT16   ConstrainedOpsUsed;//!< mask of the atomic opts used in this program.

        // Runtime range checking variables for various opcodes
        GLint    MinTexOffset;      //!< min offset value for most tex opcodes
        GLint    MaxTexOffset;      //!< max offset value for most tex opcodes
        GLint    MinTxgoOffset;     //!< min offset value for TXG0 opcode
        GLint    MaxTxgoOffset;     //!< max offset value for TXGO opcode
        bool     AllowLiterals;     //!< if true allow usage of const vector/scalars
        const GpuPgmGen::PassthruRegs *PassthruRegs;  //! pointer to lookup table or registers that can be passed through this shader
    };

    // Class to encapsulate all the data needed to compose a complete shader instruction.
    class PgmInstruction
    {
        //---------------------------------------------------------------------
        // These variables get re-initialized prior to picking a new opcode via
        // InitOp().
        //
    public:
        PgmInstruction()
        {
            Init(nullptr, 0);
        }
        PgmInstruction(GenState *pGS, int opId)
        {
            Init(pGS, opId);
        }
        void Init(GenState * pGenState, int opId); //!< Initialize all the internal data
        const char *        GetName() const;
        int                 GetOpInstType() const;
        int                 GetExtNeeded(GLRandom::pgmKind kind) const;
        int                 GetNumArgs() const;
        int                 GetArgType(int idx) const;
        int                 GetResultType() const;
        int                 GetResultMask() const;
        int                 GetOpModMask() const;
        void                GetTypeMasks(GLRandomTest *pGLRandom,vector<GLRandom::opDataType>*pOpMasks) const;
        void                GetDynamicTypeMasks(GLRandomTest *pGLRandom,vector<GLRandom::opDataType>*pOpMasks) const;
        GLRandom::opDataType GetDefaultType() const;
        bool                GetIsTx() const;
        int                 GetScoreMult() const;
        int                 GetScoreAdd() const;
        bool                HasComplexLwinstExpansion() const;
        void                SetUsrComment(const char *szComment);
        void                SetUsrComment(const string &str);
        bool    ForceRqdWrite;      //!< if true the next result register must be a required output reg.
        GLRandom::opDataType srcDT; //!< The data type for the source operand (not used too often)float, signed/unsigned int
        GLRandom::opDataType dstDT; //!< The data type for the result variable,  float, signed/unsigned int
        int     OpId;              //!< Current Op's ID
        int     OpModifier;        //!< bits: 0-15 std modifiers
                                   //!< bits 31-16 special atomic op modifiers
        RegUse  OutReg;            //!< output reg number & indirect addressing info
        int     OutMask;           //!< 0xf means ".xyzw"
        int     CCtoRead;          //!< CC0 or CC1 (or RND_GEOM_VXPGM_CCnone)
        int     OutTest;           //!< RND_GPU_PROG_OUT_TEST_ne, for example
        int     CCSwizCount;       //!< none, one component, or all 4 components
        UINT08  CCSwiz[4];         //!< read-CC swizzle
        int     LwrInReg;          //!< current input register we are processing.
        RegUse  InReg[MaxIn];      //!< input reg numbers, addressing, swiz, abs, neg
        int     TxFetcher;         //!< for TEX ops, which of 16 fetchers to read from, if equal to MaxTxFetchers its bindless.
        UINT32  TxAttr;            //!< Fetcher binding attributes
        int     TxOffsetFmt;       //!< what type of texture offset format to use, toNone,toConstant, or toProgrammable
        int     TxOffsetReg;       //!< register use for programmable texture offset
        int     TxOffset[4];       //!< Tex offset values for .xyzw components.
        INT32   TxgTxComponent;    //!< Tex component to use for TXG/TXGO instructions.
        UINT08  ExtSwiz[4];        //!< Extended swizzle components, 0,1,x,y,z,w
        int     ExtSwizCount;      //!< number of components to swizzle.
        GLRandom::ImageUnit IU;    //!<
        string  UsrComment;        //!< optional comments to add after the instruction.
    private:
        const   OpData * pDef;      //!< pointer to this op's definition data.
        const   GenState *pGS;      //!< pointer to this instructions GenState.
    };

public:
    // public methods:
    virtual ~GpuPgmGen();
    virtual UINT32          CalcScore(PgmInstruction *pOp, int reg, int argIdx);
    GLRandom::VxAttribute   GetPgmCol0Alias();          //!< Return the reg. that holds the primary color
    int                     GetPgmElwSize();            //!< Return the size of this pgmElw[]
    virtual int             GetPropIOAttr(ProgProperty) const =0;//!< Return Input/Output capability for this property $
    int                     GetMaxTxCdSz(int compMask); //!< Get max number of components for this XYZW mask
    int                     GetRqdTxCdMask(int Attr);   //!< Get required text component mask for this target
    const PassthruRegs &    GetPassthruReg(GLRandom::ProgProperty Prop);
    int                     GetUsedTempReg(int WriteMask);
    int                     GetAtomicOperand0Reg(PgmInstruction *pOp);
    int                     GetAtomicSwiz(PgmInstruction *pOp);
    int                     GetAtomicIndex(PgmInstruction *pOp);
    int                     GetDataTypeSize(opDataType dt);
    void InitGenState
    (
        int numOps,                         //!< total number of opcodes
        const OpData *pOpData,              //!< additional opcodes to append
        GLRandom::PgmRequirements *pRqmts,  //!< optional program requirements
        const PassthruRegs * pPTRegs,       //!< pointer to lookup table of pass-thru regs
        bool bAllowLiterals                 //!< allow usage of literals
    );
    void                    InitProg(GLRandom::Program *pProg,              //!< Initialize the Program struct
                                     unsigned int pgmTarget,                //!< vertex or geom or fragment pgm?
                                     GLRandom::PgmRequirements *pRqmts);    //!< input/output requirements
    void                    InitOp (int opId = 0);
    void                    InitRegUse(RegUse * pru);
    void                    SetPgmCol0Alias(GLRandom::VxAttribute col0Alias); //!< set the register to hold the primary color
    virtual void            SetContext(FancyPicker::FpContext * pFpCtx);
    void                    SetProgramTemplate(int templateType);
    void                    SetProgramXfbAttributes(UINT32 stdAttr,
                                                    UINT32 usrAttr);

    static GLint LightToRegIdx(GLuint light);
    static const char * EndStrategyToString(int strategy);
    static string PrimitiveToString (int Primitive);
    static string PropToString (INT32 prop);
    static string CompToString (INT32 comp);
    static const char* CompMaskToString (INT32 comp);

    //! Generate an empty placeholder "random" program.
    //! Used for strict-linking mode to skip a pgmKind in a chain picked at
    //! restart-time.
    void GenerateDummyProgram(Programs * pProgramStorage, GLuint target);

    virtual void GenerateRandomProgram
    (
        GLRandom::PgmRequirements *pRqmts,
        Programs * pProgramStorage,
        const string & pgmName
    ) = 0;

    virtual void GenerateFixedPrograms
    (
        Programs * pProgramStorage
    ) = 0;

    virtual RC GenerateXfbPassthruProgram
    (
        Programs * pProgramStorage
    );

    // Report what ISA instructions were used for this shader.
    virtual RC              ReportTestCoverage(const string & pgmStr);
    //Initialize anything that needs to be done at the start of a frame.
    virtual void Restart();

protected:
    // protected methods:
    GpuPgmGen
    (
        GLRandomTest *     pGLRandom,
        FancyPickerArray * pPicker,
        pgmKind            kind
    );

    void                    AddPgmComment( const char * szComment);

    // Default program environment accessors
    virtual int             GetNumTxFetchers();

    // Abstract program environment accessors
    virtual int             GetConstReg() const =0;
    virtual int             GetConstVectReg() const =0;
    virtual int             GetIndexReg(IndexRegType t) const =0;
    virtual bool            IsBindlessTextures() const;
    virtual bool            IsLwrrGMProgPassThru() const;
    virtual bool            IsIndexReg(int regId) const =0;
    virtual int             GetFirstTempReg() const =0;
    virtual int             GetGarbageReg() const =0;
    virtual int             GetLiteralReg() const =0;
    virtual int             GetFirstTxCdReg() const =0;
    virtual int             GetLastTxCdReg() const =0;
    virtual int             GetLocalElwReg() const =0;
    virtual int             GetNumTempRegs() const =0;
    virtual int             GetMinOps()   const =0;
    virtual int             GetMaxOps()   const =0;
    virtual int             GetPaboReg()    const =0;
    virtual int             GetSubsReg() const =0;
    virtual GLRandom::ProgProperty  GetProp(int reg) const =0;
    virtual int             GetTxCdScalingMethod(PgmInstruction *pOp);
    virtual int             GetTxCd(int reg) const =0;      // return a texture coordinate for this register or <0 on error.
    virtual string          GetRegName(int reg) const =0;

    // Abstract RandomPicker accessors.
    virtual int             PickAbs() const =0;
    virtual int             PickNeg() const=0;
    virtual int             PickNumOps() const=0;
    virtual int             PickOp() const=0;
    virtual int             PickPgmTemplate() const=0;
    virtual int             PickRelAddr() const=0;
    virtual int             PickResultReg() const=0;
    virtual int             PickTempReg() const=0;
    virtual int             PickTxTarget() const=0;

    // Default picking logic
    virtual int             PickTxFetcher(int Access);
    virtual bool            PickImageUnit(PgmInstruction *pOp, int Access);
    virtual int             PickCCtoRead() const;       //!< returns 0 or 1
    virtual int             PickCCTest(PgmInstruction *pOp) const;         //!< "EQ", "NE", etc
    virtual int             PickMultisample() const;    //!< Use explicit_multisample extension?
    virtual int             PickOperand(PgmInstruction *pOp); //!< pick an input register
    virtual int             PickOpModifier() const;
    virtual int             PickAtomicOpModifier();
    virtual GLRandom::opDataType PickOpDataType(PgmInstruction *pOp) const;
    virtual int             PickSwizzleSuffix() const;
    virtual int             PickSwizzleOrder() const;
    virtual int             PickWriteMask() const;
    virtual int             PickTxOffset() const;

    // BNF Grammer rules (high level)
    virtual void            ProgramSequence(const char *pHeader);
    virtual void            DeclSequence();
    virtual void            OptionSequence();
    virtual void            ArrayVarInitSequence();
    virtual void            StatementSequence();
    virtual void            Instruction();
    virtual void            SetOpenGLFeatures(PgmInstruction * pOp);
    virtual bool            SpecialInstruction(PgmInstruction *pOp);

    // BNF Grammar rules (mid level)
    virtual bool            VECTORop_instruction(PgmInstruction *pOp);         //!< see <VECTORop_instruction>
    virtual bool            SCALARop_instruction(PgmInstruction *pOp);         //!< see <SCALARop_instruction>
    virtual bool            BINSCop_instruction(PgmInstruction *pOp);          //!< see <BINSCop_instruction>
    virtual bool            VECSCAop_instruction(PgmInstruction *pOp);         //!< see <VECSCAop_instruction>
    virtual bool            BINop_instruction(PgmInstruction *pOp);            //!< see <BINop_instruction>
    virtual bool            TRIop_instruction(PgmInstruction *pOp);            //!< see <TRIop_instruction>
    virtual bool            SWZop_instruction(PgmInstruction *pOp);            //!< see <SWZop_instruction>
    virtual bool            TEXop_instruction(PgmInstruction *pOp);            //!< see <TEXop_instruction>
    virtual bool            TEX2op_instruction(PgmInstruction *pOp);           //!< see <TEX2op_instruction> in LW_gpu_program4_1
    virtual bool            TXFMSop_instruction(PgmInstruction *pOp);
    virtual bool            TXDop_instruction(PgmInstruction *pOp);            //!< see <TXDop_instruction>
    virtual bool            TXQop_instruction(PgmInstruction *pOp);
    virtual bool            TXGOop_instruction(PgmInstruction *pOp);           //!< see <TXDop_instruction>
    virtual bool            BRAop_instruction(PgmInstruction *pOp);            //!< see <BRAop_instruction>
    virtual bool            CALIop_instruction(PgmInstruction *pOp);           //!< see <CALI_instruction>
    virtual bool            FLOWCCop_instruction(PgmInstruction *pOp);         //!< see <FLOWCCop_instruction>
    virtual bool            IFop_instruction(PgmInstruction *pOp);             //!< see <IFop_instruction>
    virtual bool            REPop_instruction(PgmInstruction *pOp);            //!< see <REPop_instruction>
    virtual bool            BITFLDop_instruction(PgmInstruction *pOp);         //!<
    virtual bool            CVTop_instruction(PgmInstruction *pOp);            //!<
    virtual bool            PK64op_instruction(PgmInstruction *pOp);
    virtual bool            UP64op_instruction(PgmInstruction *pOp);
    virtual bool            MEMop_instruction(PgmInstruction *pOp);
    virtual bool            MEMBARop_instruction(PgmInstruction *pOp);
    virtual bool            IMAGEop_instruction(PgmInstruction *pOp);

    // BNF Grammar rules (low level)
    virtual void            OpModifiers(PgmInstruction *pOp);                  //!< see <opModifier>
    virtual void            InstOperandV(PgmInstruction *pOp);                 //!< see <instOperandV>
    virtual void            InstOperandS(PgmInstruction *pOp);                 //!< see <instOperandS>
    virtual bool            InstOperandTxCd( PgmInstruction *pOp);             //!< special processing for Texture instructions
    virtual void            InstOperandVNS(PgmInstruction *pOp);               //!< see <instOperandVNS>
    virtual void            InstOperand(PgmInstruction *pOp,
                                         int dfltReg,       //!< see <instOperand???>
                                         int minReg,
                                         int maxReg);
    virtual void            InstOperand(PgmInstruction *pOp,
                                        int Pick = rndPick); //!< see <instOperand???>
    virtual void            InstResult(PgmInstruction *pOp);                   //!< see <instResult> grammar rule.
    virtual void            InstResultBase(PgmInstruction *pOp);               //!< see <instResultBase> grammar rule.
    virtual void            InstLiteral(PgmInstruction *pOp, int reg, int numComp, double v0, double v1, double v2, double v3);
    virtual void            InstLiteral(PgmInstruction *pOp, int reg, int numComp, float v0, float v1, float v2, float v3);
    virtual void            InstLiteral(PgmInstruction *pOp, int reg, int numComp, UINT32 v0, UINT32 v1, UINT32 v2, UINT32 v3);
    virtual void            InstLiteral(PgmInstruction *pOp, int reg, int numComp, INT32 v0, INT32 v1, INT32 v2, INT32 v3);
    virtual void            TempUseW(PgmInstruction *pOp);
    virtual void            ResultUseW(PgmInstruction *pOp);
    virtual bool            ResultBasic(PgmInstruction *pOp);
    virtual bool            ResultVarName(PgmInstruction *pOp);
    virtual bool            CCMask(PgmInstruction *pOp,
                                   int Pick = rndPick);      //!< returns true/false to indicate if a mask was used.
    virtual bool            CCTest(PgmInstruction *pOp,
                                   int Pick = rndPick,       //!< force a valid pick of the CCMask & CCTest
                                   int CompMask = ccMaskAll); //!< If true only use the <componentMask>, not the <swizzleMask>
    virtual void            OptWriteMask(PgmInstruction *pOp,
                                    int Pick = rndPick);//!< common behavior implemented here
    virtual void            SwizzleSuffix(PgmInstruction *pOp,UINT08 *pSwizzle, int *pNumComp, UINT08 ReadableMask = 0x0f);
    virtual void            ScalarSuffix(PgmInstruction *pOp, UINT08 ReadableMask);
    virtual void            ExtendedSwizzle(PgmInstruction *pOp);
    virtual bool            TexAccess(PgmInstruction *pOp, int Access = iuaRead);
    virtual void            TexOffset(PgmInstruction *pOp,
                                    int toFmt);

    virtual RC              ReportISATestCoverage(UINT32 opId);
    virtual RC              ReportISATestCoverage(const string & instr);

    // Helper methods
    //!< last chance to validate proper form.
    virtual int             CommitInstruction(PgmInstruction *pOp,
                                              bool bUpdateVarMaybe = false,
                                              bool bApplyScoring = true);
    virtual int             FilterTexTarget(int target) const;
    virtual bool            IsTexTargetValid( int OpId,
                                              int TxFetcher,
                                              int TxAttr);
    //!< Opportunity to add special instructions just after the "main:" statement.
    virtual void            MainSequenceStart();
    //!< Opportunity to add special instruction just before the "END" statement.
    virtual void            MainSequenceEnd();

    virtual void InitMailwars (Block * pMainBlock) = 0;
    virtual void            AddLayeredOutputRegs(int LayerReg, int VportIdxReg, int VportMaskReg);

    void MarkRegUsed(int regIndex);
    void UpdateRegScore(int regIndex, int mask, int newScore);

    void CommitInstText(PgmInstruction *pOp, const string &str, bool bCountStatement = true);
    void CommitInstText(PgmInstruction *pOp, const char *szComment, bool bCountStatement = true);
    void CommitInstText(PgmInstruction *pOp, bool bCountStatement = true);

    void PgmAppend (const string& s);
    void PgmAppend (const char * s);
    void PgmOptionAppend(const string& s);
    void PgmOptionAppend(const char *s);

    const RegState & GetVar (int id) const;

    // Utility methods to configure a PgmInstruction object.
    void UtilOpDst (PgmInstruction * pOp, int opCode, int dstReg);
    void UtilOpDst (PgmInstruction * pOp, int opCode, int dstReg, int outMask, int dstDT);
    void UtilOpInReg (PgmInstruction * pOp, const RegUse & regUseToCopyFromOtherOp);
    void UtilOpInReg (PgmInstruction * pOp, int regId);
    void UtilOpInReg (PgmInstruction *pOp, int regId, int swizComp);
    void UtilOpInReg (PgmInstruction *pOp,int regId,
                      int swiz0,int swiz1,int swiz2,int swiz3);

    void UtilOpInConstU32 (PgmInstruction * pOp, UINT32 inUI);
    void UtilOpInConstF32 (PgmInstruction * pOp, FLOAT32 inF);
    void UtilOpInConstF32 (PgmInstruction * pOp, FLOAT32 inFx, FLOAT32 inFy, FLOAT32 inFz, FLOAT32 inFw);
    void UtilOpInConstS32 (PgmInstruction * pOp, INT32 inSx, INT32 inSy, INT32 inSz, INT32 inSw);
    void UtilOpCommit (GpuPgmGen::PgmInstruction * pOp, const char * comment = NULL, bool bApplyScoring = true); //$

    // Utility methods to add basic instruction to the current program block.
    void UtilBranchOp( int si);
    void UtilCopyConstVect( int DstReg, float Vals[4]);
    void UtilCopyConstVect( int DstReg, int Vals[4]);
    void UtilCopyReg( int DstReg, int SrcReg);
    void UtilCopyOperand(PgmInstruction *pOp, int DstReg, int InputRegIdx, int WriteMask = 0);
    void UtilSanitizeInput(PgmInstruction *pOp, int GarbageReg, int TmpReg, int InputRegIdx, int WriteMask); //$
    void UtilSanitizeInputAgainstZero(PgmInstruction *pOp, int TmpReg, int InputRegIdx, int WriteMask); //$
    void UtilScaleRegToImage(PgmInstruction * pOp);

    void UtilProcessPassthruRegs();

    void UtilBinOp( int Instr, int DstReg, int Operand1, int Operand2);
    void UtilBinOp( int Instr, int DstReg, int Operand1, float Vals[4]);
    void UtilTriOp( int Instr, int DstReg, int Operand1, int Operand2, int Operand3);

    //  Utility method to get the proper Tx handle index.
    int UtilMapTxAttrToTxHandleIdx( UINT32 txAttr);

    // Utility method to create a complete instruction text.
    virtual string          FormatDataType( GLRandom::opDataType dt,
                                            GLRandom::opDataType defaultType);
    virtual string          FormatExtSwizzle( UINT08* pSwizzle, int numComp);
    virtual string          FormatInstruction(PgmInstruction *pOp);
    virtual string          FormatLiteral(PgmInstruction *pOp, int idx);
    virtual string          FormatOperand(PgmInstruction *pOp, int idx);
    virtual string          FormatAtomicModifier(int am);
    virtual string          FormatSwizzleSuffix( UINT08* pSwizzle, int numComp);
    virtual string          FormatTxOffset(PgmInstruction *pOp);
    virtual string          FormatTxgTxComponent(PgmInstruction *pOp);
    virtual string          FormatArrayIndex(const RegUse & ru);
    virtual string          FormatccMask(PgmInstruction *pOp);
    virtual string          FormatccTest(PgmInstruction *pOp);
    virtual string          FormatTargetName(int TxAttr);
    //!< String containing the program header
    string                  m_PgmHeader;

private:
    void ApplyWAR1815953(PgmInstruction *pOp);

    // Methods to support program linking
    bool IsPropertyBuiltIn(GLRandom::ProgProperty pp);
    void UpdatePgmRequirements();
    void InitVarRqdFromPgmRequirements();
    bool CheckImageUnitRequirements(
        PgmInstruction *pOp,
        ImageUnit& rImg,
        int Access);
    bool IsImageUnitUsed(GLRandom::PgmRequirements::TxfMap& rTxfMap, int Txf)
    { return rTxfMap.count(Txf) && rTxfMap[Txf].IU.unit >= 0; }
    // Memory management routines.
    void ResetBlocks();
    bool ResizeOps(int numOps);

    void ShuffleAtomicModifiers();
    int GetMemAccessAlignment(GLRandom::opDataType dt);
    void PickIndexing(RegUse * pRegUse);

    // Method to support bindless textures
    void LoadTxHandle(PgmInstruction * pOp);

    void FixRandomLiterals(PgmInstruction *pOp, int idx);

public:
    // public data (BAD, fix this):
    GenState                m_GS;
    static const OpData     s_GpuOpData4 [ RND_GPU_PROG_OPCODE_NUM_OPCODES ];

protected:
    // protected data (BAD, fix this):
    //!< FancyPicker context for non-user controlled picks.
    FancyPicker::FpContext *m_pFpCtx;

    //!< The type of template we should be generating.
    int                     m_Template;
    vector <UINT32>         m_Scores;

    //-------------------------------------------------------------------------
    // These variables are not directly accessed by methods in this class.
    // They are here as a colwience for the derived class.
    FancyPickerArray *      m_pPickers;
    GLRandomTest *          m_pGLRandom;

    //! Holds a constraint table for ATOMic and STORE ops.
    static const UINT32     s_OpConstraints[ocNUM];
    vector <UINT32>         m_AtomicModifiers;

    //!< Current index into the AtomicModifiers vector
    int                     m_AmIdx;

    //The extension that dictates the correct BNF grammar to use.
    GLRandomTest::ExtensionId m_BnfExt;

    //The max size of the pgmElw[]
    int                     m_ElwSize;

    //Does this program write to multiple layers/viewports
    bool                    m_ThisProgWritesLayerVport;

private:
    // Private data:
    const pgmKind m_PgmKind;
};

//------------------------------------------------------------------------------
// VxGpuPgmGen
//
//  This class is a subclass for the GpuPgmGmr4_0 class and implements the
//  following:
//  1)Defines the Vertex programming environment
//  2)Implements the LW_vertex_program4 grammar.
//  3)Implements the abstract interfaces defined by GpuPgmGrm4_0 class.
//------------------------------------------------------------------------------
class VxGpuPgmGen : public GpuPgmGen
{
public:
    VxGpuPgmGen
    (
        GLRandomTest * pGLRandom,
        FancyPickerArray *pPicker
    );
    ~VxGpuPgmGen();

    virtual void GenerateRandomProgram
    (
        GLRandom::PgmRequirements *pRqmts,
        Programs * pProgramStorage,
        const string & pgmName
    );
    virtual void GenerateFixedPrograms
    (
        Programs * pProgramStorage
    );

    virtual RC GenerateXfbPassthruProgram
    (
        Programs * pProgramStorage
    );

protected:
    //Initialize the programming environment
    virtual void            InitPgmElw(GLRandom::PgmRequirements *pRqmts);
    virtual UINT32 CalcScore(PgmInstruction *pOp, int reg, int argIdx);

    // Overloaded Program Environment Accessors
    virtual int             GetNumTxFetchers();

    // Routines to support the grammar
    virtual int             GetConstReg() const;
    virtual int             GetConstVectReg() const;
    virtual int             GetIndexReg(IndexRegType t) const;
    virtual bool            IsIndexReg(int regId) const;
    virtual int             GetFirstTempReg() const;
    virtual int             GetGarbageReg() const;
    virtual int             GetLiteralReg() const;
    virtual int             GetFirstTxCdReg() const;
    virtual int             GetLastTxCdReg() const;
    virtual int             GetLocalElwReg() const;
    virtual int             GetNumTempRegs() const;
    virtual int             GetMinOps() const;
    virtual int             GetMaxOps() const;
    virtual int             GetPaboReg() const;
    virtual int             GetSubsReg() const;
    virtual GLRandom::ProgProperty  GetProp(int reg) const;
    virtual string          GetRegName(int reg) const;
    //!< return a texture coordinate for this register or <0 on error.
    virtual int             GetTxCd(int reg) const;

    //!< Add special instructions just after the "main:" label
    virtual void            MainSequenceStart();

    //!< Add special instruction just befor the "END" statement.
    virtual void            MainSequenceEnd();
    virtual void            OptionSequence();

    // Random picks that are required by the GpuPgmGen class.
    virtual int             PickNeg() const;
    virtual int             PickAbs() const;
    virtual int             PickCCtoRead() const;     // 0 or 1
    virtual int             PickCCTest(PgmInstruction *pOp) const;//"EQ","NE",etc
    virtual int             PickMultisample() const;
    virtual int             PickNumOps() const;
    virtual int             PickOp() const;
    virtual int             PickOpModifier() const;
    virtual int             PickPgmTemplate() const;
    virtual int             PickRelAddr() const;
    virtual int             PickResultReg() const;
    virtual int             PickSwizzleSuffix() const;
    virtual int             PickSwizzleOrder() const;
    virtual int             PickWriteMask() const;
    virtual int             PickTempReg() const;
    virtual int             PickTxOffset() const;
    virtual int             PickTxTarget() const;

    // grammar specific overloads

    virtual void InitMailwars (Block * pMainBlock);

private:
    void GenFixedAttrUse(Program * pProg, int whichAttr);
    void GenFixedIdxLight(Program * pProg, int firstReg);
    void GenFixedOpUse(Program * pProg, int whichOp);
    void GenFixedOpUse11(Program * pProg, int whichOp);
    void GenFixedPosIlwar(Program * pProg);
    void GetHighestSupportedExt();
    int  GetPropIOAttr(ProgProperty) const;

};

//------------------------------------------------------------------------------
// TessGpuPgmGen -- Base class for stubbing-out tessellation-program-generators
//------------------------------------------------------------------------------
class TessGpuPgmGen : public GpuPgmGen
{
public:
    TessGpuPgmGen
    (
        GLRandomTest * pGLRandom,
        FancyPickerArray *pPicker,
        pgmKind           kind
    );
    ~TessGpuPgmGen();

protected:
    virtual void            InitPgmElw(GLRandom::PgmRequirements *pRqmts);

    // Routines to support the grammar
    virtual int             GetConstReg() const;
    virtual int             GetConstVectReg() const;
    virtual int             GetIndexReg(IndexRegType t) const;
    virtual bool            IsIndexReg(int regId) const;
    virtual int             GetFirstTempReg() const;
    virtual int             GetLiteralReg() const;
    virtual int             GetFirstTxCdReg() const;
    virtual int             GetGarbageReg() const;
    virtual int             GetLastTxCdReg() const;
    virtual int             GetLocalElwReg() const;
    virtual int             GetNumTempRegs() const;
    virtual int             GetMinOps() const;
    virtual int             GetMaxOps() const;
    virtual int             GetPaboReg() const;
    virtual int             GetSubsReg() const;
    virtual GLRandom::ProgProperty  GetProp(int reg) const;
    virtual string          GetRegName(int reg) const;

    //!< return a texture coordinate for this register or <0 on error.
    virtual int             GetTxCd(int reg) const;

    // Random picks that are required by the GpuPgmGen class.
    virtual int             PickNeg() const;
    virtual int             PickAbs() const;
    virtual int             PickMultisample() const;
    virtual int             PickNumOps() const;
    virtual int             PickOp() const;
    virtual int             PickPgmTemplate() const;
    virtual int             PickRelAddr() const;
    virtual int             PickResultReg() const;
    virtual int             PickTempReg() const;
    virtual int             PickTxOffset() const;
    virtual int             PickTxTarget() const;

    // grammar specific overloads

    virtual void InitMailwars (Block * pMainBlock);

private:
    int  GetPropIOAttr(ProgProperty) const;
};

//------------------------------------------------------------------------------
// TcGpuPgmGen: Generates tessellation-control gpu programs.
//------------------------------------------------------------------------------
class TcGpuPgmGen : public TessGpuPgmGen
{
public:
    enum
    {
        // Layout of vertex-program const regs:
        //
        //  0..3 and 4..7 are transform matrices for vertex programs,
        //  but tessellation control programs uses 0 and 1 for tess-rates,
        //  leaving 2..7 filled with {1.0,1.0.1.0.1.0}.
        TessOuterReg = 0,
        TessInnerReg = 1,
    };

    TcGpuPgmGen
    (
        GLRandomTest * pGLRandom,
        FancyPickerArray *pPicker
    );
    ~TcGpuPgmGen();

    virtual void GenerateRandomProgram
    (
        GLRandom::PgmRequirements *pRqmts,
        Programs * pProgramStorage,
        const string & pgmName
    );
    virtual void GenerateFixedPrograms
    (
        Programs * pProgramStorage
    );

private:
    void GenerateProgram
    (
        GLRandom::PgmRequirements *pRqmts,
        Program * pProg
    );
};

//------------------------------------------------------------------------------
// TeGpuPgmGen: Generates tessellation-evaluation gpu programs.
//------------------------------------------------------------------------------
class TeGpuPgmGen : public TessGpuPgmGen
{
public:
    enum
    {
        // Layout of vertex-program const regs:
        //
        //  0..3 and 4..7 are transform matrices for vertex programs,
        //  but tessellation eval programs uses 0.x for the "bulge" effect
        //  where we perturb position along the surface normal for inner vx.
        //  1..7 are unused and filled with {1.0,1.0,1.0,1.0}
        TessBulge    = 0
    };

    TeGpuPgmGen
    (
        GLRandomTest * pGLRandom,
        FancyPickerArray *pPicker
    );
    ~TeGpuPgmGen();

    virtual void GenerateRandomProgram
    (
        GLRandom::PgmRequirements *pRqmts,
        Programs * pProgramStorage,
        const string & pgmName
    );
    virtual void GenerateFixedPrograms
    (
        Programs * pProgramStorage
    );

private:
    void GenerateProgram
    (
        GLRandom::PgmRequirements *pRqmts,
        GLenum    tessMode,
        GLenum    tessSpacing,
        bool      isClockwise,
        Program * pProgram
    );
};

//------------------------------------------------------------------------------
//  1)Defines the Geometry programming environment
//  2)Implements the LW_geometry_program4 grammar.
//  3)Implements the abstract interfaces defined by GpuPgmGen class.
//------------------------------------------------------------------------------
class GmGpuPgmGen : public GpuPgmGen {

public:
    enum {
        // result[n].position = vertex[n].position
        actPassThru,

        // result[n].position = vertex[n].position + offset
        actOffset,

        // result[n].position = LRP(vertex[n].position, vertex[n+1].position) + offset
        actInterpOffset,
    };
    struct GmFixedPgms
    {
        GLint primIn; // input primitive type
        GLint primOut; //output primitive type
        float vxMultiplier;
        const char * name;
        const char * pgm;
    };

    GmGpuPgmGen(GLRandomTest * pGLRandom, FancyPickerArray *pPicker);
    ~GmGpuPgmGen();

    // Interfaces required by GpuPgmGen
    virtual void GenerateRandomProgram
    (
        GLRandom::PgmRequirements *pRqmts,
        Programs * pProgramStorage,
        const string & pgmName
    );
    virtual void GenerateFixedPrograms
    (
        Programs * pProgramStorage
    );

    virtual bool            IsLwrrGMProgPassThru() const;

protected:
    //Initialize the programming environment
    virtual void            InitPgmElw(GLRandom::PgmRequirements *pRqmts);
    virtual UINT32 CalcScore(PgmInstruction *pOp, int reg, int argIdx);

    // Routines to support the grammar
    // program environment accessors
    virtual int             GetConstReg() const;
    virtual int             GetConstVectReg() const;
    virtual int             GetIndexReg(IndexRegType t) const;
    virtual bool            IsIndexReg(int regId) const;
    virtual int             GetFirstTempReg() const;
    virtual int             GetGarbageReg() const;
    virtual int             GetLiteralReg() const;
    virtual int             GetFirstTxCdReg() const;
    virtual int             GetLastTxCdReg() const;
    virtual int             GetLocalElwReg() const;
    virtual int             GetNumTempRegs() const;
    virtual int             GetMinOps()   const;
    virtual int             GetMaxOps()   const;
    virtual int             GetPaboReg() const;
    virtual int             GetSubsReg() const;
    virtual GLRandom::ProgProperty  GetProp(int reg) const;

    // return a texture coordinate for this register or <0 on error.
    virtual int             GetTxCd(int reg) const;
    virtual string          GetRegName(int reg) const;

    // RandomPicker accessors.
    virtual int             PickAbs() const;
    virtual int             PickMultisample() const;
    virtual int             PickNeg() const;
    virtual int             PickNumOps() const;
    virtual int             PickOp() const;
    virtual int             PickPgmTemplate() const;
    virtual int             PickRelAddr() const;
    virtual int             PickResultReg() const;
    virtual int             PickTempReg() const;
    virtual int             PickTxOffset() const;
    virtual int             PickTxTarget() const;

    // grammar specific overloads
    virtual void            DeclSequence();
    virtual void            OptionSequence();
    virtual void            StatementSequence();

    virtual void InitMailwars (Block * pMainBlock);

private:
    //!< Temporary copy of the pgm requirements pointer.
    GLRandom::PgmRequirements * m_pPgmRqmts;
    //!< Number of input vertices that can be read
    int                     m_NumVtxIn;
    //!< Number of vertices per primitive to write.
    int                     m_NumVtxOut;
    //!< Number of geometry program instances.
    int                     m_NumIlwocations;
    //!< The current input vertex
    int                     m_IlwtxId;
    //!< is this program a passthrough?
    bool                    m_IsLwrrGMProgPassThru;

    void                    StatementSequenceEnd(int Action, float Blend);

    void DoVertex(int numStatements, int action, float blendFactor);
    void GetHighestSupportedExt();
    int  GetPropIOAttr(ProgProperty) const;
};

//------------------------------------------------------------------------------
// FrGpuPgmGen
//
//  This class is a subclass for the GpuPgmGen class and implements the
//  following:
//  1)Defines the Fragment programming environment
//  2)Implements the LW_fragment_program4 grammar.
//  3)Implements the abstract interfaces defined by GpuPgmGen class.
//------------------------------------------------------------------------------
class FrGpuPgmGen : public GpuPgmGen
{

public:
    enum {
        // Layout of fragment-program const regs:
        //
        //  0..7  Random values             (0.0 - 1.0)
        //  8..11 Random values             (-1.0 - 1.0)
        // 12     relative texture index    (0, 1, 2, 3)
        // 13     relative pgmElw index     (0-128,0-128,0-128,0-128) for lw50+

        // then, N directional lights:
        // a+0    light direction
        // a+1    light-eye-half angle
        // a+2    light color
        // a+3    light spelwlar power (x), next light index (y), light num%4 (z)
        //

        // Layout of fragment-program const regs:
        //
        //  0     relative texture index    (0, 1, 2, 3)
        //  1     relative pgmElw index     (0-128,0-128,0-128,0-128) for lw30+
        RandomNrmReg      = 0,
        RandomSgnNrmReg   = 8,
    };

    FrGpuPgmGen
    (
        GLRandomTest * pGLRandom,
        FancyPickerArray *pPicker
    );
    ~FrGpuPgmGen();

    //  Get vx attrib index to use for passing in primary color.
    //
    // For full coverage of vertex programs, we get material color from
    // various attributes:
    //  - always att_COL0 if vertex programs not enabled,
    //  - else one of 1,COL0,6,7,TEX4,TEX5,TEX6,TEX7
    GLRandom::VxAttribute Col0Alias() const;

    virtual void GenerateRandomProgram
    (
        GLRandom::PgmRequirements *pRqmts,
        Programs * pProgramStorage,
        const string & pgmName
    );
    virtual void GenerateFixedPrograms
    (
        Programs * pProgramStorage
    );

protected:
    virtual UINT32          CalcScore(PgmInstruction *pOp, int reg, int argIdx);
    //!< Initialize the programming environment
    virtual void            InitPgmElw();

    // Routines to support the grammar
    virtual int             GetConstReg() const;
    virtual int             GetConstVectReg() const;
    virtual int             GetIndexReg(IndexRegType t) const;
    virtual bool            IsIndexReg(int regId) const;
    virtual int             GetFirstTempReg() const;
    virtual int             GetLiteralReg() const;
    virtual int             GetFirstTxCdReg() const;
    virtual int             GetGarbageReg() const;
    virtual int             GetLastTxCdReg() const;
    virtual int             GetLocalElwReg() const;
    virtual int             GetNumTempRegs() const;
    virtual int             GetMinOps() const;
    virtual int             GetMaxOps() const;
    virtual int             GetPaboReg() const;
    virtual int             GetSubsReg() const;
    virtual GLRandom::ProgProperty  GetProp(int reg) const;
    virtual string          GetRegName(int reg) const;
    //!< return a texture coordinate for this register or <0 on error.
    virtual int             GetTxCd(int reg) const;
    virtual int             GetTxCdScalingMethod(PgmInstruction *pOp);

    // !< Opportunity to add special instruction just before the "END" statement.
    virtual void            MainSequenceEnd();

    virtual int             PickNeg() const;
    virtual int             PickAbs() const;
    virtual int             PickCCtoRead() const;     //!< 0 or 1
    virtual int             PickCCTest(PgmInstruction *pOp) const;
    virtual int             PickMultisample() const;
    virtual int             PickNumOps() const;
    virtual int             PickOp() const;
    virtual int             PickOpModifier() const;
    virtual int             PickPgmTemplate() const;
    virtual int             PickRelAddr() const;
    virtual int             PickResultReg() const;
    virtual int             PickSwizzleSuffix() const;
    virtual int             PickSwizzleOrder() const;
    virtual int             PickWriteMask() const;
    virtual int             PickTempReg() const;
    virtual int             PickTxOffset() const;
    virtual int             PickTxTarget() const;

    // grammar specific overloads
    virtual void            DeclSequence();
    virtual bool            SpecialInstruction(PgmInstruction *pOp);
    virtual void            OptionSequence();

    virtual bool            IsTexTargetValid( int OpId,
                                              int TxFetcher,
                                              int TxAttr);

    virtual void InitMailwars (Block * pMainBlock);

private:
    // These opcodes get appended to the s_GpuOpData[] to form a complete set.
    static const OpData     s_FrOpData4 [ RND_GPU_PROG_FR_OPCODE_NUM_OPCODES -
                                          RND_GPU_PROG_OPCODE_NUM_OPCODES];

    //! Use the ARB_draw_buffers extension to draw different output colors
    //! to each of multiple attached color surfaces.
    bool                    m_ARB_draw_buffers;

    //! Use to range check the IPAO instruction
    float                   m_MaxIplOffset;
    float                   m_MinIplOffset;
    int                     m_SboSubIdx;

    //! How we will finish the fragment-shader (RND_GPU_PROG_FR_END_STRATEGY).
    int                     m_EndStrategy;

    bool                    AddGuardedSboInst( PgmInstruction * pOp);
    void                    ApplyMrtFloorsweepStrategy();
    void                    GetHighestSupportedExt();
    void                    ProcessSboRequirements(PgmInstruction * pOp);
    void                    RestrictInputRegsForFloorsweep(RegState * pReg);
    bool                    UsingMrtFloorsweepStrategy() const;
    void                    SkipRegisterIfUnsupported(RegState * pReg);
    int                     GetPropIOAttr(ProgProperty) const;
    void                    ApplyCoarseShadingStrategy();

};

