#ifndef STRING_H
#define STRING_H

// 字符串复制函数
void strcpy(char* dest, const char* src);

// 字符串比较函数
int strcmp(const char* s1, const char* s2);

// 比较指定长度的字符串
int strncmp(const char* s1, const char* s2, unsigned int n);

// 字符串长度函数
unsigned int strlen(const char* str);

// 内存复制函数
void memcpy(void* dest, const void* src, unsigned int n);

// 内存填充函数
void memset(void* ptr, int value, unsigned int n);

// 在字符串中查找字符的第一次出现
char* strchr(const char* s, int c);

// 在字符串中查找字符的最后一次出现
char* strrchr(const char* s, int c);

// 将指定长度的字符串复制到目标字符串
char* strncpy(char* dest, const char* src, unsigned int n);

// 比较内存区域
int memcmp(const void* s1, const void* s2, unsigned int n);

#endif // STRING_H
