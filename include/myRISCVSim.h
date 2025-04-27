#ifndef MYRISCVSIM_H
#define MYRISCVSIM_H

#include <cstdint>
#include <unordered_map>
#include <string>
#include <map>

// Define pipeline register structure for IF/ID
struct IF_ID {
    uint32_t PC;
    uint32_t instruction;
};

// Define pipeline register structure for ID/EX with detailed control signals
struct ID_EX {
    uint32_t instr_PC;    // Original PC of the instruction
    uint32_t PC;          // Current PC (unused in this fix, kept for compatibility)
    uint32_t predicted_pc;// Predicted PC for branch/jump verification
    uint32_t rs1_val;     // Value of source register 1
    uint32_t rs2_val;     // Value of source register 2
    uint32_t base_reg;
    uint32_t imm;         // Immediate value
    uint32_t rd;          // Destination register
    uint32_t opcode;
    uint32_t func3;       // Function code 3 bits
    uint32_t func7;       // Function code 7 bits

    // Specific instruction flags
    bool is_add;
    bool is_sub;
    bool is_and;
    bool is_or;
    bool is_addi;
    bool is_andi;
    bool is_ori;
    bool is_lw;
    bool is_sw;
    bool is_lui;
    bool is_auipc;
    bool is_beq;
    bool is_bne;  
    bool is_jal;
    bool is_jalr;
    bool is_mul;
    bool is_blt;
    bool is_bge;
    bool is_sll;
    bool is_srl;
    bool is_sra;
    bool is_rem;
    bool is_lb;
    bool is_sb;
    bool is_byte_op;  // true for lb and sb

    uint32_t alu_op;   // 0: ADD, 1: SUB, 2: AND, 3: OR, etc.
    bool reg_write;    // Enable register writeback
    bool mem_read;     // Enable memory read
    bool mem_write;    // Enable memory write
};

// Define pipeline register structure for EX/MEM
struct EX_MEM {
    uint32_t PC;
    uint32_t alu_result;  // Result of ALU operation
    uint32_t rs2_val;     // Value for store instructions
    uint32_t rd;          // Destination register
    uint32_t base_reg;
    bool reg_write;       // Enable register writeback
    bool mem_read;        // Enable memory read
    bool mem_write;       // Enable memory write
    bool is_branch;       // Flag for branch taken
    uint32_t branch_target; // Target PC for branch/jump
    bool is_byte_op;
};

// Define pipeline register structure for MEM/WB
struct MEM_WB {
    uint32_t alu_result;  // ALU result or memory read data
    uint32_t rd;          // Destination register
    bool reg_write;       // Enable register writeback
    uint32_t mem_data;    // Data read from memory (for lw)
    bool mem_read; //added for lw instructions
};

// Branch Prediction Unit (BPU) Entry
struct BPU {
    bool predictor_state;  // 0 = Not Taken, 1 = Taken
    uint32_t btb_target;   // Predicted target address
    bool btb_valid;        // Is this BTB entry valid?
    BPU() : predictor_state(false), btb_target(0), btb_valid(false) {}
};

// Branch Predictor Variables
extern std::map<uint32_t, BPU> branch_prediction_unit; // PC -> BPU entry
extern bool branch_predictor_init;  // Knob6: Initialize predictor state (0: NT, 1: T)
extern uint32_t total_predictions;  // Total branch predictions made
extern uint32_t correct_predictions;  // Correct branch predictions
extern uint32_t branch_mispredictions;  // Stat10: Number of branch mispredictions
extern bool print_bpu_details;  // Knob6: Enable/disable BPU details printing

// Global variables
extern uint32_t PC;
extern uint32_t cycle;
extern IF_ID if_id;
extern ID_EX id_ex;
extern EX_MEM ex_mem;
extern MEM_WB mem_wb;
extern std::unordered_map<uint32_t, uint8_t> instruction_memory;
extern std::unordered_map<uint32_t, uint8_t> data_memory;
extern uint32_t reg_file[32];  // Register file (x0 to x31)

extern bool stall;
extern bool control_hazard_flush;
extern bool is_draining;

// Knobs
extern bool pipelining_enabled;  // Knob1
extern bool forwarding_enabled;  // Knob2

// Statistics
extern uint32_t total_cycles;
extern uint32_t instructions_executed;
extern uint32_t data_hazards;
extern uint32_t control_hazards;
extern uint32_t stalls;
extern uint32_t data_transfer_instructions; // Stat4
extern uint32_t alu_instructions;          // Stat5
extern uint32_t control_instructions;      // Stat6

// Function declarations
void fetch();
void decode();
void execute();
void memory_access();
void writeback();
void load_mc_file(const std::string& filename);
uint32_t read_word(uint32_t addr, const std::unordered_map<uint32_t, uint8_t>& mem);
void write_word(uint32_t addr, uint32_t value, std::unordered_map<uint32_t, uint8_t>& mem);
int32_t sign_extend(uint32_t value, int bits);
bool is_valid_instruction(uint32_t instruction);
void print_bpu_state();

#endif