#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdint>
#include <regex>
#include <fstream>
#include <exception>
#include <unordered_map>
#include <memory>
#include <cassert>

static const uint32_t HS_BOOTROM_ADDRESS = 0x10000000;

class match_error : public std::runtime_error {
public:
    explicit match_error(void);
};

match_error::match_error(void) : runtime_error("") {}

struct InstructionExelwted {
    size_t timestamp;
    size_t instructionNum;
    uint32_t pc;
    std::vector<uint8_t> machineCode;
    std::string instructionName;
    uint32_t sp;
    std::string info;
    size_t cyclesPerInstruction;

public:
    InstructionExelwted(std::string toParse);

    std::string toString(void) const;

    inline size_t getCyclesPerInstruction(void) const {
        return cyclesPerInstruction;
    }

};

// These CPIs are in the worst case.
// More accurate numbers can be obtained by adding callwlateCpi(void) to InstructionExelwted,
// and examining the info member.
// We can determine, for example, if a branch is not taken, and only charge 1 CPI instead of 4.
// CPI for wait, dmwait, and imwait are set to 1.
static std::unordered_map<std::string, size_t> instructionToCycles{
    {"ADD", 1},
    {"ADDD", 1},
    {"ADDSP", 2},
    {"AND", 1},
    {"ASR", 1},
    {"BR", 4},
    {"BREQ", 4},
    {"BRZ", 4},
    {"BRA", 4},
    {"BRNE", 4},
    {"BRC", 4},
    {"BRNC", 4},
    {"BRLT", 4},
    {"BRLTE", 4},
    {"BRGT", 4},
    {"BRGTE", 4},
    {"BRF0", 4},
    {"BRF1", 4},
    {"BRR2", 4},
    {"BRF3", 4},
    {"BRF4", 4},
    {"BRF5", 4},
    {"BRF6", 4},
    {"BRF7", 4},
    {"BRPL", 4},
    {"BRV", 4},
    {"CALL", 4},
    {"CCI", 1},
    {"CLR", 1},
    {"CLRB", 1},
    {"CMP", 1},
    {"CMPS", 1},
    {"CMPU", 1},
    {"CMPBREQ", 5},
    {"CMPBREQR", 5},
    {"CMPBRNE", 5},
    {"CPL", 1},
    {"CPLB", 1},
    {"DMFENCE", 1},
    {"DMREAD", 3},
    {"DMREAD2", 3},
    {"DMWAIT", 1},
    {"DMWRITE", 3},
    {"DMWRITE2", 3},
    {"SETDTAG", 3},
    {"DMBLK", 3},
    {"DMCLEAN", 3},
    {"DMTAG", 3},
    {"DMILW", 3},
    {"DMLVL", 3},
    {"GETB", 1},
    {"HALT", 1},
    {"IMBLK", 3},
    {"IMILW", 3},
    {"IMREAD", 3},
    {"IMREAD2", 3},
    {"IMTAG", 3},
    {"IMWAIT", 1},
    {"JMP", 4},
    {"LCALL", 4},
    {"LJMP", 4},
    {"LDD", 4},
    {"LDDSP", 4},
    {"LDX", 6},
    {"LDXB", 6},
    {"LSL", 1},
    {"LSLC", 1},
    {"LSR", 1},
    {"LSRC", 1},
    {"MRG", 1},
    {"MULS", 3},
    {"MULU", 3},
    {"MV", 1},
    {"MVI", 1},
    {"NEG", 1},
    {"NOP", 1},
    {"OR", 1},
    {"POP", 4},
    {"POPM", 19},
    {"POPMA", 19},
    {"POPMR", 24},
    {"POPMAR", 24},
    {"PUSH", 4},
    {"PUSHM", 16},
    {"RET", 7},
    {"RETI", 7},
    {"RSPR", 1},
    {"SCLRB", 1},
    {"SCPLB", 1},
    {"SETB", 1},
    {"SETHI", 1},
    {"SEXT", 1},
    {"SGETB", 1},
    {"SPUTB", 1},
    {"SSETB", 1},
    {"SGETB", 1},
    {"STD", 3},
    {"STDSP", 3},
    {"STX", 5},
    {"STXB", 5},
    {"SUB", 1},
    {"SUBC", 1},
    {"SWAP", 1},
    {"SXTR", 1},
    {"TST", 1},
    {"TRAP0", 5},
    {"TRAP1", 5},
    {"TRAP2", 5},
    {"TRAP3", 5},
    {"UDIV", 34},
    {"UXTR", 1},
    {"UMOD", 34},
    {"WAIT", 1},
    {"EVTSYNC", 1},
    {"WSPR", 2},
    {"XOR", 1},
};

InstructionExelwted::InstructionExelwted(std::string toParse) {
    static std::regex instructionRegex(
        "^\\t\\((\\d+)\\) "
        "(\\d+)::\\t"
        "PC=0x([[:xdigit:]]+):"
        "((?: [[:xdigit:]]{2})+) +"
        "([\\.[:upper:][:digit:]]+) *\\t"
        "SP\\{0x([[:xdigit:]]+)\\} +\t"
        "(.*)$"
        );
    std::smatch result;
    if(!std::regex_match(toParse, result, instructionRegex)) {
        throw match_error();
    }
    
    std::stringstream(result[1]) >> timestamp;
    std::stringstream(result[2]) >> instructionNum;
    std::stringstream pcSs(result[3]);
    pcSs << std::hex;
    pcSs >> pc;
    std::stringstream machineCodeSs(result[4]);
    unsigned int byte;
    while(machineCodeSs >> std::hex >> byte) {
        machineCode.push_back(byte);
    }
    instructionName = result[5];
    std::stringstream spSs(result[6]);
    spSs << std::hex;
    spSs >> sp;
    info = result[7];
    std::size_t dotLocation = instructionName.find('.');
    std::string instructionNameBase;
    if(dotLocation == std::string::npos) {
        instructionNameBase = instructionName;
    } else {
        instructionNameBase = instructionName.substr(0, dotLocation);
    }
    if(instructionToCycles.find(instructionNameBase) == instructionToCycles.end()) {
        std::cout << "Warning: no known CPI for " << instructionNameBase
                  << ".  Using default 1 CPI." << std::endl;
        instructionToCycles[instructionNameBase] = 1;
    }
    cyclesPerInstruction = instructionToCycles[instructionNameBase];
}

std::string InstructionExelwted::toString(void) const {
    std::stringstream stream;
    stream << "InstructionExelwted(";
    stream << "timestamp(" << timestamp << "), ";
    stream << "instructionNum(" << instructionNum << "), ";
    stream << "PC(0x" << std::setfill('0') << std::setw(8) << std::hex << pc << "), ";
    stream << "machineCode( ";
    for(auto byte : machineCode)
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)byte << " ";
    stream << "), ";
    stream << "instructionName(" << instructionName << "), ";
    stream << "SP(0x" << std::setfill('0') << std::setw(8) << std::hex << sp << "), ";
    stream << "info(" << info << "))";
    return stream.str();
}

class Instruction {
public:
    struct FunctionCalls {
        size_t nCalls;
        size_t callsCostInCycles;
    };
    
private:
    uint32_t address;
    std::vector<uint8_t> machineCode;
    std::string assembly;

    std::string sourceFile;
    size_t sourceLine;
    std::string objectFile;

    size_t timesExelwted;
    size_t cyclesExelwted;

    std::unordered_map<uint32_t, FunctionCalls> functionCalls;

public:
    Instruction(uint32_t address, std::vector<uint8_t> machineCode,
                std::string assembly, std::string sourceFile,
                size_t sourceLine, std::string objectFile);

    inline void addCycles(size_t cycles) {
        timesExelwted++;
        cyclesExelwted += cycles;
    }

    inline const std::string &getSourceFile(void) const {
        return sourceFile;
    }

    inline const std::string &getObjectFile(void) const {
        return objectFile;
    }

    inline const std::string &getAssembly(void) const {
        return assembly;
    }

    inline size_t getSourceLine(void) const {
        return sourceLine;
    }

    inline uint32_t getAddress(void) const {
        return address;
    }

    inline size_t getCycles(void) const {
        return cyclesExelwted;
    }

    inline size_t getTimesExelwted(void) const {
        return timesExelwted;
    }

    inline const std::vector<uint8_t> &getMachineCode(void) const {
        return machineCode;
    }

    const std::unordered_map<uint32_t, FunctionCalls> &getFunctionCalls(void) const {
        return functionCalls;
    }

    void addFunctionCall(uint32_t functionAddress, size_t cost);

    std::string toString(void) const;
};

Instruction::Instruction(uint32_t address, std::vector<uint8_t> machineCode,
                         std::string assembly, std::string sourceFile,
                         size_t sourceLine, std::string objectFile)
    : address(address), machineCode(machineCode), assembly(assembly),
      sourceFile(sourceFile), sourceLine(sourceLine), objectFile(objectFile),
      timesExelwted(0), cyclesExelwted(0) {}

void Instruction::addFunctionCall(uint32_t functionAddress, size_t cost) {
    if(functionCalls.find(functionAddress) == functionCalls.end()) {
        // insert a new FunctionCall
        functionCalls[functionAddress] = FunctionCalls{1, cost};
    } else {
        FunctionCalls functionAddressCalls = functionCalls[functionAddress];
        functionAddressCalls.nCalls++;
        functionAddressCalls.callsCostInCycles += cost;
        functionCalls[functionAddress] = functionAddressCalls;
    }
}

std::string Instruction::toString(void) const {
    std::stringstream stream;
    stream << "Instruction(";
    stream << "address(0x" << std::setfill('0') << std::setw(8) << std::hex << address << "), ";
    stream << "machineCode( ";
    for(auto byte : machineCode) {
        stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)byte << " ";
    }
    stream << "), ";
    stream << "assembly(" << assembly << "), ";
    stream << "sourceFile(" << sourceFile << "), ";
    stream << "sourceLine(" << sourceLine << "), ";
    stream << "timesExelwted(" << timesExelwted << "), ";
    stream << "cyclesExelwted(" << cyclesExelwted << "))";
    return stream.str();
}

static std::unordered_map<uint32_t, std::shared_ptr<Instruction>> instructionAddressToInstruction;
static std::unordered_map<uint32_t, uint32_t> instructionAddressToFunctionAddress;
static std::unordered_map<uint32_t, std::string> functionAddressToFunctionName;

struct FunctionLabel {
    uint32_t functionAddress;
    std::vector<std::shared_ptr<Instruction>> instructions;
};

static std::vector<FunctionLabel> functionLabels;

static void parseDump(const char *dumpFile) {
    std::string lwrrentFile;
    size_t lwrrentLine;
    uint32_t lwrrentFunctionAddr = 0;
    std::string lwrrentObjectFile;
    std::ifstream dumpStream(dumpFile, std::ios::in);
    if(dumpStream.is_open()) {
        std::string line;
        while(std::getline(dumpStream, line)) {
            static std::regex instructionRegex(
                "^ *([[:xdigit:]]+):\\t"
                "((?:[[:xdigit:]]{2} )+) *\\t *"
                "(.*;)$"
                );

            static std::regex fileLineRegex(
                "^(.+):(\\d+)$"
                );

            static std::regex emptyRegex(
                "^$"
                );

            static std::regex headerRegex(
                "^(.+): *file format (.+)$"
                );

            static std::regex sectionRegex(
                "^Disassembly of section (.+):$"
                );

            static std::regex symbolRegex(
                "^([[:xdigit:]]+) <(.+)>:$"
                );

            static std::regex functionRegex(
                "^(.+)\\(\\):$"
                );

            static std::regex ellipsisRegex(
                "^\\t...$"
                );

            std::smatch result;

            if(std::regex_match(line, result, emptyRegex)) {

            } else if(std::regex_match(line, result, headerRegex)) {
                lwrrentObjectFile = result[1];
            } else if(std::regex_match(line, result, sectionRegex)) {

            } else if(std::regex_match(line, result, symbolRegex)) {
                std::stringstream(result[1]) >> std::hex >> lwrrentFunctionAddr;

                // add to global function list
                functionLabels.push_back(FunctionLabel{lwrrentFunctionAddr,
                            std::vector<std::shared_ptr<Instruction>>()});

                // add to map of function names
                functionAddressToFunctionName[lwrrentFunctionAddr] = result[2];
            } else if(std::regex_match(line, result, functionRegex)) {

            } else if(std::regex_match(line, result, instructionRegex)) {
                uint32_t instrAddress;
                std::stringstream(result[1]) >> std::hex >> instrAddress;

                std::vector<uint8_t> machineCode;
                std::stringstream machineCodeSs(result[2]);
                unsigned int byte;
                while(machineCodeSs >> std::hex >> byte) {
                    machineCode.push_back(byte);
                }

                if(functionLabels.empty()) {
                    throw std::logic_error("Encountered an instruction before a function label");
                }
                functionLabels.back().instructions.push_back(
                    std::shared_ptr<Instruction>(new Instruction(
                        instrAddress, machineCode, result[3], lwrrentFile,
                        lwrrentLine, lwrrentObjectFile)));

                instructionAddressToInstruction[instrAddress] = functionLabels.back().instructions.back();
                instructionAddressToFunctionAddress[instrAddress] = lwrrentFunctionAddr;
            } else if (std::regex_match(line, result, fileLineRegex)) {
                lwrrentFile = result[1];
                std::stringstream(result[2]) >> lwrrentLine;
            } else if(std::regex_match(line, result, ellipsisRegex)) {

            } else {
                std::cout << "Could not match " << line << std::endl;
                return;
            }
        }
    } else {
        std::cout << "Could not open file " << dumpFile << std::endl;
        throw std::runtime_error("IO");
    }
}

static void parseTrace(void) {
    std::string line;

    bool controlIsInHsBootrom = false;

    struct StackFrame {
        uint32_t callingInstruction;
        uint32_t sp;
        uint32_t calledFunctionAddress;
        size_t cyclesInFunctionBody;
        size_t cyclesInFunctionCalls;
    };

    struct StackContext {
        uint32_t prevInstr;
        uint32_t prevSp;
        uint32_t prevFuncAddr;
        bool prevInstrValid;
        std::stack<StackFrame> callStack;
    };

    // this is the key in @stacks corresponding to the current StackContext
    uint32_t lwrrentStackFrameSp;
    std::unordered_map<uint32_t, std::shared_ptr<StackContext>> stacks;
    std::shared_ptr<StackContext> lwrrentCtx;

    auto switchStackContext = [&] (uint32_t stackFrameSp) {
        lwrrentStackFrameSp = stackFrameSp;
        
        if(stacks.find(stackFrameSp) == stacks.end()) {
            // set up a new stack
            stacks[lwrrentStackFrameSp] = std::shared_ptr<StackContext>(new StackContext{
                    UINT32_MAX, stackFrameSp, UINT32_MAX, false, std::stack<StackFrame>()});
        } else {
            // just restore the context of an old stack
        }
        
        lwrrentCtx = stacks[stackFrameSp];
    };

    switchStackContext(UINT32_MAX);

    auto callStackPush = [&] (uint32_t calledFunctionAddress) {
        lwrrentCtx->callStack.push(StackFrame{lwrrentCtx->prevInstr,
                    lwrrentCtx->prevSp,
                    calledFunctionAddress, 0, 0});
    };

    auto callStackPop = [&] () {
        // charge the calledFunctionAddress with the current StackFrame's cycles
        
        StackFrame stackFrame = lwrrentCtx->callStack.top();
        lwrrentCtx->callStack.pop();
                        
        size_t callCost = stackFrame.cyclesInFunctionBody + stackFrame.cyclesInFunctionCalls;
        if(!lwrrentCtx->callStack.empty()) {
            lwrrentCtx->callStack.top().cyclesInFunctionCalls += callCost;
            instructionAddressToInstruction[stackFrame.callingInstruction]
                ->addFunctionCall(stackFrame.calledFunctionAddress, callCost);
        }
    };

    auto flushLwrrentCallStackCosts = [&] () {
        lwrrentCtx->prevInstrValid = false;
        controlIsInHsBootrom = false;
        while (!lwrrentCtx->callStack.empty()) {
            callStackPop();
        }
    };

    auto flushCostsOnAllStacks = [&] () {
        while(!stacks.empty()) {
            flushLwrrentCallStackCosts();
            stacks.erase(lwrrentStackFrameSp);
            if(stacks.empty()) {
                lwrrentStackFrameSp = UINT32_MAX;
            } else {
                lwrrentStackFrameSp = stacks.begin()->first;
            }
        }

        // reset the state of the stacks in case we restart the CPU and execute more instructions
        switchStackContext(UINT32_MAX);
        
    };

    size_t lineNumber = 0;
    size_t charsRead = 0;
    while(std::getline(std::cin, line)) {
        lineNumber++;
        charsRead += line.size();

        static const size_t LINE_UPDATE_INTERVAL = 10000;
        
        if(lineNumber % LINE_UPDATE_INTERVAL == 0) {
            std::cout << "Processed\t" << lineNumber << " lines\t(" << (double)charsRead/1024/1024
                      << " MB)" << std::endl;
        }
        
        // std::cout << line << std::endl;
        try {
            InstructionExelwted instr(line);
            if(controlIsInHsBootrom) {
                
                instructionAddressToInstruction[HS_BOOTROM_ADDRESS]->addCycles(
                    instr.getCyclesPerInstruction());

                lwrrentCtx->callStack.top().cyclesInFunctionBody += instr.getCyclesPerInstruction();

                if(instr.instructionName == "RETI") {
                    controlIsInHsBootrom = false;
                    callStackPop();
                }
            } else if(instructionAddressToInstruction.find(instr.pc) ==
                      instructionAddressToInstruction.end() ||
                      instr.machineCode != instructionAddressToInstruction[instr.pc]->getMachineCode())
            {
                if (instructionAddressToInstruction.find(instr.pc) ==
                    instructionAddressToInstruction.end()) {
                    std::cerr << "No instruction at address 0x" << std::hex
                              << instr.pc
                              << " in provided object files to match trace: "
                              << lineNumber << ": " << line << std::endl;
                } else {
                    std::cerr
                        << "Object file machine code for instruction at address 0x"
                        << std::hex << instr.pc << " did not match " << line
                        << std::endl;
                }
                std::cerr << "It's possible falcon is now exelwting bad code."
                          << std::endl;

                // TODO: add config option to continue exelwtion anyway
                throw std::logic_error("Unmatched instruction");
            } else {
                uint32_t functionAddress = instructionAddressToFunctionAddress[instr.pc];
                if(!lwrrentCtx->prevInstrValid) {
                    // First instruction.  Set up a new stack.
                    callStackPush(functionAddress);
                } else if (lwrrentCtx->prevInstrValid &&
                         functionAddress != lwrrentCtx->prevFuncAddr)
                {
                    // there was a call or return (call, ret, reti, popm . 0x1,
                    // popma . 0x1, jmp, etc...)
                    if(lwrrentCtx->prevSp < lwrrentCtx->callStack.top().sp) {
                        // call
                        callStackPush(functionAddress);
                    } else if(lwrrentCtx->prevSp > lwrrentCtx->callStack.top().sp) {
                        // return
                        if(lwrrentCtx->callStack.size() <= 1) {
                            throw std::logic_error("Tried to return from an empty call stack.");
                        } else {
                            callStackPop();
                        }
                    } else {
                        // We somehow jumped out of the previous function, but the
                        // call stack did not grow or shrink. For now, charge the
                        // cycles of the new function to the old function.
                        std::cerr << "Unexpected function call or return" << std::endl;
                    }
                }

                assert(!lwrrentCtx->callStack.empty());

                instructionAddressToInstruction[instr.pc]->addCycles(instr.getCyclesPerInstruction());
                
                lwrrentCtx->callStack.top().cyclesInFunctionBody += instr.getCyclesPerInstruction();

                uint32_t prevStackFrameSp = lwrrentCtx->prevSp;

                lwrrentCtx->prevInstr = instr.pc;
                lwrrentCtx->prevSp = instr.sp;
                lwrrentCtx->prevFuncAddr = functionAddress;
                lwrrentCtx->prevInstrValid = true;

                if (instructionAddressToInstruction[instr.pc]->getAssembly().find("wspr SP") == 0 &&
                    instr.sp != prevStackFrameSp)
                {
                    // falcon has switched stacks
                    if(lwrrentCtx->callStack.empty()) {
                        // There are no stack frames, so delete this stack.
                        stacks.erase(lwrrentStackFrameSp);
                    } else if(lwrrentCtx->callStack.size() == 1) {
                        // There is no caller of the current stack frame,
                        // so we might as well clean it up now.
                        callStackPop();
                        stacks.erase(lwrrentStackFrameSp);
                    } else {
                        // We need to keep the current stack context around
                        // in case we return to it later.

                        // Remap the old stack context to its new corresponding SP in case it changed.
                        stacks.erase(lwrrentStackFrameSp);
                        stacks[prevStackFrameSp] = lwrrentCtx;
                    }
                    switchStackContext(instr.sp);
                }
            }
        } catch(match_error e) {
            // Not an instruction. There may be other useful information.
            
            static std::regex startCpuRegex(
                "^\\t\\((\\d+)\\) INFO: CPUCTL Write to STARTCPU$"
                );

            static std::regex interruptRegex(
                "^\\t\\((\\d+)\\)\\t\\tTaking a level (\\d+) interrupt "
                "\\(STAT: 0x([[:xdigit:]]+)  MASK: 0x([[:xdigit:]]+)  DEST: 0x([[:xdigit:]]+)\\)$"
                );

            static std::regex exceptionRegex(
                "\\t\\((\\d+)\\)\\t\\tTaking an exception "
                "\\(CSW\\.E: 0x([[:xdigit:]]+)\\tPC: 0x([[:xdigit:]]+)\\tInstBytes: 0x([[:xdigit:]]+), "
                "EXCI=0x([[:xdigit:]]+), EXCI2=0x([[:xdigit:]]+)\\)$"
                );

            static std::regex exceptionInfoRegex(
                "E:: (.*)$"
                );

            static std::regex exceptionHaltInfoRegex(
                "EH:: (.*)$"
                );

            std::smatch result;

            if(std::regex_match(line, result, startCpuRegex)) {
                flushCostsOnAllStacks();
            } else if(std::regex_match(line, result, interruptRegex)) {
                lwrrentCtx->prevSp -= 4;
            } else if(std::regex_match(line, result, exceptionRegex)) {
                uint32_t exci;
                std::stringstream(result[5]) >> std::hex >> exci;
                uint32_t excause = exci >> 20;
                // 0x9 (invalid instruction) is always treated as a transition
                // into the HS bootrom.
                // Previously fmodel used 0xc especially for this purpose, so it
                // is added for backwards compatibility.
                if (excause == 0xc || excause == 0x9) {
                    controlIsInHsBootrom = true;

                    callStackPush(HS_BOOTROM_ADDRESS);
                } else {
                    lwrrentCtx->prevSp -= 4;
                }
            } else if(std::regex_match(line, result, exceptionInfoRegex)) {

            } else if(std::regex_match(line, result, exceptionHaltInfoRegex)) {

            } else {
                std::cout << "no match for \"" << line << "\"" << std::endl;
                exit(1);
            }
        }
    }

    if(std::cin.bad()) {
        std::cerr << "File IO error" << std::endl;
        throw std::runtime_error("IO");
    }

    // When we reach eof, we probably still have some stack frames. Make sure to
    // charge the calling functions with all the cycles on the call stack.
    flushCostsOnAllStacks();
}

static std::string removeLeadingUnderscore(const std::string &str) {
    if(str.empty()) {
        return str;
    } else {
        if(str[0] == '_') {
            return str.substr(1);
        } else {
            return str;
        }
    }
}

static void writeCallgrindOutput(const char *callgrindFile) {
    std::ofstream callgrindStream(callgrindFile, std::ios::out);

    callgrindStream
        << "positions: instr line"
        << std::endl
        << "events: Cycles Instructions"
        << std::endl
        << std::endl;

    for(auto functionLabel : functionLabels) {
        callgrindStream
            << "ob="
            << instructionAddressToInstruction[functionLabel.functionAddress]->getObjectFile()
            << std::endl
            << "fl="
            << instructionAddressToInstruction[functionLabel.functionAddress]->getSourceFile()
            << std::endl
            << "fn="
            << removeLeadingUnderscore(functionAddressToFunctionName[functionLabel.functionAddress])
            << std::endl;
        for(auto& instruction : functionLabel.instructions) {
            callgrindStream
                // << "ob="
                // << instruction->getObjectFile()
                // << std::endl
                << "fi="
                << instruction->getSourceFile()
                << std::endl
                << "0x" << std::hex << instruction->getAddress() << " "
                << std::dec << instruction->getSourceLine() << " "
                << instruction->getCycles() << " "
                << instruction->getTimesExelwted()
                << std::endl;

            for(auto functionCallsPair : instruction->getFunctionCalls()) {
                callgrindStream
                    << "cob="
                    << instructionAddressToInstruction[functionCallsPair.first]
                    ->getObjectFile() << std::endl
                    << "cfi="
                    << instructionAddressToInstruction[functionCallsPair.first]
                    ->getSourceFile() << std::endl
                    << "cfn=" << removeLeadingUnderscore(
                        functionAddressToFunctionName[
                                functionCallsPair.first]) << std::endl
                    << "calls=" << functionCallsPair.second.nCalls << " "
                    << "0x" << std::hex << functionCallsPair.first
                    << std::dec << " "
                    << instructionAddressToInstruction[functionCallsPair.first]
                    ->getSourceLine() << std::endl << "0x" << std::hex
                    << instruction->getAddress() << " " << std::dec
                    << instructionAddressToInstruction[instruction->getAddress()]
                    ->getSourceLine() << " "
                    << functionCallsPair.second.callsCostInCycles
                    << std::endl;
            }
        }
    }
    callgrindStream << std::endl;
}

static void addHsBootromCode(void) {
    std::shared_ptr<Instruction> instructionPtr = std::shared_ptr<Instruction>(
        new Instruction(HS_BOOTROM_ADDRESS, std::vector<uint8_t>(), "", "???", 0, "???"));
    
    // add to global function list
    functionLabels.push_back(FunctionLabel{HS_BOOTROM_ADDRESS,
                std::vector<std::shared_ptr<Instruction>>{instructionPtr}});

    // add to maps
    instructionAddressToFunctionAddress[HS_BOOTROM_ADDRESS] = HS_BOOTROM_ADDRESS;
    instructionAddressToInstruction[HS_BOOTROM_ADDRESS] = instructionPtr;
    functionAddressToFunctionName[HS_BOOTROM_ADDRESS] = "_HS_BOOTROM";
}

int main(int argc, const char *argv[]) {
    if(argc != 3) {
        std::cout << "Please do not run prof directly.  Instead use prof.sh" << std::endl;
        std::cout << "Usage: prof [INPUT FILE] [OUTPUT FILE]" << std::endl;
        throw std::runtime_error("usage");
    }
    
    parseDump(argv[1]);
    addHsBootromCode();
    
    parseTrace();

    writeCallgrindOutput(argv[2]);
    
    return 0;
}
