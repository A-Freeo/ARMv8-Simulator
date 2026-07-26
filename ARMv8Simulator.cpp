#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
#include <iomanip>
using namespace std;

// Commands
enum class Opcode {
    LDR, STR, ADD, SUB, MOV, MOVZ, MOVK ,PRINT, CBZ, CMP, B, DUMP, HLT
};

// Types of operands
enum class OperandType {
    NONE, REGISTER, IMMEDIATE, LABEL
};

// Whole operand + holds type so it can access the correct value (immediate / label / register)
struct Operand {
    OperandType type = OperandType::NONE;
    string reg;
    uint64_t imm = 0;
    string label;

    int resolvedTarget = -1;   // only used for LABEL operands
};

// Instructions, possible holders (e.g. shift type + amount)
struct Instruction {
    Opcode op;
    Operand arg1;
    Operand arg2;
    Operand arg3;
    string shiftType;
    uint64_t shiftImm; 
    string cmpType;
};

// Holds the parsed instructions plus a label -> instruction index map used to resolve branches
struct Program {
    vector<Instruction> instructions;
    map<string, int> labels;
};

class Parser {
public:
    Program parse(const string& path) {
         // scans first token of a line -> seperates lables & commands
        Program program;
        ifstream inputfile(path);
        string line;

        while (getline(inputfile, line)) {
            istringstream lineStream(line);
            string firstToken;

            if (!(lineStream >> firstToken)) continue;
            if (firstToken.back() == ':') { // If back is : then its a label
                string label = firstToken.substr(0, firstToken.size() - 1); // isolate just the label name not the :
                program.labels[label] = program.instructions.size(); // label is the key and value is the size / index of instruction after label
            } else {
                // needs to be tokenized before resolveLabels
                program.instructions.push_back(tokenizeLine(line));
            }
        }
        resolveLabels(program);
        return program;
    }

private:
    Instruction tokenizeLine(const string& line) {
        // tokenizes line, commands & args
        istringstream lineStream(line);
        Instruction instr;
        string opcodeStr;
        lineStream >> opcodeStr;

        if (opcodeStr == "LDR")        instr.op = Opcode::LDR;
        else if (opcodeStr == "STR")   instr.op = Opcode::STR;
        else if (opcodeStr == "ADD")   instr.op = Opcode::ADD;
        else if (opcodeStr == "SUB")    instr.op = Opcode::SUB;
        else if (opcodeStr == "MOV")   instr.op = Opcode::MOV;
        else if (opcodeStr == "MOVZ")  instr.op = Opcode::MOVZ;
        else if (opcodeStr == "MOVK")  instr.op = Opcode::MOVK;
        else if (opcodeStr == "PRINT") instr.op = Opcode::PRINT;
        else if (opcodeStr == "CBZ")    instr.op = Opcode::CBZ;
        else if (opcodeStr == "CMP")    instr.op = Opcode::CMP;
        else if (opcodeStr == "B")   instr.op = Opcode::B;
        // For B.COMPARISON
        else if (opcodeStr[0] == 'B')   instr.op = Opcode::B;
        else if (opcodeStr == "DUMP")  instr.op = Opcode::DUMP;
        else if (opcodeStr == "HLT")  instr.op = Opcode::HLT;
        else {
            cerr << "ERROR: unknown command '" << opcodeStr << "'\n";
            exit(1);
        }

        switch (instr.op) {
            // Both take a register followed by an address, either a register or an immediate (immediate must represent a memory address). In addition also a shift operator
            case Opcode::LDR:
            case Opcode::STR: {
                string regStr, addrStr, thirdTokenStr, shiftKeywordStr, shiftAmountStr;
                lineStream >> regStr >> addrStr;

                requireComma(regStr);
                regStr = sanitize(regStr);
                string addrStrRaw = addrStr;
                addrStr = sanitize(addrStr);

                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr;

                if (lineStream >> thirdTokenStr) {
                    requireComma(addrStrRaw);
                    string thirdTokenStrRaw = thirdTokenStr;
                    thirdTokenStr = sanitize(thirdTokenStr);

                    if (thirdTokenStr == "LSL" || thirdTokenStr == "LSR") {
                        // SCENARIO A: "STR X0, X1, LSL #3" (2 operands + shift)
                        // This means the destination (X0) is also the first source register
                        instr.arg2 = instr.arg1;

                        // Parse X1 as source 2
                        if (addrStr[0] == 'X' || addrStr == "SP") {
                            instr.arg3.type = OperandType::REGISTER;
                            instr.arg3.reg = addrStr;
                        } else {
                            instr.arg3.type = OperandType::IMMEDIATE;
                            instr.arg3.imm = parseImmediate(addrStr);
                        }

                        // thirdTokenStr is the shift type, read the next token for the shift amount
                        instr.shiftType = thirdTokenStr;
                        lineStream >> shiftAmountStr;
                        instr.shiftImm = parseImmediate(sanitize(shiftAmountStr));

                    } else {
                        // SCENARIO B: "ADD X0, X1, X2..." or "ADD X0, X1, #5..." (3 operands)
                        // Parse operandStr as source 1
                        if (addrStr[0] == 'X' || addrStr == "SP") {
                            instr.arg2.type = OperandType::REGISTER;
                            instr.arg2.reg = addrStr;
                        } else {
                            instr.arg2.type = OperandType::IMMEDIATE;
                            instr.arg2.imm = parseImmediate(addrStr);
                        }

                        // Parse thirdTokenStr as source 2
                        if (thirdTokenStr[0] == 'X' || thirdTokenStr == "SP") {
                            instr.arg3.type = OperandType::REGISTER;
                            instr.arg3.reg = thirdTokenStr;
                        } else {
                            instr.arg3.type = OperandType::IMMEDIATE;
                            instr.arg3.imm = parseImmediate(thirdTokenStr);
                        }

                        // Check if there is an optional shift after the 3rd operand (e.g., ADD X0, X1, X2, LSL #3)
                        if (lineStream >> shiftKeywordStr >> shiftAmountStr) {
                            requireComma(thirdTokenStrRaw);
                            instr.shiftType = sanitize(shiftKeywordStr);
                            instr.shiftImm = parseImmediate(sanitize(shiftAmountStr));
                        } else {
                            instr.shiftType = "";
                            instr.shiftImm = 0;
                        }
                    }
                } else {
                    // SCENARIO C: Basic 2-operand instruction with no shift (e.g., "ADD X0, #5")
                    // This defaults to X0 = X0 + 5
                    instr.arg2 = instr.arg1;

                    if (addrStr[0] == 'X' || addrStr == "SP") {
                        instr.arg3.type = OperandType::REGISTER;
                        instr.arg3.reg = addrStr;
                    } else {
                        instr.arg3.type = OperandType::IMMEDIATE;
                        instr.arg3.imm = parseImmediate(addrStr);
                    }
                    instr.shiftType = "";
                    instr.shiftImm = 0;
                }
                break;
            }
            // Both takes a register and a register/immediate. In addition also a shift operator
            case Opcode::ADD:
            case Opcode::SUB: {
                string destStr, srcStr, thirdTokenStr, shiftKeywordStr, shiftAmountStr;
                lineStream >> destStr >> srcStr;

                requireComma(destStr);
                destStr = sanitize(destStr);
                string srcStrRaw = srcStr;
                srcStr = sanitize(srcStr);

                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = destStr;

                if (lineStream >> thirdTokenStr) {
                    requireComma(srcStrRaw);
                    string thirdTokenStrRaw = thirdTokenStr;
                    thirdTokenStr = sanitize(thirdTokenStr);

                    if (thirdTokenStr == "LSL" || thirdTokenStr == "LSR") {
                        instr.arg2 = instr.arg1;

                        if (srcStr[0] == 'X' || srcStr == "SP") {
                            instr.arg3.type = OperandType::REGISTER;
                            instr.arg3.reg = srcStr;
                        } else {
                            instr.arg3.type = OperandType::IMMEDIATE;
                            instr.arg3.imm = parseImmediate(srcStr);
                        }

                        instr.shiftType = thirdTokenStr;
                        lineStream >> shiftAmountStr;
                        instr.shiftImm = parseImmediate(sanitize(shiftAmountStr));

                    } else {
                        if (srcStr[0] == 'X' || srcStr == "SP") {
                            instr.arg2.type = OperandType::REGISTER;
                            instr.arg2.reg = srcStr;
                        } else {
                            instr.arg2.type = OperandType::IMMEDIATE;
                            instr.arg2.imm = parseImmediate(srcStr);
                        }

                        if (thirdTokenStr[0] == 'X' || thirdTokenStr == "SP") {
                            instr.arg3.type = OperandType::REGISTER;
                            instr.arg3.reg = thirdTokenStr;
                        } else {
                            instr.arg3.type = OperandType::IMMEDIATE;
                            instr.arg3.imm = parseImmediate(thirdTokenStr);
                        }

                        if (lineStream >> shiftKeywordStr >> shiftAmountStr) {
                            requireComma(thirdTokenStrRaw);
                            instr.shiftType = sanitize(shiftKeywordStr);
                            instr.shiftImm = parseImmediate(sanitize(shiftAmountStr));
                        } else {
                            instr.shiftType = "";
                            instr.shiftImm = 0;
                        }
                    }
                } else {
                    instr.arg2 = instr.arg1;

                    if (srcStr[0] == 'X' || srcStr == "SP") {
                        instr.arg3.type = OperandType::REGISTER;
                        instr.arg3.reg = srcStr;
                    } else {
                        instr.arg3.type = OperandType::IMMEDIATE;
                        instr.arg3.imm = parseImmediate(srcStr);
                    }
                    instr.shiftType = "";
                    instr.shiftImm = 0;
                }
                break;
            }
            // Takes a register and a register/immediate (MOVZ only allows an immediate)
            case Opcode::MOVZ:
            case Opcode::MOV: {
                string destStr, srcStr;
                lineStream >> destStr >> srcStr;
                requireComma(destStr);
                destStr = sanitize(destStr);
                srcStr = sanitize(srcStr);
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = destStr;

                if (!srcStr.empty() && srcStr[0] == 'X' || srcStr == "SP") {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = srcStr;
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = parseImmediate(srcStr);
                }

                if (instr.op == Opcode::MOVZ && instr.arg2.type == OperandType::REGISTER) {
                    cerr << "ERROR: MOVZ does not accept a register operand\n";
                    exit(1);
                }
                break;
            }
            // Similar to move but has option to take a shift operator
            case Opcode::MOVK: {
                string destStr, srcStr, shiftKeywordStr, shiftAmountStr;
                lineStream >> destStr >> srcStr;
                requireComma(destStr);
                destStr = sanitize(destStr);
                string srcStrRaw = srcStr;
                srcStr = sanitize(srcStr);
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = destStr;

                if (!srcStr.empty() && srcStr[0] == 'X' || srcStr == "SP") {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = srcStr;
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = parseImmediate(srcStr);
                }
                if(lineStream >> shiftKeywordStr >> shiftAmountStr){
                    requireComma(srcStrRaw);
                    shiftKeywordStr = sanitize(shiftKeywordStr);
                    shiftAmountStr = sanitize(shiftAmountStr);
                    if(shiftKeywordStr == "LSL" || shiftKeywordStr == "LSR"){
                        instr.shiftType = shiftKeywordStr;
                        instr.shiftImm = parseImmediate(shiftAmountStr);
                    }
                }
                break;
            }
            // Takes a register to print (Not real ARMv8 but useful instead of a more realistic debugger + outputting stack/memory addresses)
            case Opcode::PRINT: {
                string regStr;
                lineStream >> regStr;

                regStr = sanitize(regStr);

                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr;
                break;
            }
            // Takes a register followed by a label
            case Opcode::CBZ: {
                string regStr, labelStr;
                lineStream >> regStr >> labelStr;

                requireComma(regStr);
                regStr = sanitize(regStr);
                labelStr = sanitize(labelStr);

                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr;
                instr.arg2.type = OperandType::LABEL;
                instr.arg2.label = labelStr;
                break;
            }
            // Takes a label or checks for a comparison type to tokenize for the CPU
            case Opcode::B: {
                if(opcodeStr[1] == '.'){
                    string comparison = opcodeStr.substr(2);
                    instr.cmpType = comparison;
                }else{
                    instr.cmpType = "";
                }

                string labelStr;
                lineStream >> labelStr;

                labelStr = sanitize(labelStr);
                instr.arg1.type = OperandType::LABEL;
                instr.arg1.label = labelStr;
                break;
            }
            // Takes a register and a register/immediate
            case Opcode::CMP:{
                string lhsStr, rhsStr, shiftKeywordStr, shiftAmountStr;
                lineStream >> lhsStr >> rhsStr;

                requireComma(lhsStr);
                lhsStr = sanitize(lhsStr);
                string rhsStrRaw = rhsStr;
                rhsStr = sanitize(rhsStr);
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = lhsStr;

                 if (!rhsStr.empty() && rhsStr[0] == 'X' || rhsStr == "SP") {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = rhsStr;
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = parseImmediate(rhsStr);
                }

                if (lineStream >> shiftKeywordStr >> shiftAmountStr) {
                    requireComma(rhsStrRaw);
                    instr.shiftType = sanitize(shiftKeywordStr);
                    instr.shiftImm = parseImmediate(sanitize(shiftAmountStr));
                } else {
                    instr.shiftType = "";
                    instr.shiftImm = 0;
                }

                break;
            }
            
            // Takes a starting memory index, either a register (indirect) or an immediate. (Same as print... not real ARMv8)
            case Opcode::DUMP: {
                string startStr;
                lineStream >> startStr;
                startStr = sanitize(startStr);
                if (!startStr.empty() && startStr[0] == 'X' || startStr == "SP") {
                    instr.arg1.type = OperandType::REGISTER;
                    instr.arg1.reg = startStr;
                } else {
                    instr.arg1.type = OperandType::IMMEDIATE;
                    instr.arg1.imm = parseImmediate(startStr);
                }
                break;
            }
            // Doesn't take in anything, just terminates runtime
            case Opcode::HLT:
                break;
        }
        return instr;

    }

    void resolveLabels(Program& program) {
        for (Instruction& instr : program.instructions) {
            for (Operand* operand : { &instr.arg1, &instr.arg2 }) {
                if (operand->type == OperandType::LABEL) {
                    // assigns the location for the label inside of resolvedTarget
                    // CBZ and B connect to lables, takes string label assigned and converts it to a index
                    // pc is unconditionally incremented after every instruction, so land one
                    // before the target and let that +1 carry it there
                    operand->resolvedTarget = program.labels.at(operand->label) - 1;
                }
            }
        }
    }

    string sanitize(string str){
        string clean = "";
        for(char c : str){
            if (c != ',' && c != '[' && c != ']' && c != '#') {
                clean += c;
            }
        }
        return clean;
    }

    // Parses a token as an immediate, failing cleanly instead of throwing on a missing/malformed operand
    uint64_t parseImmediate(const string& str){
        if (str.empty()) {
            cerr << "ERROR: expected a number, got a missing operand\n";
            exit(1);
        }
        try {
            return stoull(str, nullptr, 0);
        } catch (const exception&) {
            cerr << "ERROR: expected a number, got '" << str << "'\n";
            exit(1);
        }
    }

    // Enforces real ARMv8 comma spacing: every operand except the last on a line
    // must be immediately followed by a comma (checked on the raw, pre-sanitized token)
    void requireComma(const string& raw){
        if (raw.empty() || raw.back() != ',') {
            cerr << "ERROR: expected ',' after operand '" << raw << "'\n";
            exit(1);
        }
    }
};


// -------------------------------------
// Memory / Register classes
// Memory that can be read and written by address, valid indexes are 0-63 (1 byte per index)
class Memory {
    private:
        uint8_t bytes[64] = {0};
    public:
        static constexpr int SIZE = 64;

        void checkBounds(uint16_t addr){
            if (addr + sizeof(uint64_t) > static_cast<size_t>(SIZE)) {
                cerr << "ERROR: memory access at address " << addr << " is out of bounds (size " << SIZE << ")\n";
                exit(1);
            }
        }

        uint64_t read(uint16_t addr){
            checkBounds(addr);
            uint64_t value;
            memcpy(&value, &bytes[addr], sizeof(uint64_t));
            return value;

        }
        void write(uint16_t addr, uint64_t value){
            checkBounds(addr);
            memcpy(&bytes[addr], &value, sizeof(uint64_t));
        }
};

// General-purpose register file, addressed by name (e.g. "X0")
class Registers {
    private:
        map<string, uint64_t> values;
    public:
        uint64_t get(const string& reg) { return values[reg]; }

        void set(const string& reg, uint64_t value) { values[reg] = value; }
};
// -------------------------------------


class CPU {
    private:
        void execute(const Instruction& instr) {
            // run each instruction... looped
            // Details of what each instructions take in found in parser
            switch (instr.op) {
                // Loads from or stores to memory, taking in and checking operand types + shift type/value
                case Opcode::LDR:
                case Opcode::STR: {
                    uint64_t base = 0;
                    uint64_t offset = 0;

                    if(instr.arg3.type == OperandType::NONE){
                        base = (instr.arg2.type == OperandType::REGISTER) ? registers.get(instr.arg2.reg) : instr.arg2.imm;
                    }else{
                        base = (instr.arg2.type == OperandType::REGISTER) ? registers.get(instr.arg2.reg) : instr.arg2.imm;
                        offset = (instr.arg3.type == OperandType::REGISTER) ? registers.get(instr.arg3.reg) : instr.arg3.imm;
                    }
                    if (instr.shiftType == "LSL") {
                        offset <<= instr.shiftImm;
                    } else if (instr.shiftType == "LSR") {
                        offset >>= instr.shiftImm;
                    }

                    uint64_t addr = base + offset;
                    instr.op == Opcode::STR ? memory.write(addr, registers.get(instr.arg1.reg)): registers.set(instr.arg1.reg, memory.read(addr));
                    break;
                }

                // Adds / subs and applies it to a register's value. Taking in and checking operand types + shift type/value
                case Opcode::ADD:
                case Opcode::SUB: {
                    string rd = instr.arg1.reg;
                    uint64_t lhs = 0;
                    uint64_t rhs = 0;


                    if(instr.arg3.type == OperandType::NONE){
                        lhs = registers.get(rd);
                        rhs = (instr.arg2.type == OperandType::REGISTER) ? registers.get(instr.arg2.reg) : instr.arg2.imm;
                    }else{
                        lhs = (instr.arg2.type == OperandType::REGISTER) ? registers.get(instr.arg2.reg) : instr.arg2.imm;
                        rhs = (instr.arg3.type == OperandType::REGISTER) ? registers.get(instr.arg3.reg) : instr.arg3.imm;
                    }

                    if (instr.shiftType == "LSL") {
                        rhs <<= instr.shiftImm;
                    } else if (instr.shiftType == "LSR") {
                        rhs >>= instr.shiftImm;
                    }

                    if (instr.op == Opcode::ADD) {
                        registers.set(rd, lhs + rhs);
                    } else {
                        registers.set(rd, lhs - rhs);
                    }
                    break;
                }
                
                // Moves values into a register checking operand types (immediate or get value out of register)
                case Opcode::MOVZ:
                case Opcode::MOV: {
                    uint64_t operandValue = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;
                    registers.set(instr.arg1.reg, operandValue);
                    break;
                }
                case Opcode::MOVK: {
                    uint64_t operandValue = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;

                    if (instr.shiftType == "LSL") {
                        operandValue <<= instr.shiftImm;
                    } else if (instr.shiftType == "LSR") {
                        operandValue >>= instr.shiftImm;
                    }

                    operandValue += registers.get(instr.arg1.reg);

                    registers.set(instr.arg1.reg, operandValue);
                    break;
                }
                
                // Prints by getting a registers value
                case Opcode::PRINT:
                    cout << registers.get(instr.arg1.reg) << endl;
                    break;

                // Gets a registers value and checks if it equals 0
                case Opcode::CBZ:
                    if (registers.get(instr.arg1.reg) == 0) {
                        pc = instr.arg2.resolvedTarget;
                    }
                    break;
                    
                // Checks if branch has a comparison to check. Checks CPU flags and decides path. But if no comparison to check, branches unconditionally
                case Opcode::B:
                    if(instr.cmpType == "LT"){
                        if(carryFlag  == true){
                            pc = instr.arg1.resolvedTarget;
                        }
                    }else if(instr.cmpType == "GT"){
                        if(carryFlag == false && zeroFlag == false){
                            pc = instr.arg1.resolvedTarget;
                        }
                    }else{
                        pc = instr.arg1.resolvedTarget;
                    }
                    break;

                // Compares values of registers or a register and a immediate and set CPU flags for branch comparisions
                case Opcode::CMP:{
                    int64_t lhs = registers.get(instr.arg1.reg);
                    int64_t rhs = (instr.arg2.type == OperandType::REGISTER) ? registers.get(instr.arg2.reg) : instr.arg2.imm;

                    lhs - rhs == 0 ? zeroFlag = true : zeroFlag = false;
                    lhs - rhs >= 0 ? carryFlag = false : carryFlag = true;

                    break;
                }
                    
                // Dumps from a starting memory address -> that address + 7 (8 bytes total)
                case Opcode::DUMP: {
                    int start = (instr.arg1.type == OperandType::REGISTER)
                        ? registers.get(instr.arg1.reg)
                        : instr.arg1.imm;

                    cout << "Memory[" << start << " to " << start + 8 << "] = 0x";
                    uint64_t window = memory.read(start);
                    for (int i = 0; i < 8; i++) {
                        int byteValue = (window >> (i * 8)) & 0xFF;
                        cout << uppercase << hex << setfill('0') << setw(2) << byteValue << " ";
                    }
                    cout << dec << endl;
                    break;
                }

                // Ends running, loop terminates  
                case Opcode::HLT:
                    running = false;
                    break;
            }

            // pc always advances by one; CBZ/B instead land resolvedTarget one short (see resolveLabels)
            // so this same increment carries them onto the label's actual instruction
            pc++;
        }

        Registers registers;
        Memory memory;
        int pc = 0;
        bool running = true;
        // ZF CF 
        bool zeroFlag = false; // if result = 0 (equal)
        bool carryFlag = false; // if A < B -> true (if B has a carry over)

    public:
        void run(const Program& program) {
            // Where the loop runs
            while (running && pc < (int)program.instructions.size()) {
                execute(program.instructions[pc]);
            }
        }
        // Sets the Stack Pointer to the highest memory address (stack goes high -> low)
        CPU(){ registers.set("SP", Memory::SIZE);
    }
};

int main() {
    Parser parser; // takes in script / file
    Program program = parser.parse("script2.txt"); // program holds tokenized commands

    // cpu runs the program
    CPU cpu;
    cpu.run(program);

    return 0;
}
