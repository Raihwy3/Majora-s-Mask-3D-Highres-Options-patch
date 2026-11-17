#include "display_options.h"
#include <string.h>

static bool valid(MmMode mode) { return (unsigned)mode < MM_MODE_COUNT; }
static const unsigned strengths[]={0,50,75,100,125,150,175,200,250,320};
static const char *const strength_names[]={"Off","Very low","Low","Normal","Higher","High","Very high","Extra high","Extreme","Maximum"};
const char *mm_options_strength_label(const MmOptions *s) {
    return strength_names[s && s->strength_index<10?s->strength_index:3];
}
unsigned mm_options_strength_percent(const MmOptions *s) {
    return s && s->strength_index<sizeof(strengths)/sizeof(strengths[0]) ?
        strengths[s->strength_index] : 100;
}
static void step_strength(MmOptions *s,int direction) {
    unsigned n=sizeof(strengths)/sizeof(strengths[0]);
    if(s->strength_index>=n) s->strength_index=3;
    if(direction>0 && s->strength_index+1u<n) ++s->strength_index;
    else if(direction<0 && s->strength_index) --s->strength_index;
}

static const unsigned sensitivities[]={50,75,100,125,150,175,200};
static const char *const sensitivity_names[]={"Very low","Low","Normal","Higher","High","Very high","Maximum"};
unsigned mm_options_sensitivity_percent(const MmOptions *s) {
    return sensitivities[s && s->sensitivity_index<7?s->sensitivity_index:2];
}
const char *mm_options_sensitivity_label(const MmOptions *s) {
    return sensitivity_names[s && s->sensitivity_index<7?s->sensitivity_index:2];
}
static void step_sensitivity(MmOptions *s,int d) {
    if(s->sensitivity_index>=7) s->sensitivity_index=2;
    if(d>0 && s->sensitivity_index<6) ++s->sensitivity_index;
    else if(d<0 && s->sensitivity_index) --s->sensitivity_index;
}
MmResult mm_options_save(MmOptions *s) {
    if(!s->unsaved) return s->result=MM_OK;
    uint8_t bytes[MM_SETTINGS_SIZE];
    MmSettings settings={s->active,s->strength_index,s->sensitivity_index,0};
    if(!mm_settings_encode_all(&settings,bytes) || !s->backend.save ||
       !s->backend.save(s->backend.user,bytes,sizeof(bytes))) return s->result=MM_SAVE_FAILED;
    s->unsaved=false;
    return s->result=MM_OK;
}

bool mm_profile(MmMode mode, bool slider_on, MmProfile *out) {
    if (!out || !valid(mode)) return false;
    MmProfile p = {0};
    p.mode = mode;
    p.scene_width = p.output_width = 400;
    p.scene_height = p.output_height = p.bottom_height = 240;
    p.bottom_width = 320;
    p.eye_passes = p.eye_outputs = 1;
    switch (mode) {
    case MM_ORIGINAL:
        p.stereo_allowed = true;
        p.eye_outputs = 2;
        p.stereo = slider_on;
        p.eye_passes = slider_on ? 2 : 1;
        if (!slider_on) { p.scene_height = 480; p.resolve = MM_SCALE_HALF_Y; }
        break;
    case MM_NATIVE_2D: break;
    case MM_HIGH_RES_2D:
        p.scene_width = p.output_width = 800;
        p.scene_height = 480;
        p.wide_output = true;
        p.resolve = MM_SCALE_HALF_Y;
        break;
    case MM_SUPERSAMPLE_2D:
    case MM_SUPERSAMPLE_3D:
        p.scene_width = mode == MM_SUPERSAMPLE_3D ? 400 : 800;
        p.scene_height = 480;
        p.resolve = mode == MM_SUPERSAMPLE_3D ? MM_SCALE_HALF_Y : MM_SCALE_HALF_XY;
        p.stereo_allowed = mode == MM_SUPERSAMPLE_3D;
        p.eye_outputs = p.stereo_allowed ? 2 : 1;
        p.stereo = mode == MM_SUPERSAMPLE_3D && slider_on;
        p.eye_passes = p.stereo ? 2 : 1;
        break;
    default: return false;
    }
    *out = p;
    return true;
}

const char *mm_mode_label(MmMode mode) {
    static const char *const labels[] = {
        "Original", "Performance 2D", "High-resolution 2D",
        "Supersampling 2D", "Supersampling + 3D"
    };
    return valid(mode) ? labels[mode] : "Unknown";
}

const char *mm_mode_description(MmMode mode) {
    static const char *const text[] = {
        "Original game rendering and 3D slider behavior.",
        "Original screen resolution. One view, without supersampling.",
        "Wide high-resolution output. 3D is disabled.",
        "Higher-resolution rendering reduced to the normal screen size.",
        "Supersampling for each eye. Use the 3D slider for depth."
    };
    return valid(mode) ? text[mode] : "Unavailable";
}

const char *mm_result_text(MmResult result) {
    switch (result) {
    case MM_OK: return "";
    case MM_UNAVAILABLE: return "This mode is not available in this build.";
    case MM_WAIT_FAILED: return "Could not finish the current frame. Mode unchanged.";
    case MM_BUILD_FAILED: return "Could not prepare this mode. Mode unchanged.";
    case MM_COMMIT_FAILED: return "Could not switch modes. Previous mode retained.";
    case MM_SAVE_FAILED: return "Mode applied, but the setting could not be saved.";
    case MM_NOT_PAUSED: return "Open Graphics from a paused options menu.";
    default: return "Could not change the graphics setting.";
    }
}

void mm_options_init(MmOptions *s, const MmBackend *backend, MmMode active) {
    memset(s, 0, sizeof(*s));
    if (backend) s->backend = *backend;
    s->active = s->selected = valid(active) ? active : MM_ORIGINAL;
    s->strength_index=3;
    s->sensitivity_index=2;
}

void mm_options_enter_graphics_page(MmOptions *s) {
    if (s->ui != MM_UI_CLOSED) return;
    s->ui = MM_UI_GRAPHICS_OPTIONS;
    s->selected = s->active;
    s->graphics_row=0;
    s->result = MM_OK;
}

void mm_options_leave_graphics_page(MmOptions *s) {
    if (s->ui == MM_UI_GRAPHICS_OPTIONS) s->ui = MM_UI_CLOSED;
}

static bool enabled(const MmOptions *s, MmMode mode) {
    return valid(mode) && (s->backend.available_modes & (1u << mode)) != 0;
}

MmResult mm_options_apply(MmOptions *s, bool slider_on) {
    MmBackend *b = &s->backend;
    if (s->ui != MM_UI_GRAPHICS_OPTIONS &&
        !(s->ui == MM_UI_GRAPHICS_PAUSE && s->owns_pause))
        return s->result = MM_NOT_PAUSED;
    if (!enabled(s, s->selected)) return s->result = MM_UNAVAILABLE;
    bool change = s->selected != s->active;
    if (change) {
        if (!b->wait_gpu || !b->build || !b->commit || !b->discard)
            return s->result = MM_UNAVAILABLE;
        if (!b->wait_gpu(b->user)) return s->result = MM_WAIT_FAILED;
        MmProfile profile;
        if (!mm_profile(s->selected, slider_on, &profile))
            return s->result = MM_UNAVAILABLE;
        void *candidate = NULL;
        if (!b->build(b->user, &profile, &candidate) || !candidate) {
            if (candidate) b->discard(b->user, candidate);
            return s->result = MM_BUILD_FAILED;
        }
        if (!b->commit(b->user, candidate)) {
            b->discard(b->user, candidate);
            return s->result = MM_COMMIT_FAILED;
        }
        s->active = s->selected;
        s->unsaved = true;
    }
    return mm_options_save(s);
}

static void close_pause(MmOptions *s) {
    if (s->owns_pause && s->backend.end_pause)
        s->backend.end_pause(s->backend.user, s->pause_token);
    s->owns_pause = false;
    s->ui = MM_UI_CLOSED;
}

static void step_mode(MmOptions *s, int direction) {
    /* All choices remain inspectable; unavailable modes cannot be applied. */
    s->selected = (MmMode)(((int)s->selected + direction + MM_MODE_COUNT) % MM_MODE_COUNT);
    s->result = MM_OK;
}

uint32_t mm_options_input(MmOptions *s, uint32_t held, bool gameplay,
                          bool slider_on) {
    uint32_t pressed = held & ~s->previous_buttons;
    s->previous_buttons = held;
    s->suppress_until_release &= held;
    uint32_t consumed = s->suppress_until_release;
    pressed &= ~consumed;
    if (s->ui == MM_UI_CLOSED) {
        if (!gameplay || !(pressed & MM_KEY_START)) return consumed;
        if (!s->backend.begin_pause || !s->backend.end_pause ||
            !s->backend.begin_pause(s->backend.user, &s->pause_token))
            return consumed;
        s->owns_pause = true;
        s->ui = MM_UI_PAUSE;
        s->pause_row = 0;
        s->selected = s->active;
        s->result = MM_OK;
        s->suppress_until_release |= held;
        return consumed | held;
    }
    /* Pause/Graphics owns input until closing keys have also been released. */
    consumed |= held;
    if (s->ui == MM_UI_PAUSE) {
        if (pressed & (MM_KEY_B | MM_KEY_START)) {
            close_pause(s);
        } else if (pressed & MM_KEY_UP) {
            s->pause_row = (uint8_t)((s->pause_row + 3) % 4);
        } else if (pressed & MM_KEY_DOWN) {
            s->pause_row = (uint8_t)((s->pause_row + 1) % 4);
        } else if (pressed & MM_KEY_A) {
            switch (s->pause_row) {
            case 0: close_pause(s); break;
            case 1: s->ui = MM_UI_GRAPHICS_PAUSE; s->graphics_row=0; break;
            case 2:
                /* Backend takes over the existing pause on success. It must
                 * preserve native return context. On failure keep our menu. */
                if (s->backend.open_options && s->backend.open_options(s->backend.user)) {
                    s->owns_pause = false;
                    s->ui = MM_UI_CLOSED;
                }
                break;
            case 3:
                if (s->backend.open_notebook && s->backend.open_notebook(s->backend.user)) {
                    s->owns_pause = false;
                    s->ui = MM_UI_CLOSED;
                }
                break;
            }
        }
    } else {
        unsigned old_strength=s->strength_index, old_sensitivity=s->sensitivity_index;
        if (pressed & (MM_KEY_B | MM_KEY_START)) {
            if(s->unsaved) (void)mm_options_save(s);
            if (s->ui == MM_UI_GRAPHICS_PAUSE) {
                s->ui = MM_UI_PAUSE;
                s->selected = s->active;
            } else {
                s->ui = MM_UI_CLOSED;
            }
        } else if (pressed & MM_KEY_UP) s->graphics_row=(s->graphics_row+2)%3;
        else if (pressed & MM_KEY_DOWN) s->graphics_row=(s->graphics_row+1)%3;
        else if (pressed & MM_KEY_LEFT) {
            if(s->graphics_row==1) step_strength(s,-1);
            else if(s->graphics_row==2) step_sensitivity(s,-1); else step_mode(s,-1);
        } else if (pressed & MM_KEY_RIGHT) {
            if(s->graphics_row==1) step_strength(s,1);
            else if(s->graphics_row==2) step_sensitivity(s,1); else step_mode(s,1);
        } else if (pressed & MM_KEY_A) {
            if(!s->graphics_row) (void)mm_options_apply(s, slider_on);
            else (void)mm_options_save(s);
        }
        if(old_strength!=s->strength_index || old_sensitivity!=s->sensitivity_index) {
            s->unsaved=true; (void)mm_options_save(s);
        }
    }
    s->suppress_until_release |= held;
    return consumed;
}

static uint32_t crc32(const uint8_t *p, size_t size) {
    uint32_t c = UINT32_MAX;
    while (size--) {
        c ^= *p++;
        for (unsigned i = 0; i < 8; ++i)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}

static void put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (i * 8));
}
static uint32_t get32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool mm_settings_encode(MmMode mode, uint8_t out[MM_SETTINGS_SIZE]) {
    if (!out || !valid(mode)) return false;
    memset(out, 0, MM_SETTINGS_SIZE);
    memcpy(out, "MMGD", 4);
    out[4] = 1;
    out[6] = MM_SETTINGS_SIZE;
    out[8] = (uint8_t)mode;
    put32(out + 12, crc32(out, 12));
    return true;
}

bool mm_settings_encode_all(const MmSettings *s,uint8_t out[MM_SETTINGS_SIZE]) {
    if(!s || !out || !valid(s->mode) || s->strength>=10 || s->sensitivity>=7) return false;
    memset(out,0,MM_SETTINGS_SIZE);memcpy(out,"MMGD",4);
    out[4]=2;out[6]=MM_SETTINGS_SIZE;out[8]=s->mode;
    out[9]=s->strength;out[10]=s->sensitivity;out[11]=s->sequence;
    put32(out+12,crc32(out,12));return true;
}
bool mm_settings_decode_all(const uint8_t *p,size_t size,MmSettings *out) {
    if(!p || !out || size!=MM_SETTINGS_SIZE || memcmp(p,"MMGD",4) ||
       (p[4]!=1 && p[4]!=2) || p[5] || p[6]!=MM_SETTINGS_SIZE || p[7] ||
       p[8]>=MM_MODE_COUNT || get32(p+12)!=crc32(p,12)) return false;
    if(p[4]==1 && (p[9] || p[10] || p[11])) return false;
    if(p[4]==2 && (p[9]>=10 || p[10]>=7)) return false;
    MmSettings s={(MmMode)p[8],p[4]==1?3:p[9],p[4]==1?2:p[10],p[4]==1?0:p[11]};
    *out=s;return true;
}
bool mm_settings_decode(const uint8_t *p,size_t size,MmMode *out) {
    MmSettings s;
    if(!out || !mm_settings_decode_all(p,size,&s)) return false;
    *out=s.mode;return true;
}
