0x0	0x00500193	addi x3 x0 5	addi x3, x0, 5 # x3 = 5
0x4	0x00218213	addi x4 x3 2	addi x4, x3, 2 # x4 = x3 + 2 = 7
0x8	0x00120293	addi x5 x4 1	addi x5, x4, 1 # x5 = x4 + 1 = 8
0xc	0x0001A303	lw x6 0(x3)	lw x6, 0(x3) # x6 = memory[x3] = 0 (uninitialized)
0x10	0x005303B3	add x7 x6 x5	add x7, x6, x5 # x7 = x6 + x5 = 0 + 8 = 8
0x14 0xFFFFFFFF