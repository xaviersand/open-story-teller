/*
The MIT License

Copyright (c) 2022 Anthony Rabine

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/


#include "chip32_assembler.h"

#include <sstream>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstdint>

namespace Chip32
{
// =============================================================================
// GLOBAL UTILITY FUNCTIONS
// =============================================================================
static const char* ws = " \t\n\r\f\v";

// trim from end of string (right)
static inline std::string& rtrim(std::string& s, const char* t = ws)
{
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from beginning of string (left)
static inline std::string& ltrim(std::string& s, const char* t = ws)
{
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from both ends of string (right then left)
static inline std::string& trim(std::string& s, const char* t = ws)
{
    return ltrim(rtrim(s, t), t);
}

static std::vector<std::string> Split(const std::string &theString)
{
    std::vector<std::string> result;
    std::istringstream iss(theString);
    for(std::string s; iss >> s; )
        result.push_back(s);
    return result;
}

static std::string ToLower(const std::string &text)
{
    std::string newText = text;
    std::transform(newText.begin(), newText.end(), newText.begin(), [](unsigned char c){ return std::tolower(c); });
    return newText;
}

static const RegNames AllRegs[] = { { R0, "r0" }, { R1, "r1" }, { R2, "r2" }, { R3, "r3" }, { R4, "r4" }, { R5, "r5" },
    { R6, "r6" }, { R7, "r7" }, { R8, "r8" }, { R9, "r9" }, { PC, "pc" }, { SP, "sp" }, { RA, "ra" }
};

static const uint32_t NbRegs = sizeof(AllRegs) / sizeof(AllRegs[0]);

// Keep same order than the opcodes list!!
static const std::string Mnemonics[] = {
    "nop", "halt", "syscall", "lcons", "mov", "push", "pop", "call", "ret", "store", "load", "add", "sub", "mul", "div",
    "shiftl", "shiftr", "ishiftr", "and", "or", "xor", "not", "jump", "jumpr", "skipz", "skipnz"
};

static OpCode OpCodes[] = OPCODES_LIST;

static const uint32_t nbOpCodes = sizeof(OpCodes) / sizeof(OpCodes[0]);

static bool IsOpCode(const std::string &label, OpCode &op)
{
    bool success = false;
    std::string lowLabel = ToLower(label);

    for (uint32_t i = 0; i < nbOpCodes; i++)
    {
        if (Mnemonics[i] == lowLabel)
        {
            success = true;
            op = OpCodes[i];
            break;
        }
    }
    return success;
}

static void GetArgs(Instr &instr, const std::string &data)
{
    std::string value;
    std::istringstream iss(data);
    while (getline(iss, value, ','))
    {
        instr.args.push_back(trim(value));
    }
}

static inline void leu32_put(std::vector<std::uint8_t> &container, uint32_t data)
{
    container.push_back(data & 0xFFU);
    container.push_back((data >> 8U) & 0xFFU);
    container.push_back((data >> 16U) & 0xFFU);
    container.push_back((data >> 24U) & 0xFFU);
}

static inline void leu16_put(std::vector<std::uint8_t> &container, uint16_t data)
{
    container.push_back(data & 0xFFU);
    container.push_back((data >> 8U) & 0xFFU);
}

#define GET_REG(name, ra) if (!GetRegister(name, ra)) {\
    std::stringstream ss; \
    ss << "ERROR! Bad register name: " << name << std::endl;\
    m_lastError = ss.str(); \
    return false; }

#define CHIP32_CHECK(instr, cond, error) if (!(cond)) { \
    std::stringstream ss; \
    ss << "error line: " << instr.line << ": " << error << std::endl; \
    m_lastError = ss.str(); \
    return false; } \

// =============================================================================
// ASSEMBLER CLASS
// =============================================================================
bool Assembler::GetRegister(const std::string &regName, uint8_t &reg)
{
    std::string lowReg = ToLower(regName);
    for (uint32_t i = 0; i < NbRegs; i++)
    {
        if (lowReg == AllRegs[i].name)
        {
            reg = AllRegs[i].reg;
            return true;
        }
    }
    return false;
}

bool Assembler::GetRegisterName(uint8_t reg, std::string &regName)
{
    for (uint32_t i = 0; i < NbRegs; i++)
    {
        if (reg == AllRegs[i].reg)
        {
            regName = AllRegs[i].name;
            return true;
        }
    }
    return false;
}

bool Assembler::CompileMnemonicArguments(Instr &instr)
{
    uint8_t ra, rb;

    switch(instr.code.opcode)
    {
    case OP_NOP:
    case OP_HALT:
    case OP_RET:
        // no arguments, just use the opcode
        break;
    case OP_SYSCALL:
        instr.compiledArgs.push_back(static_cast<uint8_t>(strtol(instr.args[0].c_str(),  NULL, 0)));
        break;
    case OP_LCONS:
        GET_REG(instr.args[0], ra);
        instr.compiledArgs.push_back(ra);
        // Detect address or immedate value
        if (instr.args[1].at(0) == '$') {
            instr.useLabel = true;
            leu32_put(instr.compiledArgs, 0); // reserve 4 bytes
        } else { // immediate value
            leu32_put(instr.compiledArgs, static_cast<uint32_t>(strtol(instr.args[1].c_str(),  NULL, 0)));
        }
        break;
    case OP_POP:
    case OP_PUSH:
    case OP_SKIPZ:
    case OP_SKIPNZ:
    case OP_JR:
        GET_REG(instr.args[0], ra);
        instr.compiledArgs.push_back(ra);
        break;
    case OP_MOV:
    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_DIV:
    case OP_SHL:
    case OP_SHR:
    case OP_ISHR:
    case OP_AND:
    case OP_OR:
    case OP_XOR:
    case OP_NOT:
        GET_REG(instr.args[0], ra);
        GET_REG(instr.args[1], rb);
        instr.compiledArgs.push_back(ra);
        instr.compiledArgs.push_back(rb);
        break;
    case OP_JMP:
    case OP_CALL:
        // Reserve 2 bytes for address, it will be filled at the end
        instr.useLabel = true;
        instr.compiledArgs.push_back(0);
        instr.compiledArgs.push_back(0);
        break;
    case OP_STORE: // store @r4, r1, 2
        CHIP32_CHECK(instr, instr.args[0].at(0) == '@', "Missing @ sign before register")
        instr.args[0].erase(0, 1);
        GET_REG(instr.args[0], ra);
        GET_REG(instr.args[1], rb);
        instr.compiledArgs.push_back(ra);
        instr.compiledArgs.push_back(rb);
        instr.compiledArgs.push_back(static_cast<uint32_t>(strtol(instr.args[2].c_str(),  NULL, 0)));
        break;
    case OP_LOAD:
        CHIP32_CHECK(instr, instr.args[1].at(0) == '@', "Missing @ sign before register")
        instr.args[1].erase(0, 1);
        GET_REG(instr.args[0], ra);
        GET_REG(instr.args[1], rb);
        instr.compiledArgs.push_back(ra);
        instr.compiledArgs.push_back(rb);
        instr.compiledArgs.push_back(static_cast<uint32_t>(strtol(instr.args[2].c_str(),  NULL, 0)));
        break;
    default:
        CHIP32_CHECK(instr, false, "Unsupported mnemonic: " << instr.mnemonic);
        break;
    }
    return true;
}

bool Assembler::CompileConstantArguments(Instr &instr)
{
    for (auto &a : instr.args)
    {
        // Check string
        if (a.size() > 2)
        {
            // Detected string
            if ((a[0] == '"') && (a[a.size() - 1] == '"'))
            {
                for (int i = 1; i < (a.size() - 1); i++)
                {
                    instr.compiledArgs.push_back(a[i]);
                }
                instr.compiledArgs.push_back(0);
                continue;
            }
        }

        // here, we check if the intergers are correct
        uint32_t intVal = static_cast<uint32_t>(strtol(a.c_str(),  NULL, 0));

        bool sizeOk = false;
        if (((intVal <= UINT8_MAX) && (instr.dataTypeSize == 8)) ||
            ((intVal <= UINT16_MAX) && (instr.dataTypeSize == 16)) ||
            ((intVal <= UINT32_MAX) && (instr.dataTypeSize == 32))) {
            sizeOk = true;
        }
        CHIP32_CHECK(instr, sizeOk, "integer too high: " << intVal);
        if (instr.dataTypeSize == 8) {
            instr.compiledArgs.push_back(intVal);
        } else if (instr.dataTypeSize == 16) {
            leu16_put(instr.compiledArgs, intVal);
        } else {
            leu32_put(instr.compiledArgs, intVal);
        }
    }

    return true;
}

bool Assembler::BuildBinary(std::vector<uint8_t> &program, Result &result)
{
    program.clear();
    result = { 0, 0, 0}; // clear stuff!

    // serialize each instruction and arguments to program memory, assign address to variables (rom or ram)
    for (auto &i : m_instructions)
    {
        if (i.isRamData)
        {
            result.ramUsageSize += i.dataLen * i.dataTypeSize/8;
        }
        else
        {
            if (i.isRomCode())
            {
                program.push_back(i.code.opcode);
            }
            result.constantsSize += i.compiledArgs.size();
            std::copy (i.compiledArgs.begin(), i.compiledArgs.end(), std::back_inserter(program));
        }
    }
    result.romUsageSize = program.size();
    return true;
}

bool Assembler::Parse(const std::string &data)
{
    std::stringstream data_stream(data);
    std::string line;

    Clear();
    int code_addr = 0;
    int ram_addr = 0;
    int lineNum = 0;
    while(std::getline(data_stream, line))
    {
        lineNum++;
        Instr instr;
        instr.line = lineNum;

        line = trim(line);

        int pos = line.find_first_of(";");
        if (pos != std::string::npos) {
            line.erase(pos);
        }

        if (line.length() <= 0) continue;

        // Split the line
        std::vector<std::string> lineParts = Split(line);

        CHIP32_CHECK(instr, (lineParts.size() > 0), " not a valid line");

        // Ok until now
        std::string opcode = lineParts[0];

        // =======================================================================================
        // LABEL
        // =======================================================================================
        if (opcode[0] == '.')
        {
            CHIP32_CHECK(instr, (opcode[opcode.length() - 1] == ':') && (lineParts.size() == 1), "label must end with ':'");
            // Label
            opcode.pop_back(); // remove the colon character
            instr.mnemonic = opcode;
            instr.isLabel = true;
            instr.addr = code_addr;
            CHIP32_CHECK(instr, m_labels.count(opcode) == 0, "duplicated label : " << opcode);
            m_labels[opcode] = instr;
            m_instructions.push_back(instr);
        }

        // =======================================================================================
        // INSTRUCTIONS
        // =======================================================================================
        else if (IsOpCode(opcode, instr.code))
        {
            instr.mnemonic = opcode;
            bool nbArgsSuccess = false;
            // Test nedded arguments
            if ((instr.code.nbAargs == 0) && (lineParts.size() == 1))
            {
                nbArgsSuccess = true; // no arguments, solo mnemonic
            }
            else if ((instr.code.nbAargs > 0) && (lineParts.size() >= 2))
            {
                // Compute arguments
                for (int i = 1; i < lineParts.size(); i++)
                {
                    GetArgs(instr, lineParts[i]);
                }

                CHIP32_CHECK(instr, instr.args.size() == instr.code.nbAargs,
                             "Bad number of parameters. Required: " << static_cast<int>(instr.code.nbAargs) << ", got: " << instr.args.size());
                nbArgsSuccess = true;
            }
            else
            {
                CHIP32_CHECK(instr, false, "Bad number of parameters");
            }

            if (nbArgsSuccess)
            {
                CHIP32_CHECK(instr, CompileMnemonicArguments(instr) == true, "Compile failure");

                instr.addr = code_addr;
                code_addr += 1 + instr.compiledArgs.size();
                m_instructions.push_back(instr);
            }
        }
        // =======================================================================================
        // CONSTANTS IN ROM OR RAM (eg: $yourLabel  DC8 "a string", 5, 4, 8  (DV32 for RAM)
        // C for Constant, V stands for Volatile
        // =======================================================================================
        else if (opcode[0] == '$')
        {
            instr.mnemonic = opcode;
            CHIP32_CHECK(instr, (lineParts.size() >= 3), "bad number of parameters");

            std::string type = lineParts[1];

            CHIP32_CHECK(instr, (type.size() >= 3), "bad data type size");
            CHIP32_CHECK(instr, (type[0] == 'D') && ((type[1] == 'C') || (type[1] == 'V')), "bad data type (must be DCxx or DVxx");
            CHIP32_CHECK(instr, m_labels.count(opcode) == 0, "duplicated label : " << opcode);

            instr.isRomData = type[1] == 'C' ? true : false;
            instr.isRamData = type[1] == 'V' ? true : false;
            type.erase(0, 2);
            instr.dataTypeSize = static_cast<uint32_t>(strtol(type.c_str(),  NULL, 0));

            if (instr.isRomData)
            {
                for (int i = 2; i < lineParts.size(); i++)
                {
                    GetArgs(instr, lineParts[i]);
                }
                CHIP32_CHECK(instr, CompileConstantArguments(instr), "Compile error, stopping.");

                instr.addr = code_addr;
                code_addr += instr.compiledArgs.size();
            }
            else // RAM DATA
            {
                instr.addr = ram_addr;
                instr.dataLen = static_cast<uint16_t>(strtol(lineParts[2].c_str(),  NULL, 0));
                ram_addr += instr.dataLen;
            }
            m_labels[opcode] = instr;
            m_instructions.push_back(instr);
        }
    }

    // 2. Second pass: replace all label or RAM data by the real address in memory
    for (auto &instr : m_instructions)
    {
        if (instr.useLabel && (instr.args.size() > 0))
        {
            // label is the first argument for jump, call, store
            // in second position for load!
            uint16_t argsIndex = instr.code.opcode == OP_LCONS ? 1 : 0;
            std::string label = instr.args[argsIndex];
            CHIP32_CHECK(instr, m_labels.count(label) > 0, "label not found: " << label);
            uint16_t addr = m_labels[label].addr;
            // std::cout << "LABEL: " << label << " , addr: " << addr << std::endl;
            instr.compiledArgs[argsIndex] = addr & 0xFF;
            instr.compiledArgs[argsIndex+1] = (addr >> 8U) & 0xFF;
            if (instr.code.opcode == OP_LCONS) {
                // We precise if we load from RAM or ROM
                instr.compiledArgs[argsIndex+3] = m_labels[label].isRamData ? 0x80 : 0;
            }

        }
    }

    return true;
}

} // namespace Chip32
