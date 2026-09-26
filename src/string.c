#include "string.h"

void* memcpy(void* dst, const void* src, size_t n) {
    void* d = dst;
    __asm__ volatile ("rep movsb" : "+D"(d), "+S"(src), "+c"(n) : : "memory");
    return dst;
}

void* memset(void* dst, int c, size_t n) {
    void* d = dst;
    __asm__ volatile ("rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
    return dst;
}
