#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned char opt[0x100],menu[0x200],widgets[9][0x140];
static unsigned char back[0x140];
static bool missing;
static unsigned calculated;
extern bool mm_tabs_available(void *);
extern bool mm_tabs_update(void *,unsigned);
static const char *const names[]={"display_tabs_g","btn_page1_g","btn_page2_g","display_tab1_idle","display_tab1_selected","display_tab2_idle","display_tab2_selected","display_tab3_idle","display_tab3_selected"};
void *mm_game_find_widget(void *l,const char *name) {
    assert(l==menu);
    if(!strcmp(name,"btn_return_l")) return back;
    for(unsigned i=0;i<9;i++) if(!strcmp(name,names[i])) return missing && i==8?NULL:widgets[i];
    return NULL;
}
static uint32_t *word(unsigned i,unsigned off) {return (uint32_t *)(widgets[i]+off);}
void mm_game_layout_calc(void *l,float dt) {assert(l==menu && dt==0);++calculated;}
int main(void) {
    void *p=menu;memcpy(opt+0x10,&p,sizeof(p));
    assert(mm_tabs_available(opt));
    for(unsigned page=1;page<=3;page++) {
        memset(widgets,0x55,sizeof(widgets));
        memset(back,0,sizeof(back));
        assert(mm_tabs_update(opt,page));
        assert(!!(*(uint32_t *)(back+0x84)&1)==(page==3));
        assert(!!(*(uint32_t *)(back+0x88)&0x20000)==(page==3));
        assert(*word(0,0x84)&1);
        assert(*word(0,0x88)&0x20000);
        assert(!(*word(1,0x88)&0x20000) && !(*word(2,0x88)&0x20000));
        assert(!(*word(1,0x84)&1) && !(*word(2,0x84)&1));
        for(unsigned n=1;n<=3;n++) {
            assert(!!(*word(3+2*(n-1),0x84)&1)==(n!=page));
            assert(!!(*word(4+2*(n-1),0x84)&1)==(n==page));
            assert(!!(*word(3+2*(n-1),0x88)&0x20000)==(n!=page));
            assert(!!(*word(4+2*(n-1),0x88)&0x20000)==(n==page));
        }
    }
    assert(calculated==3);
    unsigned char before[sizeof(widgets)];memcpy(before,widgets,sizeof(before));
    missing=true;
    assert(!mm_tabs_update(opt,3) && calculated==3 && !memcmp(before,widgets,sizeof(before)));
    assert(!mm_tabs_update(opt,0) && !mm_tabs_update(opt,4));
    puts("PASS: three native tab states; exactly one selected; original bars hidden; missing asset leaves native visibility intact.");
}
