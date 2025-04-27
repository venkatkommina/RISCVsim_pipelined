#include "../include/myRISCVSim.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <unordered_map>

using namespace std;

void load_mc_file(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << filename << endl;
        exit(1);
    }
    string line;
    while (getline(file, line)) {
        if (line.empty()) continue;
        uint32_t addr, value;
        sscanf(line.c_str(), "0x%x 0x%x", &addr, &value);
        instruction_memory[addr] = value & 0xFF;
        instruction_memory[addr + 1] = (value >> 8) & 0xFF;
        instruction_memory[addr + 2] = (value >> 16) & 0xFF;
        instruction_memory[addr + 3] = (value >> 24) & 0xFF;
    }
    file.close();
}

int main(int argc, char* argv[]) {
    if (argc < 9) {
        cerr << "Usage: " << argv[0] << " <input.mc> <pipelining_enabled> <forwarding_enabled> <branch_predictor_init> <print_bpu_details> <print_register_file_arg> <print_pipeline_registers> <track_instruction> [instruction_number]" << endl;
        return 1;
    }
    
    load_mc_file(argv[1]);
    pipelining_enabled = (stoi(argv[2]) != 0);
    forwarding_enabled = (stoi(argv[3]) != 0);
    branch_predictor_init = (stoi(argv[4]) != 0);
    print_bpu_details = (stoi(argv[5]) != 0);
    print_register_file_arg = (stoi(argv[6]) != 0);
    print_pipeline_registers = (stoi(argv[7]) != 0);
    track_instruction = (stoi(argv[8]) != 0);
    if (track_instruction) {
        if (argc < 10) {
            cerr << "Error: Instruction number required for track_instruction" << endl;
            return 1;
        }
        int instruction_number = stoi(argv[9]);
        tracked_pc = (instruction_number - 1) * 4; // starts at PC=0, increment by 4
    }
    bool exit_detected = false;
    uint32_t drain_cycles = 0;

    if_id = IF_ID();
    id_ex = ID_EX();
    ex_mem = EX_MEM();
    mem_wb = MEM_WB();

    if (!pipelining_enabled) {
        while (true) {
            fetch();
            decode();
            execute();
            memory_access();
            writeback();

            if (if_id.instruction == 0xFFFFFFFF && !exit_detected) {
                exit_detected = true;
            }
            if (exit_detected) {
                drain_cycles++;
                if (drain_cycles >= 1) break;
            }
            total_cycles++;
        }
    } else {
        while (true) {
            writeback();
            if (track_instruction && mem_wb.instr_PC == tracked_pc) {
                cout << "Cycle " << total_cycles << ": Instruction at PC=0x" << hex << tracked_pc << " completes writeback." << endl;
                print_register_file();
            }
            memory_access();
            if (track_instruction && ex_mem.instr_PC == tracked_pc) {
                cout << "Cycle " << total_cycles << ": Instruction at PC=0x" << hex << tracked_pc << " completes memory access." << endl;
                print_MEM_WB();
            }
            execute();
            if (track_instruction && id_ex.instr_PC == tracked_pc) {
                cout << "Cycle " << total_cycles << ": Instruction at PC=0x" << hex << tracked_pc << " completes execute." << endl;
                print_EX_MEM();
            }
            decode();
            if (track_instruction && if_id.PC == tracked_pc) {
                cout << "Cycle " << total_cycles << ": Instruction at PC=0x" << hex << tracked_pc << " completes decode." << endl;
                print_ID_EX();
            }
            if (stall) {
                id_ex = ID_EX();
                stall = false;
                stalls++;
                cout << "Pipeline stalled this cycle" << endl;
            } else if (control_hazard_flush) {
                cout << "Fetch: Skipped due to control hazard (Cycle " << total_cycles << ")" << endl;
            } else {
                fetch();
                if (track_instruction && if_id.PC == tracked_pc) {
                    cout << "Cycle " << total_cycles << ": Instruction at PC=0x" << hex << tracked_pc << " completes fetch." << endl;
                    print_IF_ID();
                }
            }
            control_hazard_flush = false;
            if (if_id.instruction == 0xFFFFFFFF && !exit_detected) {
                exit_detected = true;
            }
            if (exit_detected) {
                drain_cycles++;
                if (drain_cycles > 4) break;
            }
            if (print_pipeline_registers) {
                cout << "Cycle " << total_cycles << " Pipeline Registers:" << endl;
                print_IF_ID();
                print_ID_EX();
                print_EX_MEM();
                print_MEM_WB();
            }
            if (print_register_file_arg) {
                cout << "Cycle " << total_cycles << " Register File:" << endl;
                print_register_file();
            }
            total_cycles++;
            print_bpu_state();
        }
    }

    cout << "Simulation complete." << endl;
    cout << "Total Cycles: " << total_cycles << endl;
    cout << "Instructions Executed: " << instructions_executed << endl;
    cout << "Stalls: " << stalls << endl;
    cout << "Data Hazards: " << data_hazards << endl;
    cout << "Control Hazards: " << control_hazards << endl;
    cout << "Data Transfer Instructions: " << data_transfer_instructions << endl;
    cout << "ALU Instructions: " << alu_instructions << endl;
    cout << "Control Instructions: " << control_instructions << endl;
    cout << "Total Branch Predictions: " << total_predictions << endl;
    cout << "Correct Branch Predictions: " << correct_predictions << endl;
    cout << "Branch Mispredictions: " << branch_mispredictions << endl;
    
    double hit_rate = total_predictions > 0 ? (double)correct_predictions / total_predictions * 100.0 : 0.0;
    cout << "Branch Prediction Hit Rate: " << correct_predictions << "/" << total_predictions
         << " (" << fixed << setprecision(2) << hit_rate << "%)" << endl;

    cout << "\nFinal Register File State:" << endl;
    cout << "-------------------------" << endl;
    for (int i = 0; i < 32; i++) {
        cout << "x" << dec << setw(2) << setfill('0') << i << ": 0x" << hex << setw(8) << setfill('0') 
             << reg_file[i] << dec << endl;
    }

    cout << "\nFinal Data Memory State:" << endl;
    cout << "-----------------------" << endl;
    if (data_memory.empty()) {
        cout << "No data written to memory." << endl;
    } else {
        // Find min and max aligned addresses
        uint32_t min_aligned_addr = UINT32_MAX;
        uint32_t max_aligned_addr = 0;
        for (const auto& pair : data_memory) {
            uint32_t aligned_addr = pair.first & ~0x3; // Align to 4-byte boundary
            min_aligned_addr = min(min_aligned_addr, aligned_addr);
            max_aligned_addr = max(max_aligned_addr, aligned_addr);
        }

        // Iterate over aligned addresses and print if all 4 bytes exist
        for (uint32_t addr = min_aligned_addr; addr <= max_aligned_addr; addr += 4) {
            try {
                uint32_t value = (data_memory.at(addr) << 0) |
                                (data_memory.at(addr + 1) << 8) |
                                (data_memory.at(addr + 2) << 16) |
                                (data_memory.at(addr + 3) << 24);
                cout << "0x" << hex << setw(8) << setfill('0') << addr << ": 0x" << setw(8) << setfill('0')
                    << value << dec << endl;
            } catch (const out_of_range&) {
                // Skip addresses where not all 4 bytes are initialized
            }
        }
    }
    return 0;
}