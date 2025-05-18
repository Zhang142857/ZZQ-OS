#include "string.h"

// 字符串复制函数
void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++) != 0);
}

// 字符串比较函数
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// 比较指定长度的字符串
int strncmp(const char* s1, const char* s2, unsigned int n) {
    while (n-- && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return n == (unsigned int)-1 ? 0 : *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// 字符串长度函数
unsigned int strlen(const char* str) {
    const char* s = str;
    while (*s) s++;
    return s - str;
}

// 内存复制函数
void memcpy(void* dest, const void* src, unsigned int n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
}

// 内存填充函数
void memset(void* ptr, int value, unsigned int n) {
    unsigned char* p = (unsigned char*)ptr;
    while (n--) *p++ = (unsigned char)value;
}

// 比较内存区域
int memcmp(const void* s1, const void* s2, unsigned int n) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}
