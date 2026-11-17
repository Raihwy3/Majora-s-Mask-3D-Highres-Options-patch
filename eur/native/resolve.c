/* Owned FCRAM staging for a cropped tiled render target. Submit the repack
 * through the native command queue, so it tracks the additional PPF event. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "display_options.h"
extern MmOptions mm_native_options;
#ifdef MM_NATIVE_HOST_TEST
extern uint8_t *mm_test_registry;
#define REGISTRY mm_test_registry
#else
#define REGISTRY ((uint8_t *)0x7b2700)
#endif
extern void mm_game_make_buffers(unsigned,uint32_t *);
extern void *mm_game_bind_buffer(uint32_t);
extern void mm_game_allocate_buffer(unsigned,unsigned,unsigned,unsigned);
extern void mm_game_texture_copy(void *,unsigned,unsigned,void *,unsigned,unsigned,unsigned);
extern void mm_game_surface_packet(void *,unsigned,unsigned,unsigned,void *,unsigned,unsigned,unsigned,unsigned,int,unsigned);
static uint32_t scratch_id;
static void *scratch;
static bool attempted;
static uint32_t read(void *p,unsigned n) {uint32_t v;memcpy(&v,(uint8_t *)p+n,4);return v;}
bool mm_resolve_prepare(void) {
    if(attempted) return scratch!=0;
    if(!REGISTRY) return false;
    unsigned slot=read(REGISTRY,0x124);
    if(slot>2) return false;
    void *saved=(void *)(uintptr_t)read(REGISTRY,0x128+4*slot);
    uint32_t saved_id=saved?read(saved,0):0;
    attempted=true;
    mm_game_make_buffers(1,&scratch_id);
    if(!scratch_id) return false;
    void *record=mm_game_bind_buffer(scratch_id);
    if(record) {
        /* Native RGB output allocations use this same FCRAM region. One
           RGBA8 buffer, 480x800 = 1536000 bytes; no extra VRAM allocation. */
        mm_game_allocate_buffer(0x8058,480,800,0x10000);
        if(read(record,8)==0x8058 && read(record,0xc)==480 &&
           read(record,0x10)==800 && read(record,0x14)==0x10000)
            scratch=(void *)(uintptr_t)read(record,4);
    }
    mm_game_bind_buffer(saved_id);
    return scratch!=0;
}
void mm_resolve_packet(void *source,unsigned iw,unsigned ih,unsigned input_format,
                       void *destination,unsigned ow,unsigned oh,unsigned output_format,
                       unsigned scale,int flip,unsigned block) {
    unsigned x=scale==0x500?1:2,y=scale==0x502?2:1;
    unsigned width=ow*x,height=oh*y;
    /* Match only the three native top-resolution profiles. Leave bottom,
       the proven High-res800 output and unrelated transfer callers alone. */
    /* SS3D now renders400x480 per eye inside retained512x800 backing.
       Its rotated400-row image occupies the final400 rows. Resolve directly
       from that tail, declaring only the remaining512x400 input region. */
    bool direct_ss3d=mm_native_options.active==MM_SUPERSAMPLE_3D &&
        scale==0x501 && ow==240 && oh==400 && iw==512 && ih==800 &&
        input_format==0x8058 && output_format==0x8051 && block==8 && !flip;
    if(direct_ss3d) {
        source=(void *)((uintptr_t)source+(ih-height)*iw*4);
        ih=height;
    }
    if(!direct_ss3d && scratch && oh==400 && ow==240 && scale>=0x500 && scale<=0x502 &&
       input_format==0x8058 && output_format==0x8051 && block==8 && !flip &&
       iw==512 && ih==800 && width<=480 && height<=800) {
        unsigned line=width*8*4,gap=(iw-width)*8*4;
        /* PICA framebuffer Y is inverted against its physical height. A
           viewport at Y=0 occupies the LAST height rows of retained backing,
           not the first. Both 400-wide scene profiles therefore start at
           row400 of the 800-row rotated image. Rows are tile-aligned here. */
        source=(void *)((uintptr_t)source+(ih-height)*iw*4);
        /* Tiled8 rows occupy contiguous runs of width*8 RGBA pixels.
           Copy those runs and skip unused source columns. Both commands
           remain in the game's queue in draw -> repack -> resolve order. */
        mm_game_texture_copy(source,line,gap,scratch,line,0,width*height*4);
        source=scratch;iw=width;ih=height;
    }
    mm_game_surface_packet(source,iw,ih,input_format,destination,ow,oh,
                           output_format,scale,flip,block);
}
