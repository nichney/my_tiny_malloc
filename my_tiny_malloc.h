#ifndef MY_TINY_MALLOC_H
#define MY_TINY_MALLOC_H

#include <stddef.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/resource.h>
#include <pthread.h>

//#define ALLOC_STRATEGY_BEST_FIT
#define MAGIC_NUMBER 0xAABBCCDD

typedef struct mem_chunk {
    struct mem_chunk* next;
    struct mem_chunk* prev;
    size_t size;
} mem_chunk;

typedef struct block_header {
    size_t size;
    int free;
    unsigned int magic;
    mem_chunk* parent;
} block_header;


extern long long int counter_total_allocated;
extern long long int counter_total_free;
extern long long int counter_number_of_blocks;
extern long long int counter_peak_usage;
extern long long int counter_total_chunks;

void* my_tiny_malloc(size_t size);
void my_tiny_free(void* ptr);

#endif