0x0	0x00100193	addi x3 x0 1	addi x3, x0, 1
0x4	0x00018513	addi x10 x3 0	addi x10, x3, 0 #raw
0x8	0x000FA503	lw x10 0(x31)	lw x10, 0(x31)
0xc	0x00A58533	add x10 x11 x10	add x10, x11, x10 #load-use
0x10 0xFFFFFFFF  // exit