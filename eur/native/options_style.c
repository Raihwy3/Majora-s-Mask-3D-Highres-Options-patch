/* Graphics presentation using BasicPlate and complete MessageWindow models.
 * The original MenuOption continues to supply the background and return button.
 * No shared message window or shared text stream is rebound on this path. */
#include "display_options.h"
#include "options_motion.h"
#include <string.h>
extern MmOptions mm_native_options;
extern void mm_game_animation_play(void *,unsigned,int,float);

extern unsigned mm_game_resource_index(void *, const char *);
extern void *mm_game_layout_create(void *, unsigned, unsigned);
extern void mm_game_layout_free(void *, void *);
extern void mm_game_layout_calc(void *, float);
extern void *mm_game_find_widget(void *, const char *);
extern void *mm_game_current_widget(void *);
extern void *mm_game_text_pane(void *);
extern void *mm_game_layout_manager(void);
extern void mm_game_layout_submit(void *, void *, int, int);
extern void mm_native_stream_init(void *, void *, unsigned);
extern int mm_game_parse_utf16(void *, const uint16_t *);
extern int mm_game_sprite_set_stream(void *, void *);
#ifdef MM_NATIVE_HOST_TEST
extern void *mm_test_resource_manager;
#define RESOURCE_MANAGER mm_test_resource_manager
#else
#define RESOURCE_MANAGER ((void *)0x7cdc9c)
#endif
static void *ptr(void *p, unsigned n) {
    uint32_t v; memcpy(&v, (unsigned char *)p+n, sizeof(v)); return (void *)(uintptr_t)v;
}
static uint32_t *word(void *p, unsigned n) { return (uint32_t *)((unsigned char *)p+n); }
static void position(void *layout, float x, float y) {
    memcpy((unsigned char *)layout+0x44, &x, 4);
    memcpy((unsigned char *)layout+0x48, &y, 4);
    *word(layout,0x8c) |= 0x700;
    *word(layout,0x90) &= ~0x700;
}
static void opacity(void *layout,float alpha) {
    *(float *)((unsigned char *)layout+0x88)=alpha;
    *word(layout,0x8c)|=0x4000000;
    if(alpha==1) *word(layout,0x90)|=0x4000000;
    else *word(layout,0x90)&=~0x4000000u;
}
static void widget_position(void *w,float x,float y) {
    memcpy((unsigned char *)w+0x3c,&x,4);memcpy((unsigned char *)w+0x40,&y,4);
    *word(w,0x84)|=0x300;*word(w,0x88)&=~0x300u;
}
static void visible(void *w, bool on) {
    if (!w) return;
    /* Native visibility property bit 0 and dirty bit 0x20000. */
    *word(w,0x84) = (*word(w,0x84)&~1u) | (on?1u:0u) | 0x20000;
    /* Native setter keeps the desired visibility in the active-property
       word as well: 0x606b9c..0x606bb0 ORs 0x20000 when showing a widget. */
    if(on) *word(w,0x88)|=0x20000u;
    else *word(w,0x88)&=~0x20000u;
}
static struct { void *widget; uint32_t flags, active, x, y; } hidden[5];
static unsigned hidden_count;
static void *plate, *strength_plate, *sensitivity_plate, *labels[9], *owner, *help_panel;
/* Retain the original destructor/type/interface entries. Only these owned
   labels bypass the closed-dialog update; never patch a shared game vtable. */
static uint32_t label_vtables[10][24];
static bool static_label(void *layout,unsigned index) {
    uint32_t *original=ptr(layout,0);
    if(!original || original[4]!=0x606a90) return false;
    memcpy(label_vtables[index],original-2,sizeof(label_vtables[index]));
    label_vtables[index][2+4]=0x161bfc;
    *word(layout,0)=(uint32_t)(uintptr_t)(label_vtables[index]+2);
    return true;
}
static struct { void *layout; uint32_t x,y,flags,active,alpha,computed_alpha; } cursor;
static bool ready,initial_update;
static unsigned last_row;
static struct { void *controller; uint32_t animation,frame; uint8_t flags[3]; } live_player;
static void suspend_live_player(void *opt) {
    void *c=ptr(opt,0xe4);
    if(!c) return;
    live_player.controller=c;
    live_player.animation=*word(c,0xc);live_player.frame=*word(c,0x10);
    memcpy(live_player.flags,(unsigned char *)c+8,3);
    /* A stopped native player still applies its final tracks in180130.
       Detach that animation while page3 owns the previous-page transform. */
    *word(c,0xc)=0;*((unsigned char *)c+8)=0;
}
static void restore_live_player(void) {
    void *c=live_player.controller;
    /* R may have started a new native page2->1 transition meanwhile.
       Preserve that new player rather than restoring the old page2 pose. */
    if(c && !*word(c,0xc)) {
        *word(c,0xc)=live_player.animation;*word(c,0x10)=live_player.frame;
        memcpy((unsigned char *)c+8,live_player.flags,3);
    }
    memset(&live_player,0,sizeof(live_player));
}
typedef struct {
    _Alignas(8) unsigned char stream[0x4c];
    _Alignas(8) uint32_t arena[1024];
    uint16_t text[160];
    bool parsed;
} Label;
static Label strings[39];

static void *create(const char *name) {
    unsigned index=mm_game_resource_index((unsigned char *)RESOURCE_MANAGER+4,name);
    if ((index&0xffffu)==0xffffu) return 0;
    return mm_game_layout_create(RESOURCE_MANAGER,index,0x30000);
}
static void *model_pane(void *layout,const char *name) {
    /* Same complete e_text -> locale instance -> first text pane path used
       by the existing native help window. Do not create EdgeText standalone:
       its external text parameter is empty without the parent MessageWindow. */
    void *widget=mm_game_find_widget(layout,name);
    void *child=widget?ptr(widget,0x20):0;
    void *instance=child?mm_game_current_widget(child):0;
    void *panes=instance && *word(instance,0x1c)>0 ? ptr(instance,0x18):0;
    return panes?ptr(panes,0):0;
}
static void *font_pane(void *layout) { return model_pane(layout,"e_text"); }
static bool bind(void *layout,void *stream) {
    void *pane=font_pane(layout);
    void *sprite=pane?mm_game_text_pane(pane):0;
    return sprite && mm_game_sprite_set_stream(sprite,stream);
}
static void reveal(void *w) {
    for(unsigned depth=0;w && depth<16;depth++,w=ptr(w,8)) {
        visible(w,true);
        *word(w,0x84)|=0x4020000;
        *word(w,0x88)|=0x4020000;
        *(float *)((unsigned char *)w+0x80)=1.0f;
        *(float *)((unsigned char *)w+0x134)=1.0f;
    }
}
static void text_only(void *layout) {
    void *w=mm_game_find_widget(layout,"e_text");
    for(unsigned depth=0;w && depth<12;depth++) {
        /* Owned labels must not inherit the dialog's animated placement or
           opening scale. Keep only the explicitly positioned root transform. */
        if(w!=(unsigned char *)layout+8) {
            *(float *)((unsigned char *)w+0x3c)=0.0f;
            *(float *)((unsigned char *)w+0x40)=0.0f;
            *(float *)((unsigned char *)w+0x44)=0.0f;
        }
        for(unsigned n=0x48;n<=0x50;n+=4)
            *(float *)((unsigned char *)w+n)=1.0f;
        for(unsigned n=0x54;n<=0x5c;n+=4)
            *(float *)((unsigned char *)w+n)=0.0f;
        *word(w,0x84)|=0x1ff00;
        *word(w,0x88)&=~0x1ff00u;
        *word(w,0x84)|=0x4020001;
        *word(w,0x88)|=0x4020000;
        *(float *)((unsigned char *)w+0x80)=1.0f;
        *(float *)((unsigned char *)w+0x134)=1.0f;
        w=ptr(w,8);
    }
    /* The locale pane lives inside the nested EdgeText instance. Walking
       only e_text's outer parents never reaches this actual glyph branch. */
    reveal(font_pane(layout));
    /* Hide decorative branches; leave all ancestors of the selected
       locale's glyph pane enabled. */
    static const char *const decoration[]={
        "peke","notes","waitIcon","itemIcon","branchArrow","number","num_base","plate"
    };
    for(unsigned i=0;i<sizeof(decoration)/sizeof(decoration[0]);i++) {
        void *candidate=mm_game_find_widget(layout,decoration[i]);
        bool ancestor=false;
        w=mm_game_find_widget(layout,"e_text");
        for(unsigned depth=0;w && depth<12;depth++,w=ptr(w,8))
            if(w==candidate) ancestor=true;
        if(!ancestor) visible(candidate,false);
    }
}
static void panel_branch(void *layout,void *w) {
    for(unsigned depth=0;w && depth<16;depth++,w=ptr(w,8)) {
        if(w!=(unsigned char *)layout+8)
            for(unsigned n=0x3c;n<=0x44;n+=4) *(float *)((unsigned char *)w+n)=0.0f;
        for(unsigned n=0x48;n<=0x50;n+=4) *(float *)((unsigned char *)w+n)=1.0f;
        for(unsigned n=0x54;n<=0x5c;n+=4) *(float *)((unsigned char *)w+n)=0.0f;
        *word(w,0x84)|=0x1ff00;*word(w,0x88)&=~0x1ff00u;
        reveal(w);
    }
}
static void panel_only(void *layout) {
    panel_branch(layout,mm_game_find_widget(layout,"plate"));
    panel_branch(layout,model_pane(layout,"mainPlate"));
    visible(mm_game_find_widget(layout,"front"),false);
    visible(mm_game_find_widget(layout,"text"),false);
    visible(mm_game_find_widget(layout,"treble_clef_us_en"),false);
}
static void heading_color(void *layout) {
    void *w=(unsigned char *)layout+8;
    /* Native RGBA property setter uses channels +0x74..+0x80 and
       masks 0x00800000,0x01000000,0x02000000,0x04000000. */
    *(float *)((unsigned char *)w+0x74)=1.0f;
    /* Native Options reference capture: full-coverage glyph pixels FFBE00. */
    *(float *)((unsigned char *)w+0x78)=190.0f/255.0f;
    *(float *)((unsigned char *)w+0x7c)=0.0f;
    *word(w,0x84)|=0x7800000;
    *word(w,0x88)=(*word(w,0x88)&~0x7800000u)|0x4800000;
}
static void *label_stream(unsigned key,const char *text) {
    Label *s=&strings[key];
    if (!s->parsed) {
        mm_native_stream_init(s->stream,s->arena,sizeof(s->arena));
        unsigned i=0;
        while(text[i] && i+1<sizeof(s->text)/sizeof(s->text[0])) {
            s->text[i]=(unsigned char)text[i]; ++i;
        }
        s->text[i]=0;
        if(!mm_game_parse_utf16(s->stream,s->text)) return 0;
        s->parsed=true;
    }
    return s->stream;
}
bool mm_style_busy(void) {return ready && mm_motion_state()->busy;}
bool mm_style_finished(void) {return ready && mm_motion_state()->finished;}
bool mm_style_exit(bool to_first) {return ready && mm_motion_exit(to_first);}
void mm_style_close(void) {
    const MmOptionsMotion *motion=mm_motion_state();
    bool to_first=motion->finished && motion->to_first;
    for(unsigned i=0;i<hidden_count;i++) {
        *word(hidden[i].widget,0x84)=hidden[i].flags;
        *word(hidden[i].widget,0x88)=hidden[i].active;
    }
    for(unsigned i=0;i<hidden_count && i<2;i++) {
        float x,y;memcpy(&x,&hidden[i].x,4);memcpy(&y,&hidden[i].y,4);
        if(to_first) {x=0;y=i?240:0;}
        if(*(float *)((unsigned char *)hidden[i].widget+0x3c)!=x ||
           *(float *)((unsigned char *)hidden[i].widget+0x40)!=y)
            widget_position(hidden[i].widget,x,y);
    }
    restore_live_player();
    mm_motion_close(RESOURCE_MANAGER);
    hidden_count=0;
    if(cursor.layout) {
        *word(cursor.layout,0x44)=cursor.x; *word(cursor.layout,0x48)=cursor.y;
        *word(cursor.layout,0x88)=cursor.alpha;
        *word(cursor.layout,0x13c)=cursor.computed_alpha;
        *word(cursor.layout,0x8c)=cursor.flags; *word(cursor.layout,0x90)=cursor.active;
    }
    memset(&cursor,0,sizeof(cursor));
    for(unsigned i=0;i<9;i++) if(labels[i]) {
        mm_game_layout_free(RESOURCE_MANAGER,labels[i]); labels[i]=0;
    }
    if(help_panel) mm_game_layout_free(RESOURCE_MANAGER,help_panel);
    if(plate) mm_game_layout_free(RESOURCE_MANAGER,plate);
    if(strength_plate) mm_game_layout_free(RESOURCE_MANAGER,strength_plate);
    if(sensitivity_plate) mm_game_layout_free(RESOURCE_MANAGER,sensitivity_plate);
    help_panel=plate=strength_plate=sensitivity_plate=owner=0; ready=false;
}
bool mm_style_update(void *opt) {
    if (!ready || opt!=owner) return false;
    if(initial_update) initial_update=false;
    else mm_motion_tick(1.0f/30.0f);
    const MmOptionsMotion *motion=mm_motion_state();
    for(unsigned i=0;i<hidden_count;i++) visible(hidden[i].widget,false);
    if(motion->busy) {
        unsigned native_page=motion->exiting && motion->to_first?0:1;
        visible(hidden[native_page].widget,true);
        widget_position(hidden[native_page].widget,0,motion->outgoing_y);
    }
    /* The console font parser treats ASCII LF as a glyph. Each description
       line therefore has its own native label instead of embedding '\n'. */
    static const char *const descriptions[][2]={
        {"Original resolution.","3D follows the slider."},
        {"400 x 240, one view.","3D is disabled."},
        {"800 x 240 output.","3D is disabled."},
        {"800 x 480 rendering.","400 x 240 output. No 3D."},
        {"400 x 480 per eye.","Use the 3D slider for depth."}
    };
    static const char *const values[]={"Original","2D","High-res","SS 2D","SS + 3D"};
    static const char *const headings[]={"DISPLAY: ORIGINAL","DISPLAY: 2D","DISPLAY: HIGH-RES","DISPLAY: SS 2D","DISPLAY: SS + 3D"};
    unsigned m=mm_native_options.selected;
    if(m>=MM_MODE_COUNT) return false;
    bool available=(mm_native_options.backend.available_modes&(1u<<m))!=0;
    const char *line1=available?descriptions[m][0]:"Temporarily unavailable.";
    const char *line2=available?descriptions[m][1]:
                      "Output buffers are not ready.";
    if(available && m!=mm_native_options.active) line2="Press A to apply this mode.";
    if(mm_native_options.unsaved && mm_native_options.result==MM_SAVE_FAILED) line2="Could not save. Press A to retry.";
    const char *strength=mm_options_strength_label(&mm_native_options);
    const char *sensitivity=mm_options_sensitivity_label(&mm_native_options);
    const char *changes[]={strength,line1,line2,sensitivity};const unsigned slots[]={26,31,32,34};
    for(unsigned i=0;i<4;i++) {
        Label *cached=&strings[slots[i]];unsigned j=0;
        while(changes[i][j] && cached->text[j]==(unsigned char)changes[i][j]) ++j;
        if(cached->text[j]!=(unsigned char)changes[i][j]) cached->parsed=false;
    }
    bool depth=mm_native_options.graphics_row==1;
    bool look=mm_native_options.graphics_row==2;
    void *streams[]={label_stream(1,"Display"),label_stream(2+m,values[m]),
                    look?label_stream(36,"Adjust C-stick camera speed."):depth?label_stream(29,"Adjust stereo depth."):label_stream(31,line1),
                    look?(mm_native_options.unsaved?label_stream(32,line2):label_stream(37,"Normal is the original speed.")):depth?(mm_native_options.unsaved?label_stream(32,line2):label_stream(30,"Normal is the original strength.")):label_stream(32,line2),
                    look?label_stream(35,"LOOK SPEED"):depth?label_stream(28,"3D STRENGTH"):label_stream(21+m,headings[m]),
                    label_stream(27,"3D strength"),label_stream(26,strength),
                    label_stream(33,"Look speed"),label_stream(34,sensitivity)};
    for(unsigned i=0;i<9;i++) {
        if(!streams[i] || !bind(labels[i],streams[i])) return false;
        text_only(labels[i]);
        if(i==4) heading_color(labels[i]);
        mm_game_layout_calc(labels[i],0.0f);
        text_only(labels[i]);
    }
    panel_only(help_panel);
    mm_game_layout_calc(help_panel,0.0f);
    panel_only(help_panel);
    /* Run the original BasicPlate players at the native frame step. */
    void *plates[]={plate,strength_plate,sensitivity_plate};
    /* page2_g has a serialized off-screen X of320. Hidden native groups
       are not a valid translation source for this independently drawn page. */
    for(unsigned i=0;i<3;i++) {
        position(plates[i],8.0f,4.0f+48.0f*i+motion->incoming_y);
        opacity(plates[i],motion->alpha[i]);
        mm_game_layout_calc(plates[i],1.0f/30.0f);
    }
    for(unsigned i=0;i<9;i++) {
        if(i==2 || i==3 || i==4) continue;
        position(labels[i],(i==0 || i==5 || i==7?76.0f:234.0f),
                 (i<2?22.0f:i<7?70.0f:118.0f)+motion->incoming_y);
        opacity(labels[i],motion->alpha[i<2?0:i<7?1:2]);
        mm_game_layout_calc(labels[i],0.0f);
    }
    if(mm_native_options.graphics_row!=last_row) {
        unsigned index=mm_native_options.graphics_row==(last_row+1)%3?3:4;
        void *controller=ptr(opt,0x104);
        unsigned animation=*word(opt,0x10c+4*index);
        if(controller) mm_game_animation_play(controller,animation,0,0.0f);
        last_row=mm_native_options.graphics_row;
    }
    if(cursor.layout) {
        position(cursor.layout,8.0f,4.0f+48.0f*mm_native_options.graphics_row+motion->incoming_y);
        opacity(cursor.layout,motion->alpha[mm_native_options.graphics_row]);
        mm_game_layout_calc(cursor.layout,0.0f);
    }
    return true;
}
bool mm_style_open(void *opt, void *help) {
    if(ready) return owner==opt;
    void *menu=ptr(opt,0x10);
    if(!menu) return false;
    static const char *const groups[]={"page1_g","page2_g","btn_page1_g","btn_page2_g"};
    /* Require the exact native groups before mutating the live layout. */
    for(unsigned i=0;i<4;i++) {
        void *w=mm_game_find_widget(menu,groups[i]);
        if(!w) { mm_style_close(); return false; }
        hidden[i].widget=w; hidden[i].flags=*word(w,0x84);
        hidden[i].active=*word(w,0x88);
        hidden[i].x=*word(w,0x3c);hidden[i].y=*word(w,0x40); ++hidden_count;
    }
    if(help) {
        void *w=(unsigned char *)help+8;
        hidden[4].widget=w; hidden[4].flags=*word(w,0x84);
        hidden[4].active=*word(w,0x88); ++hidden_count;
    }
    plate=create("Joker.Main.Play.Option.BasicPlate");
    if(!plate) { mm_style_close(); return false; }
    position(plate,8.0f,4.0f);
    strength_plate=create("Joker.Main.Play.Option.BasicPlate");
    if(!strength_plate) { mm_style_close(); return false; }
    position(strength_plate,8.0f,52.0f);
    sensitivity_plate=create("Joker.Main.Play.Option.BasicPlate");
    if(!sensitivity_plate) { mm_style_close(); return false; }
    position(sensitivity_plate,8.0f,100.0f);
    static const char *const old_text[]={"choice_text_g","switching_text_g","switching_volume_text_g"};
    for(unsigned i=0;i<3;i++) {
        visible(mm_game_find_widget(plate,old_text[i]),false);
        visible(mm_game_find_widget(strength_plate,old_text[i]),false);
        visible(mm_game_find_widget(sensitivity_plate,old_text[i]),false);
    }
    static const float xy[9][2]={{76,22},{234,22},{200,199},{200,217},{200,181},{76,70},{234,70},{76,118},{234,118}};
    for(unsigned i=0;i<9;i++) {
        labels[i]=create("Joker.Main.MessageWindow");
        if(!labels[i] || !static_label(labels[i],i)) { mm_style_close(); return false; }
        /* Hardware capture confirms a centered single-line baseline at
           the layout origin. The serialized rectangle is not a text origin. */
        position(labels[i],xy[i][0],xy[i][1]);
        text_only(labels[i]);
        if(i==4) heading_color(labels[i]);
        mm_game_layout_calc(labels[i],0.0f);
        text_only(labels[i]);
    }
    /* Native English help plate: (-152,-42,304,84), centered at (200,196).
       This matches the supplied top screenshot's 400x240 logical coordinates. */
    help_panel=create("Joker.Main.MessageWindow");
    if(!help_panel || !static_label(help_panel,9)) { mm_style_close(); return false; }
    position(help_panel,200.0f,196.0f);
    panel_only(help_panel);
    mm_game_layout_calc(help_panel,0.0f);
    panel_only(help_panel);
    cursor.layout=ptr(opt,0x18);
    if(cursor.layout) {
        cursor.x=*word(cursor.layout,0x44); cursor.y=*word(cursor.layout,0x48);
        cursor.alpha=*word(cursor.layout,0x88);cursor.computed_alpha=*word(cursor.layout,0x13c);
        cursor.flags=*word(cursor.layout,0x8c); cursor.active=*word(cursor.layout,0x90);
    }
    owner=opt; ready=true;initial_update=true;last_row=mm_native_options.graphics_row;
    suspend_live_player(opt);
    (void)mm_motion_open(RESOURCE_MANAGER);
    if(!mm_style_update(opt)) { mm_style_close(); return false; }
    return true;
}
bool mm_style_draw(void *opt) {
    if(!ready || owner!=opt) return false;
    void *manager=mm_game_layout_manager();
    mm_game_layout_submit(manager,plate,5,3);
    mm_game_layout_submit(manager,strength_plate,5,3);
    mm_game_layout_submit(manager,sensitivity_plate,5,3);
    mm_game_layout_submit(manager,help_panel,3,0);
    for(unsigned i=0;i<9;i++) {
        bool top=i==2 || i==3 || i==4;
        mm_game_layout_submit(manager,labels[i],top?3:5,top?1:5);
    }
    return true;
}

/* Native arrow models implement idle/activated/pressed through vtable+0x28,
   the same calls made by original Options input at5a2c5c and5a2e64. */
static void arrow_state(void *layout,unsigned state) {
    if(!layout) return;
    uint32_t *vtable=ptr(layout,0);
    if(!vtable || !vtable[10]) return;
    void (*set)(void *,unsigned)=(void *)(uintptr_t)vtable[10];
    set(layout,state);
}
void mm_style_buttons(uint32_t keys) {
    if(!ready) return;
    void *plates[]={plate,strength_plate,sensitivity_plate};
    for(unsigned i=0;i<3;i++) {
        bool selected=i==mm_native_options.graphics_row;
        void *l=mm_game_find_widget(plates[i],"direct_left_l");
        void *r=mm_game_find_widget(plates[i],"direct_right_l");
        l=l?ptr(l,0x20):0;r=r?ptr(r,0x20):0;
        arrow_state(l,selected && (keys&MM_KEY_LEFT)?2:0);
        arrow_state(r,selected && (keys&MM_KEY_RIGHT)?2:0);
    }
}
