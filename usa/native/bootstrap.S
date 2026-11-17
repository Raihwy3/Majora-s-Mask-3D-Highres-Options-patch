/* Own loader. The gap at 0x61cf44 is a weak-runtime handle and must stay zero. */
.syntax unified
.arch armv6k
.arm
.section .bootstrap,"ax",%progbits
.global mm_bootstrap
mm_bootstrap:
    push {r0-r12, lr}
    mrs r6, cpsr
    push {r6, r7}
    ldr r1, =0xffff8001
    svc 0x35                       /* GetProcessId: input r1 = handle; output r1 = PID */
    cmp r0, #0
    bmi mm_boot_abort
    svc 0x33                       /* OpenProcess: input r1 = PID; output r1 = handle */
    cmp r0, #0
    bmi mm_boot_abort
    mov r7, r1
    mov r0, r1
    ldr r1, =__module_start
    mov r2, #0
    ldr r3, =__module_rx_size
    mov r4, #6                     /* PROTECT */
    mov r5, #5                     /* RX, only this module's code pages */
    svc 0x70
    mov r6, r0
    mov r0, r7
    svc 0x23                       /* CloseHandle even if protection failed */
    b mm_boot_continue
    .org 0x64
    .word 0                        /* Runtime 0x61cf44: retain null weak handle */
mm_boot_continue:
    cmp r6, #0
    bmi mm_boot_abort
    bl mm_native_init
    pop {r6, r7}
    msr cpsr_f, r6
    pop {r0-r12, lr}
    bl mm_original_clear_bss
    b mm_original_entry_continue
mm_boot_abort:
    /* Never jump into non-executable memory after a kernel error. */
    svc 0x03
    b mm_boot_abort
    .ltorg
