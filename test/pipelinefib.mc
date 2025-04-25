0x0	0x80000137	lui x2 524288	lui sp, 0x80000
0x4	0xFDC10113	addi x2 x2 -36	addi sp, sp, -36
0x8	0x00A00593	addi x11 x0 10	addi a1, x0, 10 # Length of Fibonacci sequence (5 numbers)
0xc	0x10000637	lui x12 65536	lui a2, 0x10000
0x10	0x10060613	addi x12 x12 256	addi a2, a2, 0x100 # a2 = 0x10000100 (destination memory)
0x14	0x00000293	addi x5 x0 0	addi t0, x0, 0 # t0 = 0
0x18	0x00100313	addi x6 x0 1	addi t1, x0, 1 # t1 = 1
0x1c	0x00058E13	addi x28 x11 0	addi t3, a1, 0 # t3 = a1 (original sequence length)
0x20	0x040E0063	beq x28 x0 64	beq t3, x0, end # if t3 == 0, jump to end
0x24	0x00562023	sw x5 0(x12)	sw t0, 0(a2) # Write first value (0)
0x28	0xFFF58593	addi x11 x11 -1	addi a1, a1, -1
0x2c	0x02058A63	beq x11 x0 52	beq a1, x0, end # if length now 0, done
0x30	0x00460613	addi x12 x12 4	addi a2, a2, 4
0x34	0x00662023	sw x6 0(x12)	sw t1, 0(a2) # Write second value (1)
0x38	0xFFF58593	addi x11 x11 -1	addi a1, a1, -1
0x3c	0x02058263	beq x11 x0 36	beq a1, x0, end # if length now 0, done
0x40	0x005303B3	add x7 x6 x5	add t2, t1, t0
0x44	0x00460613	addi x12 x12 4	addi a2, a2, 4
0x48	0x00762023	sw x7 0(x12)	sw t2, 0(a2)
0x4c	0x00030293	addi x5 x6 0	addi t0, t1, 0 # t0 = t1
0x50	0x00038313	addi x6 x7 0	addi t1, t2, 0 # t1 = t2
0x54	0xFFF58593	addi x11 x11 -1	addi a1, a1, -1
0x58	0x00058463	beq x11 x0 8	beq a1, x0, end
0x5c	0xFE5FF06F	jal x0 -28	jal x0, Fibonacci # unconditional jump
0x60	0x00000013	addi x0 x0 0	nop
0x64    0xffffffff