#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "display_options.h"
MmOptions mm_native_options;
static unsigned char registry[0x140],old_record[28],scratch_record[28];
uint8_t *mm_test_registry=registry;
static unsigned created,bound,allocated,sequence;
static bool bypass,fail_allocation,direct;
static uintptr_t wanted_destination=0x14109120;
static uint32_t word(void *p,unsigned n) {uint32_t v;memcpy(&v,(char *)p+n,4);return v;}
static void set(void *p,unsigned n,uint32_t v) {memcpy((char *)p+n,&v,4);}
extern bool mm_resolve_prepare(void);
extern void mm_resolve_packet(void *,unsigned,unsigned,unsigned,void *,unsigned,unsigned,unsigned,unsigned,int,unsigned);
void mm_game_make_buffers(unsigned n,uint32_t *out) {assert(n==1);*out=99;++created;set(scratch_record,0,99);}
void *mm_game_bind_buffer(uint32_t id) {++bound;assert(id==99 || id==7);return id==99?scratch_record:old_record;}
void mm_game_allocate_buffer(unsigned f,unsigned h,unsigned w,unsigned region) {
    assert(f==0x8058 && h==480 && w==800 && region==0x10000);++allocated;
    set(scratch_record,4,fail_allocation?0:0x14800000);set(scratch_record,8,f);set(scratch_record,12,h);
    set(scratch_record,16,w);set(scratch_record,20,region);
}
static unsigned wanted_width,wanted_height,wanted_scale;
void mm_game_texture_copy(void *source,unsigned line,unsigned gap,void *dest,unsigned out_line,unsigned out_gap,unsigned bytes) {
    assert(!bypass && !direct && sequence++==0 && dest==(void *)0x14800000);
    assert((uintptr_t)source==0x1f16ffc0u+(800-wanted_height)*512*4);
    assert(line==wanted_width*8*4 && gap==(512-wanted_width)*8*4);
    assert(out_line==line && !out_gap && bytes==wanted_width*wanted_height*4 && bytes<=1536000);
    assert(!(line%16) && !(gap%16) && !(bytes%16));
}
void mm_game_surface_packet(void *source,unsigned iw,unsigned ih,unsigned f,void *dest,unsigned ow,unsigned oh,unsigned of,unsigned scale,int flip,unsigned block) {
    assert((uintptr_t)dest==wanted_destination && f==0x8058 && of==0x8051 && !flip && block==8);
    assert(scale==wanted_scale && ow==240);
    if(direct) {assert(sequence++==0 && (uintptr_t)source==0x1f16ffc0u+819200 && iw==512 && ih==400 && oh==400);}
    else if(bypass) {assert(sequence++==0 && source==(void *)0x1f16ffc0 && iw==512 && ih==800 && oh==320);}
    else {assert(sequence++==1 && source==(void *)0x14800000 && iw==wanted_width && ih==wanted_height && oh==400);}
}
int main(int argc,char **argv) {
    (void)argv;fail_allocation=argc>1;
    /* Both eye destinations keep native direct-transfer arguments, even
       before staging exists. No extra TextureCopy command may be emitted. */
    mm_native_options.active=MM_SUPERSAMPLE_3D;direct=true;wanted_scale=0x501;
    for(unsigned eye=0;eye<2;eye++) {
        sequence=0;wanted_destination=0x14109120+eye*0x100000;
        mm_resolve_packet((void *)0x1f16ffc0,512,800,0x8058,(void *)wanted_destination,240,400,0x8051,0x501,0,8);
        assert(sequence==1 && !created && !allocated);
    }
    direct=false;wanted_destination=0x14109120;
    mm_native_options.active=MM_SUPERSAMPLE_2D;
    set(registry,0x124,8);assert(!mm_resolve_prepare() && !created);
    set(registry,0x124,0);set(old_record,0,7);
    assert((uintptr_t)old_record<=UINT32_MAX);set(registry,0x128,(uint32_t)(uintptr_t)old_record);
    uint8_t before[sizeof(registry)];memcpy(before,registry,sizeof(before));
    if(fail_allocation) {
        assert(!mm_resolve_prepare() && created==1 && allocated==1 && bound==2);
        assert(!memcmp(before,registry,sizeof(before)));
        assert(!mm_resolve_prepare() && created==1 && allocated==1 && bound==2);
        puts("PASS: failed FCRAM allocation restores native binding, rejects resolve and does not retry/leak allocations.");
        return 0;
    }
    assert(mm_resolve_prepare() && created==1 && allocated==1 && bound==2);
    assert(!memcmp(before,registry,sizeof(before)) && word(old_record,0)==7);
    assert(mm_resolve_prepare() && created==1 && allocated==1 && bound==2);
    for(unsigned scale=0x500;scale<=0x502;scale++) {
        sequence=0;wanted_scale=scale;wanted_width=scale==0x500?240:480;wanted_height=scale==0x502?800:400;
        mm_resolve_packet((void *)0x1f16ffc0,512,800,0x8058,(void *)0x14109120,240,400,0x8051,scale,0,8);
        assert(sequence==2);
    }
    /* Switching to SS3D and back preserves staging for SS2D and the
       corrected native-resolution paths, without freeing queued memory. */
    direct=true;sequence=0;wanted_scale=0x501;
    mm_native_options.active=MM_SUPERSAMPLE_3D;
    mm_resolve_packet((void *)0x1f16ffc0,512,800,0x8058,(void *)wanted_destination,240,400,0x8051,0x501,0,8);
    assert(sequence==1 && created==1 && allocated==1 && bound==2);
    mm_native_options.active=MM_SUPERSAMPLE_2D;direct=false;sequence=0;wanted_scale=0x502;
    mm_resolve_packet((void *)0x1f16ffc0,512,800,0x8058,(void *)wanted_destination,240,400,0x8051,0x502,0,8);
    assert(sequence==2);
    bypass=true;sequence=0;wanted_scale=0x500;
    mm_resolve_packet((void *)0x1f16ffc0,512,800,0x8058,(void *)0x14109120,240,320,0x8051,0x500,0,8);
    assert(sequence==1);
    puts("PASS: 400x480 SS3D emits one direct offset ScaleX resolve for both eye destinations; other modes retain repack; mode roundtrip preserves staging; native origin and bottom preserved.");
}
