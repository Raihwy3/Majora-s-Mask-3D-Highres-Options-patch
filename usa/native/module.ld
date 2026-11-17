OUTPUT_ARCH(arm)
ENTRY(mm_bootstrap)
SECTIONS {
    .bootstrap 0x61d400 : { KEEP(*(.bootstrap)) }
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
mm_game_set_display_mode = 0x11bf50;
mm_game_create_target = 0x1090a8;
mm_game_free_target = 0x1253bc;
mm_game_destroy_target = 0x5e6d0c;
mm_game_help_message = 0x1ccfdc;
mm_game_find_widget = 0x13bf78;
mm_game_current_widget = 0x17eb88;
mm_game_text_pane = 0x17ea60;
mm_game_ui_context = 0x1c51c8;
mm_game_parse_utf16 = 0x1b8bac;
mm_game_sprite_set_stream = 0x195fa0;

mm_game_stream_construct = 0x13549c;
mm_game_surface_transfer = 0x125678;
mm_game_options_input = 0x5a2e30;
mm_game_options_touch = 0x5a28e8;
mm_game_options_page = 0x5a1f7c;
mm_game_options_select = 0x5a2194;
mm_game_help_calc = 0x196c04;
mm_game_layout_manager = 0x1697f0;
mm_game_layout_submit = 0x17ec24;

/* Matched to native Options resource creation/destruction and calc calls. */
mm_game_resource_index = 0x169fe0;
mm_game_layout_create = 0x1695f8;
mm_game_layout_free = 0x169600;
mm_game_layout_calc = 0x161ac8;

mm_game_present = 0x15bc3c;


mm_game_make_buffers = 0x1299a0;
mm_game_bind_buffer = 0x129450;
mm_game_allocate_buffer = 0x129558;
mm_game_texture_copy = 0x1369dc;
mm_game_surface_packet = 0x136990;

mm_game_stick_normalize = 0x12ea04;
mm_game_ui_sound = 0x2006d4;
mm_game_animation_play = 0x191fd4;

mm_game_animation_controller = 0x151394;
mm_game_animation_find = 0x169e1c;
