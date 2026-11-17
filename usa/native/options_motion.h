#ifndef MM_OPTIONS_MOTION_H
#define MM_OPTIONS_MOTION_H
#include <stdbool.h>
typedef struct {
    float incoming_y,outgoing_y,alpha[3];
    bool busy,exiting,finished,to_first;
} MmOptionsMotion;
bool mm_motion_open(void *resources);
void mm_motion_close(void *resources);
bool mm_motion_exit(bool to_first);
void mm_motion_tick(float dt);
const MmOptionsMotion *mm_motion_state(void);
#endif
