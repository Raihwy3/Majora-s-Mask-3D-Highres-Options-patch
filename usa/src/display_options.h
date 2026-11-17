#ifndef MM_DISPLAY_OPTIONS_H
#define MM_DISPLAY_OPTIONS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    MM_ORIGINAL,
    MM_NATIVE_2D,
    MM_HIGH_RES_2D,
    MM_SUPERSAMPLE_2D,
    MM_SUPERSAMPLE_3D,
    MM_MODE_COUNT
} MmMode;

typedef enum { MM_SCALE_NONE, MM_SCALE_HALF_Y, MM_SCALE_HALF_XY } MmScale;

/* Logical, upright dimensions. A native adapter must rotate these for PICA.
 * These are desired profiles, not proof that a native renderer supports them.
 * One scene target may be reused for the two sequential eye passes. */
typedef struct {
    MmMode mode;
    uint16_t scene_width, scene_height;
    uint16_t output_width, output_height;
    uint16_t bottom_width, bottom_height;
    uint8_t eye_passes;
    uint8_t eye_outputs;
    bool wide_output;
    bool stereo;
    bool stereo_allowed;
    MmScale resolve;
} MmProfile;

typedef enum {
    MM_UI_CLOSED,
    MM_UI_PAUSE,
    MM_UI_GRAPHICS_PAUSE,
    MM_UI_GRAPHICS_OPTIONS
} MmUi;

typedef enum {
    MM_OK,
    MM_UNAVAILABLE,
    MM_WAIT_FAILED,
    MM_BUILD_FAILED,
    MM_COMMIT_FAILED,
    MM_SAVE_FAILED,
    MM_NOT_PAUSED
} MmResult;

enum {
    MM_KEY_A = 1, MM_KEY_B = 2, MM_KEY_START = 8,
    MM_KEY_RIGHT = 16, MM_KEY_LEFT = 32, MM_KEY_UP = 64,
    MM_KEY_DOWN = 128, MM_KEY_R = 256, MM_KEY_L = 512
};

/* The native adapter owns GPU resources and native pause state. No callbacks
 * here may change fixed gameplay speed or overwrite Restoration's bindings.
 * begin_pause saves the exact native state; end_pause restores that token.
 * build creates a complete candidate without changing current resources.
 * commit must leave current resources intact on failure. On success it owns
 * candidate, changes all related renderer state, then retires old resources.
 * discard is called only for a candidate that has not been committed. */
typedef struct {
    void *user;
    uint32_t available_modes;
    bool (*begin_pause)(void *user, uintptr_t *token);
    void (*end_pause)(void *user, uintptr_t token);
    bool (*wait_gpu)(void *user);
    bool (*build)(void *user, const MmProfile *profile, void **candidate);
    bool (*commit)(void *user, void *candidate);
    void (*discard)(void *user, void *candidate);
    bool (*save)(void *user, const uint8_t *bytes, size_t size);
    bool (*open_options)(void *user);
    bool (*open_notebook)(void *user);
} MmBackend;

typedef struct {
    MmBackend backend;
    MmMode active, selected;
    MmUi ui;
    MmResult result;
    uint8_t pause_row;
    uint8_t graphics_row, strength_index, sensitivity_index;
    uint32_t previous_buttons, suppress_until_release;
    uintptr_t pause_token;
    bool owns_pause, unsaved;
} MmOptions;

bool mm_profile(MmMode mode, bool slider_on, MmProfile *out);
unsigned mm_options_sensitivity_percent(const MmOptions *state);
const char *mm_options_sensitivity_label(const MmOptions *state);
MmResult mm_options_save(MmOptions *state);
unsigned mm_options_strength_percent(const MmOptions *state);
const char *mm_options_strength_label(const MmOptions *state);
const char *mm_mode_label(MmMode mode);
const char *mm_mode_description(MmMode mode);
const char *mm_result_text(MmResult result);
void mm_options_init(MmOptions *state, const MmBackend *backend, MmMode active);
/* Native options adapter calls enter only after its own menu has paused play.
 * The core must not unpause a pause that belongs to the original menu. */
void mm_options_enter_graphics_page(MmOptions *state);
void mm_options_leave_graphics_page(MmOptions *state);
/* Called at the paused frame boundary, never from the raw HID interrupt. */
MmResult mm_options_apply(MmOptions *state, bool slider_on);
/* Returns the keys the adapter must consume. Gameplay=false covers loading,
 * cutscenes, title, notebook, instrument and other native menu contexts. */
uint32_t mm_options_input(MmOptions *state, uint32_t held, bool gameplay,
                          bool slider_on);

#define MM_SETTINGS_SIZE 16
typedef struct { MmMode mode; uint8_t strength, sensitivity, sequence; } MmSettings;
bool mm_settings_encode_all(const MmSettings *settings, uint8_t out[MM_SETTINGS_SIZE]);
bool mm_settings_decode_all(const uint8_t *data, size_t size, MmSettings *out);
bool mm_settings_encode(MmMode mode, uint8_t out[MM_SETTINGS_SIZE]);
bool mm_settings_decode(const uint8_t *data, size_t size, MmMode *out);

#endif
