.syntax unified
.arch armv6k
.arm
.section .text.renderhooks,"ax",%progbits
.global mm_hook_stereo_depth
mm_hook_stereo_depth:
    push {r12}
    ldr r12, =mm_stereo_depth
    vldr s0, [r12]
    pop {r12}
    b 0x106920
/* Non-linking branches replace individual instructions, retaining LR and flags. */
.global mm_hook_rect_width, mm_hook_scene_size, mm_hook_copy_a
.global mm_hook_copy_b, mm_hook_total_width, mm_hook_inverse_width
mm_hook_rect_width:
    ldr r3, =mm_scene_width
    ldr r3, [r3]
    b 0x110adc
mm_hook_scene_size:
    ldr r0, =mm_scene_width
    ldr r0, [r0]
    ldr r3, =mm_scene_height
    ldr r3, [r3]
    b 0x1250c8
mm_hook_copy_a:
    /* 0x136c14 is a packed texture copy, not a logical viewport operation.
       Both retained source/destination textures keep 512x800 backing in all
       modes. Narrowing this copy treats the old rows as a different image. */
    mov r3, #800
    b 0x125900
mm_hook_copy_b:
    mov r3, #800
    b 0x125acc
mm_hook_total_width:
    /* Top-screen rectangle branch only (native +0x25bf4). The native helper
       otherwise selects 240/480 and doubles Y solely from the stereo flag.
       Supersampled stereo needs 480 here; Performance 2D needs 240.
       Calculate the selected height/Y scale explicitly, then take the native
       non-scaling branch. r1 is scratch inside this rectangle helper, not the
       engine stereo state. Bottom-screen entry bypasses this hook entirely. */
    ldr r12, =mm_scene_width
    ldr r12, [r12]
    ldr r6, =mm_scene_height
    ldr r6, [r6]
    mov r5, r6
    cmp r6, #480
    lsleq r7, r7, #1
    lsleq r8, r8, #1
    mov r1, #1
    b 0x125bf8
mm_hook_inverse_width:
    ldr r2, =mm_scene_width
    ldr r2, [r2]
    sub r2, r2, r9
    b 0x125c20
.ltorg

/* Callable native continuations for four entry hooks: replay precisely the
   displaced prologue, then resume original code. */
.global mm_game_options_draw, mm_game_main_calc, mm_game_main_draw
mm_game_options_draw:
    push {r4,r5,lr}
    b 0x5a31c0
mm_game_main_calc:
    push {r4-r11,lr}
    b 0x59b6a4
mm_game_main_draw:
    push {r4-r6,lr}
    b 0x59b280

.global mm_game_options_close
mm_game_options_close:
    push {r4-r6,lr}
    b 0x5a331c

.global mm_game_options_finish
mm_game_options_finish:
    push {r4-r8,lr}
    b 0x5a34d4
