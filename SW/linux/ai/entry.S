/* BL33 entry at EL2. FSBL has already loaded the Image, DTB and initramfs. */
.global _start
.section .text
_start:
    msr daifset, #0xf
    mov x0, #0x22000000
    mov x1, xzr
    mov x2, xzr
    mov x3, xzr
    mov x4, #0x20200000
    dsb sy
    isb
    br x4
