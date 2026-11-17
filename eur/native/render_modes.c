/* All output modes share retained maximum backing storage. Logical scene
 * rectangles vary independently from physical texture-copy dimensions.
 * Native scanout records and display widths must change together. */
#include "display_options.h"
#include <string.h>
extern MmOptions mm_native_options;
extern bool mm_output_buffers_ready(void *);
extern bool mm_output_buffers_width(void *,unsigned);
extern void mm_game_set_display_mode(unsigned mode);
extern void mm_game_surface_transfer(void *, unsigned, int, int, unsigned);

volatile uint32_t mm_scene_width = 800, mm_scene_height = 480;
/* Native camera default is10.0; the UI adjusts this owned data value. */
float mm_stereo_depth=10.0f;
static MmProfile candidate;
static MmMode render_mode = MM_HIGH_RES_2D;
static bool slider;
extern void mm_game_present(unsigned,unsigned,void *,void *,unsigned,unsigned,unsigned);
/* This receives the exact final native LCD submission, after all native
 * pointer/format calculations. Keep both addresses valid for mono output. */
void mm_render_present(unsigned screen,unsigned index,void *left,void *right,
                       unsigned stride,unsigned format,unsigned status) {
    if(screen==0) {
        if(!slider || render_mode==MM_NATIVE_2D || render_mode==MM_HIGH_RES_2D || render_mode==MM_SUPERSAMPLE_2D)
            right=left;
    }
    mm_game_present(screen,index,left,right,stride,format,status);
}
#ifdef MM_NATIVE_HOST_TEST
extern uint8_t *mm_test_hid, *mm_test_owner;
#define HID_BYTES mm_test_hid
#define OWNER_ADDRESS mm_test_owner
#else
#define HID_BYTES ((volatile const uint8_t *)0x1ff81000)
/* Native 0x105b6c uses this address directly as r4; it is a static
 * owner structure, not a global pointer. Dereferencing it reads its allocator. */
#define OWNER_ADDRESS ((uint8_t *)0x6a3a48)
#endif

static uint8_t *owner_pointer(void) { return OWNER_ADDRESS; }

static uint8_t *display_pointer(uint8_t *owner) {
    uint8_t *display = 0;
    if (owner) memcpy(&display, owner + 4, sizeof(display));
    return display;
}
extern bool mm_resolve_prepare(void);
static void frame_profile(MmMode mode,bool slider_on,MmProfile *p) {
    (void)mm_profile(mode,slider_on,p);
}
static bool supported(MmMode mode) {return (unsigned)mode<MM_MODE_COUNT;}
uint32_t mm_render_available_modes(void) {
    uint32_t modes=1u<<MM_HIGH_RES_2D;
    if(mm_output_buffers_ready(display_pointer(owner_pointer()))) modes=(1u<<MM_MODE_COUNT)-1;
    return modes;
}
bool mm_render_wait(void *user) {
    (void)user;
    /* Immutable maximum backing allocations; commit changes no GPU payload,
       submits no frees, and is consumed at the next stereo-query boundary. */
    return true;
}
bool mm_render_build(void *user, const MmProfile *profile, void **out) {
    (void)user;
    if(out) *out=0;
    if(!out || !profile || !supported(profile->mode)) return false;
    MmProfile expected;
    if(!mm_profile(profile->mode,profile->stereo,&expected)) return false;
    if(profile->scene_width!=expected.scene_width || profile->scene_height!=expected.scene_height ||
       profile->output_width!=expected.output_width || profile->output_height!=expected.output_height ||
       profile->bottom_width!=expected.bottom_width || profile->bottom_height!=expected.bottom_height ||
       profile->resolve!=expected.resolve || profile->wide_output!=expected.wide_output ||
       profile->stereo_allowed!=expected.stereo_allowed || profile->stereo!=expected.stereo ||
       profile->eye_passes!=expected.eye_passes || profile->eye_outputs!=expected.eye_outputs) return false;
    if(profile->mode!=MM_HIGH_RES_2D &&
       !mm_output_buffers_ready(display_pointer(owner_pointer()))) return false;
    if(profile->mode!=MM_HIGH_RES_2D && !mm_resolve_prepare()) return false;
    candidate = *profile;
    *out = &candidate;
    return true;
}
bool mm_render_commit(void *user, void *value) {
    (void)user;
    uint8_t *owner = owner_pointer();
    return value == &candidate && display_pointer(owner);
    /* Core active mode is queued; consume it at the next stereo query. */
}
void mm_render_discard(void *user, void *value) { (void)user; (void)value; }

/* Native 0x61be64 originally reads these exact MCU/HID shared-memory fields. */
int mm_render_stereo(void) {
    mm_stereo_depth=(float)mm_options_strength_percent(&mm_native_options)*0.1f;
    volatile const uint8_t *hid = HID_BYTES;
    slider = !hid[0x84] && *(volatile const float *)(hid + 0x80) > 0.0f;
    mm_native_options.backend.available_modes=mm_render_available_modes();
    render_mode = mm_native_options.active;
    if(!supported(render_mode) || !(mm_native_options.backend.available_modes&(1u<<render_mode))) {
        render_mode=MM_HIGH_RES_2D;
        mm_native_options.active=render_mode;
        mm_native_options.result=MM_UNAVAILABLE;
    }
    MmProfile p;
    frame_profile(render_mode,slider,&p);
    mm_scene_width = p.scene_width;
    mm_scene_height = p.scene_height;
    mm_game_set_display_mode(p.wide_output ? 0x701 : p.stereo_allowed ? 0x702 : 0x700);
    /* Both physical backing surfaces remain at the maximum width allocated at
       startup. Synchronize logical output widths and native scanout metadata. */
    uint8_t *owner = owner_pointer();
    if (owner) {
        uint8_t *display = display_pointer(owner);
        if (display) {
            unsigned record_width=p.output_width;
            if(!mm_output_buffers_width(display,record_width)) {
                /* Before registration exists, only the 800-wide boot path
                   can be active. Never invent a narrower allocation. */
                *(uint32_t *)(display+0x40)=800;
                *(uint32_t *)(display+0x6c)=800;
            } else {
                *(uint32_t *)(display+0x40)=p.output_width;
                *(uint32_t *)(display+0x6c)=p.output_width;
            }
        }
    }
    return p.stereo;
}
void mm_render_transfer(void *surface, unsigned flags, int x, int y, unsigned arg) {
    MmProfile p;
    frame_profile(render_mode,slider,&p);
    flags = p.resolve == MM_SCALE_HALF_XY ? 0x502 :
            p.resolve == MM_SCALE_HALF_Y ? 0x501 : 0x500;
    mm_game_surface_transfer(surface, flags, x, y, arg);
}

