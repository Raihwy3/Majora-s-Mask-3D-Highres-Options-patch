/* Small two-slot SD journal. Never opens the game's save archive. IPC uses
 * the already initialized native FS session and this thread's command buffer.
 * Every call restores the caller's TLS command words and closes owned handles.
 * Protocol definitions checked against devkitPro libctru services/fs.c. */
#include "display_options.h"
#include <string.h>
extern MmOptions mm_native_options;
extern uint32_t *mm_ipc_buffer(void);
extern int32_t mm_ipc_send(uint32_t);
extern int32_t mm_ipc_close(uint32_t);
#ifdef MM_NATIVE_HOST_TEST
extern uint32_t mm_test_fs_session;
#define FS_SESSION mm_test_fs_session
#else
#define FS_SESSION (*(volatile uint32_t *)0x6a4edc)
#endif
static const char paths[2][80]={
    "/luma/titles/0004000000125500/display-settings-a.bin",
    "/luma/titles/0004000000125500/display-settings-b.bin"
};
#ifdef MM_NATIVE_HOST_TEST
extern uint32_t mm_test_ipc_pointer(const void *);
#define WIRE(p) mm_test_ipc_pointer(p)
#else
#define WIRE(p) ((uint32_t)(uintptr_t)(p))
#endif
static bool loaded;
static int current_slot=-1;
static uint8_t sequence;
static bool pending;
static MmMode pending_mode;
extern uint32_t mm_render_available_modes(void);
extern bool mm_render_build(void *,const MmProfile *,void **);
extern bool mm_render_commit(void *,void *);
extern void mm_render_discard(void *,void *);
static bool request(uint32_t handle) {
    return mm_ipc_send(handle)>=0 && (int32_t)mm_ipc_buffer()[1]>=0;
}
static bool open_file(unsigned slot,bool write,uint32_t *handle) {
    uint32_t *c=mm_ipc_buffer();
    unsigned length=0;while(paths[slot][length]) ++length;++length;
    memset(c,0,13*4);
    c[0]=0x08030204;c[2]=9; /* SDMC */
    c[3]=1;c[4]=1; /* empty archive path */
    c[5]=3;c[6]=length;c[7]=write?6:1; /* ASCII; write/create or read */
    c[9]=(1u<<14)|(2u<<10)|2u;c[10]=WIRE("");
    c[11]=(length<<14)|2u;c[12]=WIRE(paths[slot]);
    if(!request(FS_SESSION)) return false;
    *handle=c[3];return *handle!=0;
}
static bool close_file(uint32_t handle) {
    uint32_t *c=mm_ipc_buffer();c[0]=0x08080000;
    bool ok=request(handle);
    if(mm_ipc_close(handle)<0) ok=false;
    return ok;
}
static bool read_slot(unsigned slot,uint8_t bytes[MM_SETTINGS_SIZE]) {
    uint32_t handle=0,*c=mm_ipc_buffer();bool ok=false;
    if(!open_file(slot,false,&handle)) return false;
    c[0]=0x08040000;
    if(request(handle) && c[2]==MM_SETTINGS_SIZE && !c[3]) {
        c[0]=0x080200c2;c[1]=c[2]=0;c[3]=MM_SETTINGS_SIZE;
        c[4]=(MM_SETTINGS_SIZE<<4)|12;c[5]=WIRE(bytes);
        ok=request(handle) && c[2]==MM_SETTINGS_SIZE;
    }
    bool closed=close_file(handle);return ok && closed;
}
static bool write_slot(unsigned slot,const uint8_t bytes[MM_SETTINGS_SIZE]) {
    uint32_t handle=0,*c=mm_ipc_buffer();bool ok=false;
    if(!open_file(slot,true,&handle)) return false;
    c[0]=0x08050080;c[1]=MM_SETTINGS_SIZE;c[2]=0;
    if(request(handle)) {
        c[0]=0x08030102;c[1]=c[2]=0;c[3]=MM_SETTINGS_SIZE;c[4]=1; /* flush */
        c[5]=(MM_SETTINGS_SIZE<<4)|10;c[6]=WIRE(bytes);
        ok=request(handle) && c[2]==MM_SETTINGS_SIZE;
    }
    bool closed=close_file(handle);return ok && closed;
}
static void load(void) {
    uint8_t bytes[2][MM_SETTINGS_SIZE];MmSettings settings[2];bool valid[2];
    uint32_t saved[64];memcpy(saved,mm_ipc_buffer(),sizeof(saved));
    for(unsigned i=0;i<2;i++) valid[i]=read_slot(i,bytes[i]) &&
                                      mm_settings_decode_all(bytes[i],sizeof(bytes[i]),&settings[i]);
    memcpy(mm_ipc_buffer(),saved,sizeof(saved));
    loaded=true;
    int slot=valid[0]?0:-1;
    if(valid[1] && (slot<0 || (uint8_t)(settings[1].sequence-settings[0].sequence)<128u)) slot=1;
    if(slot<0) return;
    current_slot=slot;sequence=settings[slot].sequence;
    mm_native_options.strength_index=settings[slot].strength;
    mm_native_options.sensitivity_index=settings[slot].sensitivity;
    pending_mode=settings[slot].mode;pending=true;
}
void mm_settings_tick(void) {
    if(!FS_SESSION) return; /* bootstrap runs before service initialization */
    if(!loaded) load();
    if(!pending || mm_native_options.ui!=MM_UI_CLOSED) return;
    uint32_t available=mm_render_available_modes();
    if(!(available&(1u<<pending_mode))) return;
    MmProfile p;void *candidate=0;
    if(!mm_profile(pending_mode,false,&p) || !mm_render_build(0,&p,&candidate)) return;
    if(!mm_render_commit(0,candidate)) {mm_render_discard(0,candidate);return;}
    mm_native_options.active=mm_native_options.selected=pending_mode;
    pending=false;
}
bool mm_settings_save(void *user,const uint8_t *bytes,size_t size) {
    (void)user;MmSettings s;
    if(!loaded || !FS_SESSION || !mm_settings_decode_all(bytes,size,&s)) return false;
    s.sequence=(uint8_t)(sequence+1);
    uint8_t encoded[MM_SETTINGS_SIZE],verify[MM_SETTINGS_SIZE];
    if(!mm_settings_encode_all(&s,encoded)) return false;
    unsigned slot=current_slot==0?1:0;
    uint32_t saved[64];memcpy(saved,mm_ipc_buffer(),sizeof(saved));
    bool ok=write_slot(slot,encoded) && read_slot(slot,verify) && !memcmp(encoded,verify,sizeof(encoded));
    memcpy(mm_ipc_buffer(),saved,sizeof(saved));
    if(ok) {current_slot=(int)slot;sequence=s.sequence;pending=false;}
    return ok;
}
