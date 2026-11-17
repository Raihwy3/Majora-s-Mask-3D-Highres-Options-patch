OUTPUT_ARCH(arm)
ENTRY(mm_bootstrap)
SECTIONS {
    .bootstrap 0x61cee0 : { KEEP(*(.bootstrap)) }
    ASSERT(SIZEOF(.bootstrap) <= 0x120, "Bootstrap exceeds existing text padding")
    . = 0x8f8000;
    __module_start = .;
    .text : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    . = ALIGN(0x1000);
    __module_rx_end = .;
    __module_rx_size = __module_rx_end - __module_start;
    .data : { *(.data .data.*) }
    . = ALIGN(0x1000);
    __module_file_end = .;
    .bss (NOLOAD) : { *(.bss .bss.* COMMON) }
    __module_bss_end = .;
    /DISCARD/ : { *(.comment .note.* .ARM.exidx* .ARM.extab*) }
}
mm_original_clear_bss = 0x100024;
mm_original_entry_continue = 0x100004;
mm_restoration_start = 0x61cecc;
mm_game_set_display_mode = 0x11be50;
mm_game_create_target = 0x108fac;
mm_game_free_target = 0x12521c;
mm_game_destroy_target = 0x5e69a0;
mm_game_help_message = 0x1ccfe4;
mm_game_find_widget = 0x13be78;
mm_game_current_widget = 0x17ebbc;
mm_game_text_pane = 0x17ea94;
mm_game_ui_context = 0x1c51d0;
mm_game_parse_utf16 = 0x1b8bb4;
mm_game_sprite_set_stream = 0x195fd4;

mm_game_stream_construct = 0x13539c;
mm_game_surface_transfer = 0x1254d8;
mm_game_options_input = 0x5a2ac8;
mm_game_options_touch = 0x5a2580;
mm_game_options_page = 0x5a1c14;
mm_game_options_select = 0x5a1e2c;
mm_game_help_calc = 0x196c38;
mm_game_layout_manager = 0x169824;
mm_game_layout_submit = 0x17ec58;

/* Matched to native Options resource creation/destruction and calc calls. */
mm_game_resource_index = 0x16a014;
mm_game_layout_create = 0x16962c;
mm_game_layout_free = 0x169634;
mm_game_layout_calc = 0x161ae8;

mm_game_present = 0x15bc60;


mm_game_make_buffers = 0x129800;
mm_game_bind_buffer = 0x1292b0;
mm_game_allocate_buffer = 0x1293b8;
mm_game_texture_copy = 0x1368dc;
mm_game_surface_packet = 0x136890;

mm_game_stick_normalize = 0x12e904;
mm_game_ui_sound = 0x2006e4;
mm_game_animation_play = 0x192008;

mm_game_animation_controller = 0x1513b8;
mm_game_animation_find = 0x169e50;
