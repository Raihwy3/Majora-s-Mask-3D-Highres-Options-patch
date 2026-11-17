#include "../native/options_motion.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned char resources[16],driver[0x200],controller[64],pages[2][0x140],rows[3][0x140];
static unsigned plays,freed,created,frames,animation;
static bool unavailable,stalled,invalid;
static void set(void *p,unsigned n,float v) {memcpy((char *)p+n,&v,4);}
unsigned mm_game_resource_index(void *r,const char *s) {
    assert(r==resources+4 && !strcmp(s,"Joker.Main.Play.Option.MenuOption"));return unavailable?0xffff:42;
}
void *mm_game_layout_create(void *r,unsigned index,unsigned flags) {
    assert(r==resources && index==42 && flags==0x30000);++created;return driver;
}
void mm_game_layout_free(void *r,void *d) {assert(r==resources && d==driver);++freed;}
void *mm_game_animation_controller(void *d,const char *s) {assert(d==driver && !strcmp(s,"main"));return controller;}
unsigned mm_game_animation_find(void *d,const char *s) {
    assert(d==driver);
    if(!strcmp(s,"from_page1to2_ani")) return 10;
    assert(!strcmp(s,"from_page2to1_ani"));return 11;
}
void *mm_game_find_widget(void *d,const char *s) {
    assert(d==driver);
    const char *names[]={"page1_g","page2_g","b_plate_subjective_cam_l","b_plate_motion_l","b_plate_swim_l"};
    void *p[]={pages[0],pages[1],rows[0],rows[1],rows[2]};
    for(unsigned i=0;i<5;i++) if(!strcmp(s,names[i])) return p[i];
    assert(0);return 0;
}
void mm_game_animation_play(void *c,unsigned a,int flags,float start) {
    assert(c==controller && (a==10 || a==11) && !flags && start==0);
    animation=a;frames=0;controller[8]=1;++plays;
}
void mm_game_layout_calc(void *d,float dt) {
    assert(d==driver && (dt==0 || dt==1.0f/30));
    if(dt && !stalled) ++frames;
    unsigned duration=animation==10?6:7;
    float f=(float)frames/duration;if(f>1) f=1;
        /* Native animation modifies the evaluated block; the base pose stays
       fixed. V35 read these base fields and therefore copied no movement. */
    set(pages[0],0x40,0);set(pages[1],0x40,0);
    set(pages[0],0x90,animation==10?-240*f:-240+240*f);
    set(pages[1],0x3c,320); /* Off-screen horizontal value must never be copied. */
    set(pages[1],0x90,invalid?999:animation==10?240-240*f:240*f);
    for(unsigned i=0;i<3;i++) {set(rows[i],0x80,1);set(rows[i],0xd0,animation==10?f:1-f);}
    if(frames>=duration) controller[8]=0;
}
int main(void) {
    const MmOptionsMotion *m=mm_motion_state();
    assert(mm_motion_open(resources) && m->busy && plays==1);
    assert(m->incoming_y==240 && m->outgoing_y==0 && m->alpha[0]==0);
    for(unsigned i=0;i<5;i++) {
        mm_motion_tick(1.0f/30);
        assert(m->busy && m->incoming_y>0 && m->incoming_y<240 && m->outgoing_y<0);
        assert(m->incoming_y+m->outgoing_y<240);
        assert(m->alpha[1]>=0 && m->alpha[1]<=1);
    }
    mm_motion_tick(1.0f/30);assert(!m->busy && m->incoming_y==0 && m->alpha[0]==1);
    assert(mm_motion_exit(false));
    for(unsigned i=0;i<7;i++) mm_motion_tick(1.0f/30);
    assert(m->finished && m->exiting && !m->to_first);
    mm_motion_close(resources);assert(freed==created);
    assert(mm_motion_open(resources));assert(mm_motion_exit(true));
    for(unsigned i=0;i<7;i++) mm_motion_tick(1.0f/30);
    assert(m->finished && m->to_first);
    mm_motion_close(resources);
    stalled=true;assert(mm_motion_open(resources));
    for(unsigned i=0;i<12;i++) mm_motion_tick(1.0f/30);
    assert(!m->busy && m->incoming_y==0 && m->alpha[0]==1);
    mm_motion_close(resources);stalled=false;invalid=true;
    assert(!mm_motion_open(resources) && !m->busy && m->incoming_y==0 && m->alpha[0]==1);
    mm_motion_close(resources);invalid=false;unavailable=true;
    assert(!mm_motion_open(resources) && !m->busy && m->alpha[0]==1);
    mm_motion_close(resources);assert(created==freed);
    puts("PASS: private native animation instance, channel following, forward/back completion, stalled/invalid/missing-player visibility fallback and cleanup");
}
