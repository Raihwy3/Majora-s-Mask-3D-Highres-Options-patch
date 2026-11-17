#include "display_options.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
MmOptions mm_native_options;
static bool staging_ready=true;
bool mm_resolve_prepare(void) {return staging_ready;}
static _Alignas(8) uint8_t hid[0x100], owner[32], display[0x194];
uint8_t *mm_test_hid = hid, *mm_test_owner = owner;
static _Alignas(8) uint8_t registry[0x180],records[4][28];
uint8_t *mm_test_registry=0;
static void word(void *p,unsigned n,uint32_t v) {memcpy((uint8_t *)p+n,&v,4);}
static uint32_t get(void *p,unsigned n) {uint32_t v;memcpy(&v,(uint8_t *)p+n,4);return v;}
static void pointer(void *p,unsigned n,void *v) {assert((uintptr_t)v<=UINT32_MAX);word(p,n,(uint32_t)(uintptr_t)v);}
static unsigned lcd_mode, expected_flags,expected_format=0x141;
static void *expected_secondary;
extern void mm_render_present(unsigned,unsigned,void *,void *,unsigned,unsigned,unsigned);
void mm_game_present(unsigned screen,unsigned index,void *left,void *right,unsigned stride,unsigned format,unsigned status) {
    assert(screen<=1 && index==1 && left==display && right==expected_secondary);
    assert(stride==720 && format==expected_format && status==7);
}
extern volatile uint32_t mm_scene_width, mm_scene_height;
extern float mm_stereo_depth;
extern int mm_render_stereo(void);
extern void mm_render_transfer(void *, unsigned, int, int, unsigned);
extern uint32_t mm_render_available_modes(void);
extern bool mm_render_build(void *, const MmProfile *, void **);
extern bool mm_render_commit(void *, void *);
void mm_game_set_display_mode(unsigned mode) { lcd_mode = mode; }
void mm_game_surface_transfer(void *surface, unsigned scale, int x, int y, unsigned arg) {
    assert((surface == display || surface==display+0x24 || surface==display+0x50) && scale == expected_flags && x == 3 && y == 7 && arg == 11);
}
int main(void) {
    /* Deliberately unaligned on a 64-bit host; production pointers are ARM32. */
    /* Offset 0 is a distinct allocator pointer, never the owner itself.
       Native factory 0x105b78 loads owner address 0x6a4a48 directly. */
    const uint8_t allocator[0x194] = {0};
    void *allocator_pointer = (void *)allocator;
    memcpy(owner, &allocator_pointer, sizeof(allocator_pointer));
    void *p = display;
    memcpy(owner + 4, &p, sizeof(p));
    memset(display, 0x5a, sizeof(display));
    assert(mm_render_available_modes()==(1u<<MM_HIGH_RES_2D));
    const MmMode forced[]={MM_ORIGINAL,MM_NATIVE_2D,MM_HIGH_RES_2D,MM_SUPERSAMPLE_2D,MM_SUPERSAMPLE_3D,(MmMode)99};
    unsigned char bottom[0x2c];memcpy(bottom,display+0x7c,sizeof(bottom));
    for(unsigned i=0;i<sizeof(forced)/sizeof(forced[0]);i++) for(unsigned on=0;on<2;on++) {
        MmMode mode=forced[i];
        float value=on?0.75f:0;
        memcpy(hid+0x80,&value,4);
        mm_native_options.active=mode;mm_native_options.result=MM_OK;
        assert(!mm_render_stereo());
        assert(mm_native_options.active==MM_HIGH_RES_2D);
        assert(mm_native_options.result==(mode==MM_HIGH_RES_2D?MM_OK:MM_UNAVAILABLE));
        assert(mm_scene_width==800 && mm_scene_height==480 && lcd_mode==0x701);
        assert(*(uint32_t *)(display+0x40)==800 && *(uint32_t *)(display+0x6c)==800);
        assert(!memcmp(bottom,display+0x7c,sizeof(bottom)));
        expected_flags=0x501;
        mm_render_transfer(display,0xdead,3,7,11);
        if(mode>=MM_MODE_COUNT) continue;
        MmProfile profile;assert(mm_profile(mode,on,&profile));
        void *candidate=(void *)0x123;
        unsigned char snapshot[sizeof(display)];memcpy(snapshot,display,sizeof(display));
        assert(mm_render_build(0,&profile,&candidate)==(mode==MM_HIGH_RES_2D));
        assert(!memcmp(snapshot,display,sizeof(display)));
        if(mode!=MM_HIGH_RES_2D) { assert(!candidate);continue; }
        assert(candidate && mm_render_commit(0,candidate));
        assert(!mm_render_commit(0,display));
        mm_test_owner=0;assert(!mm_render_commit(0,candidate));mm_test_owner=owner;
        profile.scene_width=400;candidate=(void *)0x123;
        assert(!mm_render_build(0,&profile,&candidate) && !candidate);
    }
    assert(!mm_render_build(0,0,0));
    mm_native_options.strength_index=3;mm_render_stereo();assert(mm_stereo_depth==10.0f);
    mm_native_options.strength_index=9;mm_render_stereo();assert(mm_stereo_depth==32.0f);
    mm_native_options.strength_index=0;mm_render_stereo();assert(mm_stereo_depth==0.0f);
    mm_native_options.strength_index=3;
    hid[0x84]=1;
    float on=1;memcpy(hid+0x80,&on,4);
    mm_native_options.active=MM_ORIGINAL;assert(!mm_render_stereo());
    /* Native hash collisions and both eye double buffers. Storage remains
       800 columns; only the record width used by scanout is synchronized. */
    const unsigned ids[]={1,33,2,34},offsets[]={0x28,0x2c,0x54,0x58};
    for(unsigned i=0;i<4;i++) {
        word(display,offsets[i],ids[i]);word(records[i],0,ids[i]);
        word(records[i],4,0x18000000+i*0x100000);word(records[i],8,0x8051);
        word(records[i],0xc,240);word(records[i],0x10,800);word(records[i],0x14,0x10000);
    }
    pointer(registry,0xa4+4,records[0]);pointer(records[0],0x18,records[1]);
    pointer(registry,0xa4+8,records[2]);pointer(records[2],0x18,records[3]);
    word(registry,0x124,0x12345678);mm_test_registry=registry;
    assert(mm_render_available_modes()==((1u<<MM_MODE_COUNT)-1));
    assert(mm_render_available_modes()==((1u<<MM_MODE_COUNT)-1));
    const MmMode trips[]={MM_NATIVE_2D,MM_HIGH_RES_2D,MM_ORIGINAL,MM_SUPERSAMPLE_3D,
                          MM_HIGH_RES_2D,MM_SUPERSAMPLE_2D,MM_ORIGINAL,MM_NATIVE_2D};
    for(unsigned n=0;n<sizeof(trips)/sizeof(trips[0]);n++) for(unsigned on=0;on<3;on++) {
        MmMode mode=trips[n];bool stereo=on==1 && (mode==MM_ORIGINAL || mode==MM_SUPERSAMPLE_3D);
        float v=on?0.75f:0;memcpy(hid+0x80,&v,4);hid[0x84]=on==2;
        MmProfile profile;assert(mm_profile(mode,stereo,&profile));
        void *candidate=0;assert(mm_render_build(0,&profile,&candidate) && candidate && mm_render_commit(0,candidate));
        mm_native_options.active=mode;assert(mm_render_stereo()==stereo);
        unsigned record_width=profile.output_width;
        assert(mm_native_options.active==mode && mm_scene_width==profile.scene_width && mm_scene_height==profile.scene_height);
        assert(lcd_mode==(profile.wide_output?0x701u:profile.stereo_allowed?0x702u:0x700u));
        for(unsigned i=0;i<4;i++) {
            assert(get(records[i],0x10)==record_width);
            assert(get(records[i],4)==0x18000000+i*0x100000 && get(records[i],0xc)==240);
            assert(get(records[i],0)==ids[i] && get(records[i],8)==0x8051 && get(records[i],0x14)==0x10000);
        }
        assert(get(registry,0x124)==0x12345678); /* no current-binding side effect */
        assert(get(display,0x40)==profile.output_width && get(display,0x6c)==profile.output_width);
        assert(!memcmp(bottom,display+0x7c,sizeof(bottom)));
        expected_flags=profile.resolve==MM_SCALE_HALF_XY?0x502:profile.resolve==MM_SCALE_HALF_Y?0x501:0x500;
        mm_render_transfer(display+0x24,0xdead,3,7,11);
        mm_render_transfer(display+0x50,0xdead,3,7,11);
        expected_secondary=stereo?owner:display;
        mm_render_present(0,1,display,owner,720,0x141,7);
        expected_secondary=owner;
        mm_render_present(1,1,display,owner,720,0x141,7); /* bottom untouched */
        /* Reject inconsistent mode/scene/resolve combinations before mutation. */
        profile.scene_width++;assert(!mm_render_build(0,&profile,&candidate) && !candidate);
    }
    staging_ready=false;
    MmProfile unavailable;void *no_staging=(void *)0x123;
    assert(mm_profile(MM_SUPERSAMPLE_2D,false,&unavailable));
    assert(!mm_render_build(0,&unavailable,&no_staging) && !no_staging);
    staging_ready=true;
    /* Missing record rejects the narrower mode before any metadata changes. */
    word(display,0x58,9999);
    uint8_t before[sizeof(records)];memcpy(before,records,sizeof(before));
    MmProfile ss;assert(mm_profile(MM_SUPERSAMPLE_2D,false,&ss));
    void *bad=(void *)0x123;assert(!mm_render_build(0,&ss,&bad) && !bad);
    assert(!memcmp(before,records,sizeof(before)) && mm_render_available_modes()==(1u<<MM_HIGH_RES_2D));
    puts("PASS: isolated LCD/resolve diagnostic paths and remaining profiles; slider on/off/disabled transitions; complete registered backing required; four-record width sync; High-res round trips; no VRAM/height/bottom/binding changes; malformed/missing record rejection.");
}
