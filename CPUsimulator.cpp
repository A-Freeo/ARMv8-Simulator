#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
using namespace std;

// Commands
enum class Opcode {
    LDR, STR, ADD, SUB, MOV, PRINT, CBZ, B, DUMP, HLT
};

// Types of orerands
enum class OperandType {
    NONE, REGISTER, IMMEDIATE, LABEL
};

// Whole operand + holds type so it can acesss the correct value (immediate / label / register)
struct Operand {
    OperandType type = OperandType::NONE;
    string reg;
    int imm = 0;
    string label;

    int resolvedTarget = -1;   // only used for LABEL operands
};


// Whole instruction, holds up to 1 operand, and 2 arguments (in current setup)
// e.g of issues with current setup
// e.g. str x0, [sp, #3] <- no way of taking in SP/register + # memory 
// e.g. ldr x0, [sp, #10]
// movz x0, 0xXXXX. mov x0, 0xXXXX, lsl #16 ... lsl #32... lsl #48 (fills whole 8 bit register) <- no way of taking in 3 arguments
struct Instruction {
    Opcode op;
    Operand arg1;
    Operand arg2;
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
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr;

                if (!addrStr.empty() && addrStr[0] == 'X') {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = addrStr;
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = stoi(addrStr);
                }
                break;
            }
            // Both take a register followed by a register or an immediate
            case Opcode::ADD:
            case Opcode::SUB: {
                string regStr, operandStr;
                lineStream >> regStr >> operandStr;
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr;

                if (!operandStr.empty() && operandStr[0] == 'X') {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = operandStr;
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = stoi(operandStr);
                }
                break;
            }

            // Takes a register followed by a register or an immediate
            case Opcode::MOV: {
                string destStr, srcStr;
                lineStream >> destStr >> srcStr;
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = destStr;

                if (!srcStr.empty() && srcStr[0] == 'X') {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = srcStr;
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = stoi(srcStr);
                }
                break;
            }
            // Takes a register
            case Opcode::PRINT: {
                string regStr;
                lineStream >> regStr;
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr;
                break;
            }
            // Takes a register followed by a label
            case Opcode::CBZ: {
                string regStr, labelStr;
                lineStream >> regStr >> labelStr;
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
                instr.arg1.type = OperandType::LABEL;
                instr.arg1.label = labelStr;
                break;
            }
            // Takes a starting and ending memory index, each either a register (indirect) or an immediate
            case Opcode::DUMP: {
                string startStr, endStr;
                lineStream >> startStr >> endStr;

                if (!startStr.empty() && startStr[0] == 'X') {
                    instr.arg1.type = OperandType::REGISTER;
                    instr.arg1.reg = startStr;
                } else {
                    instr.arg1.type = OperandType::IMMEDIATE;
                    instr.arg1.imm = stoi(startStr);
                }

                if (!endStr.empty() && endStr[0] == 'X') {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = endStr;
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = stoi(endStr);
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
};


// memory / register control
// -------------------------------------
// Backing store for LDR/STR, addressed by immediate offset
class Memory {
    private:
        int cells[256] = {0};
    public:
        int read(int addr) {return cells[addr]; }
        void write(int addr, int value) { cells[addr] = value; }
};

// General-purpose register file, addressed by name (e.g. "X0")
class Registers {
    private:
        map<string, int> values;
    public:
        int get(const string& reg) { return values[reg]; }

        void set(const string& reg, int value) { values[reg] = value; }
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

                case Opcode::ADD: {
                // gets the register or immediate value of arg 2 and sets the register at arg1 its self + value
                    int operandValue = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;
                    registers.set(instr.arg1.reg, registers.get(instr.arg1.reg) + operandValue);
                    break;
                }

                case Opcode::SUB: {
                // same as ADD but subtraction
                    int operandValue = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;
                    registers.set(instr.arg1.reg, registers.get(instr.arg1.reg) - operandValue);
                    break;
                }
        
                case Opcode::MOV: {
                    // gets the register or immediate value and sets the register at arg1 to that value
                    int operandValue = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;
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
                    int end = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;
                    for (int i = start; i <= end; i++) {
                        cout << "Memory[" << i << "] = " << memory.read(i) << endl;
                    }
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
};




int main() {
    Parser parser; // takes in script / file
    Program program = parser.parse("script.txt"); // program holds finished commands

    // cpu runs the program
    CPU cpu;
    cpu.run(program);

    return 0;
}
