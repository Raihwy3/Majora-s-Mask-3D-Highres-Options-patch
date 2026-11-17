#include "display_options.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    bool pause_fail, wait_fail, build_fail, partial_build, commit_fail, save_fail;
    bool handoff_fail, paused;
    unsigned resumes, waits, builds, commits, discards, saves, notebooks, options;
    int candidate;
    MmProfile current;
    uint8_t saved[MM_SETTINGS_SIZE];
    char events[256];
    unsigned event_count;
} Fake;

static void event(Fake *f, char c) { assert(f->event_count+1<sizeof(f->events));f->events[f->event_count++] = c; }
static bool pause_game(void *u, uintptr_t *token) {
    Fake *f = u;
    if (f->pause_fail) return false;
    assert(!f->paused);
    f->paused = true;
    *token = 0x579;
    return true;
}
static void resume(void *u, uintptr_t token) {
    Fake *f = u;
    assert(token == 0x579 && f->paused);
    f->paused = false;
    ++f->resumes;
}
static bool wait_gpu(void *u) {
    Fake *f = u;
    assert(f->paused); ++f->waits; event(f, 'W');
    return !f->wait_fail;
}
static bool build(void *u, const MmProfile *p, void **candidate) {
    Fake *f = u;
    assert(f->paused); ++f->builds; event(f, 'B');
    assert(p->bottom_width == 320 && p->bottom_height == 240);
    if (!f->build_fail || f->partial_build) *candidate = &f->candidate;
    if (f->build_fail) return false;
    /* Candidate construction must not update live state. */
    f->candidate = (int)p->mode;
    return true;
}
static bool commit(void *u, void *candidate) {
    Fake *f = u;
    assert(f->paused && candidate == &f->candidate);
    ++f->commits; event(f, 'C');
    if (f->commit_fail) return false;
    assert(mm_profile((MmMode)f->candidate, false, &f->current));
    return true;
}
static void discard(void *u, void *candidate) {
    Fake *f = u;
    assert(candidate == &f->candidate);
    ++f->discards; event(f, 'D');
}
static bool save(void *u, const uint8_t *p, size_t size) {
    Fake *f = u;
    assert(size == MM_SETTINGS_SIZE);
    ++f->saves; event(f, 'S');
    if (f->save_fail) return false;
    memcpy(f->saved, p, size);
    return true;
}
static bool notebook(void *u) {
    Fake *f = u;
    ++f->notebooks;
    return !f->handoff_fail;
}
static bool options(void *u) {
    Fake *f = u;
    ++f->options;
    return !f->handoff_fail;
}
static MmOptions init(Fake *f) {
    memset(f, 0, sizeof(*f));
    MmBackend b = {f, (1u << MM_MODE_COUNT) - 1, pause_game, resume, wait_gpu,
                   build, commit, discard, save, options, notebook};
    MmOptions s;
    mm_options_init(&s, &b, MM_ORIGINAL);
    return s;
}
static uint32_t press(MmOptions *s, uint32_t buttons) {
    (void)mm_options_input(s, 0, true, false);
    return mm_options_input(s, buttons, true, false);
}
static void graphics(MmOptions *s) {
    assert(press(s, MM_KEY_START) & MM_KEY_START);
    press(s, MM_KEY_DOWN);
    press(s, MM_KEY_A);
    assert(s->ui == MM_UI_GRAPHICS_PAUSE && s->owns_pause);
}

static void test_profiles(void) {
    for (MmMode mode = MM_ORIGINAL; mode < MM_MODE_COUNT; ++mode) {
        for (unsigned slider = 0; slider < 2; ++slider) {
            MmProfile p;
            assert(mm_profile(mode, slider != 0, &p));
            assert(p.mode == mode && p.bottom_width == 320 && p.bottom_height == 240);
            unsigned sx = p.resolve == MM_SCALE_HALF_XY ? 2 : 1;
            unsigned sy = p.resolve == MM_SCALE_NONE ? 1 : 2;
            assert(p.scene_width == p.output_width * sx);
            assert(p.scene_height == p.output_height * sy);
            bool stereo_mode = mode == MM_ORIGINAL || mode == MM_SUPERSAMPLE_3D;
            assert(p.stereo_allowed == stereo_mode);
            assert(p.eye_outputs == (stereo_mode ? 2 : 1));
            assert(p.stereo == (stereo_mode && slider));
            assert(p.eye_passes == (p.stereo ? 2 : 1));
            assert(p.wide_output == (mode == MM_HIGH_RES_2D));
            if(mode==MM_SUPERSAMPLE_3D) {
                assert(p.scene_width==400 && p.scene_height==480);
                assert(p.output_width==400 && p.output_height==240);
                assert(p.resolve==MM_SCALE_HALF_Y);
            }
        }
    }
    MmProfile p;
    assert(!mm_profile((MmMode)-1, true, &p));
    assert(!mm_profile(MM_MODE_COUNT, true, &p));
    assert(!mm_profile(MM_ORIGINAL, true, NULL));
}

static void test_settings(void) {
    for (MmMode mode = MM_ORIGINAL; mode < MM_MODE_COUNT; ++mode) {
        uint8_t p[MM_SETTINGS_SIZE]; MmMode out = MM_MODE_COUNT;
        assert(mm_settings_encode(mode, p));
        assert(mm_settings_decode(p, sizeof(p), &out) && out == mode);
        for (unsigned byte = 0; byte < sizeof(p); ++byte) {
            for (unsigned bit = 0; bit < 8; ++bit) {
                p[byte] ^= (uint8_t)(1u << bit);
                out = MM_MODE_COUNT;
                assert(!mm_settings_decode(p, sizeof(p), &out));
                assert(out == MM_MODE_COUNT);
                p[byte] ^= (uint8_t)(1u << bit);
            }
        }
        for (size_t size = 0; size < sizeof(p); ++size)
            assert(!mm_settings_decode(p, size, &out));
        assert(!mm_settings_decode(p, sizeof(p) + 1, &out));
    }
}

static void test_pause_input(void) {
    Fake f; MmOptions s = init(&f);
    assert(mm_options_input(&s, MM_KEY_START, false, false) == 0);
    assert(s.ui == MM_UI_CLOSED && !f.paused);
    (void)mm_options_input(&s, 0, true, false);
    f.pause_fail = true;
    assert(mm_options_input(&s, MM_KEY_START, true, false) == 0);
    assert(s.ui == MM_UI_CLOSED);
    f.pause_fail = false;
    assert(press(&s, MM_KEY_START) == MM_KEY_START);
    assert(s.ui == MM_UI_PAUSE && f.paused);
    mm_options_input(&s, MM_KEY_START, true, false);
    assert(s.ui == MM_UI_PAUSE); /* Opening key cannot also close. */
    press(&s, MM_KEY_A);
    assert(s.ui == MM_UI_CLOSED && f.resumes == 1);
    assert(mm_options_input(&s, MM_KEY_A, true, false) & MM_KEY_A);
    assert(mm_options_input(&s, 0, true, false) == 0);
    assert(mm_options_apply(&s, false) == MM_NOT_PAUSED);

    s = init(&f); graphics(&s);
    press(&s, MM_KEY_LEFT);
    assert(s.selected == MM_SUPERSAMPLE_3D);
    press(&s, MM_KEY_RIGHT);
    assert(s.selected == MM_ORIGINAL);
    press(&s, MM_KEY_B);
    assert(s.ui == MM_UI_PAUSE && f.paused);
    press(&s, MM_KEY_START);
    assert(s.ui == MM_UI_CLOSED && !f.paused);
}

static void test_switching(void) {
    for (MmMode mode = MM_NATIVE_2D; mode < MM_MODE_COUNT; ++mode) {
        Fake f; MmOptions s = init(&f); graphics(&s);
        s.selected = mode;
        assert(mm_options_apply(&s, false) == MM_OK);
        assert(s.active == mode && !s.unsaved);
        assert(strcmp(f.events, "WBCS") == 0);
        MmMode loaded;
        assert(mm_settings_decode(f.saved, sizeof(f.saved), &loaded) && loaded == mode);
        s.selected = MM_ORIGINAL;
        assert(mm_options_apply(&s, false) == MM_OK && s.active == MM_ORIGINAL);
        assert(f.discards == 0 && f.commits == 2);
    }
    for (unsigned fail = 0; fail < 5; ++fail) {
        Fake f; MmOptions s = init(&f); graphics(&s);
        s.selected = MM_HIGH_RES_2D;
        if (fail == 0) s.backend.available_modes = 1u << MM_ORIGINAL;
        if (fail == 1) f.wait_fail = true;
        if (fail == 2) f.build_fail = true;
        if (fail == 3) f.build_fail = f.partial_build = true;
        if (fail == 4) f.commit_fail = true;
        assert(mm_options_apply(&s, false) != MM_OK);
        assert(s.active == MM_ORIGINAL && f.saves == 0 && f.paused);
        assert(f.discards == (fail >= 3 ? 1u : 0u));
        assert(f.commits == (fail == 4 ? 1u : 0u));
    }
    Fake f; MmOptions s = init(&f); graphics(&s);
    s.selected = MM_NATIVE_2D; f.save_fail = true;
    assert(mm_options_apply(&s, false) == MM_SAVE_FAILED);
    assert(s.active == MM_NATIVE_2D && s.unsaved && f.commits == 1);
    f.save_fail = false;
    assert(mm_options_apply(&s, false) == MM_OK);
    assert(!s.unsaved && f.commits == 1 && f.saves == 2);
}

static void test_handoffs(void) {
    for (unsigned row = 2; row <= 3; ++row) {
        Fake f; MmOptions s = init(&f);
        press(&s, MM_KEY_START); s.pause_row = (uint8_t)row;
        f.handoff_fail = true;
        press(&s, MM_KEY_A);
        assert(s.ui == MM_UI_PAUSE && s.owns_pause);
        f.handoff_fail = false;
        press(&s, MM_KEY_A);
        assert(s.ui == MM_UI_CLOSED && !s.owns_pause && f.resumes == 0);
        assert((row == 2 ? f.options : f.notebooks) == 2);
    }
    Fake f; MmOptions s = init(&f);
    f.paused = true; /* Original Options menu owns this pause. */
    mm_options_enter_graphics_page(&s);
    s.selected = MM_HIGH_RES_2D;
    assert(mm_options_apply(&s, false) == MM_OK);
    press(&s, MM_KEY_B);
    assert(s.ui == MM_UI_CLOSED && f.paused && f.resumes == 0);
}

static void test_strength_input(void) {
    Fake f;MmOptions s=init(&f);
    assert(mm_options_strength_percent(&s)==100);
    assert(!strcmp(mm_options_strength_label(&s),"Normal"));
    graphics(&s);
    press(&s,MM_KEY_DOWN);
    assert(s.graphics_row==1);
    MmMode before=s.selected;
    press(&s,MM_KEY_RIGHT);
    assert(mm_options_strength_percent(&s)==125 && s.selected==before);
    press(&s,MM_KEY_A);
    assert(!f.builds && !f.commits); /* strength never rebuilds targets */
    for(unsigned i=0;i<20;i++) press(&s,MM_KEY_RIGHT);
    assert(mm_options_strength_percent(&s)==320);
    for(unsigned i=0;i<20;i++) press(&s,MM_KEY_LEFT);
    assert(mm_options_strength_percent(&s)==0);
    assert(!strcmp(mm_options_strength_label(&s),"Off"));
    press(&s,MM_KEY_UP);assert(!s.graphics_row);
    press(&s,MM_KEY_RIGHT);assert(s.selected!=before);
    press(&s,MM_KEY_B);assert(s.ui==MM_UI_PAUSE);
    press(&s,MM_KEY_B);
    assert(s.ui==MM_UI_CLOSED && f.resumes==1);
    mm_options_enter_graphics_page(&s);
    assert(!s.graphics_row && mm_options_strength_percent(&s)==0);
}
static void test_all_settings(void) {
    for(unsigned mode=0;mode<MM_MODE_COUNT;mode++) for(unsigned depth=0;depth<10;depth++)
    for(unsigned look=0;look<7;look++) {
        MmSettings original={(MmMode)mode,depth,look,255},decoded={MM_MODE_COUNT,99,99,99};
        uint8_t bytes[16];assert(mm_settings_encode_all(&original,bytes));
        assert(mm_settings_decode_all(bytes,sizeof(bytes),&decoded));
        assert(decoded.mode==original.mode && decoded.strength==depth && decoded.sensitivity==look && decoded.sequence==255);
        for(unsigned i=0;i<16;i++) {
            bytes[i]^=0x80;assert(!mm_settings_decode_all(bytes,sizeof(bytes),&decoded));bytes[i]^=0x80;
        }
    }
    Fake f;MmOptions s=init(&f);graphics(&s);
    assert(mm_options_sensitivity_percent(&s)==100);
    press(&s,MM_KEY_UP);assert(s.graphics_row==2); /* wrap from first to last */
    press(&s,MM_KEY_RIGHT);assert(s.sensitivity_index==3 && f.saves==1);
    MmSettings decoded;assert(mm_settings_decode_all(f.saved,sizeof(f.saved),&decoded));
    assert(decoded.sensitivity==3 && decoded.strength==3 && decoded.mode==MM_ORIGINAL);
    unsigned count=f.saves;s.suppress_until_release=0;s.previous_buttons=0;
    mm_options_input(&s,MM_KEY_RIGHT,false,false);
    mm_options_input(&s,MM_KEY_RIGHT,false,false);assert(f.saves==count+1); /* held key saves once */
    f.save_fail=true;press(&s,MM_KEY_RIGHT);assert(s.unsaved && s.result==MM_SAVE_FAILED);
    f.save_fail=false;press(&s,MM_KEY_A);assert(!s.unsaved && s.result==MM_OK);
    unsigned look=s.sensitivity_index;press(&s,MM_KEY_UP);assert(s.graphics_row==1);
    press(&s,MM_KEY_RIGHT);assert(s.sensitivity_index==look && s.strength_index==4);
    assert(mm_settings_decode_all(f.saved,sizeof(f.saved),&decoded));
    assert(decoded.sensitivity==look && decoded.strength==4);
    press(&s,MM_KEY_DOWN);press(&s,MM_KEY_DOWN);assert(s.graphics_row==0);
    assert(!f.builds && !f.commits);
}
int main(void) {
    test_profiles(); test_settings(); test_pause_input(); test_switching(); test_handoffs();
    test_strength_input();test_all_settings();
    puts("PASS: profiles; 640 corrupt-settings cases; input consumption; transactional rollback; native-menu pause ownership.");
    return 0;
}
