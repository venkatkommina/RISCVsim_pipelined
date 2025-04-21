0x0	0x00500193	addi x3 x0 5	addi x3, x0, 5 # x3 = 5
0x4	0x00218213	addi x4 x3 2	addi x4, x3, 2 # x4 = 7
0x8	0x00120293	addi x5 x4 1	addi x5, x4, 1 # x5 = 8
0xc	0x00502823	sw x5 16(x0)	sw x5, 16(x0) # memory[0x10] = 8
0x10	0x01002303	lw x6 16(x0)	lw x6, 16(x0) # x6 = memory[0x10] = 8
0x14	0x003303B3	add x7 x6 x3	add x7, x6, x3 # x7 = 8 + 5 = 13
0x18	0x40438433	sub x8 x7 x4	sub x8, x7, x4 # x8 = 13 - 7 = 6
0x1c    0xFFFFFFFF