/* Initialize menus with the High-res default. SD settings load later, after FS/GPU startup. */
#include "display_options.h"

volatile uint32_t mm_native_signature = 0x4d4d4744;
volatile uint32_t mm_native_boot_count;
MmOptions mm_native_options;

extern void mm_native_backend(MmBackend *backend);

void mm_native_init(void) {
    MmBackend backend = {0};
    mm_native_backend(&backend);
    mm_options_init(&mm_native_options, &backend, MM_HIGH_RES_2D);
    ++mm_native_boot_count;
}

void *memset(void *dest, int byte, size_t size) {
    unsigned char *p = dest;
    while (size--) *p++ = (unsigned char)byte;
    return dest;
}
void *memcpy(void *dest, const void *source, size_t size) {
    unsigned char *d = dest;
    const unsigned char *s = source;
    while (size--) *d++ = *s++;
    return dest;
}
int memcmp(const void *left, const void *right, size_t size) {
    const unsigned char *a = left, *b = right;
    while (size--) {
        if (*a != *b) return (int)*a - (int)*b;
        ++a;
        ++b;
    }
    return 0;
}
