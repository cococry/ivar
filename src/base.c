#include "base.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

void* _malloc(size_t size) {
  void* buf = malloc(size); 
  if(!buf) {
    fprintf(stderr, "ivar: failed to allocate %lu bytes.: %s\n", 
            size, strerror(errno));
    return NULL;
  }
  return buf;
}

void* _calloc(size_t n, size_t size) {
  void* buf = calloc(n, size);
  if(!buf) {
    fprintf(stderr, "ivar: failed to calloc %lu bytes.: %s\n", 
            size, strerror(errno));
    return NULL;
  }
  return buf;
}

void*  _realloc(void* ptr, size_t size) {
  void* buf = realloc(ptr, size);
  if(!buf) {
    fprintf(stderr, "ivar: failed to reallocate %lu bytes.: %s\n", 
            size, strerror(errno));
    return NULL;
  }
  return buf;
}

uint8_t readfile(char** o_buf, const char* filepath) {
  FILE* fp = fopen(filepath, "r");
  if(!fp) {
    fprintf(stderr, "ivar: failed to open file: %s\n", strerror(errno));
    return 1;
  }
  fseek(fp, 0L, SEEK_END);
  long len = ftell(fp);
  fseek(fp, 0L, SEEK_SET);

  char* filebuf = (char*)_malloc(len + 1);
  assert(filebuf);

  unsigned long nread = fread(filebuf, 1, len, fp);
  if(nread != len || nread == 0) {
    fprintf(stderr, "ivar: fread failed(): should read %li bytes but only read %lu.\n",
            len, nread);
    return 1;
  }
  filebuf[len] = '\0';

  fclose(fp);

  *o_buf = filebuf;

  return 0;
}

