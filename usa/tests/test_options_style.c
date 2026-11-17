#include "display_options.h"
#include "../native/options_motion.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
MmOptions mm_native_options;
static unsigned motion_ticks;
static MmOptionsMotion motion={0,0,{1,1,1},false,false,false,false};
const MmOptionsMotion *mm_motion_state(void) {return &motion;}
bool mm_motion_open(void *r) {assert(r);return false;}
void mm_motion_close(void *r) {assert(r);motion=(MmOptionsMotion){0,0,{1,1,1},false,false,false,false};}
bool mm_motion_exit(bool f) {(void)f;return false;}
void mm_motion_tick(float dt) {assert(dt==1.0f/30.0f);++motion_ticks;}
static unsigned animation_plays;
void mm_game_animation_play(void *c,unsigned a,int flags,float start) {assert(c && a && !flags && start==0);++animation_plays;}
unsigned mm_game_animation_find(void *l,const char *s) {(void)l;(void)s;return 1;}
void *mm_game_animation_controller(void *l,const char *s) {(void)s;return l;}

static _Alignas(8) unsigned char manager[0x100], live_controller[32], opt[0x200], menu[0x200],help[0x200],cursor[0x200];
static _Alignas(8) unsigned char groups[4][0x140], plate[0x400], strength_plate[0x400], sensitivity_plate[0x400], old_text[3][3][0x140];
static struct {
    _Alignas(8) unsigned char layout[0x400],widget[0x140],child[32],instance[64],panes[16],pane[0x140],font_parent[0x140];
    _Alignas(8) unsigned char decor[11][0x140],sprite[1][0x320];
} label[10];
void *mm_test_resource_manager=manager;
static uint32_t message_vtable[24];
static unsigned allocated,freed,submitted,calculated,parse_count,plates_created,labels_created;
static int failure, missing_group;
extern bool mm_style_open(void *,void *);
extern bool mm_style_update(void *);
extern bool mm_style_draw(void *);
extern void mm_style_close(void);
static void put(void *p,unsigned n,void *v) { assert((uintptr_t)v<=UINT32_MAX); uint32_t a=(uint32_t)(uintptr_t)v; memcpy((char *)p+n,&a,4); }
static uint32_t *word(void *p,unsigned n) { return (uint32_t *)((char *)p+n); }
unsigned mm_game_resource_index(void *p,const char *s) {
    assert(p==manager+4);
    if(failure==1) return 0xffff;
    if(!strcmp(s,"Joker.Main.Play.Option.BasicPlate")) return 0;
    assert(!strcmp(s,"Joker.Main.MessageWindow"));return 1;
}
void *mm_game_layout_create(void *p,unsigned index,unsigned flags) {
    assert(p==manager && flags==0x30000);
    if(failure==2 && allocated==3) return 0;
    assert(allocated<13);++allocated;
    if(index==0) {
        void *plates[]={plate,strength_plate,sensitivity_plate};
        assert(plates_created<3);return plates[plates_created++];
    }
    assert(index==1 && labels_created<10);
    void *result=label[labels_created++].layout;
    put(result,0,message_vtable+2);
    return result;
}
void mm_game_layout_free(void *p,void *l) { assert(p==manager && l);++freed; }
void mm_game_layout_calc(void *l,float dt) {
    assert(l && (dt==0 || dt==1.0f/30.0f));
    if(l!=plate && l!=strength_plate && l!=sensitivity_plate && l!=cursor) {
        uint32_t *v=(void *)(uintptr_t)*word(l,0);
        assert(v && v!=message_vtable+2 && v[4]==0x161bdc);
        /* A real closed MessageWindow skips this base calculation entirely. */
        assert(message_vtable[6]==0x606de8);
    }
    ++calculated;
}
void *mm_game_find_widget(void *l,const char *s) {
    static const char *const names[]={"page1_g","page2_g","btn_page1_g","btn_page2_g"};
    if(l==menu) {
        for(unsigned i=0;i<4;i++) if(!strcmp(s,names[i])) return missing_group ? NULL:groups[i];
        assert(0);
    }
    if(l==plate || l==strength_plate || l==sensitivity_plate) {
        unsigned row=l==sensitivity_plate?2:l==strength_plate?1:0;
        if(!strcmp(s,"choice_text_g")) return old_text[row][0];
        if(!strcmp(s,"switching_text_g")) return old_text[row][1];
        assert(!strcmp(s,"switching_volume_text_g")); return old_text[row][2];
    }
    for(unsigned i=0;i<10;i++) if(l==label[i].layout) {
        if(!strcmp(s,"e_text") || (i==9 && !strcmp(s,"mainPlate"))) return label[i].widget;
        static const char *const names[]={"front","plate","peke","notes","waitIcon","itemIcon","branchArrow","number","num_base","text","treble_clef_us_en"};
        for(unsigned j=0;j<11;j++) if(!strcmp(s,names[j])) return label[i].decor[j];
    }
    assert(0);return 0;
}
void *mm_game_current_widget(void *l) {
    for(unsigned i=0;i<10;i++) if(l==label[i].child) return label[i].instance;
    assert(0);return 0;
}
void *mm_game_text_pane(void *w) {
    if(failure==3) return 0;
    for(unsigned i=0;i<10;i++) {
        if(w==label[i].widget) return 0; /* builtin child Layout, not a sprite */
        if(w==label[i].pane) return label[i].sprite[0];

    }
    assert(0);return 0;
}
void *mm_game_layout_manager(void) { return manager; }
void mm_game_layout_submit(void *m,void *l,int layer,int priority) {
    assert(m==manager && l);
    if(l==plate || l==strength_plate || l==sensitivity_plate) assert(layer==5 && priority==3);
    else if(l==label[9].layout) assert(layer==3 && priority==0);
    else {
        int i=0;while(i<9 && l!=label[i].layout) ++i;
        assert(i<9);
        bool top=i==2 || i==3 || i==4;
        assert(layer==(top?3:5) && priority==(top?1:5));
    }
    ++submitted;
}
void *mm_game_stream_construct(void *s) { memset(s,0,0x4c);return s; }
int mm_game_parse_utf16(void *s,const uint16_t *text) {
    assert(s && text[0]);
    for(unsigned i=0;text[i];i++) assert(text[i]!='\n');
    ++parse_count;return 1;
}
int mm_game_sprite_set_stream(void *s,void *stream) { put(s,0x124,stream);return 1; }
static void reset(void) {
    allocated=freed=submitted=calculated=parse_count=plates_created=labels_created=0;
    message_vtable[6]=0x606de8;
    memset(label,0,sizeof(label));
    for(unsigned i=0;i<10;i++) {
        put(label[i].widget,0x20,label[i].child);
        put(label[i].widget,8,label[i].decor[0]);
        put(label[i].decor[0],8,label[i].layout+8);
        put(label[i].instance,0x18,label[i].panes);
        *word(label[i].instance,0x1c)=1;
        put(label[i].panes,0,label[i].pane);
        put(label[i].pane,8,label[i].font_parent);
        /* Factory-created nested font remains separate from outer e_text. */
        *(float *)(label[i].font_parent+0x80)=0.0f;
    }
    put(label[9].decor[1],8,label[9].layout+8);
    put(label[9].pane,8,label[9].decor[1]);
}
int main(void) {
    put(opt,0x10,menu);put(opt,0x18,cursor);
    put(opt,0xe4,live_controller);
    *word(live_controller,0xc)=0x223344;*word(live_controller,0x10)=0x40c00000;
    live_controller[8]=0;live_controller[9]=1;live_controller[10]=1;put(opt,0x104,cursor);
    *word(opt,0xf8)=7;*word(opt,0x118)=8;*word(opt,0x11c)=9;
    for(unsigned i=0;i<4;i++) { *word(groups[i],0x84)=0x123401;*word(groups[i],0x88)=0x54321; }
    *word(help,0x8c)=0x123401;*word(help,0x90)=0x54321;
    *word(cursor,0x44)=0x123;*word(cursor,0x48)=0x456;
    *word(cursor,0x8c)=0x789;*word(cursor,0x90)=0xabc;
    mm_native_options.backend.available_modes=1u<<MM_HIGH_RES_2D;
    mm_native_options.active=MM_HIGH_RES_2D;
    mm_native_options.selected=MM_HIGH_RES_2D;
    reset();
    assert(mm_style_open(opt,help) && allocated==13 && !freed);
    assert(motion_ticks==0); /* setup does not consume an animation frame */
    assert(!*word(live_controller,0xc) && !live_controller[8]);
    const float xy[10][2]={{76,22},{234,22},{200,199},{200,217},{200,181},{76,70},{234,70},{76,118},{234,118},{200,196}};
    for(unsigned i=0;i<10;i++) {
        assert(*(float *)(label[i].layout+0x44)==xy[i][0]);
        assert(*(float *)(label[i].layout+0x48)==xy[i][1]);
    }
    for(unsigned m=0;m<MM_MODE_COUNT;m++) {
        mm_native_options.selected=m;
        assert(mm_style_update(opt));
        assert(mm_style_draw(opt));
        for(unsigned i=0;i<4;i++) assert(!(*word(groups[i],0x84)&1));
        assert(!(*word(help,0x8c)&1));
        for(unsigned i=0;i<9;i++) {
            assert(*word(label[i].decor[0],0x84)&1); /* text ancestor stays visible */
            for(unsigned j=1;j<9;j++) assert(!(*word(label[i].decor[j],0x84)&1));
            assert(*(float *)(label[i].widget+0x3c)==0.0f);
            assert(*(float *)(label[i].widget+0x48)==1.0f);
            assert(!(*word(label[i].decor[1],0x84)&1));
            /* Each label has its own complete central text model/stream. */
            assert(*word(label[i].sprite[0],0x124));
            assert(*word(label[i].pane,0x84)&1);
            assert(*word(label[i].font_parent,0x84)&1);
            assert(*(float *)(label[i].font_parent+0x80)==1.0f);
            if(i) assert(*word(label[i].sprite[0],0x124)!=*word(label[i-1].sprite[0],0x124));
        }
    }
    assert(submitted==65 && calculated>=54);
    assert(!(*word(label[9].decor[0],0x84)&1)); /* panel front hidden */
    assert(!(*word(label[9].decor[9],0x84)&1)); /* panel text hidden */
    assert(*word(label[9].decor[1],0x84)&1); /* native plate retained */
    assert(*(float *)(label[4].layout+8+0x74)==1.0f);
    assert(*(float *)(label[4].layout+8+0x78)==190.0f/255.0f);
    assert(*(float *)(label[4].layout+8+0x7c)==0.0f);
    unsigned parsed=parse_count;
    assert(mm_style_update(opt) && parse_count==parsed); /* persistent token cache */
    mm_native_options.strength_index++;
    assert(mm_style_update(opt) && parse_count==parsed+1); /* strength refresh */
    assert(mm_style_update(opt) && parse_count==parsed+1); /* value remains cached */
    mm_native_options.graphics_row=1;
    assert(mm_style_update(opt));
    assert(*(float *)(cursor+0x48)==52.0f);
    mm_native_options.graphics_row=2;
    assert(mm_style_update(opt));
    assert(*(float *)(cursor+0x48)==100.0f);
    *(float *)(groups[1]+0x3c)=320.0f;
    assert(mm_style_update(opt));
    assert(*(float *)(sensitivity_plate+0x44)==8.0f);
    assert(*(float *)(label[7].layout+0x44)==76.0f);
    assert(*(float *)(cursor+0x44)==8.0f);
    *(float *)(groups[1]+0x3c)=0;
    assert(animation_plays>=2);
    mm_native_options.graphics_row=0;
    assert(mm_style_update(opt));
    assert(*(float *)(cursor+0x48)==4.0f);
    motion.busy=true;motion.incoming_y=120;motion.outgoing_y=-120;
    motion.alpha[0]=0.25f;motion.alpha[1]=0.5f;motion.alpha[2]=0.75f;
    assert(mm_style_update(opt));
    assert(*(float *)(plate+0x48)==124 && *(float *)(label[0].layout+0x48)==142);
    assert(*(float *)(label[5].layout+0x88)==0.5f);
    assert(*(float *)(label[7].layout+0x88)==0.75f);
    assert(*(float *)(cursor+0x88)==0.25f);
    assert(*(float *)(groups[1]+0x40)==-120 && (*word(groups[1],0x84)&1));
    mm_style_close();assert(freed==allocated && !mm_style_draw(opt));
    assert(*word(live_controller,0xc)==0x223344 && *word(live_controller,0x10)==0x40c00000);
    assert(!live_controller[8] && live_controller[9]==1 && live_controller[10]==1);
    for(unsigned i=0;i<4;i++) assert((*word(groups[i],0x84)&~0x300u)==(0x123401&~0x300u) && (*word(groups[i],0x88)&~0x300u)==(0x54321&~0x300u));
    assert(*word(help,0x8c)==0x123401 && *word(help,0x90)==0x54321);
    assert(*word(cursor,0x44)==0x123 && *word(cursor,0x48)==0x456 &&
           *word(cursor,0x8c)==0x789 && *word(cursor,0x90)==0xabc);
    for(failure=1;failure<=3;failure++) {
        reset();assert(!mm_style_open(opt,help));assert(freed==allocated);
        for(unsigned i=0;i<4;i++) assert((*word(groups[i],0x84)&~0x300u)==(0x123401&~0x300u));
    }
    failure=0;reset();assert(mm_style_open(opt,help));
    /* A newly started native R transition must survive Page3 cleanup. */
    *word(live_controller,0xc)=0x556677;*word(live_controller,0x10)=0x3f800000;live_controller[8]=1;
    mm_style_close();assert(freed==allocated);
    assert(*word(live_controller,0xc)==0x556677 && *word(live_controller,0x10)==0x3f800000 && live_controller[8]);
    failure=0;missing_group=1;reset();
    assert(!mm_style_open(opt,help) && !allocated);
    puts("PASS: native row/complete MessageWindow resource paths; all modes; independent text streams; no shared text overwrite; position/visibility restoration; missing asset/allocation/text cleanup.");
}
