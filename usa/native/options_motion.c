/* Evaluate the supplied MenuOption's ACTUAL animation players on a private
 * complete native layout. It is calculated, never submitted. Its visible
 * page groups give page3 the same slide/fade channels as the original UI,
 * without restarting the live Options controller or reading hidden groups. */
#include "options_motion.h"
#include <stdint.h>
#include <string.h>
extern unsigned mm_game_resource_index(void *,const char *);
extern void *mm_game_layout_create(void *,unsigned,unsigned);
extern void mm_game_layout_free(void *,void *);
extern void mm_game_layout_calc(void *,float);
extern void *mm_game_find_widget(void *,const char *);
extern void *mm_game_animation_controller(void *,const char *);
extern unsigned mm_game_animation_find(void *,const char *);
extern void mm_game_animation_play(void *,unsigned,int,float);
static void *driver,*controller,*pages[2],*rows[3];
static unsigned entry,exit_animation,frames;
static MmOptionsMotion state;
static float scalar(void *p,unsigned offset) {float v;memcpy(&v,(char *)p+offset,4);return v;}
static void settled(void) {
    state.busy=false;state.incoming_y=state.outgoing_y=0;
    for(unsigned i=0;i<3;i++) state.alpha[i]=1;
    if(state.exiting) state.finished=true;
}
const MmOptionsMotion *mm_motion_state(void) {return &state;}
void mm_motion_close(void *resources) {
    if(driver) mm_game_layout_free(resources,driver);
    driver=controller=0;memset(pages,0,sizeof(pages));memset(rows,0,sizeof(rows));
    memset(&state,0,sizeof(state));settled();
}
static void sample(void) {
    /* Native1989c4 selects widget+8c for evaluated animation properties;
       1bbc9c writes Y at that block+4. The base pose at3c/40 is unchanged.
       Alpha is block+44=d0, not the base alpha at80. */
    float incoming=scalar(pages[1],0x90),outgoing=scalar(pages[0],0x90);
    /* Native tracks cover -240..240. Reject invalid models/NaN instead of
       applying arbitrary off-screen transforms to our visible controls. */
    if(!(incoming>=-240 && incoming<=240 && outgoing>=-240 && outgoing<=240)) {settled();return;}
    state.incoming_y=incoming;state.outgoing_y=outgoing;
    for(unsigned i=0;i<3;i++) {
        float alpha=scalar(rows[i],0xd0);
        state.alpha[i]=alpha>=0 && alpha<=1?alpha:1;
    }
}
bool mm_motion_open(void *resources) {
    mm_motion_close(resources);
    unsigned index=mm_game_resource_index((char *)resources+4,"Joker.Main.Play.Option.MenuOption");
    if((index&0xffff)==0xffff) return false;
    driver=mm_game_layout_create(resources,index,0x30000);
    if(!driver) return false;
    controller=mm_game_animation_controller(driver,"main");
    entry=mm_game_animation_find(driver,"from_page1to2_ani");
    exit_animation=mm_game_animation_find(driver,"from_page2to1_ani");
    pages[0]=mm_game_find_widget(driver,"page1_g");pages[1]=mm_game_find_widget(driver,"page2_g");
    static const char *const names[]={"b_plate_subjective_cam_l","b_plate_motion_l","b_plate_swim_l"};
    for(unsigned i=0;i<3;i++) rows[i]=mm_game_find_widget(driver,names[i]);
    if(!controller || !entry || !exit_animation || !pages[0] || !pages[1] || !rows[0] || !rows[1] || !rows[2]) {
        mm_motion_close(resources);return false;
    }
    /* Both groups stay enabled in the private instance. Unlike v33, its
       native calculator can update every channel while the live groups hide. */
    for(unsigned i=0;i<2;i++) {
        uint32_t v;memcpy(&v,(char *)pages[i]+0x84,4);v|=0x20001;memcpy((char *)pages[i]+0x84,&v,4);
        memcpy(&v,(char *)pages[i]+0x88,4);v|=0x20000;memcpy((char *)pages[i]+0x88,&v,4);
    }
    mm_game_animation_play(controller,entry,0,0);
    mm_game_layout_calc(driver,0);
    state.busy=true;state.exiting=state.finished=state.to_first=false;frames=0;sample();
    return state.busy;
}
bool mm_motion_exit(bool to_first) {
    state.to_first=to_first;state.exiting=true;state.finished=false;
    if(!driver || !controller) {settled();return false;}
    mm_game_animation_play(controller,exit_animation,0,0);
    mm_game_layout_calc(driver,0);
    state.busy=true;frames=0;sample();return state.busy;
}
void mm_motion_tick(float dt) {
    if(!driver || !state.busy) return;
    if(!(dt>0 && dt<=0.1f)) dt=1.0f/30.0f;
    mm_game_layout_calc(driver,dt);sample();
    if(!state.busy) return;
    ++frames;
    /* Native transitions last6/7 frames at30Hz. A bounded guard prevents an
       unavailable/stalled player from leaving a hidden or locked menu. */
    unsigned duration=state.exiting?7:6;
    if(!*((uint8_t *)controller+8) || frames>duration+3) settled();
}
