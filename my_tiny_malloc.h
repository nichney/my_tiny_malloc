#ifndef MY_TINY_MALLOC_H
#define MY_TINY_MALLOC_H

#include <stddef.h>

#define HEAP_SIZE (1024 * 1024)

typedef struct block_header {
    size_t size;
    int free;
} block_header;

extern void* heap_start;
extern void* heap_end;

void heap_init(void);
void* my_tiny_malloc(size_t size);
void my_tiny_free(void* ptr);

#endif