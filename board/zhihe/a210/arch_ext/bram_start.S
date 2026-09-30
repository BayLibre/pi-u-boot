/*
 * Copyright(C) 2025 Zhihe Computing Technology (Shenzhen) Co., Ltd.
 */

#define BRAM_STACK_TOP 0x120000

.section .bram.text
_bram_start:
    .globl  bram_entry
    .type   bram_entry, %function
bram_entry:
    /* Save SPL general registers to SPL stack */
    addi sp,sp,-280
    //sd x0,16(sp) //zero
    sd x1,24(sp)  
    //sd x2,32(sp) //sp
    sd x3,40(sp)
    sd x4,48(sp)
    sd x5,56(sp)
    sd x6,64(sp)
    sd x7,72(sp)
    sd x8,80(sp)
    sd x9,88(sp)
    sd x10,96(sp)  //a0
    sd x11,104(sp) //a1
    sd x12,112(sp)
    sd x13,120(sp)
    sd x14,128(sp)
    sd x15,136(sp)
    sd x16,144(sp)
    sd x17,152(sp)
    sd x18,160(sp)
    sd x19,168(sp)
    sd x20,176(sp)
    sd x21,184(sp)
    sd x22,192(sp)
    sd x23,200(sp)
    sd x24,208(sp)
    sd x25,216(sp)
    sd x26,224(sp)
    sd x27,232(sp)
    sd x28,240(sp)
    sd x29,248(sp)
    sd x30,256(sp)
    sd x31,264(sp)

    /* Save SPL SP to new Stack top */
    li t0, BRAM_STACK_TOP
    addi t0, t0, -8
    sd sp, 0(t0)

    /* Switch to new Stack */
    mv sp, t0

    /* Main */
    jal bram_main

    /* Restore SPL Stack from new Stack top */
    la t0, BRAM_STACK_TOP
    addi t0,t0,-8
    ld sp,0(t0)

    /* Restore SPL general registers */
    //ld x0,16(sp) //zero
    ld x1,24(sp)
    //ld x2,32(sp) //sp
    ld x3,40(sp)
    ld x4,48(sp)
    ld x5,56(sp)
    ld x6,64(sp)
    ld x7,72(sp)
    ld x8,80(sp)
    ld x9,88(sp)
    ld x10,96(sp)
    ld x11,104(sp)
    ld x12,112(sp)
    ld x13,120(sp)
    ld x14,128(sp)
    ld x15,136(sp)
    ld x16,144(sp)
    ld x17,152(sp)
    ld x18,160(sp)
    ld x19,168(sp)
    ld x20,176(sp)
    ld x21,184(sp)
    ld x22,192(sp)
    ld x23,200(sp)
    ld x24,208(sp)
    ld x25,216(sp)
    ld x26,224(sp)
    ld x27,232(sp)
    ld x28,240(sp)
    ld x29,248(sp)
    ld x30,256(sp)
    ld x31,264(sp)
    addi sp,sp,280
    ret
