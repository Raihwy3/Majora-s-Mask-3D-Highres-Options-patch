#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void mm_options_text_probe(void *, int, int);
static _Alignas(void *) uint8_t help[64], widget[64], layout[64];
static _Alignas(void *) uint8_t instance[64], panes[16], pane[32], sprite[32];
static uint8_t game_shared_stream[0x4c];
static void *private_stream;
static int missing, original_calls, draw_calls, stage, initializes, parses, parsed_this_call;

static void pointer_at(void *base, unsigned offset, void *value) {
    memcpy((uint8_t *)base + offset, &value, sizeof(value));
}
void mm_game_help_message(void *obj, int message, int context) {
    assert(obj == help || !obj);
    assert(message == 45 && context == 7);
    assert(stage++ == 0);
    ++original_calls;
}
void *mm_game_find_widget(void *obj, const char *name) {
    assert(obj == help && strcmp(name, "e_text") == 0 && stage++ == 1);
    return missing == 1 ? NULL : widget;
}
void *mm_game_current_widget(void *obj) {
    assert(obj == layout && stage++ == 2);
    return missing == 3 ? NULL : instance;
}
void *mm_game_text_pane(void *obj) {
    assert(obj == pane && stage++ == 3);
    return missing == 6 ? NULL : sprite;
}
void *mm_game_stream_construct(void *stream) {
    assert(stream != game_shared_stream && (uintptr_t)stream % 8 == 0);
    memset(stream, 0, 0x4c);
    private_stream = stream;
    ++initializes;
    return stream;
}
int mm_game_parse_utf16(void *stream, const uint16_t *text) {
    assert(stream == private_stream && stream != game_shared_stream);
    assert(stage++ == 4);
    const uint32_t *words = stream;
    assert(words[0x1c / 4] != 0 && (words[0x1c / 4] & 7) == 0);
    assert(words[0x20 / 4] == 4096);
    assert(words[0x24 / 4] == 0 && words[0x2c / 4] == 0);
    assert(words[0x28 / 4] == words[0x1c / 4]);
    static const uint16_t expected[] = u"Graphics menu\nNative UI test";
    assert(memcmp(text, expected, sizeof(expected)) == 0);
    ++parses;
    parsed_this_call = 1;
    return missing == 7 ? 0 : 1;
}
int mm_game_sprite_set_stream(void *obj, void *stream) {
    assert(obj == sprite && stream == private_stream && stage++ == 4 + parsed_this_call);
    ++draw_calls;
    return 1;
}
static void run(int absent) {
    missing = absent;
    memset(game_shared_stream, 0xa5, sizeof(game_shared_stream));
    pointer_at(widget, 0x20, missing == 2 ? NULL : layout);
    pointer_at(instance, 0x18, missing == 4 ? NULL : panes);
    pointer_at(panes, 0, missing == 5 ? NULL : pane);
    stage = original_calls = draw_calls = parsed_this_call = 0;
    mm_options_text_probe(missing == 8 ? NULL : help, 45, 7);
    assert(original_calls == 1);
    assert(draw_calls == (missing == 0));
    for (unsigned i = 0; i < sizeof(game_shared_stream); ++i)
        assert(game_shared_stream[i] == 0xa5);
}
int main(void) {
    /* Fail parsing first, retry successfully, then revisit all resource gates. */
    run(7);
    run(0);
    for (int absent = 1; absent <= 6; ++absent) run(absent);
    run(8);
    run(0);
    assert(initializes == 1 && parses == 2);
    puts("PASS: native text bridge; real arena initializer and token-table base; shared-stream canary; parse failure/retry; resource gates.");
    return 0;
}
