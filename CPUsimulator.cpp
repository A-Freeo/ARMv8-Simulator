#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdlib>
using namespace std;

enum class Opcode {
    LOAD, STORE, ADD, SUB, MOV, PRINT, JZ, JMP, DUMP, HALT
};

enum class OperandType {
    NONE, REGISTER, IMMEDIATE, LABEL
};

struct Operand {
    OperandType type = OperandType::NONE;
    char reg = '\0';
    int imm = 0;
    string label;

    int resolvedTarget = -1;   // only used for LABEL operands
};

struct Instruction {
    Opcode op;
    Operand arg1;
    Operand arg2;
};

struct Program {
    vector<Instruction> instructions;
    map<string, int> labels;
};

class Parser {
public:
    Program parse(const string& path) {
        Program program;
        ifstream inputfile(path);
        string line;

        while (getline(inputfile, line)) {
            istringstream lineStream(line);
            string firstToken;
            if (!(lineStream >> firstToken)) continue;

            if (firstToken.back() == ':') {
                string label = firstToken.substr(0, firstToken.size() - 1);
                program.labels[label] = program.instructions.size(); // index starts from zero and size at 1 so lines up to the next instruction after the label
            } else {
                program.instructions.push_back(tokenizeLine(line));
            }
        }

        resolveLabels(program);
        return program;
    }

private:
    Instruction tokenizeLine(const string& line) {
        istringstream lineStream(line);
        Instruction instr;
        string opcodeStr;
        lineStream >> opcodeStr;

        if (opcodeStr == "LOAD")       instr.op = Opcode::LOAD;
        else if (opcodeStr == "STORE") instr.op = Opcode::STORE;
        else if (opcodeStr == "ADD")   instr.op = Opcode::ADD;
        else if (opcodeStr == "SUB")    instr.op = Opcode::SUB;
        else if (opcodeStr == "MOV")   instr.op = Opcode::MOV;
        else if (opcodeStr == "PRINT") instr.op = Opcode::PRINT;
        else if (opcodeStr == "JZ")    instr.op = Opcode::JZ;
        else if (opcodeStr == "JMP")   instr.op = Opcode::JMP;
        else if (opcodeStr == "DUMP")  instr.op = Opcode::DUMP;
        else if (opcodeStr == "HALT")  instr.op = Opcode::HALT;
        else {
            cerr << "ERROR: unknown command '" << opcodeStr << "'\n";
            exit(1);
        }

        switch (instr.op) {
            case Opcode::LOAD:
            case Opcode::STORE: {
                string regStr; int addr;
                lineStream >> regStr >> addr;
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr[0];
                instr.arg2.type = OperandType::IMMEDIATE;
                instr.arg2.imm = addr;
                break;
            }
            case Opcode::ADD:
            case Opcode::SUB: {
                string regStr, operandStr;
                lineStream >> regStr >> operandStr;
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr[0];

                if (operandStr == "A" || operandStr == "B") {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = operandStr[0];
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = stoi(operandStr);
                }
                break;
            }

            case Opcode::MOV: {
                string destStr, srcStr;
                lineStream >> destStr >> srcStr;
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = destStr[0];

                if (srcStr == "A" || srcStr == "B") {
                    instr.arg2.type = OperandType::REGISTER;
                    instr.arg2.reg = srcStr[0];
                } else {
                    instr.arg2.type = OperandType::IMMEDIATE;
                    instr.arg2.imm = stoi(srcStr);
                }
                break;
            }
            case Opcode::PRINT: {
                string regStr;
                lineStream >> regStr;
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr[0];
                break;
            }
            case Opcode::JZ: {
                string regStr, labelStr;
                lineStream >> regStr >> labelStr;
                instr.arg1.type = OperandType::REGISTER;
                instr.arg1.reg = regStr[0];
                instr.arg2.type = OperandType::LABEL;
                instr.arg2.label = labelStr;
                break;
            }
            case Opcode::JMP: {
                string labelStr;
                lineStream >> labelStr;
                instr.arg1.type = OperandType::LABEL;
                instr.arg1.label = labelStr;
                break;
            }
            case Opcode::DUMP: {
                int start, end;
                lineStream >> start >> end;
                instr.arg1.type = OperandType::IMMEDIATE;
                instr.arg1.imm = start;
                instr.arg2.type = OperandType::IMMEDIATE;
                instr.arg2.imm = end;
                break;
            }
            case Opcode::HALT:
                break;
        }

        return instr;

    }

    void resolveLabels(Program& program) {
        for (Instruction& instr : program.instructions) {
            for (Operand* operand : { &instr.arg1, &instr.arg2 }) {
                if (operand->type == OperandType::LABEL) {
                    operand->resolvedTarget = program.labels.at(operand->label);
                }
            }
        }
    }
};

// Memory / Register management
class Memory {
    private:
        int cells[256] = {0};
    public:
        int read(int addr) {return cells[addr]; }
        void write(int addr, int value) { cells[addr] = value; }
};

class Registers {
    private:
        map<char, int> values;
    public:
        int get(char reg) { return values[reg]; }

        void set(char reg, int value) { values[reg] = value; }
};
// -------------------------------------


class CPU {
    private:
        void execute(const Instruction& instr) {
            bool jumped = false;

            switch (instr.op) {
                case Opcode::LOAD:
                    registers.set(instr.arg1.reg, memory.read(instr.arg2.imm));
                    break;

                case Opcode::STORE:
                    memory.write(instr.arg2.imm, registers.get(instr.arg1.reg));
                    break;

                case Opcode::ADD: {
                    int operandValue = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;
                    registers.set(instr.arg1.reg, registers.get(instr.arg1.reg) + operandValue);
                    break;
                }

                case Opcode::SUB: {
                    int operandValue = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;
                    registers.set(instr.arg1.reg, registers.get(instr.arg1.reg) - operandValue);
                    break;
                }

                case Opcode::MOV: {
                    int operandValue = (instr.arg2.type == OperandType::REGISTER)
                        ? registers.get(instr.arg2.reg)
                        : instr.arg2.imm;
                    registers.set(instr.arg1.reg, operandValue);
                    break;
                }

                case Opcode::PRINT:
                    cout << registers.get(instr.arg1.reg) << endl;
                    break;

                case Opcode::JZ:
                    if (registers.get(instr.arg1.reg) == 0) {
                        pc = instr.arg2.resolvedTarget;
                        jumped = true;
                    }
                    break;

                case Opcode::JMP:
                    pc = instr.arg1.resolvedTarget;
                    jumped = true;
                    break;

                case Opcode::DUMP:
                    for (int i = instr.arg1.imm; i <= instr.arg2.imm; i++) {
                        cout << "Memory[" << i << "] = " << memory.read(i) << endl;
                    }
                    break;

                case Opcode::HALT:
                    running = false;
                    break;
            }

            if (!jumped) {
                pc++;
            }
        }

        Registers registers;
        Memory memory;
        int pc = 0;
        bool running = true;


    public:
        void run(const Program& program) {
            while (running && pc < (int)program.instructions.size()) {
                execute(program.instructions[pc]);
            }
        }
};





int main() {
    Parser parser;
    Program program = parser.parse("script.txt");

    CPU cpu;
    cpu.run(program);

    return 0;
}