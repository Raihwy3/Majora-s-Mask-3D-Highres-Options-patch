/* Temporary native-UI integration probe, scoped to the Options help call site.
 * Draws through the game's existing text parser and sprite; owns no game memory.
 * It does not change rendering modes or install a START handler.
 */
#include <stdint.h>

extern void mm_game_help_message(void *help, int message, int context);
extern void *mm_game_find_widget(void *layout, const char *name);
extern void *mm_game_current_widget(void *layout);
extern void *mm_game_text_pane(void *widget);
extern void mm_native_stream_init(void *stream, void *buffer, unsigned size);
extern int mm_game_parse_utf16(void *stream, const uint16_t *text);
extern int mm_game_sprite_set_stream(void *sprite, void *stream);

static void *field_pointer(void *base, unsigned offset) {
    return *(void **)((uint8_t *)base + offset);
}

void mm_options_text_probe(void *help, int message, int context) {
    /* Preserve native message/window state before replacing this test's text. */
    mm_game_help_message(help, message, context);
    if (!help) return;
    void *widget = mm_game_find_widget(help, "e_text");
    if (!widget) return;
    void *layout = field_pointer(widget, 0x20);
    if (!layout) return;
    void *instance = mm_game_current_widget(layout);
    if (!instance) return;
    void *panes = field_pointer(instance, 0x18);
    if (!panes) return;
    void *pane = field_pointer(panes, 0);
    if (!pane) return;
    void *sprite = mm_game_text_pane(pane);
    if (!sprite) return;
    /* The game's shared UI stream is also consumed by its timed message
       player. Replacing it while playback is active invalidates that player's
       token/index state. Keep this label and its parsed tokens entirely owned
       by the mod, with a lifetime covering every sprite that references it. */
    static _Alignas(8) uint8_t stream[0x4c];
    static _Alignas(8) uint32_t buffer[1024];
    static unsigned initialized, parsed;
    if (!initialized) {
        mm_native_stream_init(stream, buffer, sizeof(buffer));
        initialized = 1;
    }
    static const uint16_t label[] = u"Graphics menu\nNative UI test";
    if (!parsed) {
        if (!mm_game_parse_utf16(stream, label)) return;
        parsed = 1;
    }
    (void)mm_game_sprite_set_stream(sprite, stream);
}
