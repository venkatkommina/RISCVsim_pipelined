0x0	0x00200193	addi x3 x0 2	addi x3, x0, 2 # x3 = 2
0x4	0x00200213	addi x4 x0 2	addi x4, x0, 2 # x4 = 2
0x8	0x00418663	beq x3 x4 12	beq x3, x4, 12 # Branch to PC=0x14 (taken, offset=12)
0xc	0x00A00293	addi x5 x0 10	addi x5, x0, 10 # Should not execute
0x10	0x01400313	addi x6 x0 20	addi x6, x0, 20 # Should not execute
0x14	0x00100393	addi x7 x0 1	addi x7, x0, 1 # x7 = 1
0x18 0xFFFFFFFF