0x0	0x00200193	addi x3 x0 2	addi x3, x0, 2 # x3 = 2 (loop counter)
0x4	0x100000B7	lui x1 65536	lui x1, 0x10000 # x1 = 0x10000 (memory address)
0x8	0x0030A023	sw x3 0(x1)	sw x3, 0(x1) # memory[0x10000] = 2
0xc	0x0000A203	lw x4 0(x1)	lw x4, 0(x1) # x4 = memory[0x10000] = 2
0x10	0x00120213	addi x4 x4 1	addi x4, x4, 1 # x4 = x4 + 1
0x14	0x0040A023	sw x4 0(x1)	sw x4, 0(x1) # memory[0x10000] = x4
0x18	0xFFF18193	addi x3 x3 -1	addi x3, x3, -1 # x3 = x3 - 1
0x1c	0x00018463	beq x3 x0 8	beq x3, x0, done # if x3 == 0, exit loop
0x20	0xFEDFF06F	jal x0 -20	jal x0, loop
0x24	0x00000013	addi x0 x0 0	addi x0, x0, 0 # done: nop (end)
0x28    0xffffffff