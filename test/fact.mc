0x0	0x80000137	lui x2 524288	lui sp, 0x80000 # Initialize sp to 0x7FFFFFDC
0x4	0x00000013	addi x0 x0 0	nop
0x8	0x00000013	addi x0 x0 0	nop
0xc	0x00000013	addi x0 x0 0	nop
0x10	0x00000013	addi x0 x0 0	nop
0x14	0xFDC10113	addi x2 x2 -36	addi sp, sp, -36
0x18	0x00100293	addi x5 x0 1	li t0, 1 # t0 = result
0x1c	0x00530313	addi x6 x6 5	addi t1, t1, 5
0x20	0x00000013	addi x0 x0 0	nop
0x24	0x00000013	addi x0 x0 0	nop
0x28	0x00000013	addi x0 x0 0	nop
0x2c	0x00000013	addi x0 x0 0	nop
0x30	0x02030063	beq x6 x0 32	beq t1, zero, done # if n == 0, exit
0x34	0x026282B3	mul x5 x5 x6	mul t0, t0, t1 # result *= n
0x38	0xFFF30313	addi x6 x6 -1	addi t1, t1, -1 # n--
0x3c	0x00000013	addi x0 x0 0	nop
0x40	0x00000013	addi x0 x0 0	nop
0x44	0x00000013	addi x0 x0 0	nop
0x48	0x00000013	addi x0 x0 0	nop
0x4c	0xFE5FF06F	jal x0 -28	j loop
0x50	0x00028513	addi x10 x5 0	mv a0, t0 # Return result in a0
0x54    0xFFFFFFFF