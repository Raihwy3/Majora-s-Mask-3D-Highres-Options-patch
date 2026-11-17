/* Secondary-stick floating-point conversion calls at116748 and11675c.
 * The game's positive-only1.0 cap must run BEFORE our multiplier. Scaling
 * int16 inputs earlier lets negative movement grow while positive saturates.
 * Leave calibration, raw input, primary stick and normalization unchanged. */
#include "display_options.h"
extern MmOptions mm_native_options;
extern float mm_game_stick_normalize(void *,int);
float mm_cstick_speed(void *calibration,int value) {
    float normalized=mm_game_stick_normalize(calibration,value);
    unsigned speed=mm_options_sensitivity_percent(&mm_native_options);
    if(speed==100) return normalized;
    return normalized*((float)speed/100.0f);
}
