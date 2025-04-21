0x0	0x00300193	addi x3 x0 3	addi x3, x0, 3 # x3 = 3 (loop counter)
0x4	0x01000093	addi x1 x0 16	addi x1, x0, 16 # x1 = 0x10 (target memory)
0x8	0x0000A023	sw x0 0(x1)	sw x0, 0(x1) # memory[0x10] = 0 (initialize counter)
0xc	0x0000A203	lw x4 0(x1)	loop: lw x4, 0(x1) # x4 = memory[0x10]
0x10	0x00120213	addi x4 x4 1	addi x4, x4, 1 # x4 = x4 + 1
0x14	0x0040A023	sw x4 0(x1)	sw x4, 0(x1) # memory[0x10] = x4
0x18	0xFFF18193	addi x3 x3 -1	addi x3, x3, -1 # x3 = x3 - 1
0x1c	0x00018463	beq x3 x0 8	beq x3, x0, done # if x3 == 0, go to 'done'
0x20	0xFEDFF06F	jal x0 -20	jal x0, loop # unconditional jump to 'loop'
0x24	0x00000013	addi x0 x0 0	done: nop # program end
0x28   0xFFFFFFFF