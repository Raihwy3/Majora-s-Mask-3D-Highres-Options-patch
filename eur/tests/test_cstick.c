#include "display_options.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
MmOptions mm_native_options;
static int calibration;
float mm_game_stick_normalize(void *c,int v) {
    assert(c==&calibration);
    /* Exact native12e904 behavior for the standard105-unit calibration. */
    if(!v) return 0;
    return v>=105?1.0f:(float)v/105.0f;
}
extern float mm_cstick_speed(void *,int);
int main(void) {
    const unsigned gain[]={50,75,100,125,150,175,200};
    for(unsigned i=0;i<7;i++) {
        mm_native_options.sensitivity_index=i;
        for(int v=-210;v<=210;v++) {
            float native=mm_game_stick_normalize(&calibration,v);
            float got=mm_cstick_speed(&calibration,v);
            float expected=native*((float)gain[i]/100.0f);
            assert(fabsf(got-expected)<0.000001f);
            if(gain[i]==100) assert(got==native);
        }
        /* Same full-deflection multiplier for left/right and up/down. */
        assert(mm_cstick_speed(&calibration,105)==-mm_cstick_speed(&calibration,-105));
        if(gain[i]>100) assert(mm_cstick_speed(&calibration,105)>1);
        assert(mm_cstick_speed(&calibration,0)==0);
    }
    puts("PASS: post-normalization speed gain; both signs and axes; positive cap regression; Normal identity");
}
