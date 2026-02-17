#include <stdint.h>
#include <unistd.h>

void*  _malloc(size_t len);
void*  _realloc(void* ptr, size_t len);
uint8_t readfile(char** o_buf, const char* filepath);
