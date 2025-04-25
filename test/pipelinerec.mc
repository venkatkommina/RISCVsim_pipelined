0x0	0x80000137	lui x2 524288	lui sp, 0x80000
0x4	0xFDC10113	addi x2 x2 -36	addi sp, sp, -36 # sp = 0x7FFFFFDC
0x8	0x00300513	addi x10 x0 3	addi x10, x0, 3 # Input n = 5 (stored in x10)
0xc	0x008000EF	jal x1 8	jal x1, fact # Call factorial
0x10	0x0500006F	jal x0 80	jal x0, full_exit # Exit
0x14	0xFF810113	addi x2 x2 -8	addi sp, sp, -8
0x18	0x00112223	sw x1 4(x2)	sw x1, 4(sp)
0x1c	0x00A12023	sw x10 0(x2)	sw x10, 0(sp)
0x20	0xFFF50293	addi x5 x10 -1	addi x5, x10, -1 # x5 = x10 - 1
0x24	0x00000313	addi x6 x0 0	addi x6, x0, 0
0x28	0x00628463	beq x5 x6 8	beq x5, x6, recurse # if x5 == 0 (i.e., x10 == 1), recurse
0x2c	0x02050263	beq x10 x0 36	beq x10, x0, base # if x10 == 0, return 1 (base case)
0x30	0xFFF50513	addi x10 x10 -1	addi x10, x10, -1 # n = n - 1
0x34	0xFE1FF0EF	jal x1 -32	jal x1, fact # fact(n - 1)
0x38	0x00050333	add x6 x10 x0	add x6, x10, x0 # x6 = fact(n - 1)
0x3c	0x00012503	lw x10 0(x2)	lw x10, 0(sp) # restore original n
0x40	0x00412083	lw x1 4(x2)	lw x1, 4(sp)
0x44	0x00810113	addi x2 x2 8	addi sp, sp, 8
0x48	0x02650533	mul x10 x10 x6	mul x10, x10, x6 # n * fact(n - 1)
0x4c	0x00008067	jalr x0 x1 0	jalr x0, x1, 0
0x50	0x00100513	addi x10 x0 1	addi x10, x0, 1 # return 1
0x54	0x00412083	lw x1 4(x2)	lw x1, 4(sp)
0x58	0x00810113	addi x2 x2 8	addi sp, sp, 8
0x5c	0x00008067	jalr x0 x1 0	jalr x0, x1, 0
0x60	0x00000013	addi x0 x0 0	nop # Done
0x64    0xffffffff