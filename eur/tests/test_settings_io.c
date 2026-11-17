#include "display_options.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
MmOptions mm_native_options;
uint32_t mm_test_fs_session;
static uint32_t command[64];
static const void *pointers[32];static unsigned pointer_count;
static FILE *files[8];static unsigned opens,closes;
static const char *directory;static bool fail_write,fail_close,not_ready;
uint32_t *mm_ipc_buffer(void) {return command;}
uint32_t mm_test_ipc_pointer(const void *p) {
    for(unsigned i=0;i<pointer_count;i++) if(pointers[i]==p) return i+1;
    assert(pointer_count<32);pointers[pointer_count++]=p;return pointer_count;
}
static void *address(uint32_t p) {assert(p && p<=pointer_count);return (void *)pointers[p-1];}
int32_t mm_ipc_send(uint32_t handle) {
    uint32_t cmd=command[0];int result=0;
    if(handle==77) {
        assert(cmd==0x08030204 && command[1]==0 && command[2]==9);
        assert(command[3]==1 && command[4]==1 && command[5]==3 && !command[8]);
        assert(command[9]==((1<<14)|(2<<10)|2));
        assert(!strcmp(address(command[10]),""));
        const char *path=address(command[12]);
        assert(command[6]==strlen(path)+1 && command[11]==((command[6]<<14)|2));
        assert(!strcmp(path,"/luma/titles/0004000000125600/display-settings-a.bin") ||
               !strcmp(path,"/luma/titles/0004000000125600/display-settings-b.bin"));
        char local[512];snprintf(local,sizeof(local),"%s/%s",directory,strrchr(path,'/')+1);
        bool write=command[7]==6;assert(write || command[7]==1);
        FILE *f=fopen(local,write?"r+b":"rb");
        if(!f && write) f=fopen(local,"w+b");
        if(!f) result=-1;
        else {
            unsigned slot=0;while(slot<8 && files[slot]) ++slot;assert(slot<8);
            files[slot]=f;command[3]=slot+100;++opens;
        }
    } else {
        assert(handle>=100 && handle<108 && files[handle-100]);FILE *f=files[handle-100];
        switch(cmd) {
        case 0x08040000:
            assert(!fseek(f,0,SEEK_END));command[2]=(uint32_t)ftell(f);command[3]=0;break;
        case 0x08050080:
            assert(command[1]==16 && !command[2]);break;
        case 0x080200c2:
            assert(!command[1] && !command[2] && command[3]==16 && command[4]==((16<<4)|12));
            rewind(f);command[2]=(uint32_t)fread(address(command[5]),1,16,f);break;
        case 0x08030102:
            assert(!command[1] && !command[2] && command[3]==16 && command[4]==1 && command[5]==((16<<4)|10));
            rewind(f);command[2]=(uint32_t)fwrite(address(command[6]),1,fail_write?11:16,f);
            assert(!fflush(f));if(fail_write) result=-1;break;
        case 0x08080000: if(fail_close) result=-1;break;
        default:assert(0);
        }
    }
    command[1]=(uint32_t)result;return 0;
}
int32_t mm_ipc_close(uint32_t h) {assert(h>=100 && h<108 && files[h-100]);fclose(files[h-100]);files[h-100]=0;++closes;return 0;}
uint32_t mm_render_available_modes(void) {return not_ready?1u<<MM_HIGH_RES_2D:31;}
static MmProfile candidate;
bool mm_render_build(void *u,const MmProfile *p,void **out) {(void)u;candidate=*p;*out=&candidate;return true;}
bool mm_render_commit(void *u,void *p) {(void)u;assert(p==&candidate);return true;}
void mm_render_discard(void *u,void *p) {(void)u;(void)p;assert(0);}
extern void mm_settings_tick(void);
extern bool mm_settings_save(void *,const uint8_t *,size_t);
static bool save(MmMode mode,unsigned strength,unsigned sensitivity) {
    MmSettings s={mode,strength,sensitivity,0};uint8_t b[16];assert(mm_settings_encode_all(&s,b));
    return mm_settings_save(0,b,sizeof(b));
}
int main(int argc,char **argv) {
    assert(argc==3);directory=argv[1];const char *phase=argv[2];
    uint32_t original[64];for(unsigned i=0;i<64;i++) original[i]=command[i]=0xaabb0000+i;
    mm_options_init(&mm_native_options,0,MM_HIGH_RES_2D);
    mm_settings_tick();assert(opens==0); /* FS not ready */
    mm_test_fs_session=77;
    not_ready=!strcmp(phase,"delayed");
    mm_settings_tick();assert(!memcmp(original,command,sizeof(command)));
    if(!strcmp(phase,"write")) {
        assert(mm_native_options.active==MM_HIGH_RES_2D);
        assert(save(MM_SUPERSAMPLE_3D,6,5));
        assert(save(MM_ORIGINAL,3,2));
        fail_write=true;assert(!save(MM_NATIVE_2D,1,1));fail_write=false;
    } else if(!strcmp(phase,"load") || !strcmp(phase,"delayed")) {
        if(not_ready) {assert(mm_native_options.active==MM_HIGH_RES_2D);not_ready=false;mm_settings_tick();}
        assert(mm_native_options.active==MM_ORIGINAL);
        assert(mm_native_options.strength_index==3 && mm_native_options.sensitivity_index==2);
    } else if(!strcmp(phase,"fallback")) {
        assert(mm_native_options.active==MM_HIGH_RES_2D); /* other slot was intentionally torn */
        assert(mm_native_options.strength_index==3 && mm_native_options.sensitivity_index==2);
    } else if(!strcmp(phase,"wrap")) {
        assert(mm_native_options.active==MM_SUPERSAMPLE_3D);
        assert(mm_native_options.strength_index==6 && mm_native_options.sensitivity_index==5);
        assert(save(MM_ORIGINAL,3,2));
    } else assert(0);
    assert(opens==closes && !memcmp(original,command,sizeof(command)));
    puts("PASS: native IPC fields, TLS restore, file handles, journal/readback and deferred startup");
}
