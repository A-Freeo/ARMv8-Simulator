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
    LDR, STR, ADD, SUB, MOV, MOVZ, MOVK ,PRINT, CBZ, B, DUMP, HLT
};

// Types of orerands
enum class OperandType {
    NONE, REGISTER, IMMEDIATE, LABEL
};

// Whole operand + holds type so it can acesss the correct value (immediate / label / register)
struct Operand {
    OperandType type = OperandType::NONE;
    string reg;
    uint64_t imm = 0;
    string label;

    int resolvedTarget = -1;   // only used for LABEL operands
};


// Whole instruction, holds up to 1 operand, and 2 arguments (in current setup)
// e.g of issues with current setup
// e.g. str x0, [sp, #3] <- no way of taking in SP/register + # memory !! now sp is useable but needs lsl operator ability 
// e.g. ldr x0, [sp, #10] same for this
// movz x0, 0xXXXX. mov x0, 0xXXXX, lsl #16 ... lsl #32... lsl #48 (fills whole 8 bit register) <- no way of taking in 3 arguments
// need to make add / sub 3 operand
struct Instruction {
    Opcode op;
    Operand arg1;
    Operand arg2;
    Operand arg3;
    string shiftType;
    uint64_t shiftImm; 
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
        else if (opcodeStr == "B")   instr.op = Opcode::B;
        else if (opcodeStr == "DUMP")  instr.op = Opcode::DUMP;
        else if (opcodeStr == "HLT")  instr.op = Opcode::HLT;
        else {
            cerr << "ERROR: unknown command '" << opcodeStr << "'\n";
            exit(1);
        }

        switch (instr.op) {
            // Both take a register followed by an address, either a register or an immediate
            case Opcode::LDR:
            case Opcode::STR: {
                string regStr, addrStr;
                lineStream >> regStr >> addrStr;
                regStr = sanitize(regStr);
                addrStr = sanitize(addrStr);
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr;

                if (!addrStr.empty() && addrStr[0] == 'X') {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = addrStr;
                } else if (!addrStr.empty() && addrStr == "SP"){
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = addrStr;
                }else{
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = stoull(addrStr, nullptr, 0);
                }
                break;
            }
            // Both take a register followed by a register or an immediate
            case Opcode::ADD:
            case Opcode::SUB: {
                string regStr, operandStr, t3, t4, t5; 
                lineStream >> regStr >> operandStr;

                regStr = sanitize(regStr); 
                operandStr = sanitize(operandStr); 

                instr.arg1.type = OperandType::REGISTER; 
                instr.arg1.reg = regStr; 

                if (lineStream >> t3) {
                    t3 = sanitize(t3);
                    
                    if (t3 == "LSL" || t3 == "LSR") {
                        // SCENARIO A: "ADD X0, X1, LSL #3" (2 operands + shift)
                        // This means the destination (X0) is also the first source register
                        instr.arg2 = instr.arg1; 
                        
                        // Parse X1 as source 2
                        if (operandStr[0] == 'X' || operandStr == "SP") {
                            instr.arg3.type = OperandType::REGISTER;
                            instr.arg3.reg = operandStr;
                        } else {
                            instr.arg3.type = OperandType::IMMEDIATE;
                            instr.arg3.imm = stoull(operandStr, nullptr, 0);
                        }
                        
                        // t3 is the shift type, read the next token for the shift amount
                        instr.shiftType = t3;
                        lineStream >> t5;
                        instr.shiftImm = stoull(sanitize(t5), nullptr, 0);
                        
                    } else {
                        // SCENARIO B: "ADD X0, X1, X2..." or "ADD X0, X1, #5..." (3 operands)
                        // Parse operandStr as source 1
                        if (operandStr[0] == 'X' || operandStr == "SP") {
                            instr.arg2.type = OperandType::REGISTER;
                            instr.arg2.reg = operandStr;
                        } else {
                            instr.arg2.type = OperandType::IMMEDIATE;
                            instr.arg2.imm = stoull(operandStr, nullptr, 0);
                        }
                        
                        // Parse t3 as source 2
                        if (t3[0] == 'X' || t3 == "SP") { 
                            instr.arg3.type = OperandType::REGISTER;
                            instr.arg3.reg = t3; 
                        } else { 
                            instr.arg3.type = OperandType::IMMEDIATE;
                            instr.arg3.imm = stoull(t3, nullptr, 0); 
                        } 
                        
                        // Check if there is an optional shift after the 3rd operand (e.g., ADD X0, X1, X2, LSL #3)
                        if (lineStream >> t4 >> t5) {
                            instr.shiftType = sanitize(t4);
                            instr.shiftImm = stoull(sanitize(t5), nullptr, 0);
                        } else {
                            instr.shiftType = "";
                            instr.shiftImm = 0;
                        }
                    }
                } else {
                    // SCENARIO C: Basic 2-operand instruction with no shift (e.g., "ADD X0, #5")
                    // This defaults to X0 = X0 + 5
                    instr.arg2 = instr.arg1; 
                    
                    if (operandStr[0] == 'X' || operandStr == "SP") {
                        instr.arg3.type = OperandType::REGISTER;
                        instr.arg3.reg = operandStr;
                    } else {
                        instr.arg3.type = OperandType::IMMEDIATE;
                        instr.arg3.imm = stoull(operandStr, nullptr, 0);
                    }
                    instr.shiftType = "";
                    instr.shiftImm = 0;
                }

                break;
            }
            // Takes a register followed by a register or an immediate
            case Opcode::MOVZ:
            // add error if MOVZ reg, reg
            case Opcode::MOV: { 
                string destStr, srcStr;
                lineStream >> destStr >> srcStr;
                destStr = sanitize(destStr);
                srcStr = sanitize(srcStr);
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = destStr;

                if (!srcStr.empty() && srcStr[0] == 'X' || srcStr == "SP") {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = srcStr;
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = stoull(srcStr, nullptr, 0);
                }
                break;
            }
            case Opcode::MOVK: {
                string destStr, srcStr, t3, t4;
                lineStream >> destStr >> srcStr;
                destStr = sanitize(destStr);
                srcStr = sanitize(srcStr);
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = destStr;

                if (!srcStr.empty() && srcStr[0] == 'X' || srcStr == "SP") {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = srcStr;
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = stoull(srcStr, nullptr, 0);
                }
                if(lineStream >> t3 >> t4){
                    t3 = sanitize(t3);
                    t4 = sanitize(t4);
                    if(t3 == "LSL" || t3 == "LSR"){
                        instr.shiftType = t3;
                        t4 = sanitize(t4);
                        instr.shiftImm = stoull(t4, nullptr, 0);
                    }
                }
                break;
            }
            // Takes a register
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
                regStr = sanitize(regStr);
                labelStr = sanitize(labelStr);
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr;
                instr.arg2.type = OperandType::LABEL;
                instr.arg2.label = labelStr;
                break;
            }
            // Takes a label
            case Opcode::B: {
                string labelStr;
                lineStream >> labelStr;
                labelStr = sanitize(labelStr);
                instr.arg1.type = OperandType::LABEL;
                instr.arg1.label = labelStr;
                break;
            }
            // Takes a starting and ending memory index, each either a register (indirect) or an immediate
            case Opcode::DUMP: {
                string startStr;
                lineStream >> startStr;
                startStr = sanitize(startStr);
                if (!startStr.empty() && startStr[0] == 'X' || startStr == "SP") {
                    instr.arg1.type = OperandType::REGISTER;
                    instr.arg1.reg = startStr;
                } else {
                    instr.arg1.type = OperandType::IMMEDIATE;
                    instr.arg1.imm = stoull(startStr, nullptr, 0);
                }

                break;
            }
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
};


// memory / register control
// -------------------------------------
// Backing store for LDR/STR, addressed by immediate offset
class Memory {
    private:
        uint8_t bytes[64] = {0};
        // 512 bits - > 64 bytes - > 8 int-64
        // has memory adress 0 - 31 (1 index value = 1 byte)
    public:
        static constexpr int SIZE = 64;
        uint64_t read(int addr){

            uint64_t value;
            memcpy(&value, &bytes[addr], sizeof(uint64_t));
            return value;

        }

        void write(int addr, uint64_t value){

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
            // run each insrutction, this gets looped
            switch (instr.op) {
                case Opcode::LDR: {
                // sets the register in arg1 to the memory at the address in arg2 (register or immediate)
                    int addr = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;
                    registers.set(instr.arg1.reg, memory.read(addr));
                    break;
                }

                case Opcode::STR: {
                // same as LDR but saves into memory
                    int addr = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;
                    
                    memory.write(addr, registers.get(instr.arg1.reg));
                    break;
                }

                case Opcode::ADD:
                case Opcode::SUB: {
                    string rd = instr.arg1.reg;
                    uint64_t source1 = 0;
                    uint64_t source2 = 0;


                    if(instr.arg3.type == OperandType::NONE){
                        source1 = registers.get(rd);
                        source2 = (instr.arg2.type == OperandType::REGISTER) ? registers.get(instr.arg2.reg) : instr.arg2.imm;
                    }else{
                        source1 = (instr.arg2.type == OperandType::REGISTER) ? registers.get(instr.arg2.reg) : instr.arg2.imm;
                        source2 = (instr.arg3.type == OperandType::REGISTER) ? registers.get(instr.arg3.reg) : instr.arg3.imm;
                    }

                    if (instr.shiftType == "LSL") {
                        source2 <<= instr.shiftImm;
                    } else if (instr.shiftType == "LSR") {
                        source2 >>= instr.shiftImm;
                    }

                    if (instr.op == Opcode::ADD) {
                        registers.set(rd, source1 + source2);
                    } else {
                        registers.set(rd, source1 - source2);
                    }
                    break;
                }
                
                case Opcode::MOVZ:
                case Opcode::MOV: {
                    // gets the register or immediate value and sets the register at arg1 to that value
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
                
                case Opcode::PRINT:
                // prints a registers value
                    cout << registers.get(instr.arg1.reg) << endl;
                    break;
                
                case Opcode::CBZ:
                // gets the registers value and checks if its 0. true -> pc jumps to resolved target false -> pc increments normally
                    if (registers.get(instr.arg1.reg) == 0) {
                        pc = instr.arg2.resolvedTarget;
                    }
                    break;

                case Opcode::B:
                // jumps pc to the resolved target of the branch
                    pc = instr.arg1.resolvedTarget;
                    break;

                case Opcode::DUMP: {
                // dumps (prints) the memory from index (arg1) to index (arg2), each register or immediate
                    int start = (instr.arg1.type == OperandType::REGISTER)
                        ? registers.get(instr.arg1.reg)
                        : instr.arg1.imm;

                    cout << "Memory[" << start << " to " << start + 8 << "] = 0x";
                    for (int i = start; i < start + 8 && i < Memory::SIZE; i++) { 
                        uint64_t fullByte = memory.read(i); 

                        int byteValue = fullByte & 0xFF;
                        cout << uppercase << hex << setfill('0') << setw(2) << byteValue << " ";
                    }
                    cout << dec << endl;
                    break;
                }

                case Opcode::HLT:
                // stops the program
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


    public:
        void run(const Program& program) {
            // loops and sends each instruction to get executed 
            while (running && pc < (int)program.instructions.size()) {
                execute(program.instructions[pc]);
            }
        }
        CPU(){ registers.set("SP", Memory::SIZE);
 }
};




int main() {
    Parser parser; // takes in script / file
    Program program = parser.parse("script2.txt"); // program holds finished commands

    // cpu runs the program
    CPU cpu;
    cpu.run(program);

    return 0;
}
