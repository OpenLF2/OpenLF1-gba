@ SPDX-License-Identifier: MIT
@ Startup code and ROM header for the Game Boy Advance.

    .section .text.header, "ax"
    .arm
    .global _start
_start:
    b       reset                   @ entry point, branch over the header
    .space  156, 0                  @ Nintendo logo slot (filled in by tools/gbafix.py from tools/fetch_logo.py)
    .ascii  "OPENLF1\0\0\0\0\0"    @ title, 12 bytes
    .ascii  "LF1A"                  @ game code
    .ascii  "00"                    @ maker code
    .byte   0x96                    @ fixed value
    .byte   0, 0, 0                 @ main unit, device type, reserved
    .space  7, 0                    @ reserved
    .byte   0                       @ software version
    .byte   0                       @ header checksum, written by tools/gbafix.py
    .space  2, 0

    .section .text.boot, "ax"
    .arm
    .global reset
reset:
    @ Stacks: IRQ mode at the top of IWRAM, system/user mode below it.
    mov     r0, #0x12               @ IRQ mode
    msr     cpsr_c, r0
    ldr     sp, =0x03007FA0
    mov     r0, #0x1F               @ system mode
    msr     cpsr_c, r0
    ldr     sp, =0x03007F00

    @ Copy .data from its ROM image into EWRAM.
    ldr     r0, =__data_lma
    ldr     r1, =__data_start
    ldr     r2, =__data_end
1:  cmp     r1, r2
    ldrlo   r3, [r0], #4
    strlo   r3, [r1], #4
    blo     1b

    @ Zero .bss.
    ldr     r1, =__bss_start
    ldr     r2, =__bss_end
    mov     r3, #0
2:  cmp     r1, r2
    strlo   r3, [r1], #4
    blo     2b

    ldr     r0, =__libc_init_array
    mov     lr, pc
    bx      r0
    ldr     r0, =main
    mov     lr, pc
    bx      r0
3:  b       3b                      @ main returned: hang
