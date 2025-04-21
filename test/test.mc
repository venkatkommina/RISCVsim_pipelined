0x0	0x00500293	addi x5 x0 5	addi x5, x0, 5
0x4	0x00310133	add x2 x2 x3	add x2, x2, x3
0x8	0x3E22A423	sw x2 1000(x5)	sw x2, 1000(x5)
0xc	0xFE028CE3	beq x5 x0 -8	beq x5, x0, l
0x10	0x002342B7	lui x5 564	lui x5, 0x00234
0x14	0x00008067	jalr x0 x1 0	jalr x0, x1, 0
0x18	0x0000006F	jal x0 0	jal x0, 0
0x0000001C 0xFFFFFFFF  // exit instruction