/* ARM32 text-stream arena fields verified against native setup 0x145530.
 * Stream data is parsed from the high end of the arena; its token-pointer
 * table starts at the low end. Both bases must be initialized.
 */
#include <stdint.h>
extern void *mm_game_stream_construct(void *stream);
void mm_native_stream_init(void *stream, void *buffer, unsigned size) {
    (void)mm_game_stream_construct(stream);
    uint32_t *words = stream;
    words[0x1c / 4] = (uint32_t)(uintptr_t)buffer;
    words[0x20 / 4] = size;
    words[0x24 / 4] = 0;
    words[0x28 / 4] = (uint32_t)(uintptr_t)buffer;
    words[0x2c / 4] = 0;
}
