0x0	0x00200193	addi x3 x0 2	addi x3, x0, 2 # x3 = 2
0x4	0x00300213	addi x4 x0 3	addi x4, x0, 3 # x4 = 3
0x8	0x00418663	beq x3 x4 12	beq x3, x4, 12 # Not taken
0xc	0x00A00293	addi x5 x0 10	addi x5, x0, 10 # x5 = 10
0x10	0x00318663	beq x3 x3 12	beq x3, x3, 12 # Taken to 0x1c
0x14	0x01400313	addi x6 x0 20	addi x6, x0, 20 # Flushed
0x18	0x008000EF	jal x1 8	jal x1, 8 # not executed or flushed I dont know
0x1c	0x01E00393	addi x7 x0 30	addi x7, x0, 30 # x7 = 30
0x20	0x02800413	addi x8 x0 40	addi x8, x0, 40 # x8 = 40
0x24    0xFFFFFFFF