#include <stdint.h>
#include <stddef.h>

void*  _malloc(size_t size);
void*  _calloc(size_t n, size_t size);
void*  _realloc(void* ptr, size_t size);
uint8_t readfile(char** o_buf, const char* filepath);
