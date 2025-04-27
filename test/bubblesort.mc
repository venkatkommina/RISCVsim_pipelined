0x0	0x100002B7	lui x5 65536	lui x5, 0x10000 # x5 = 0x10000000
0x4	0x00500313	addi x6 x0 5	addi x6, x0, 5 # x6 = array size 5
0x8	0x00700393	addi x7 x0 7	addi x7, x0, 7
0xc	0x0072A023	sw x7 0(x5)	sw x7, 0(x5)
0x10	0x00200393	addi x7 x0 2	addi x7, x0, 2
0x14	0x0072A223	sw x7 4(x5)	sw x7, 4(x5)
0x18	0x00900393	addi x7 x0 9	addi x7, x0, 9
0x1c	0x0072A423	sw x7 8(x5)	sw x7, 8(x5)
0x20	0x00100393	addi x7 x0 1	addi x7, x0, 1
0x24	0x0072A623	sw x7 12(x5)	sw x7, 12(x5)
0x28	0x00600393	addi x7 x0 6	addi x7, x0, 6
0x2c	0x0072A823	sw x7 16(x5)	sw x7, 16(x5)
0x30	0xFFF30413	addi x8 x6 -1	addi x8, x6, -1 # outer = size-1
0x34	0x00000493	addi x9 x0 0	addi x9, x0, 0 # i = 0
0x38	0x0484D063	bge x9 x8 64	bge x9, x8, next_pass # if i >= outer, next outer loop
0x3c	0x00048593	addi x11 x9 0	addi x11, x9, 0 # x11 = i
0x40	0x00B585B3	add x11 x11 x11	add x11, x11, x11 # x11 = i*2
0x44	0x00B585B3	add x11 x11 x11	add x11, x11, x11 # x11 = i*4
0x48	0x00B28633	add x12 x5 x11	add x12, x5, x11 # x12 = base + offset
0x4c	0x00062503	lw x10 0(x12)	lw x10, 0(x12) # x10 = arr[i]
0x50	0x00148693	addi x13 x9 1	addi x13, x9, 1 # x13 = i+1
0x54	0x00D68733	add x14 x13 x13	add x14, x13, x13 # x14 = (i+1)*2
0x58	0x00E70733	add x14 x14 x14	add x14, x14, x14 # x14 = (i+1)*4
0x5c	0x00E287B3	add x15 x5 x14	add x15, x5, x14 # x15 = base + (i+1)*4
0x60	0x0007A683	lw x13 0(x15)	lw x13, 0(x15) # x13 = arr[i+1]
0x64	0x00D54663	blt x10 x13 12	blt x10, x13, no_swap # if arr[i] < arr[i+1], no swap
0x68	0x00D62023	sw x13 0(x12)	sw x13, 0(x12) # arr[i] = arr[i+1]
0x6c	0x00A7A023	sw x10 0(x15)	sw x10, 0(x15) # arr[i+1] = arr[i]
0x70	0x00148493	addi x9 x9 1	addi x9, x9, 1 # i++
0x74	0xFC5FF06F	jal x0 -60	jal x0, bubble_sort_inner
0x78	0xFFF30313	addi x6 x6 -1	addi x6, x6, -1 # size = size-1
0x7c	0x00100393	addi x7 x0 1	addi x7, x0, 1
0x80	0x0063D463	bge x7 x6 8	bge x7, x6, done # if size <= 1, done
0x84	0xFADFF06F	jal x0 -84	jal x0, bubble_sort_outer
0x88	0x00000013	addi x0 x0 0	nop
0x8c    0xffffffff