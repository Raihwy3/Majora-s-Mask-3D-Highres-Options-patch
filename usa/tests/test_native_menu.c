#include "display_options.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
MmOptions mm_native_options;
void mm_settings_tick(void) {}
bool mm_settings_save(void *u,const uint8_t *p,size_t n) {(void)u;(void)p;(void)n;return true;}
static unsigned sound_count,last_sound;
void mm_game_ui_sound(unsigned id) {assert(id==0x0100076c || id==0x01000775 || id==0x010007e1 || id==0x01000774 || id==0x0100076d);last_sound=id;++sound_count;}
void mm_style_buttons(uint32_t keys) {(void)keys;}
static bool motion_busy,motion_finished,motion_enabled;
bool mm_style_busy(void) {return motion_busy;}
bool mm_style_finished(void) {return motion_finished;}
bool mm_style_exit(bool first) {(void)first;motion_busy=motion_enabled;return motion_enabled;}


bool mm_resolve_prepare(void) {return false;}
static bool style_available=true,style_opened;
static unsigned styled_draws,last_tab;
static bool tabs_available=true;
bool mm_tabs_available(void *o) {assert(o);return tabs_available;}
bool mm_tabs_update(void *o,unsigned page) {assert(o && page>=1 && page<=3);last_tab=page;return tabs_available;}
bool mm_style_open(void *o,void *h) { assert(o && h);style_opened=style_available;return style_opened; }
bool mm_style_update(void *o) { assert(o);return style_opened; }
bool mm_style_draw(void *o) { assert(o && style_opened);++styled_draws;return true; }
void mm_style_close(void) { style_opened=false;motion_finished=false;motion_busy=false; }
static _Alignas(8) uint8_t ctx[48], owner[0x180], g[0x11030], opt[0x200];
static _Alignas(8) uint8_t ui[0x250], help[0x200], widget[0x140], layout[32];
static _Alignas(8) uint8_t instance[64], panes[32], pane[32], sprite[0x320], original_stream[0x4c];
static _Alignas(8) uint8_t graphics_owner[32], display[0x194], hid[256];
uint8_t *mm_test_owner = graphics_owner, *mm_test_hid = hid;
uint8_t *mm_test_registry=0;
static int native_input, native_touch, native_draw, native_close, main_calc, main_draw, submits, paints;
static int missing_sprite, require_help_prepare, help_prepared, finishes;
static uint8_t native_return_screen[64];
static _Alignas(8) uint8_t options_screen[0x200], notebook_screen[64];
void *mm_test_options_screen=options_screen, *mm_test_notebook_screen=notebook_screen;
extern void mm_native_backend(MmBackend *);
extern void mm_options_input_hook(void *, void *);
extern void mm_options_touch_hook(void *, void *);
extern void mm_options_draw_hook(void *, void *);
extern void mm_options_close_hook(void *, void *);
extern void mm_options_finish_hook(void *, void *);
extern void mm_help_calc_hook(void *, int, float);
extern void mm_main_calc_hook(void *, void *);
extern void mm_main_draw_hook(void *, void *);
extern int mm_render_stereo(void);
static void put(void *p, unsigned off, void *v) { memcpy((uint8_t *)p + off, &v, sizeof(v)); }
static void *get(void *p, unsigned off) { void *v; memcpy(&v, (uint8_t *)p + off, sizeof(v)); return v; }
static void keys(uint32_t held, uint32_t down) { memcpy(g+0x30, &held, 4); memcpy(g+0x34, &down, 4); }
static uint16_t pause_flags(void) { uint16_t v; memcpy(&v, g+0xaac, 2); return v; }
void mm_game_options_input(void *p, void *c) { assert(p == opt && c == ctx); ++native_input; }
void mm_game_options_touch(void *p, void *c) { assert(p == opt && c == ctx); ++native_touch; }
int mm_game_options_select(void *p,void *c,unsigned row,unsigned sound,unsigned delay) {
    assert(p==opt && c==ctx && row<8 && sound==0 && delay==0);
    assert(!style_opened); /* select after private cursor restoration */
    opt[10]=(uint8_t)row;return 1;
}
int mm_game_options_page(void *p, void *c, unsigned page) { assert(p==opt && c==ctx && page<=1); opt[8]=page?3:2; return 1; }
void mm_game_options_draw(void *p, void *c) { assert(p==opt && c==ctx); if(mm_native_options.ui == MM_UI_GRAPHICS_OPTIONS) assert(get(opt,0x10) && get(opt,0x18));
    ++native_draw;
}
void mm_game_options_close(void *p, void *c) { assert(p==opt && c==ctx && mm_native_options.ui==MM_UI_CLOSED); ++native_close; }
void mm_game_options_finish(void *p, void *c) {
    assert(p==opt && c==ctx);
    ++finishes;
    put(ctx,0xc,native_return_screen); /* native file/title destination */
}
void mm_game_main_calc(void *p, void *c) { assert(p==opt && c==ctx); ++main_calc; }
void mm_game_main_draw(void *p, void *c) { assert(p==opt && c==ctx); ++main_draw; }
void *mm_game_ui_context(void) { return ui; }
void mm_game_help_message(void *p, int message, int context) { assert(p==help && message==0x5746 && context==0); help_prepared=1; }
void mm_game_help_calc(void *p, int layer, float dt) { assert(p==help && layer==7 && dt>0); put(sprite,0x124,original_stream); }
void *mm_game_find_widget(void *p, const char *name) { assert(p==help && !strcmp(name,"e_text")); return missing_sprite || (require_help_prepare && !help_prepared) ? NULL : widget; }
void *mm_game_current_widget(void *p) { assert(p==layout); return instance; }
void *mm_game_text_pane(void *p) { assert(p==pane); return sprite; }
void *mm_game_stream_construct(void *p) { memset(p,0,0x4c); return p; }
int mm_game_parse_utf16(void *stream, const uint16_t *text) {
    const uint32_t *w=stream;
    assert(w[7] && w[7]==w[10] && w[8]==4096 && text[0]);
    /* The old footer exceeded the width of the bottom-screen help window. */
    unsigned columns=0;
    for(unsigned i=0;text[i];i++) {
        if(text[i]=='\n') columns=0;
        else { ++columns; assert(columns<=26); }
    }
    return 1;
}
int mm_game_sprite_set_stream(void *p, void *s) { assert(p==sprite && s); put(p,0x124,s); ++paints; return 1; }
void *mm_game_layout_manager(void) { return ui; }
void mm_game_layout_submit(void *p, void *l, int layer, int priority) { assert(p==ui && l==help && layer==5 && priority==4); ++submits; }
void mm_game_gpu_transfer(void *s,unsigned iw,unsigned ih,void *d,unsigned ow,unsigned oh,unsigned f) {
    (void)s;(void)iw;(void)ih;(void)d;(void)ow;(void)oh;(void)f;
}
void mm_game_present(unsigned s,unsigned i,void *l,void *r,unsigned p,unsigned f,unsigned t) {
    (void)s;(void)i;(void)l;(void)r;(void)p;(void)f;(void)t;
}
void mm_game_set_display_mode(unsigned mode) { assert(mode>=0x700 && mode<=0x702); }
void mm_game_surface_transfer(void *p,unsigned f,int x,int y,unsigned a) { (void)p;(void)f;(void)x;(void)y;(void)a; }
int main(void) {
    put(ctx,4,owner); put(owner,0x16c,g); put(ui,0x2c,help);
    put(widget,0x20,layout); put(instance,0x18,panes); put(panes,0,pane);
    put(sprite,0x124,original_stream); put(graphics_owner,4,display);
    put(opt,0x10,layout); put(opt,0x18,widget);
    MmBackend backend={0}; mm_native_backend(&backend);
    mm_options_init(&mm_native_options,&backend,MM_HIGH_RES_2D);
    opt[8]=4;
    keys(MM_KEY_R,MM_KEY_R); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_GRAPHICS_OPTIONS && !native_input);
    mm_options_touch_hook(opt,ctx); assert(!native_touch);
    mm_help_calc_hook(help,7,1.0f/30.0f); assert(get(sprite,0x124)==original_stream && last_tab==3);
    mm_options_draw_hook(opt,ctx); assert(submits==0 && native_draw==1 && styled_draws==1);
    assert(get(opt,0x10)==layout && get(opt,0x18)==widget);
    keys(0,0); mm_options_input_hook(opt,ctx);
    keys(MM_KEY_RIGHT,MM_KEY_RIGHT); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.selected==MM_SUPERSAMPLE_2D && mm_native_options.active==MM_HIGH_RES_2D);
    assert(last_sound==0x010007e1 && sound_count==2);
    keys(0,0); mm_options_input_hook(opt,ctx);
    keys(MM_KEY_A,MM_KEY_A); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.active==MM_HIGH_RES_2D && !mm_native_options.unsaved);
    assert(mm_native_options.result==MM_UNAVAILABLE); /* blocked before GPU changes */
    assert(!mm_render_stereo());
    keys(0,0); mm_options_input_hook(opt,ctx);
    keys(MM_KEY_B,MM_KEY_B); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED && get(sprite,0x124)==original_stream);
    keys(0,0); mm_options_input_hook(opt,ctx); assert(native_input==1);
    mm_options_touch_hook(opt,ctx); assert(native_touch==1);
    /* Missing style resources leave native page 2; never revive old UI. */
    style_available=false;
    keys(MM_KEY_R,MM_KEY_R); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED && !style_opened);
    keys(0,0); mm_options_input_hook(opt,ctx);
    style_available=true;
    keys(MM_KEY_R,MM_KEY_R); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_GRAPHICS_OPTIONS);
    mm_options_close_hook(opt,ctx); assert(native_close==1);
    /* The native-style three-tab bar follows physical page navigation. */
    opt[8]=2;
    keys(0,0); mm_options_input_hook(opt,ctx);
    mm_help_calc_hook(help,7,1.0f/30.0f); assert(last_tab==1);
    keys(MM_KEY_L,MM_KEY_L); mm_options_input_hook(opt,ctx);
    assert(opt[8]==3); /* wraps to Page3 through native Page2 preparation */
    opt[8]=4;
    keys(0,0); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_GRAPHICS_OPTIONS);
    mm_help_calc_hook(help,7,1.0f/30.0f); assert(last_tab==3);
    keys(MM_KEY_L,MM_KEY_L); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED && opt[8]==4);
    mm_help_calc_hook(help,7,1.0f/30.0f); assert(last_tab==2);
    keys(0,0); mm_options_input_hook(opt,ctx);
    keys(MM_KEY_R,MM_KEY_R); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_GRAPHICS_OPTIONS);
    keys(0,0); mm_options_input_hook(opt,ctx);
    keys(MM_KEY_R,MM_KEY_R); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED && opt[8]==2);
    mm_options_close_hook(opt,ctx);
    /* An old/missing RomFS layout must retain the original native route. */
    tabs_available=false;opt[8]=4;
    keys(0,0); mm_options_input_hook(opt,ctx);
    keys(MM_KEY_R,MM_KEY_R); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED && !style_opened);
    tabs_available=true;
    /* Regular Options must keep its original exit route. */
    mm_options_finish_hook(opt,ctx);
    assert(finishes==1 && get(ctx,0xc)==native_return_screen);
    put(ctx,0xc,0);
    g[0x13d]=4;g[0x13c]=2;
    /* START opens native Options directly, without preparing/painting the old
       helper menu. Targeting letterbox and pause flags survive the round trip. */
    g[0xccac]=1;
    missing_sprite=1;
    *(uint32_t *)options_screen=0x63b13c;
    unsigned old_paints=paints, old_submits=submits;
    keys(0,0); mm_main_calc_hook(opt,ctx);
    keys(MM_KEY_START,MM_KEY_START); mm_main_calc_hook(opt,ctx);
    assert(get(ctx,0xc)==options_screen && pause_flags()==1);
    assert(mm_native_options.ui==MM_UI_CLOSED && !mm_native_options.owns_pause);
    mm_main_draw_hook(opt,ctx);
    assert(paints==(int)old_paints && submits==(int)old_submits && !help_prepared);
    put(ctx,0xc,0); /* native screen manager has opened Options */
    mm_options_finish_hook(opt,ctx);
    assert(finishes==2 && get(ctx,0xc)==opt);
    assert(!pause_flags() && g[0xccac]==1);
    put(ctx,0xc,0);
    /* Reopen and use the third page through the same physical controls as
       title/file Options. An absent shared helper cannot block this route. */
    keys(0,0); mm_main_calc_hook(opt,ctx);
    keys(MM_KEY_START,MM_KEY_START); mm_main_calc_hook(opt,ctx);
    assert(get(ctx,0xc)==options_screen);
    put(ctx,0xc,0);
    missing_sprite=0;
    opt[8]=4;
    keys(0,0); mm_options_input_hook(opt,ctx);
    keys(MM_KEY_R,MM_KEY_R); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_GRAPHICS_OPTIONS && style_opened);
    keys(0,0); mm_options_input_hook(opt,ctx);
    MmMode previous=mm_native_options.selected;
    keys(MM_KEY_LEFT,MM_KEY_LEFT); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.selected==(previous+MM_MODE_COUNT-1)%MM_MODE_COUNT);
    keys(0,0); mm_options_input_hook(opt,ctx);
    keys(MM_KEY_B,MM_KEY_B); mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED);
    mm_options_finish_hook(opt,ctx);
    assert(get(ctx,0xc)==opt && !pause_flags() && g[0xccac]==1);
    /* During transitions input stays owned, then back completes from the
       native help calculation instead of destroying the page mid-animation. */
    opt[8]=4;motion_enabled=true;
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_R,MM_KEY_R);mm_options_input_hook(opt,ctx);
    assert(style_opened && mm_native_options.ui==MM_UI_GRAPHICS_OPTIONS);
    motion_busy=true;MmMode before_motion=mm_native_options.selected;
    keys(MM_KEY_RIGHT,MM_KEY_RIGHT);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.selected==before_motion);
    motion_busy=false;keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_B,MM_KEY_B);mm_options_input_hook(opt,ctx);
    assert(motion_busy && style_opened && mm_native_options.ui==MM_UI_GRAPHICS_OPTIONS);
    motion_busy=false;motion_finished=true;
    mm_help_calc_hook(help,7,1.0f/30);
    assert(!style_opened && mm_native_options.ui==MM_UI_CLOSED && last_tab==2);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_R,MM_KEY_R);mm_options_input_hook(opt,ctx);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_R,MM_KEY_R);mm_options_input_hook(opt,ctx);
    assert(motion_busy && opt[8]==2 && mm_native_options.ui==MM_UI_GRAPHICS_OPTIONS);
    motion_busy=false;motion_finished=true;mm_help_calc_hook(help,7,1.0f/30);
    assert(!style_opened && mm_native_options.ui==MM_UI_CLOSED && last_tab==1);
    /* Touch uses the native down edge and actual bottom-screen coordinates. */
    motion_enabled=false;
    keys(0,0);mm_options_input_hook(opt,ctx);
    uint16_t *xy=(uint16_t *)(g+0x14),*touch=(uint16_t *)(g+0x1c);
    xy[0]=270;xy[1]=220;touch[0]=touch[1]=1;
    mm_options_input_hook(opt,ctx);
    assert(opt[8]==3 && mm_native_options.ui==MM_UI_CLOSED);
    opt[8]=4;touch[1]=0;mm_options_input_hook(opt,ctx);
    assert(style_opened && mm_native_options.ui==MM_UI_GRAPHICS_OPTIONS);
    touch[0]=0;mm_options_input_hook(opt,ctx);
    unsigned speed=mm_native_options.sensitivity_index;
    xy[0]=292;xy[1]=124;touch[0]=touch[1]=1;
    mm_options_input_hook(opt,ctx);
    assert(mm_native_options.graphics_row==2 && mm_native_options.sensitivity_index==speed+1);
    touch[1]=0;
    for(unsigned i=0;i<20;i++) mm_options_input_hook(opt,ctx);
    assert(mm_native_options.sensitivity_index==speed+1); /* holding: no repeats */
    xy[0]=174; /* dragging onto the opposite arrow does not activate it */
    mm_options_input_hook(opt,ctx);assert(mm_native_options.sensitivity_index==speed+1);
    touch[0]=0;mm_options_input_hook(opt,ctx);
    touch[0]=touch[1]=1;mm_options_input_hook(opt,ctx);
    assert(mm_native_options.sensitivity_index==speed);
    touch[0]=touch[1]=0;mm_options_input_hook(opt,ctx);
    unsigned strength=mm_native_options.strength_index;
    xy[0]=292;xy[1]=76;touch[0]=touch[1]=1;mm_options_input_hook(opt,ctx);
    assert(mm_native_options.graphics_row==1 && mm_native_options.strength_index==strength+1);
    touch[0]=touch[1]=0;mm_options_input_hook(opt,ctx);
    xy[0]=90;xy[1]=28;touch[0]=touch[1]=1;mm_options_input_hook(opt,ctx);
    assert(mm_native_options.graphics_row==0); /* name selects, does not apply */
    touch[0]=touch[1]=0;mm_options_input_hook(opt,ctx);
    MmMode touch_before=mm_native_options.selected;
    xy[0]=292;touch[0]=touch[1]=1;mm_options_input_hook(opt,ctx);
    assert(mm_native_options.selected==(touch_before+1)%MM_MODE_COUNT);
    touch[0]=touch[1]=0;mm_options_input_hook(opt,ctx);
    mm_native_options.result=MM_OK;
    xy[0]=234;touch[0]=touch[1]=1;mm_options_input_hook(opt,ctx);
    assert(mm_native_options.result==MM_UNAVAILABLE); /* value invokes guarded apply */
    touch[0]=touch[1]=0;mm_options_input_hook(opt,ctx);
    motion_busy=true;xy[0]=292;touch[0]=touch[1]=1;
    touch_before=mm_native_options.selected;mm_options_input_hook(opt,ctx);
    assert(mm_native_options.selected==touch_before); /* no taps during slide */
    motion_busy=false;touch[1]=0;mm_options_input_hook(opt,ctx);
    assert(mm_native_options.selected==touch_before); /* held tap cannot replay */
    touch[0]=0;mm_options_input_hook(opt,ctx);
    xy[0]=180;xy[1]=220;touch[0]=touch[1]=1;mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED && opt[8]==4);
    int old_touch=native_touch;mm_options_touch_hook(opt,ctx);assert(native_touch==old_touch);
    touch[0]=touch[1]=0;mm_options_input_hook(opt,ctx);
    xy[0]=100;xy[1]=60;touch[0]=touch[1]=1;
    mm_options_touch_hook(opt,ctx);assert(native_touch==old_touch+1); /* native rows retained */
    xy[0]=10;xy[1]=230;mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED); /* outside custom tabs */
    touch[0]=touch[1]=0;
    /* Native B/Cancel hit target is independent of the custom page tabs. */
    opt[8]=4;keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_R,MM_KEY_R);mm_options_input_hook(opt,ctx);
    assert(style_opened);keys(0,0);mm_options_input_hook(opt,ctx);
    xy[0]=24;xy[1]=220;touch[0]=touch[1]=1;
    mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED && !style_opened);
    touch[0]=touch[1]=0;keys(0,0);mm_options_input_hook(opt,ctx);
    /* D-pad extends the native global row order: 0..7, then Page3. */
    keys(0,0);mm_options_input_hook(opt,ctx);opt[8]=4;opt[10]=6;
    keys(MM_KEY_DOWN,MM_KEY_DOWN);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED); /* earlier native rows untouched */
    opt[10]=7;keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_DOWN,MM_KEY_DOWN);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_GRAPHICS_OPTIONS && mm_native_options.graphics_row==0);
    keys(MM_KEY_DOWN,0);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.graphics_row==0); /* opening down cannot skip first row */
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_DOWN,MM_KEY_DOWN);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.graphics_row==1);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_DOWN,MM_KEY_DOWN);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.graphics_row==2);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_DOWN,MM_KEY_DOWN);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED && opt[8]==2 && opt[10]==0);
    /* Held navigation stays suppressed when control returns to native code. */
    keys(MM_KEY_DOWN,0);mm_options_input_hook(opt,ctx);
    assert(!(*(uint32_t *)(g+0x30)&MM_KEY_DOWN));
    keys(0,0);mm_options_input_hook(opt,ctx);opt[8]=4;opt[10]=4;
    keys(MM_KEY_R,MM_KEY_R);mm_options_input_hook(opt,ctx);
    keys(0,0);mm_options_input_hook(opt,ctx);
    motion_enabled=true;
    keys(MM_KEY_UP,MM_KEY_UP);mm_options_input_hook(opt,ctx);
    assert(motion_busy && style_opened && opt[10]==4);
    motion_busy=false;motion_finished=true;mm_help_calc_hook(help,7,1.0f/30);
    assert(!style_opened && mm_native_options.ui==MM_UI_CLOSED && opt[10]==7 && opt[8]==4);
    /* Circle Pad bits28..31 follow the same native row order and repeats. */
    motion_enabled=false;opt[8]=4;opt[10]=7;
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(0x80000000u,0x80000000u);mm_options_input_hook(opt,ctx);
    assert(style_opened && mm_native_options.graphics_row==0);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(0x80000000u,0x80000000u);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.graphics_row==1);
    unsigned old_depth=mm_native_options.strength_index;
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(0x10000000u,0x10000000u);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.strength_index==old_depth+1);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(0x20000000u,0x20000000u);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.strength_index==old_depth);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(0x40000000u,0x40000000u);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.graphics_row==0);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(0x80000000u,0x80000000u);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.graphics_row==1);
    keys(0x80000000u,0);*(uint32_t *)(g+0x8c)=0x80000000u;
    mm_options_input_hook(opt,ctx);assert(mm_native_options.graphics_row==2);
    assert(!*(uint32_t *)(g+0x8c) && !*(uint32_t *)(g+0x30));
    keys(0x80000000u,0);*(uint32_t *)(g+0x8c)=0x80000000u;
    mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED && opt[10]==0 && opt[8]==2);
    keys(0,0);mm_options_input_hook(opt,ctx);opt[8]=4;opt[10]=7;
    /* Repeat can reach Page3 even if the stick was held across native rows. */
    keys(0x80000000u,0);*(uint32_t *)(g+0x8c)=0x80000000u;
    mm_options_input_hook(opt,ctx);assert(style_opened && mm_native_options.graphics_row==0);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_DOWN|0x40000000u,MM_KEY_DOWN|0x40000000u);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.graphics_row==1); /* D-pad wins over opposing pad */
    /* Up from the first Page1 row wraps to the final Page3 row. */
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_R,MM_KEY_R);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED);
    opt[8]=2;opt[10]=0;keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_UP,MM_KEY_UP);mm_options_input_hook(opt,ctx);
    assert(opt[8]==3 && mm_native_options.ui==MM_UI_CLOSED);
    opt[8]=4;keys(MM_KEY_UP,0);mm_options_input_hook(opt,ctx);
    assert(style_opened && mm_native_options.graphics_row==2);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(MM_KEY_DOWN,MM_KEY_DOWN);mm_options_input_hook(opt,ctx);
    assert(mm_native_options.ui==MM_UI_CLOSED && opt[10]==0 && opt[8]==2);
    keys(0,0);mm_options_input_hook(opt,ctx);
    keys(0x40000000u,0x40000000u);mm_options_input_hook(opt,ctx);
    assert(opt[8]==3);
    opt[8]=4;keys(0x40000000u,0);mm_options_input_hook(opt,ctx);
    assert(style_opened && mm_native_options.graphics_row==2);
    puts("PASS: third-page entry/navigation/apply/back; post-calc text binding; touch tab entry/exit, all rows, apply guard, hold/drag suppression and transition gating; native draw/close restoration; START directly opens native Options without old overlay; targeting/letterbox restoration; gameplay third-page physical controls; native settings finish and gameplay return; invisible-menu fallback.");
}
