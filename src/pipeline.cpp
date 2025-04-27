#include "../include/myRISCVSim.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>

using namespace std;

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

map<uint32_t, BPU> branch_prediction_unit;
bool branch_predictor_init = false;  // Default to Not Taken
uint32_t total_predictions = 0;
uint32_t correct_predictions = 0;
uint32_t branch_mispredictions = 0;
bool print_bpu_details = false;  // Default to disabled

void fetch() {
    if_id.PC = PC;
    if_id.instruction = read_word(PC, instruction_memory);
    if (if_id.instruction == 0xFFFFFFFF) {
        is_draining = true;
    }
    PC += 4;
    cout << "Fetch: Instruction 0x" << hex << setw(8) << setfill('0') 
         << if_id.instruction << " at PC=0x" << if_id.PC << dec << " (Cycle " << total_cycles << ")" << endl;
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
    id_ex.instr_PC = if_id.PC;  // Store original instruction PC
    id_ex.opcode = if_id.instruction & 0x7F;
    id_ex.rd = (if_id.instruction >> 7) & 0x1F;
    id_ex.func3 = (if_id.instruction >> 12) & 0x7;
    id_ex.func7 = (if_id.instruction >> 25) & 0x7F;

    if (rs1 >= 32 || rs2 >= 32 || id_ex.rd >= 32) {
        cout << "Error: Invalid register index at PC=0x" << hex << if_id.PC << dec << endl;
        id_ex = ID_EX();
        return;
    }

    // Data Forwarding
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

    // Set instruction flags
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
    id_ex.is_bne = (id_ex.opcode == 0x63 && id_ex.func3 == 0x1);
    id_ex.is_jal = (id_ex.opcode == 0x6F);
    id_ex.is_jalr = (id_ex.opcode == 0x67 && id_ex.func3 == 0x0);
    id_ex.is_mul = (id_ex.opcode == 0x33 && id_ex.func3 == 0 && id_ex.func7 == 0x01);
    id_ex.is_blt = (id_ex.opcode == 0x63 && id_ex.func3 == 0x4);
    id_ex.is_bge = (id_ex.opcode == 0x63 && id_ex.func3 == 0x5);
    id_ex.is_sll = (id_ex.opcode == 0x33 && id_ex.func3 == 0x1 && id_ex.func7 == 0x00);
    id_ex.is_srl = (id_ex.opcode == 0x33 && id_ex.func3 == 0x5 && id_ex.func7 == 0x00);
    id_ex.is_sra = (id_ex.opcode == 0x33 && id_ex.func3 == 0x5 && id_ex.func7 == 0x20);
    id_ex.is_rem = (id_ex.opcode == 0x33 && id_ex.func3 == 0x6 && id_ex.func7 == 0x01);
    id_ex.is_lb = (id_ex.opcode == 0x03 && id_ex.func3 == 0x0);
    id_ex.is_sb = (id_ex.opcode == 0x23 && id_ex.func3 == 0x0);
    id_ex.is_byte_op = id_ex.is_lb || id_ex.is_sb;

    // Set control signals
    id_ex.reg_write = (id_ex.is_add || id_ex.is_sub || id_ex.is_and || id_ex.is_or || 
        id_ex.is_addi || id_ex.is_andi || id_ex.is_ori || id_ex.is_lw || 
        id_ex.is_lui || id_ex.is_auipc || id_ex.is_jal || id_ex.is_jalr || 
        id_ex.is_mul || id_ex.is_sll || id_ex.is_srl || id_ex.is_sra || 
        id_ex.is_rem || id_ex.is_lb);
    id_ex.mem_read = (id_ex.is_lw || id_ex.is_lb);
    id_ex.mem_write = (id_ex.is_sw || id_ex.is_sb);

    // Set base_reg for memory access
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
    if (id_ex.is_addi || id_ex.is_andi || id_ex.is_ori || id_ex.is_lw || id_ex.is_lb || id_ex.is_jalr) {
        id_ex.imm = sign_extend((if_id.instruction >> 20) & 0xFFF, 12);
        if (id_ex.is_addi || id_ex.is_andi || id_ex.is_ori || id_ex.is_lw || id_ex.is_lb) {
            id_ex.rs2_val = 0;
        }
    } else if (id_ex.is_sw || id_ex.is_sb) {
        id_ex.imm = sign_extend((((if_id.instruction >> 25) & 0x7F) << 5) | ((if_id.instruction >> 7) & 0x1F), 12);
    } else if (id_ex.is_beq || id_ex.is_bne || id_ex.is_blt || id_ex.is_bge) {
        uint32_t imm = 0;
        imm |= ((if_id.instruction >> 31) & 0x1) << 11;  // imm[12] (sign bit)
        imm |= ((if_id.instruction >> 7) & 0x1) << 10;   // imm[11]
        imm |= ((if_id.instruction >> 25) & 0x3F) << 4;  // imm[10:5]
        imm |= ((if_id.instruction >> 8) & 0xF) << 0;    // imm[4:1]
        id_ex.imm = sign_extend(imm, 12) << 1;           // Sign-extend from bit 11, shift for byte offset
    } else if (id_ex.is_lui || id_ex.is_auipc) {
        id_ex.imm = (if_id.instruction & 0xFFFFF000);
    } else if (id_ex.is_jal) {
        id_ex.imm = sign_extend((((if_id.instruction >> 31) & 0x1) << 20) | (((if_id.instruction >> 12) & 0xFF) << 12) |
                                (((if_id.instruction >> 20) & 0x1) << 11) | (((if_id.instruction >> 21) & 0x3FF) << 1), 21);
    } else {
        id_ex.imm = 0;
    }

    // Branch Prediction
    if (id_ex.is_beq || id_ex.is_bne || id_ex.is_blt || id_ex.is_bge || id_ex.is_jal || id_ex.is_jalr) {
        uint32_t instr_PC = id_ex.instr_PC;
        if (branch_prediction_unit.find(instr_PC) == branch_prediction_unit.end()) {
            branch_prediction_unit[instr_PC] = BPU();
            branch_prediction_unit[instr_PC].predictor_state = branch_predictor_init;
        }
    
        uint32_t predicted_pc;
        if (id_ex.is_beq || id_ex.is_bne || id_ex.is_blt || id_ex.is_bge) {
            total_predictions++;
            bool predicted_taken = branch_prediction_unit[instr_PC].predictor_state;
            predicted_pc = predicted_taken
                          ? (branch_prediction_unit[instr_PC].btb_valid
                             ? branch_prediction_unit[instr_PC].btb_target
                             : instr_PC + id_ex.imm)
                          : instr_PC + 4;
            cout << "Decode: " << (id_ex.is_beq ? "beq" : id_ex.is_bne ? "bne" : id_ex.is_blt ? "blt" : "bge") << " predicted "
                 << (predicted_taken ? "Taken" : "Not Taken")
                 << ", PC set to 0x" << hex << predicted_pc << dec << endl;
        } else {
            predicted_pc = branch_prediction_unit[instr_PC].btb_valid
                           ? branch_prediction_unit[instr_PC].btb_target
                           : (id_ex.is_jal ? instr_PC + id_ex.imm : (id_ex.rs1_val + id_ex.imm) & ~0x1);
            cout << "Decode: " << (id_ex.is_jal ? "jal" : "jalr")
                 << " using " << (branch_prediction_unit[instr_PC].btb_valid ? "BTB" : "computed")
                 << " target, PC set to 0x" << hex << predicted_pc << dec << endl;
        }
    
        PC = predicted_pc;
        id_ex.predicted_pc = predicted_pc;  // Store for execute stage verification
    }

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
    else if (id_ex.is_bne) ss << "Decode: bne, rs1=x" << rs1 << ", rs2=x" << rs2 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_jal) ss << "Decode: jal, rd=x" << id_ex.rd << ", offset=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_jalr) ss << "Decode: jalr, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_mul) ss << "Decode: mul, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", rs2=x" << rs2;
    else if (id_ex.is_blt) ss << "Decode: blt, rs1=x" << rs1 << ", rs2=x" << rs2 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_bge) ss << "Decode: bge, rs1=x" << rs1 << ", rs2=x" << rs2 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_sll) ss << "Decode: sll, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", rs2=x" << rs2;
    else if (id_ex.is_srl) ss << "Decode: srl, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", rs2=x" << rs2;
    else if (id_ex.is_sra) ss << "Decode: sra, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", rs2=x" << rs2;
    else if (id_ex.is_rem) ss << "Decode: rem, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", rs2=x" << rs2;
    else if (id_ex.is_lb) ss << "Decode: lb, rd=x" << id_ex.rd << ", rs1=x" << rs1 << ", imm=0x" << hex << id_ex.imm << dec;
    else if (id_ex.is_sb) ss << "Decode: sb, rs1=x" << rs1 << ", rs2=x" << rs2 << ", imm=0x" << hex << id_ex.imm << dec;
    else ss << "Decode: Unknown instruction at PC=0x" << hex << id_ex.instr_PC << dec;
    cout << ss.str() << " (Cycle " << total_cycles << ")" << endl;
}

void execute() {
    if (id_ex.opcode == 0) {
        cout << "Execute: NOP (Cycle " << total_cycles << ")" << endl;
        ex_mem = EX_MEM();
        return;
    }

    instructions_executed++;
    if (id_ex.is_lw || id_ex.is_sw || id_ex.is_lb || id_ex.is_sb) {
        data_transfer_instructions++;
    } else if (id_ex.is_add || id_ex.is_sub || id_ex.is_and || id_ex.is_or ||
               id_ex.is_addi || id_ex.is_andi || id_ex.is_ori || id_ex.is_lui ||
               id_ex.is_auipc || id_ex.is_mul || id_ex.is_sll || id_ex.is_srl ||
               id_ex.is_sra || id_ex.is_rem) {
        alu_instructions++;
    } else if (id_ex.is_beq || id_ex.is_bne || id_ex.is_jal || id_ex.is_jalr ||
               id_ex.is_blt || id_ex.is_bge) {
        control_instructions++;
    }

    ex_mem = EX_MEM();
    ex_mem.rd = id_ex.rd;
    ex_mem.reg_write = id_ex.reg_write;
    ex_mem.mem_read = id_ex.mem_read;
    ex_mem.mem_write = id_ex.mem_write;

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

    if (id_ex.is_sll) {
        ex_mem.alu_result = id_ex.rs1_val << (id_ex.rs2_val & 0x1F);
    } else if (id_ex.is_srl) {
        ex_mem.alu_result = id_ex.rs1_val >> (id_ex.rs2_val & 0x1F);
    } else if (id_ex.is_sra) {
        int32_t signed_rs1 = static_cast<int32_t>(id_ex.rs1_val);
        ex_mem.alu_result = static_cast<uint32_t>(signed_rs1 >> (id_ex.rs2_val & 0x1F));
    } else if (id_ex.is_rem) {
        if (id_ex.rs2_val == 0) {
            ex_mem.alu_result = id_ex.rs1_val;
        } else {
            int32_t signed_rs1 = static_cast<int32_t>(id_ex.rs1_val);
            int32_t signed_rs2 = static_cast<int32_t>(id_ex.rs2_val);
            ex_mem.alu_result = static_cast<uint32_t>(signed_rs1 % signed_rs2);
        }
    }

    if (id_ex.is_auipc) {
        ex_mem.alu_result = id_ex.instr_PC + id_ex.imm;
    } else if (id_ex.is_lui) {
        ex_mem.alu_result = id_ex.imm;
    } else if (id_ex.is_mul) {
        ex_mem.alu_result = id_ex.rs1_val * id_ex.rs2_val;
    }

    if (id_ex.is_beq || id_ex.is_bne) {
        uint32_t instr_PC = id_ex.instr_PC;
        bool actual_taken = id_ex.is_beq ? (id_ex.rs1_val == id_ex.rs2_val) : (id_ex.rs1_val != id_ex.rs2_val);
        ex_mem.is_branch = actual_taken;
        ex_mem.branch_target = instr_PC + id_ex.imm;
        bool predicted_taken = branch_prediction_unit[instr_PC].predictor_state;
        uint32_t predicted_pc = id_ex.predicted_pc;

        bool mispredicted = (actual_taken != predicted_taken) ||
                           (actual_taken && predicted_pc != ex_mem.branch_target);
        if (mispredicted) {
            branch_mispredictions++;
            control_hazards++;
            PC = actual_taken ? ex_mem.branch_target : instr_PC + 4;
            cout << "Execute: " << (id_ex.is_beq ? "beq" : "bne") << " Misprediction at PC=0x" << hex << instr_PC
                 << ", Actual: " << (actual_taken ? "Taken" : "Not Taken")
                 << ", Predicted: " << (predicted_taken ? "Taken" : "Not Taken")
                 << ", Correcting PC to 0x" << PC << dec << endl;
            if_id = IF_ID();
            id_ex = ID_EX();
            control_hazard_flush = true;
        } else {
            correct_predictions++;
            cout << "Execute: " << (id_ex.is_beq ? "beq" : "bne") << " Correct at PC=0x" << hex << instr_PC
                 << ", Outcome: " << (actual_taken ? "Taken" : "Not Taken") << dec << endl;
        }

        branch_prediction_unit[instr_PC].predictor_state = actual_taken;
        if (actual_taken) {
            branch_prediction_unit[instr_PC].btb_target = ex_mem.branch_target;
            branch_prediction_unit[instr_PC].btb_valid = true;
        }
        ex_mem.alu_result = instr_PC + 4;
    } else if (id_ex.is_blt) {
        uint32_t instr_PC = id_ex.instr_PC;
        bool actual_taken = (static_cast<int32_t>(id_ex.rs1_val) < static_cast<int32_t>(id_ex.rs2_val));
        ex_mem.is_branch = actual_taken;
        ex_mem.branch_target = instr_PC + id_ex.imm;
        bool predicted_taken = branch_prediction_unit[instr_PC].predictor_state;
        uint32_t predicted_pc = id_ex.predicted_pc;
    
        bool mispredicted = (actual_taken != predicted_taken) ||
                           (actual_taken && predicted_pc != ex_mem.branch_target);
        if (mispredicted) {
            branch_mispredictions++;
            control_hazards++;
            PC = actual_taken ? ex_mem.branch_target : instr_PC + 4;
            cout << "Execute: blt Misprediction at PC=0x" << hex << instr_PC
                 << ", Actual: " << (actual_taken ? "Taken" : "Not Taken")
                 << ", Predicted: " << (predicted_taken ? "Taken" : "Not Taken")
                 << ", Correcting PC to 0x" << PC << dec << endl;
            if_id = IF_ID();
            id_ex = ID_EX();
            control_hazard_flush = true;
        } else {
            correct_predictions++;
            cout << "Execute: blt Correct at PC=0x" << hex << instr_PC
                 << ", Outcome: " << (actual_taken ? "Taken" : "Not Taken") << dec << endl;
        }
    
        branch_prediction_unit[instr_PC].predictor_state = actual_taken;
        if (actual_taken) {
            branch_prediction_unit[instr_PC].btb_target = ex_mem.branch_target;
            branch_prediction_unit[instr_PC].btb_valid = true;
        }
        ex_mem.alu_result = instr_PC + 4;
    } else if (id_ex.is_bge) {
        uint32_t instr_PC = id_ex.instr_PC;
        bool actual_taken = (static_cast<int32_t>(id_ex.rs1_val) >= static_cast<int32_t>(id_ex.rs2_val));
        ex_mem.is_branch = actual_taken;
        ex_mem.branch_target = instr_PC + id_ex.imm;
        bool predicted_taken = branch_prediction_unit[instr_PC].predictor_state;
        uint32_t predicted_pc = id_ex.predicted_pc;
    
        bool mispredicted = (actual_taken != predicted_taken) ||
                           (actual_taken && predicted_pc != ex_mem.branch_target);
        if (mispredicted) {
            branch_mispredictions++;
            control_hazards++;
            PC = actual_taken ? ex_mem.branch_target : instr_PC + 4;
            cout << "Execute: bge Misprediction at PC=0x" << hex << instr_PC
                 << ", Actual: " << (actual_taken ? "Taken" : "Not Taken")
                 << ", Predicted: " << (predicted_taken ? "Taken" : "Not Taken")
                 << ", Correcting PC to 0x" << PC << dec << endl;
            if_id = IF_ID();
            id_ex = ID_EX();
            control_hazard_flush = true;
        } else {
            correct_predictions++;
            cout << "Execute: bge Correct at PC=0x" << hex << instr_PC
                 << ", Outcome: " << (actual_taken ? "Taken" : "Not Taken") << dec << endl;
        }
    
        branch_prediction_unit[instr_PC].predictor_state = actual_taken;
        if (actual_taken) {
            branch_prediction_unit[instr_PC].btb_target = ex_mem.branch_target;
            branch_prediction_unit[instr_PC].btb_valid = true;
        }
        ex_mem.alu_result = instr_PC + 4;
    } else if (id_ex.is_jal) {
        uint32_t instr_PC = id_ex.instr_PC;
        ex_mem.branch_target = instr_PC + id_ex.imm;
        uint32_t predicted_pc = id_ex.predicted_pc;

        if (predicted_pc != ex_mem.branch_target) {
            branch_mispredictions++;
            control_hazards++;
            PC = ex_mem.branch_target;
            cout << "Execute: jal Misprediction at PC=0x" << hex << instr_PC
                 << ", Correcting PC to 0x" << PC << dec << endl;
            if_id = IF_ID();
            id_ex = ID_EX();
            control_hazard_flush = true;
        } else {
            cout << "Execute: jal Correct at PC=0x" << hex << instr_PC << dec << endl;
        }

        branch_prediction_unit[instr_PC].btb_target = ex_mem.branch_target;
        branch_prediction_unit[instr_PC].btb_valid = true;
        ex_mem.alu_result = instr_PC + 4;
        ex_mem.reg_write = true;

        //for debugging
        cout << "Execute: jal at instr_PC=0x" << hex << instr_PC << ", imm=0x" << id_ex.imm
     << ", predicted_pc=0x" << predicted_pc << ", branch_target=0x" << ex_mem.branch_target << dec << endl;
    } else if (id_ex.is_jalr) {
        uint32_t instr_PC = id_ex.instr_PC;
        ex_mem.branch_target = (id_ex.rs1_val + id_ex.imm) & ~0x1;
        uint32_t predicted_pc = id_ex.predicted_pc;

        if (predicted_pc != ex_mem.branch_target) {
            branch_mispredictions++;
            control_hazards++;
            PC = ex_mem.branch_target;
            cout << "Execute: jalr Misprediction at PC=0x" << hex << instr_PC
                 << ", Correcting PC to 0x" << PC << dec << endl;
            if_id = IF_ID();
            id_ex = ID_EX();
            control_hazard_flush = true;
        } else {
            cout << "Execute: jalr Correct at PC=0x" << hex << instr_PC << dec << endl;
        }

        branch_prediction_unit[instr_PC].btb_target = ex_mem.branch_target;
        branch_prediction_unit[instr_PC].btb_valid = true;
        ex_mem.alu_result = instr_PC + 4;
        ex_mem.reg_write = true;
    }

    if (id_ex.is_sw) {
        ex_mem.rs2_val = id_ex.rs2_val;
    }

    ex_mem.is_byte_op = id_ex.is_byte_op;
    ex_mem.instr_PC = id_ex.instr_PC;

    cout << "Execute: ALU Result=0x" << hex << setw(8) << setfill('0') << ex_mem.alu_result;
    if (id_ex.is_beq || id_ex.is_bne) cout << ", Branch=" << (ex_mem.is_branch ? "Taken" : "Not Taken") << ", Target=0x" << hex << ex_mem.branch_target;
    else if (id_ex.is_jal || id_ex.is_jalr) cout << ", Jump Target=0x" << hex << ex_mem.branch_target;
    cout << dec << " (Cycle " << total_cycles << ")" << endl;
}

void memory_access() {
    mem_wb = MEM_WB();
    mem_wb.rd = ex_mem.rd;
    mem_wb.reg_write = ex_mem.reg_write;
    mem_wb.mem_read = ex_mem.mem_read;
    mem_wb.alu_result = ex_mem.alu_result;
    mem_wb.instr_PC = ex_mem.instr_PC;

    if (ex_mem.mem_read) {
        if (ex_mem.is_byte_op) {  // for lb
            try {
                uint8_t byte = data_memory.at(ex_mem.alu_result);
                mem_wb.mem_data = sign_extend(byte, 8);
            } catch (const out_of_range&) {
                mem_wb.mem_data = 0x0;
                cout << "Warning: Memory read at uninitialized address 0x" << hex << ex_mem.alu_result << dec << endl;
            }
            if (ex_mem.base_reg == 2) {
                cout << "Stack load (byte): address=0x" << hex << ex_mem.alu_result << ", data=0x" << mem_wb.mem_data << dec << endl;
            }
        } else {  // for lw
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
        }
        cout << "Memory Access: Read Data=0x" << hex << setw(8) << setfill('0') << mem_wb.mem_data 
             << " from Address=0x" << ex_mem.alu_result << dec << " (Cycle " << total_cycles << ")" << endl;
    } else if (ex_mem.mem_write) {
        if (ex_mem.is_byte_op) {  // for sb
            data_memory[ex_mem.alu_result] = ex_mem.rs2_val & 0xFF;
            if (ex_mem.base_reg == 2) {
                cout << "Stack store (byte): address=0x" << hex << ex_mem.alu_result << ", data=0x" << (ex_mem.rs2_val & 0xFF) << dec << endl;
            }
            cout << "Memory Access: Wrote 0x" << hex << setw(2) << setfill('0') << (ex_mem.rs2_val & 0xFF)
                 << " to Address=0x" << ex_mem.alu_result << dec << " (Cycle " << total_cycles << ")" << endl;
        } else {  // for sw
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
    } else {
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
             << " to x" << dec << setw(2) << setfill('0') << mem_wb.rd << " (Cycle " << total_cycles << ")" << endl;
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

void print_bpu_state() {
    if (!print_bpu_details) return;
    cout << "Cycle " << total_cycles << " BPU State:" << endl;
    cout << "-------------------------" << endl;
    if (branch_prediction_unit.empty()) {
        cout << "No branch predictions recorded." << endl;
    } else {
        for (const auto& entry : branch_prediction_unit) {
            cout << "PC=0x" << hex << setw(8) << setfill('0') << entry.first
                 << ": State=" << (entry.second.predictor_state ? "Taken" : "Not Taken")
                 << ", BTB Target=0x" << setw(8) << setfill('0') << entry.second.btb_target
                 << ", BTB Valid=" << (entry.second.btb_valid ? "Yes" : "No") << dec << endl;
        }
    }
}

bool print_register_file_arg = false;
bool print_pipeline_registers = false;
bool track_instruction = false;
uint32_t tracked_pc = 0;

void print_register_file() {
    cout << "Register File:" << endl;
    for (int i = 0; i < 32; i++) {
        cout << "x" << dec << i << ": 0x" << hex << reg_file[i] << endl;
    }
    cout << endl;
}

void print_IF_ID() {
    cout << "IF/ID: PC=0x" << hex << if_id.PC << ", Instruction=0x" << if_id.instruction << dec << endl;
}

void print_ID_EX() {
    if (id_ex.opcode == 0) {
        cout << "ID/EX: Bubble" << endl;
    } else {
        cout << "ID/EX: instr_PC=0x" << hex << id_ex.instr_PC << ", predicted_pc=0x" << id_ex.predicted_pc
             << ", rs1_val=0x" << id_ex.rs1_val << ", rs2_val=0x" << id_ex.rs2_val
             << ", imm=0x" << id_ex.imm << ", rd=x" << dec << id_ex.rd
             << ", opcode=0x" << hex << id_ex.opcode << ", func3=0x" << id_ex.func3 << ", func7=0x" << id_ex.func7
             << ", is_add=" << id_ex.is_add << ", is_sub=" << id_ex.is_sub << ", reg_write=" << id_ex.reg_write
             << ", mem_read=" << id_ex.mem_read << ", mem_write=" << id_ex.mem_write << dec << endl;
    }
}

void print_EX_MEM() {
    cout << "EX/MEM: instr_PC=0x" << hex << ex_mem.instr_PC << ", alu_result=0x" << ex_mem.alu_result
         << ", rs2_val=0x" << ex_mem.rs2_val << ", rd=x" << dec << ex_mem.rd
         << ", reg_write=" << ex_mem.reg_write << ", mem_read=" << ex_mem.mem_read << ", mem_write=" << ex_mem.mem_write
         << ", is_branch=" << ex_mem.is_branch << ", branch_target=0x" << hex << ex_mem.branch_target << dec << endl;
}

void print_MEM_WB() {
    cout << "MEM/WB: instr_PC=0x" << hex << mem_wb.instr_PC << ", alu_result=0x" << mem_wb.alu_result
         << ", rd=x" << dec << mem_wb.rd << ", reg_write=" << mem_wb.reg_write << ", mem_data=0x" << hex << mem_wb.mem_data << dec << endl;
}