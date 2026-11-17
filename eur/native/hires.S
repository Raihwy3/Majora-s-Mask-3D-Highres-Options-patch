/* Reassembled from this mod's verified stable high-res patch, not Restoration.
 * Native addresses are linked separately. Dimensions remain the known baseline.
 */
.syntax unified
.arch armv6k
.arm
.section .text.hires,"ax",%progbits
.global mm_hires_setup, mm_hires_bottom, mm_hires_create
mm_hires_setup:
    push {r0-r12, lr}
    mov r0, #0x700
    orr r0, r0, #1
    bl mm_game_set_display_mode
    ldr r0, =0x7b2700
    mov r1, #800
    str r1, [r0, #8]
    pop {r0-r12, pc}
mm_hires_bottom:
    ldr r0, =0x6a3a48
    ldr r0, [r0, #12]
    ldr r0, [r0, #160]
    bx lr
mm_hires_destroy:
    push {r0-r3, r12, lr}
    ldr r1, [r0, #160]
    mov r2, #0
    str r2, [r0, #160]
    ldr r2, =0x63bc50
    str r2, [r0]
    bl mm_game_free_target
    pop {r0-r3, r12, lr}
    b mm_game_destroy_target
mm_hires_create:
    push {r4-r8, lr}
    mov r4, r0
    mov r5, r1
    mov r6, r2
    bl mm_game_create_target
    mov r7, r0
    mov r0, r4
    mov r1, r5
    mov r2, r6
    mov r3, #400
    bl mm_game_create_target
    str r0, [r7, #160]
    ldr r0, =mm_hires_destroy
    str r0, [r7, #164]
    mov r0, #0
    str r0, [r7, #168]
    str r0, [r7, #172]
    str r0, [r7, #176]
    str r0, [r7, #180]
    str r0, [r7, #184]
    add r0, r7, #164
    str r0, [r7]
    mov r0, r7
    pop {r4-r8, pc}
    .ltorg
