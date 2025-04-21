#include "../include/myRISCVSim.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>

using namespace std;

map<uint32_t, PredictorEntry> branch_predictor;
map<uint32_t, BTBEntry> btb;
uint32_t total_predictions = 0;
uint32_t correct_predictions = 0;

// Define global variables
uint32_t PC = 0x0;
uint32_t cycle = 1;
IF_ID if_id;
ID_EX id_ex;
EX_MEM ex_mem;
MEM_WB mem_wb;
unordered_map<uint32_t, uint8_t> instruction_memory;
unordered_map<uint32_t, uint8_t> data_memory;
uint32_t reg_file[32] = {0};  // Initialize register file to zero

bool stall = false;

bool control_hazard_flush = false;

bool is_draining = false;

bool pipelining_enabled = true;
bool forwarding_enabled = true;

uint32_t total_cycles = 0;
uint32_t instructions_executed = 0;
uint32_t data_hazards = 0;
uint32_t control_hazards = 0;
uint32_t stalls = 0;
uint32_t data_transfer_instructions = 0; // Stat4
uint32_t alu_instructions = 0;          // Stat5
uint32_t control_instructions = 0;      // Stat6

void fetch() {
    if_id.PC = PC;
    if_id.instruction = read_word(PC, instruction_memory);
    if (if_id.instruction == 0xFFFFFFFF) {
        is_draining = true;
    }
    
    // Check if instruction is a branch/jump
    bool is_beq = (if_id.instruction & 0x7F) == 0x63; // beq
    bool is_jal = (if_id.instruction & 0x7F) == 0x6F; // jal
    bool is_jalr = (if_id.instruction & 0x7F) == 0x67; // jalr
    bool predict_taken = false;
    uint32_t predicted_target = PC + 4;

    // Check BTB
    bool btb_hit = (btb.find(if_id.PC) != btb.end() && btb[if_id.PC].valid);
    if (is_beq || is_jal || is_jalr) {
        if (btb_hit) {
            if (is_beq) {
                predict_taken = branch_predictor[if_id.PC].state;
            } else if (is_jal || is_jalr) {
                predict_taken = true; // Always taken
            }
            if (predict_taken) {
                predicted_target = btb[if_id.PC].target;
            }
        }
        total_predictions++; // Count prediction even on BTB miss
    }

    // Update PC
    PC = predict_taken ? predicted_target : PC + 4;

    // Output
    cout << "Fetch: Instruction 0x" << hex << setw(8) << setfill('0') 
         << if_id.instruction << " at PC=0x" << if_id.PC << dec 
         << " (Cycle " << total_cycles << ")";
    if (is_beq || is_jal || is_jalr) {
        cout << ", Predicted: " << (predict_taken ? "Taken to 0x" + to_string(predicted_target) : "Not Taken");
        cout << ", BTB: " << (btb_hit ? "Hit" : "Miss");
    }
    cout << endl;
}

void decode() {
    if (if_id.instruction == 0 || if_id.instruction == 0xFFFFFFFF || !is_valid_instruction(if_id.instruction)) {
        id_ex = ID_EX();
        if (is_draining) {
            cout << "Decode: NOP (end of program) (Cycle " << total_cycles << ")" << endl;
        } else if (if_id.instruction == 0) {
            cout << "Decode: NOP (flushed) (Cycle " << total_cycles << ")" << endl;
        } else {
            cout << "Decode: NOP (invalid instruction) (Cycle " << total_cycles << ")" << endl;
        }
        return;
    }

    // Check for load-use hazard
    uint32_t rs1 = (if_id.instruction >> 15) & 0x1F;
    uint32_t rs2 = (if_id.instruction >> 20) & 0x1F;
    if (id_ex.mem_read && id_ex.rd != 0 && (id_ex.rd == rs1 || id_ex.rd == rs2)) {
        stall = true;
        cout << "Stall: Load-use hazard detected with rd=x" << id_ex.rd << " at PC=0x" << hex << if_id.PC << dec << endl;
        id_ex = ID_EX(); // Insert bubble
        return;
    }

    id_ex = ID_EX();
    id_ex.opcode = if_id.instruction & 0x7F;
    id_ex.rd = (if_id.instruction >> 7) & 0x1F;
    id_ex.func3 = (if_id.instruction >> 12) & 0x7;
    id_ex.func7 = (if_id.instruction >> 25) & 0x7F;

    if (rs1 >= 32 || rs2 >= 32 || id_ex.rd >= 32) {
        cout << "Error: Invalid register index at PC=0x" << hex << if_id.PC << dec << endl;
        id_ex = ID_EX();
        return;
    }

    // Data Forwarding (controlled by knob)
    bool rs1_forwarded = false, rs2_forwarded = false;
    if (forwarding_enabled) {
        if (ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == rs1) {
            id_ex.rs1_val = ex_mem.alu_result;
            rs1_forwarded = true;
            data_hazards++;
            cout << "Forwarding EX_MEM.alu_result=0x" << hex << ex_mem.alu_result << " to rs1 (x" << rs1 << ")" << dec << endl;
        } else if (mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == rs1) {
            id_ex.rs1_val = mem_wb.mem_read ? mem_wb.mem_data : mem_wb.alu_result;
            rs1_forwarded = true;
            data_hazards++;
            cout << "Forwarding MEM_WB." << (mem_wb.mem_read ? "mem_data" : "alu_result") << "=0x" 
                 << hex << id_ex.rs1_val << " to rs1 (x" << rs1 << ")" << dec << endl;
        }
        if (ex_mem.reg_write && ex_mem.rd != 0 && ex_mem.rd == rs2) {
            id_ex.rs2_val = ex_mem.alu_result;
            rs2_forwarded = true;
            data_hazards++;
            cout << "Forwarding EX_MEM.alu_result=0x" << hex << ex_mem.alu_result << " to rs2 (x" << rs2 << ")" << dec << endl;
        } else if (mem_wb.reg_write && mem_wb.rd != 0 && mem_wb.rd == rs2) {
            id_ex.rs2_val = mem_wb.mem_read ? mem_wb.mem_data : mem_wb.alu_result;
            rs2_forwarded = true;
            data_hazards++;
            cout << "Forwarding MEM_WB." << (mem_wb.mem_read ? "mem_data" : "alu_result") << "=0x" 
                 << hex << id_ex.rs2_val << " to rs2 (x" << rs2 << ")" << dec << endl;
        }
    }
    if (!rs1_forwarded) {
        id_ex.rs1_val = (rs1 == 0) ? 0 : reg_file[rs1];
    }
    if (!rs2_forwarded) {
        id_ex.rs2_val = (rs2 == 0) ? 0 : reg_file[rs2];
    }

    // Set specific instruction flags
    id_ex.is_add = (id_ex.opcode == 0x33 && id_ex.func3 == 0 && id_ex.func7 == 0x00);
    id_ex.is_sub = (id_ex.opcode == 0x33 && id_ex.func3 == 0 && id_ex.func7 == 0x20);
    id_ex.is_and = (id_ex.opcode == 0x33 && id_ex.func3 == 0x7 && id_ex.func7 == 0x00);
    id_ex.is_or = (id_ex.opcode == 0x33 && id_ex.func3 == 0x6 && id_ex.func7 == 0x00);
    id_ex.is_addi = (id_ex.opcode == 0x13 && id_ex.func3 == 0);
    id_ex.is_andi = (id_ex.opcode == 0x13 && id_ex.func3 == 0x7);
    id_ex.is_ori = (id_ex.opcode == 0x13 && id_ex.func3 == 0x6);
    id_ex.is_lw = (id_ex.opcode == 0x03 && id_ex.func3 == 0x2);
    id_ex.is_sw = (id_ex.opcode == 0x23 && id_ex.func3 == 0x2);
    id_ex.is_lui = (id_ex.opcode == 0x37);
    id_ex.is_auipc = (id_ex.opcode == 0x17);
    id_ex.is_beq = (id_ex.opcode == 0x63 && id_ex.func3 == 0x0);
    id_ex.is_jal = (id_ex.opcode == 0x6F);
    id_ex.is_jalr = (id_ex.opcode == 0x67 && id_ex.func3 == 0x0);
    id_ex.is_mul = (id_ex.opcode == 0x33 && id_ex.func3 == 0 && id_ex.func7 == 0x01);

    // Set control signals
    id_ex.reg_write = (id_ex.is_add || id_ex.is_sub || id_ex.is_and || id_ex.is_or || 
                       id_ex.is_addi || id_ex.is_andi || id_ex.is_ori || id_ex.is_lw || 
                       id_ex.is_lui || id_ex.is_auipc || id_ex.is_jal || id_ex.is_jalr || 
                       id_ex.is_mul);
    id_ex.mem_read = id_ex.is_lw;
    id_ex.mem_write = id_ex.is_sw;

    // Set base_reg for memory access instructions
    if (id_ex.mem_read || id_ex.mem_write) {
        id_ex.base_reg = rs1;
    }

    // Print sp (x2) when read
    if (rs1 == 2) {
        cout << "Reading sp (x2): 0x" << hex << id_ex.rs1_val << dec << endl;
    }
    if (rs2 == 2 && (id_ex.opcode == 0x33 || id_ex.opcode == 0x23 || id_ex.opcode == 0x63)) {
        cout << "Reading sp (x2): 0x" << hex << id_ex.rs2_val << dec << endl;
    }

    // Set ALU operation
    if (id_ex.is_add || id_ex.is_addi || id_ex.is_jal || id_ex.is_jalr || id_ex.is_lw || id_ex.is_sw) id_ex.alu_op = 0;
    else if (id_ex.is_sub) id_ex.alu_op = 1;
    else if (id_ex.is_and || id_ex.is_andi) id_ex.alu_op = 2;
    else if (id_ex.is_or || id_ex.is_ori) id_ex.alu_op = 3;
    else if (id_ex.is_lui || id_ex.is_auipc) id_ex.alu_op = 0;
    else id_ex.alu_op = 0;

    // Immediate decoding
    if (id_ex.is_addi || id_ex.is_andi || id_ex.is_ori || id_ex.is_lw || id_ex.is_jalr) {
        id_ex.imm = sign_extend((if_id.instruction >> 20) & 0xFFF, 12);
        id_ex.rs2_val = 0;
    } else if (id_ex.is_sw) {
        id_ex.imm = sign_extend((((if_id.instruction >> 25) & 0x7F) << 5) | ((if_id.instruction >> 7) & 0x1F), 12);
    } else if (id_ex.is_beq) {
        id_ex.imm = (((if_id.instruction >> 31) & 0x1) << 12) | (((if_id.instruction >> 7) & 0x1) << 11) |
                    (((if_id.instruction >> 25) & 0x3F) << 5) | (((if_id.instruction >> 8) & 0x0F) << 1);
        id_ex.imm = sign_extend(id_ex.imm & 0x1FFF, 13);
    } else if (id_ex.is_lui || id_ex.is_auipc) {
        id_ex.imm = (if_id.instruction & 0xFFFFF000);
    } else if (id_ex.is_jal) {
        id_ex.imm = sign_extend((((if_id.instruction >> 31) & 0x1) << 20) | (((if_id.instruction >> 12) & 0xFF) << 12) |
                                (((if_id.instruction >> 20) & 0x1) << 11) | (((if_id.instruction >> 21) & 0x3FF) << 1), 21);
    } else {
        id_ex.imm = 0;
    }

    id_ex.PC = if_id.PC;

    // Debug output
    stringstream ss;
    if (id_ex.is_add) ss << "Decode: add, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", rs2=x" << rs2;
    else if (id_ex.is_sub) ss << "Decode: sub, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", rs2=x" << rs2;
    else if (id_ex.is_and) ss << "Decode: and, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", rs2=x" << rs2;
    else if (id_ex.is_or) ss << "Decode: or, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", rs2=x" << rs2;
    else if (id_ex.is_addi) ss << "Decode: addi, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_andi) ss << "Decode: andi, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_ori) ss << "Decode: ori, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_lw) ss << "Decode: lw, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_sw) ss << "Decode: sw, rs1=x" << rs1 << ", rs2=x" << rs2 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_lui) ss << "Decode: lui, rd=x" << id_ex.rd << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_auipc) ss << "Decode: auipc, rd=x" << id_ex.rd << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_beq) ss << "Decode: beq, rs1=x" << rs1 << ", rs2=x" << rs2 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_jal) ss << "Decode: jal, rd=x" << id_ex.rd << ", offset=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_jalr) ss << "Decode: jalr, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_mul) ss << "Decode: mul, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", rs2=x" << rs2;
    else ss << "Decode: Unknown instruction at PC=0x" << hex << id_ex.PC << dec;
    cout << ss.str() << " (Cycle " << total_cycles << ")" << endl;
}

void execute() {
    
    // Count instructions and types only if not flushed - no need
    if ( id_ex.opcode != 0) { // Check for valid instruction
        instructions_executed++;
        if (id_ex.is_lw || id_ex.is_sw) {
            data_transfer_instructions++;
        } else if (id_ex.is_add || id_ex.is_sub || id_ex.is_and || id_ex.is_or ||
                   id_ex.is_addi || id_ex.is_andi || id_ex.is_ori || id_ex.is_lui ||
                   id_ex.is_auipc || id_ex.is_mul) {
            alu_instructions++;
        } else if (id_ex.is_beq || id_ex.is_jal || id_ex.is_jalr) {
            control_instructions++;
        }
    }else {
        cout << "Execute: NOP (Cycle " << total_cycles << ")" << endl;
        ex_mem = EX_MEM();
        return;
    }
    ex_mem = EX_MEM();

    ex_mem.PC = id_ex.PC;
    ex_mem.rd = id_ex.rd;
    ex_mem.reg_write = id_ex.reg_write;
    ex_mem.mem_read = id_ex.mem_read;
    ex_mem.mem_write = id_ex.mem_write;
    ex_mem.is_branch = id_ex.is_beq;

    if (id_ex.mem_read || id_ex.mem_write) {
        ex_mem.base_reg = id_ex.base_reg;
    }

    uint32_t alu_input1 = id_ex.rs1_val;
    uint32_t alu_input2 = (id_ex.is_add || id_ex.is_sub || id_ex.is_and || id_ex.is_or || id_ex.is_mul) ? id_ex.rs2_val : id_ex.imm;

    switch (id_ex.alu_op) {
        case 0: ex_mem.alu_result = alu_input1 + alu_input2; break;
        case 1: ex_mem.alu_result = alu_input1 - alu_input2; break;
        case 2: ex_mem.alu_result = alu_input1 & alu_input2; break;
        case 3: ex_mem.alu_result = alu_input1 | alu_input2; break;
        default: ex_mem.alu_result = alu_input1 + alu_input2; break;
    }

    if (id_ex.is_auipc) {
        ex_mem.alu_result = id_ex.PC + id_ex.imm;
    } else if (id_ex.is_lui) {
        ex_mem.alu_result = id_ex.imm;
    } else if (id_ex.is_mul) {
        ex_mem.alu_result = id_ex.rs1_val * id_ex.rs2_val;
    }

    // Branch and Jump Handling
    bool actual_taken = false;
    uint32_t actual_target = id_ex.PC + 4;

    if (id_ex.is_beq) {
        ex_mem.is_branch = (id_ex.rs1_val == id_ex.rs2_val);
        ex_mem.branch_target = id_ex.PC + id_ex.imm;
        actual_taken = ex_mem.is_branch;
        actual_target = ex_mem.branch_target;
    } else if (id_ex.is_jal) {
        ex_mem.alu_result = id_ex.PC + 4; // Return address
        actual_taken = true;
        actual_target = id_ex.PC + id_ex.imm;
    } else if (id_ex.is_jalr) {
        ex_mem.alu_result = id_ex.PC + 4;
        actual_taken = true;
        actual_target = (id_ex.rs1_val + id_ex.imm) & ~1; // Align to word
    }

    // Update Predictor and BTB
    if (id_ex.is_beq || id_ex.is_jal || id_ex.is_jalr) {
        // Determine prediction
        bool predicted_taken = false;
        uint32_t predicted_target = id_ex.PC + 4;
        bool btb_hit = (btb.find(id_ex.PC) != btb.end() && btb[id_ex.PC].valid);
        if (btb_hit) {
            predicted_taken = id_ex.is_beq ? branch_predictor[id_ex.PC].state : true;
            predicted_target = btb[id_ex.PC].target;
        }

        // Check prediction correctness
        bool prediction_correct = (predicted_taken == actual_taken) && 
                                    (!actual_taken || predicted_target == actual_target);

        // Increment predictions even on BTB miss
        total_predictions++;

        if (prediction_correct) {
            if (btb_hit) {
                correct_predictions++;
            }
        } else {
            control_hazards++;
            cout << "Control Hazard Detected: PC=0x" << hex << id_ex.PC 
                    << ", Target=0x" << actual_target << dec << endl;
            cout << "Flushing pipeline due to " << (id_ex.is_beq ? "branch" : "jump") 
                    << (btb_hit ? " misprediction" : " BTB miss") << endl;
            PC = actual_target;
            if_id = IF_ID();
            id_ex = ID_EX();
            control_hazard_flush = true;
        }

        // Update predictor (only for beq) and BTB
        if (id_ex.is_beq) {
            branch_predictor[id_ex.PC].state = actual_taken;
        }
        btb[id_ex.PC].target = actual_target;
        btb[id_ex.PC].valid = true;
    }

    cout << "Execute: ALU Result=0x" << hex << setw(8) << setfill('0') << ex_mem.alu_result;
    if (id_ex.is_beq) cout << ", Branch=" << (ex_mem.is_branch ? "Taken" : "Not Taken") << ", Target=0x" << hex << ex_mem.branch_target;
    else if (id_ex.is_jal || id_ex.is_jalr) cout << ", Jump Target=0x" << hex << ex_mem.branch_target;
    cout << dec << " (Cycle " << total_cycles << ")" << endl;
}

void memory_access() {
    mem_wb = MEM_WB();

    mem_wb.rd = ex_mem.rd;
    mem_wb.reg_write = ex_mem.reg_write;
    mem_wb.mem_read = ex_mem.mem_read;

    mem_wb.alu_result = ex_mem.alu_result;

    if (ex_mem.mem_read) {
        try {
            uint32_t addr = ex_mem.alu_result & ~0x3;
            mem_wb.mem_data = (data_memory.at(addr + 3) << 24) | (data_memory.at(addr + 2) << 16) |
                              (data_memory.at(addr + 1) << 8) | data_memory.at(addr);
        } catch (const out_of_range&) {
            mem_wb.mem_data = 0x0;
            cout << "Warning: Memory read at uninitialized address 0x" << hex << ex_mem.alu_result << dec << endl;
        }
        if (ex_mem.base_reg == 2) {
            cout << "Stack load: address=0x" << hex << ex_mem.alu_result << ", data=0x" << mem_wb.mem_data << dec << endl;
        }
        cout << "Memory Access: Read Data=0x" << hex << setw(8) << setfill('0') << mem_wb.mem_data 
             << " from Address=0x" << ex_mem.alu_result << dec << " (Cycle " << total_cycles << ")" << endl;
    }
    else if (ex_mem.mem_write) {
        try {
            uint32_t addr = ex_mem.alu_result & ~0x3;
            uint32_t value = ex_mem.rs2_val;
            data_memory[addr] = value & 0xFF;
            data_memory[addr + 1] = (value >> 8) & 0xFF;
            data_memory[addr + 2] = (value >> 16) & 0xFF;
            data_memory[addr + 3] = (value >> 24) & 0xFF;
            if (ex_mem.base_reg == 2) {
                cout << "Stack store: address=0x" << hex << ex_mem.alu_result << ", data=0x" << ex_mem.rs2_val << dec << endl;
            }
        } catch (const out_of_range&) {
            cout << "Warning: Memory write at uninitialized address 0x" << hex << ex_mem.alu_result << dec << endl;
        }
        cout << "Memory Access: Wrote 0x" << hex << setw(8) << setfill('0') << ex_mem.rs2_val 
             << " to Address=0x" << ex_mem.alu_result << dec << " (Cycle " << total_cycles << ")" << endl;
    }
    else {
        cout << "Memory Access: (none) (Cycle " << total_cycles << ")" << endl;
    }
}

void writeback() {
    if (mem_wb.reg_write && mem_wb.rd != 0) {
        if (mem_wb.mem_read) {
            reg_file[mem_wb.rd] = mem_wb.mem_data;
        } else {
            reg_file[mem_wb.rd] = mem_wb.alu_result;
        }
        if (mem_wb.rd == 2) {
            cout << "Writing to sp (x2): 0x" << hex << reg_file[2] << dec << endl;
        }
        cout << "Writeback: Wrote 0x" << hex << setw(8) << setfill('0') << reg_file[mem_wb.rd] 
             << " to x" << dec << setw(2) << setfill('0') << mem_wb.rd << dec << " (Cycle " << total_cycles << ")" << endl;
    } else {
        cout << "Writeback: (none) (Cycle " << total_cycles << ")" << endl;
    }
}

uint32_t read_word(uint32_t addr, const unordered_map<uint32_t, uint8_t>& mem) {
    uint32_t aligned_addr = addr & ~0x3;
    try {
        return (mem.at(aligned_addr + 3) << 24) | (mem.at(aligned_addr + 2) << 16) |
               (mem.at(aligned_addr + 1) << 8) | mem.at(aligned_addr);
    } catch (const out_of_range&) {
        return 0x0;
    }
}

void write_word(uint32_t addr, uint32_t value, std::unordered_map<uint32_t, uint8_t>& mem) {
    addr &= ~0x3;
    mem[addr] = value & 0xFF;
    mem[addr + 1] = (value >> 8) & 0xFF;
    mem[addr + 2] = (value >> 16) & 0xFF;
    mem[addr + 3] = (value >> 24) & 0xFF;
}

int32_t sign_extend(uint32_t value, int bits) {
    if (value & (1 << (bits - 1))) {
        return value | (0xFFFFFFFF << bits);
    }
    return value;
}

bool is_valid_instruction(uint32_t instruction) {
    uint32_t opcode = instruction & 0x7F;
    return (opcode == 0x33 || opcode == 0x13 || opcode == 0x03 || opcode == 0x23 ||
            opcode == 0x63 || opcode == 0x67 || opcode == 0x37 || opcode == 0x17 || opcode == 0x6F);
}