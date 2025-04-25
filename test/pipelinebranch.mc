0x0	0x00000093	addi x1 x0 0	addi x1, x0, 0 # i = 0
0x4	0x00A00113	addi x2 x0 10	addi x2, x0, 10 # limit = 10
0x8	0x00000193	addi x3 x0 0	addi x3, x0, 0 # counter for taken branches
0xc	0x00100213	addi x4 x0 1	addi x4, x0, 1 # constant 1
0x10	0x0010F293	andi x5 x1 1	andi x5, x1, 1 # x5 = i & 1 (check LSB to alternate)
0x14	0x00028663	beq x5 x0 12	beq x5, x0, taken # if even, branch taken
0x18	0x00108093	addi x1 x1 1	addi x1, x1, 1
0x1c	0x00C0006F	jal x0 12	jal x0, end_branch
0x20	0x00118193	addi x3 x3 1	addi x3, x3, 1 # increment taken counter
0x24	0x00108093	addi x1 x1 1	addi x1, x1, 1
0x28	0x00208463	beq x1 x2 8	beq x1, x2, done # exit loop if i == 10
0x2c	0xFE5FF06F	jal x0 -28	jal x0, loop
0x30	0xffffffff