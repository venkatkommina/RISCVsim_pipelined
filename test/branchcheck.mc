0x0	0x00500293	addi x5 x0 5	addi x5, x0, 5 # x5 = 5
0x4	0x00A00313	addi x6 x0 10	addi x6, x0, 10 # x6 = 10
0x8	0x00F00393	addi x7 x0 15	addi x7, x0, 15 # x7 = 15
0xc	0x0062C463	blt x5 x6 8	blt x5, x6, label1 # since 5 < 10, branch taken
0x10	0x00100413	addi x8 x0 1	addi x8, x0, 1 # Should be skipped if branch taken
0x14	0x0063D463	bge x7 x6 8	bge x7, x6, label2 # since 15 >= 10, branch taken
0x18	0x00200413	addi x8 x0 2	addi x8, x0, 2 # Should be skipped if branch taken
0x1c	0x02A00493	addi x9 x0 42	addi x9, x0, 42 # Normal instruction
0x20    0xffffffff