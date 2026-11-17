/* Native Options third-page and gameplay pause-menu adapters. Game functions
 * and object fields are mapped from the supplied executable. No Restoration
 * implementation is included. Its original MainScreen calc remains the
 * continuation whenever this menu does not own input.
 */
#include "display_options.h"
#include <stdint.h>
#include <string.h>

extern MmOptions mm_native_options;
extern void mm_game_options_input(void *, void *);
extern void mm_game_options_touch(void *, void *);
extern int mm_game_options_page(void *, void *, unsigned);
extern int mm_game_options_select(void *, void *, unsigned, unsigned, unsigned);
extern void mm_game_options_draw(void *, void *);
extern void mm_game_options_close(void *, void *);
extern void mm_game_options_finish(void *, void *);
extern void mm_game_main_calc(void *, void *);
extern void mm_game_main_draw(void *, void *);
extern void *mm_game_ui_context(void);
extern void mm_game_help_message(void *, int, int);
extern void mm_game_help_calc(void *, int, float);
extern void *mm_game_find_widget(void *, const char *);
extern void *mm_game_current_widget(void *);
extern void *mm_game_text_pane(void *);
extern void mm_native_stream_init(void *, void *, unsigned);
extern int mm_game_parse_utf16(void *, const uint16_t *);
extern int mm_game_sprite_set_stream(void *, void *);
extern void *mm_game_layout_manager(void);
extern void mm_game_layout_submit(void *, void *, int, int);
extern uint32_t mm_render_available_modes(void);
extern bool mm_render_wait(void *);
extern bool mm_render_build(void *, const MmProfile *, void **);
extern bool mm_render_commit(void *, void *);
extern void mm_render_discard(void *, void *);

extern void mm_settings_tick(void);
extern bool mm_settings_save(void *,const uint8_t *,size_t);
static void *screen_context, *options_owner;
static int pending_native_row=-1;
static bool enter_last_row;
static uint32_t suppress_native;
extern bool mm_tabs_available(void *);
extern bool mm_tabs_update(void *,unsigned);
extern bool mm_style_open(void *, void *);
extern bool mm_style_update(void *);
extern bool mm_style_draw(void *);
extern void mm_style_close(void);
extern void mm_style_buttons(uint32_t);
extern bool mm_style_busy(void),mm_style_finished(void),mm_style_exit(bool);
extern void mm_game_ui_sound(unsigned);
static bool page_open, styled, enter_graphics_after_open;
static void *gameplay_screen;
/* Keep the native Options settings/animations, but return to the screen that
 * opened it. Native 0x5a3838 otherwise routes to a file/title menu. */
static struct {
    void *ctx, *game, *main;
    uint16_t pause;
    uint8_t letterbox;
    bool active;
} options_return;
static uint8_t pause_letterbox;

#ifdef MM_NATIVE_HOST_TEST
extern void *mm_test_options_screen, *mm_test_notebook_screen;
#define OPTIONS_SCREEN mm_test_options_screen
#define NOTEBOOK_SCREEN mm_test_notebook_screen
#else
#define OPTIONS_SCREEN ((void *)0x6f8350)
#define NOTEBOOK_SCREEN (*(void **)0x657b08)
#endif
static void *help_layout;
static uint32_t root_flags, root_active, root_x;
static bool root_saved;
static struct { void *sprite, *stream; } saved[3];
static unsigned saved_count;
static struct { void *widget; uint32_t flags, active, alpha, computed_alpha; } visibility[12];
static unsigned visibility_count;

typedef struct {
    _Alignas(8) uint8_t stream[0x4c];
    _Alignas(8) uint32_t arena[1024];
    uint16_t label[160];
    bool constructed, ready;
} Text;
/* Four pause selections and five graphics choices x four status strings.
   Streams/tokens stay alive for native sprites that still reference them. */
static Text texts[24];
static void *ptr(void *p, unsigned n) {
    void *value;
    memcpy(&value, (uint8_t *)p + n, sizeof(value));
    return value;
}
static void put_ptr(void *p, unsigned n, void *value) {
    memcpy((uint8_t *)p + n, &value, sizeof(value));
}
static uint32_t *word(void *p, unsigned n) { return (uint32_t *)((uint8_t *)p + n); }
static uint8_t *byte(void *p, unsigned n) { return (uint8_t *)p + n; }
static void *game(void *ctx) {
    void *owner = ctx ? ptr(ctx, 4) : 0;
    return owner ? ptr(owner, 0x16c) : 0;
}
static bool graphics(void) { return mm_native_options.ui == MM_UI_GRAPHICS_OPTIONS ||
                                    mm_native_options.ui == MM_UI_GRAPHICS_PAUSE; }
static void append(Text *t, unsigned *n, const char *s) {
    while (*s && *n + 1 < sizeof(t->label) / sizeof(t->label[0]))
        t->label[(*n)++] = (uint8_t)*s++;
    t->label[*n] = 0;
}
static unsigned text_key(void) {
    if (!graphics()) return mm_native_options.pause_row;
    unsigned status = mm_native_options.selected == mm_native_options.active ? 0 :
                      mm_native_options.result == MM_UNAVAILABLE ? 2 : 1;
    if (mm_native_options.selected == mm_native_options.active &&
        mm_native_options.unsaved) status = 3;
    return 4 + mm_native_options.selected * 4 + status;
}
static Text *text_for_state(void) {
    unsigned key = text_key();
    Text *t = &texts[key];
    if (t->ready) return t;
    if (!t->constructed) {
        mm_native_stream_init(t->stream, t->arena, sizeof(t->arena));
        t->constructed = true;
    }
    unsigned n = 0;
    if (!graphics()) {
        static const char *rows[] = {"Resume", "Graphics", "Options", "Notebook"};
        append(t, &n, "Paused\n< ");
        append(t, &n, rows[mm_native_options.pause_row]);
        append(t, &n, " >\nUp/Down:select A:open\nB/START:resume");
    } else {
        append(t, &n, "Graphics 3/3\n< ");
        append(t, &n, mm_mode_label(mm_native_options.selected));
        append(t, &n, " >\n");
        unsigned status = (key - 4) % 4;
        append(t, &n, status == 3 ? "Applied (this run)" :
                       status == 0 ? "Active" : status == 2 ? "Unavailable" : "A: apply");
        append(t, &n, "\n</>:mode A:apply B/L:back");
    }
    if (!mm_game_parse_utf16(t->stream, t->label)) return 0;
    t->ready = true;
    return t;
}
static void *get_help(void) {
    void *ui = mm_game_ui_context();
    return ui ? ptr(ui, 0x2c) : 0;
}
static void *get_sprite(void *help) {
    if (!help) return 0;
    void *w = mm_game_find_widget(help, "e_text");
    void *layout = w ? ptr(w, 0x20) : 0;
    void *instance = layout ? mm_game_current_widget(layout) : 0;
    void *panes = instance ? ptr(instance, 0x18) : 0;
    void *pane = panes ? ptr(panes, 0) : 0;
    return pane ? mm_game_text_pane(pane) : 0;
}
static bool private_stream(void *stream) {
    for (unsigned i = 0; i < 24; ++i) if (stream == texts[i].stream) return true;
    return false;
}
static void show_widget(void *w) {
    for (unsigned depth = 0; w && depth < 8; ++depth) {
        bool known = false;
        for (unsigned i = 0; i < visibility_count; ++i)
            if (visibility[i].widget == w) known = true;
        if (!known && visibility_count < 12) {
            unsigned i = visibility_count++;
            visibility[i].widget = w;
            visibility[i].flags = *word(w, 0x84);
            visibility[i].active = *word(w, 0x88);
            visibility[i].alpha = *word(w, 0x80);
            visibility[i].computed_alpha = *word(w, 0x134);
        }
        *word(w, 0x84) |= 0x4020001;
        *word(w, 0x88) |= 0x4020000;
        *(float *)byte(w, 0x80) = 1.0f;
        *(float *)byte(w, 0x134) = 1.0f;
        w = ptr(w, 8);
    }
}
static bool paint(void) {
    void *help = get_help();
    void *sprite = get_sprite(help);
    Text *t = text_for_state();
    if (!sprite || !t) return false;
    bool known = false;
    for (unsigned i = 0; i < saved_count; ++i) if (saved[i].sprite == sprite) known = true;
    void *original = ptr(sprite, 0x124);
    if (!known && saved_count < 3 && original && !private_stream(original)) {
        saved[saved_count].sprite = sprite;
        saved[saved_count++].stream = original;
    }
    if (!mm_game_sprite_set_stream(sprite, t->stream)) return false;
    if (!root_saved) {
        help_layout = help;
        root_flags = *word(help, 0x8c);
        root_active = *word(help, 0x90);
        root_x = *word(help, 0x44);
        root_saved = true;
    }
    show_widget(byte(help, 8));
    show_widget(mm_game_find_widget(help, "e_text"));
    /* Show the existing native window and align its text inside 320px bottom. */
    *word(help, 0x8c) |= 0x20101;
    *word(help, 0x90) = (*word(help, 0x90) | 0x20000) & ~0x100;
    *(float *)byte(help, 0x44) = 20.0f;
    return true;
}
static void restore_paint(void) {
    mm_style_close();
    styled=false;
    for (unsigned i = 0; i < visibility_count; ++i) {
        void *w = visibility[i].widget;
        *word(w, 0x84) = visibility[i].flags;
        *word(w, 0x88) = visibility[i].active;
        *word(w, 0x80) = visibility[i].alpha;
        *word(w, 0x134) = visibility[i].computed_alpha;
    }
    visibility_count = 0;
    for (unsigned i = 0; i < saved_count; ++i)
        (void)mm_game_sprite_set_stream(saved[i].sprite, saved[i].stream);
    saved_count = 0;
    if (root_saved) {
        *word(help_layout, 0x8c) = root_flags;
        *word(help_layout, 0x90) = root_active;
        *word(help_layout, 0x44) = root_x;
    }
    root_saved = false;
    /* Cursor restoration belongs before native selection: otherwise closing
       our private page would overwrite the destination row's cursor pose. */
    if(pending_native_row>=0 && options_owner && screen_context) {
        unsigned row=(unsigned)pending_native_row;
        pending_native_row=-1;
        (void)mm_game_options_select(options_owner,screen_context,row,0,0);
    }
}
static void submit(void) {
    if (!paint()) return;
    mm_game_layout_submit(mm_game_layout_manager(), get_help(), 5, 4);
}
static bool begin_pause(void *user, uintptr_t *token) {
    (void)user;
    void *g = game(screen_context);
    if (!g || *(uint16_t *)byte(g, 0xaac)) return false;
    void *help = get_help();
    if (!help) return false;
    /* During gameplay the shared help layout may not have a current text
       instance until prepared. Options has already prepared it by this point. */
    mm_game_help_message(help, 0x5746, 0);
    if (!get_sprite(help)) return false;
    pause_letterbox = *byte(g, 0xccac);
    *token = *(uint16_t *)byte(g, 0xaac);
    *(uint16_t *)byte(g, 0xaac) = (uint16_t)(*token | 1);
    return true;
}
static void end_pause(void *user, uintptr_t token) {
    (void)user;
    void *g = game(screen_context);
    restore_paint();
    if (g) {
        *(uint16_t *)byte(g, 0xaac) = (uint16_t)token;
        *byte(g, 0xccac) = pause_letterbox;
    }
}
static bool open_screen(void *next) {
    void *g = game(screen_context);
    if (!g || !next || !screen_context || ptr(screen_context, 0xc)) return false;
    restore_paint();
    put_ptr(screen_context, 0xc, next);
    *(uint16_t *)byte(g, 0xaac) = 1;
    *byte(g, 0xccac) = 0;
    return true;
}
static bool start_options(uint16_t pause, uint8_t letterbox) {
    void *obj = OPTIONS_SCREEN;
    void *g = game(screen_context);
    if (!obj || *word(obj, 0) != 0x63b13c || !g || !gameplay_screen ||
        options_return.active || !open_screen(obj)) return false;
    options_return.ctx = screen_context;
    options_return.game = g;
    options_return.main = gameplay_screen;
    options_return.pause = pause;
    options_return.letterbox = letterbox;
    options_return.active = true;
    return true;
}
static bool open_options(void *u) {
    (void)u;
    return start_options((uint16_t)mm_native_options.pause_token,pause_letterbox);
}
static bool open_notebook(void *u) { (void)u; return open_screen(NOTEBOOK_SCREEN); }
void mm_native_backend(MmBackend *b) {
    /* Availability is gated by the complete native output-buffer registry. */
    b->available_modes = mm_render_available_modes();
    b->begin_pause = begin_pause;
    b->end_pause = end_pause;
    b->wait_gpu = mm_render_wait;
    b->build = mm_render_build;
    b->commit = mm_render_commit;
    b->discard = mm_render_discard;
    b->open_options = open_options;
    b->open_notebook = open_notebook;
    b->save = mm_settings_save;
}
static uint32_t held(void *ctx) { void *g = game(ctx); return g ? *word(g, 0x30) : 0; }
static uint32_t pressed(void *ctx) { void *g = game(ctx); return g ? *word(g, 0x34) : 0; }
static void consume(void *ctx, uint32_t mask) {
    void *g = game(ctx);
    if (!g) return;
    mask|=(mask&0xf0)<<24;
    *word(g, 0x30) &= ~mask;
    *word(g, 0x34) &= ~mask;
    *word(g, 0x8c) &= ~mask;
}
/* Native touch updater 1161b4: XY at14/16, held at1c, down at1e,
 * up at20. XY is already in upright 320x240 bottom-screen coordinates.
 * Only the down edge activates a control; dragging/holding cannot apply again. */
static unsigned touch_u16(void *g,unsigned offset) {
    uint16_t v=0;if(g) memcpy(&v,byte(g,offset),sizeof(v));return v;
}
static unsigned touch_tab(void *ctx) {
    void *g=game(ctx);
    unsigned x=touch_u16(g,0x14),y=touch_u16(g,0x16);
    return x>=74 && x<310 && y>=202 && y<240 ?
        (x<154?1:x<234?2:3):0;
}
static uint32_t touch_controls(void *ctx) {
    void *g=game(ctx);
    if(!touch_u16(g,0x1e)) return 0;
    unsigned x=touch_u16(g,0x14),y=touch_u16(g,0x16);
    /* Original BtnReturn is 56x36 at (0,204); its black backing is 58x38. */
    if(x<58 && y>=202 && y<240) return MM_KEY_B;
    if(x<8 || x>=312 || y<4 || y>=148) return 0;
    unsigned row=(y-4)/48;
    bool changed=mm_native_options.graphics_row!=row;
    mm_native_options.graphics_row=row;
    if(changed) mm_game_ui_sound(0x01000775);
    unsigned local_y=y-(4+48*row);
    if(local_y>=12 && local_y<36) {
        if(x>=164 && x<188) return MM_KEY_LEFT;
        if(x>=280 && x<304) return MM_KEY_RIGHT;
        if(x>=196 && x<280) return MM_KEY_A;
    }
    return 0;
}
/* Native Options uses Circle Pad Right/Left/Up/Down in bits28..31,
   alongside D-pad bits4..7. Prefer D-pad while it is held, as native input
   does when selecting its direction source. Retain raw bits for consumption. */
static uint32_t menu_directions(uint32_t value,uint32_t held_keys) {
    return value | ((held_keys&0xf0)?0:(value&0xf0000000u)>>24);
}
void mm_options_input_hook(void *opt, void *ctx) {
    mm_settings_tick();
    uint32_t physical_keys=held(ctx);
    uint32_t keys=menu_directions(physical_keys,physical_keys);
    uint32_t down=menu_directions(pressed(ctx),physical_keys);
    void *input_game=game(ctx);
    uint32_t repeat=input_game?menu_directions(*word(input_game,0x8c),physical_keys)&keys&0xf0:0;
    options_owner = opt;
    screen_context = ctx;
    bool native_tabs=mm_tabs_available(opt);
    void *touch_game=game(ctx);
    bool tapped=touch_u16(touch_game,0x1e)!=0;
    unsigned tab=touch_tab(ctx);
    if(native_tabs && tapped && !keys && !*byte(opt,9) &&
       !(page_open && styled && mm_style_busy())) {
        if(tab && !page_open && (*byte(opt,8)==2 || *byte(opt,8)==4)) {
            if(tab==3) enter_graphics_after_open=true;
            else {
                enter_graphics_after_open=false;
                unsigned target=tab==2?1:0;
                if(*byte(opt,8)!=(tab==2?4:2))
                    (void)mm_game_options_page(opt,ctx,target);
                return;
            }
        } else if(page_open && styled) {
            uint32_t action=tab==1?MM_KEY_R:tab==2?MM_KEY_L:
                tab==3?0:touch_controls(ctx);
            keys|=action;down|=action;
        }
    }
    if(native_tabs && !page_open && !*byte(opt,9) && *byte(opt,8)==2 &&
       *byte(opt,10)==0 && ((down|repeat)&MM_KEY_UP) &&
       !touch_u16(touch_game,0x1c) && !touch_u16(touch_game,0x20)) {
        enter_last_row=true;
        enter_graphics_after_open=true;
    }
    bool dpad_enter=native_tabs && !page_open && !*byte(opt,9) &&
        *byte(opt,8)==4 && *byte(opt,10)==7 && ((down|repeat)&MM_KEY_DOWN) &&
        !touch_u16(touch_game,0x1c) && !touch_u16(touch_game,0x20);
    if(page_open && styled && !mm_style_busy()) {
        /* Reuse the game's repeat pulse, never manufacture timer rates. */
        mm_native_options.previous_buttons&=~repeat;
        mm_native_options.suppress_until_release&=~repeat;
        down|=repeat;
        uint32_t navigation=down&~mm_native_options.previous_buttons&
            ~mm_native_options.suppress_until_release;
        if((navigation&MM_KEY_UP) && mm_native_options.graphics_row==0) {
            pending_native_row=7;keys|=MM_KEY_L;down|=MM_KEY_L;
        } else if((navigation&MM_KEY_DOWN) && mm_native_options.graphics_row==2) {
            pending_native_row=0;keys|=MM_KEY_R;down|=MM_KEY_R;
        }
    }
    if(native_tabs && !page_open && !*byte(opt,9) && *byte(opt,8)==2 && (down&MM_KEY_L))
        enter_graphics_after_open=true;
    if(enter_graphics_after_open && !*byte(opt,9) && *byte(opt,8)==2) {
        (void)mm_game_options_page(opt,ctx,1);
        suppress_native|=keys; consume(ctx,keys); return;
    }
    if (native_tabs && !page_open && *byte(opt, 8) == 4 && !*byte(opt, 9) &&
        ((down & MM_KEY_R) || enter_graphics_after_open || dpad_enter)) {
        enter_graphics_after_open=false;
        mm_options_enter_graphics_page(&mm_native_options);
        if(enter_last_row) mm_native_options.graphics_row=2;
        enter_last_row=false;
        mm_native_options.previous_buttons = keys;
        page_open = true;
        styled=mm_style_open(opt,get_help());
        if(styled) mm_game_ui_sound(0x0100076c);
        if (!styled) {
            page_open = false;
            mm_options_leave_graphics_page(&mm_native_options);
            restore_paint();
            suppress_native|=keys; consume(ctx,keys); return;
        }
    }
    if(page_open && styled) {
        if(mm_style_busy()) {
            mm_native_options.previous_buttons=keys;
            mm_native_options.suppress_until_release|=keys;
            suppress_native|=keys;consume(ctx,keys);return;
        }
        uint32_t exit_keys=down&~mm_native_options.previous_buttons&
            ~mm_native_options.suppress_until_release&(MM_KEY_L|MM_KEY_R|MM_KEY_B|MM_KEY_START);
        if(exit_keys && mm_style_exit((exit_keys&MM_KEY_R)!=0)) {
            if(mm_native_options.unsaved) (void)mm_options_save(&mm_native_options);
            if(exit_keys&MM_KEY_R) (void)mm_game_options_page(opt,ctx,0);
            else mm_game_ui_sound(exit_keys&MM_KEY_L?0x0100076c:0x0100076d);
            mm_native_options.previous_buttons=keys;
            mm_native_options.suppress_until_release|=keys;
            suppress_native|=keys;consume(ctx,keys);return;
        }
    }
    if (page_open) {
        if (down & (MM_KEY_L | MM_KEY_R)) {
            /* Opening R is suppressed by previous_buttons; later shoulder
               presses return to page 2 (L) or native page 1 (R). */
            if (!(mm_native_options.previous_buttons & (MM_KEY_L | MM_KEY_R))) {
                if(mm_native_options.unsaved) (void)mm_options_save(&mm_native_options);
                mm_options_leave_graphics_page(&mm_native_options);
                page_open = false;
                restore_paint();
                if(down&MM_KEY_L) mm_game_ui_sound(0x0100076c);
                if (down & MM_KEY_R) (void)mm_game_options_page(opt, ctx, 0);
            }
        }
        if (page_open) {
            unsigned old_row=mm_native_options.graphics_row;
            MmMode old_mode=mm_native_options.selected;
            unsigned old_strength=mm_native_options.strength_index;
            unsigned old_sensitivity=mm_native_options.sensitivity_index;
            uint32_t fresh=keys&~mm_native_options.previous_buttons&~mm_native_options.suppress_until_release;
            (void)mm_options_input(&mm_native_options, keys, false, false);
            mm_style_buttons(keys);
            if(old_row!=mm_native_options.graphics_row) mm_game_ui_sound(0x01000775);
            else if(old_mode!=mm_native_options.selected || old_strength!=mm_native_options.strength_index ||
                    old_sensitivity!=mm_native_options.sensitivity_index) mm_game_ui_sound(0x010007e1);
            else if(fresh&MM_KEY_A) mm_game_ui_sound(0x01000774);
            else if(fresh&(MM_KEY_B|MM_KEY_START)) mm_game_ui_sound(0x0100076d);
            if (mm_native_options.ui == MM_UI_CLOSED) {
                page_open = false;
                restore_paint();
            }
        }
        suppress_native |= keys;
        consume(ctx, keys);
        return;
    }
    suppress_native &= keys;
    consume(ctx, suppress_native);
    mm_game_options_input(opt, ctx);
}
void mm_options_touch_hook(void *opt, void *ctx) {
    /* Custom tabs occupy different positions from the original two-tab bar.
       Do not let the native touch handler interpret the same gesture twice. */
    if (!page_open && !(mm_tabs_available(opt) && touch_tab(ctx)))
        mm_game_options_touch(opt, ctx);
}
void mm_help_calc_hook(void *help, int layer, float dt) {
    mm_game_help_calc(help, layer, dt);
    if (page_open) {
        if(styled) {
            if(!mm_style_update(options_owner) || mm_style_finished()) {
                mm_options_leave_graphics_page(&mm_native_options);
                page_open=false; restore_paint();
            }
        }
    }
    if(options_owner) {
        unsigned state=*byte(options_owner,8);
        unsigned page=page_open?3:((state==3 || state==4)?2:1);
        (void)mm_tabs_update(options_owner,page);
    }
}
void mm_options_draw_hook(void *opt, void *ctx) {
    if (!page_open || opt != options_owner) { mm_game_options_draw(opt, ctx); return; }
    if(styled) {
        mm_game_options_draw(opt,ctx);
        (void)mm_style_draw(opt);
        return;
    }
    mm_game_options_draw(opt,ctx);
}
void mm_main_calc_hook(void *main, void *ctx) {
    mm_settings_tick();
    screen_context = ctx;
    gameplay_screen = main;
    void *g = game(ctx);
    if (!g) { mm_game_main_calc(main, ctx); return; }
    uint32_t keys = held(ctx);
    if ((mm_native_options.ui == MM_UI_PAUSE || mm_native_options.ui == MM_UI_GRAPHICS_PAUSE)
        && ptr(ctx, 0xc)) {
        restore_paint();
        mm_native_options.ui = MM_UI_CLOSED;
        mm_native_options.owns_pause = false;
        return;
    }
    bool gameplay = *byte(g, 0x13d) == 4 && *byte(g, 0x13c) == 2 &&
                    /* Letterboxing is also used for targeting and is not a menu gate. */
                    !*(uint16_t *)byte(g, 0xaac) &&
                    !*(uint16_t *)byte(g, 0x8366) && !ptr(ctx, 0xc);
    bool was_open = mm_native_options.ui != MM_UI_CLOSED;
    bool start_pressed=(keys&MM_KEY_START) && !(mm_native_options.previous_buttons&MM_KEY_START);
    /* START opens the actual Options screen directly. The former helper
       pause selector must not remain the normal gameplay entry point. */
    if(gameplay && !was_open && start_pressed) {
        mm_native_options.previous_buttons=keys;
        mm_native_options.suppress_until_release|=keys;
        if(start_options(*(uint16_t *)byte(g,0xaac),*byte(g,0xccac))) {
            consume(ctx,keys);
        } else {
            mm_game_main_calc(main,ctx);
        }
        return;
    }
    uint32_t mask = mm_options_input(&mm_native_options, keys, gameplay, false);
    consume(ctx, mask);
    if(mm_native_options.ui==MM_UI_GRAPHICS_PAUSE) {
        if(start_options((uint16_t)mm_native_options.pause_token,pause_letterbox)) {
            enter_graphics_after_open=true;
        } else {
            end_pause(0,mm_native_options.pause_token);
        }
        mm_native_options.ui=MM_UI_CLOSED;
        mm_native_options.owns_pause=false;
        return;
    }
    /* Options can initialise its own UI even when the gameplay help layout
       cannot supply the pause selector. Do not silently lose START in that case. */
    if(gameplay && !was_open && start_pressed && !(mask&MM_KEY_START) &&
       mm_native_options.ui==MM_UI_CLOSED &&
       start_options(*(uint16_t *)byte(g,0xaac),*byte(g,0xccac))) {
        mm_native_options.suppress_until_release|=keys;
        consume(ctx,keys);
        return;
    }
    if (mm_native_options.ui == MM_UI_PAUSE || mm_native_options.ui == MM_UI_GRAPHICS_PAUSE) {
        void *help = get_help();
        if (help) mm_game_help_calc(help, 7, 1.0f / 30.0f);
        if (!paint()) {
            /* Do not leave the game paused behind an invisible menu. */
            end_pause(0, mm_native_options.pause_token);
            mm_native_options.ui = MM_UI_CLOSED;
            mm_native_options.owns_pause = false;
        }
        return;
    }
    if (!was_open && !mask) mm_game_main_calc(main, ctx);
}
void mm_main_draw_hook(void *main, void *ctx) {
    mm_game_main_draw(main, ctx);
    if (mm_native_options.ui == MM_UI_PAUSE || mm_native_options.ui == MM_UI_GRAPHICS_PAUSE)
        submit();
}

void mm_options_close_hook(void *opt, void *ctx) {
    if(mm_native_options.unsaved) (void)mm_options_save(&mm_native_options);
    enter_graphics_after_open=false;enter_last_row=false;
    if (page_open && options_owner == opt) {
        restore_paint();
        mm_options_leave_graphics_page(&mm_native_options);
        page_open = false;
    }
    options_owner=0;
    mm_game_options_close(opt, ctx);
}

/* Native finish commits the actual game settings before selecting its normal
 * return screen. Replace only that destination for our gameplay entry. */
void mm_options_finish_hook(void *opt, void *ctx) {
    mm_game_options_finish(opt, ctx);
    if (!options_return.active || options_return.ctx != ctx ||
        game(ctx) != options_return.game || !ptr(ctx, 0xc)) return;
    put_ptr(ctx, 0xc, options_return.main);
    *(uint16_t *)byte(options_return.game, 0xaac) = options_return.pause;
    *byte(options_return.game, 0xccac) = options_return.letterbox;
    options_return.active = false;
}
