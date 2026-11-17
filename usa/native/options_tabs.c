/* Native MenuOption sprite tabs appended to the supplied Options layout.
 * They use the game's existing Option/NumAll textures and generic widget draw.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
extern void *mm_game_find_widget(void *,const char *);
extern void mm_game_layout_calc(void *,float);
static void *ptr(void *p,unsigned n) {
    void *v;memcpy(&v,(unsigned char *)p+n,sizeof(v));return v;
}
static uint32_t *word(void *p,unsigned n) {return (uint32_t *)((unsigned char *)p+n);}
static void visible(void *w,bool on) {
    *word(w,0x84)=(*word(w,0x84)&~1u)|(on?1u:0u)|0x20000;
    /* Native setter keeps the desired visibility in the active-property
       word as well: 0x606ef4..0x606f08 ORs 0x20000 when showing a widget. */
    if(on) *word(w,0x88)|=0x20000u;
    else *word(w,0x88)&=~0x20000u;
}
bool mm_tabs_available(void *opt) {
    void *menu=opt?ptr(opt,0x10):0;
    return menu && mm_game_find_widget(menu,"display_tabs_g");
}
bool mm_tabs_update(void *opt,unsigned page) {
    if(page<1 || page>3 || !opt) return false;
    void *menu=ptr(opt,0x10);
    if(!menu) return false;
    static const char *const names[]={
        "display_tabs_g","btn_page1_g","btn_page2_g",
        "display_tab1_idle","display_tab1_selected",
        "display_tab2_idle","display_tab2_selected",
        "display_tab3_idle","display_tab3_selected"
    };
    void *widgets[9];
    /* Validate the entire asset extension before changing native visibility. */
    for(unsigned i=0;i<9;i++) if(!(widgets[i]=mm_game_find_widget(menu,names[i]))) return false;
    visible(widgets[0],true);visible(widgets[1],false);visible(widgets[2],false);
    for(unsigned p=1;p<=3;p++) {
        visible(widgets[3+2*(p-1)],p!=page);
        visible(widgets[4+2*(p-1)],p==page);
    }
    /* Original BtnReturn remains separate from the replaced page-tab groups. */
    if(page==3) {
        void *back=mm_game_find_widget(menu,"btn_return_l");
        if(back) visible(back,true);
    }
    mm_game_layout_calc(menu,0.0f);
    return true;
}
