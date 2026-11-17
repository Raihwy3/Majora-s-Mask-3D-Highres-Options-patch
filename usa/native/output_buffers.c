/* Output buffers retain their original 800-column allocation. Native scanout
 * computes an offset from record.width - display.width; synchronize that
 * record metadata with the active transfer width before scanout is queued.
 * No allocation or VRAM pointer is replaced. Native lookup: 0x129450. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#ifdef MM_NATIVE_HOST_TEST
extern uint8_t *mm_test_registry;
#define REGISTRY mm_test_registry
#else
#define REGISTRY ((uint8_t *)0x7b3700)
#endif
static uint32_t read(void *p,unsigned n) {uint32_t v;memcpy(&v,(uint8_t *)p+n,4);return v;}
static void *ptr(void *p,unsigned n) {return (void *)(uintptr_t)read(p,n);}
static void write(void *p,unsigned n,uint32_t v) {memcpy((uint8_t *)p+n,&v,4);}
static struct {void *record;uint32_t id;} capacity[4];
static void *lookup(uint32_t id) {
    if(!REGISTRY || !id) return 0;
    void *p=ptr(REGISTRY,0xa4+4*(id&31));
    for(unsigned n=0;p && n<64;n++,p=ptr(p,0x18)) if(read(p,0)==id) return p;
    return 0;
}
static bool gather(void *display,void **records) {
    if(!display) return false;
    static const unsigned offsets[]={0x28,0x2c,0x54,0x58};
    uint32_t ids[4];
    for(unsigned i=0;i<4;i++) {
        ids[i]=read(display,offsets[i]);records[i]=lookup(ids[i]);
        if(!records[i] || !read(records[i],4) || read(records[i],8)!=0x8051 ||
           read(records[i],0xc)!=240) return false;
        uint32_t width=read(records[i],0x10);
        bool known=capacity[i].record==records[i] && capacity[i].id==ids[i];
        if(width!=800 && !(known && width==400)) return false;
        for(unsigned j=0;j<i;j++) if(records[j]==records[i]) return false;
    }
    /* Capture capacity only after validating all four native records. */
    for(unsigned i=0;i<4;i++) {
        capacity[i].record=records[i];capacity[i].id=ids[i];
    }
    return true;
}
bool mm_output_buffers_ready(void *display) {void *r[4];return gather(display,r);}
bool mm_output_buffers_width(void *display,unsigned width) {
    void *r[4];
    if((width!=400 && width!=800) || !gather(display,r)) return false;
    for(unsigned i=0;i<4;i++) write(r[i],0x10,width);
    write(display,0x40,width);write(display,0x6c,width);
    return true;
}

void mm_output_buffers_probe(void *display,unsigned *registered,unsigned *driver) {
    void *records[4];
    *registered=gather(display,records)?read(records[0],0x10):0;
    *driver=REGISTRY?read(REGISTRY,8):0;
}
