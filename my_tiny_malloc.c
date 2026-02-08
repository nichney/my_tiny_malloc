#include "my_tiny_malloc.h"


mem_chunk* chunks_list = NULL;
const long int MIN_DATA_SIZE = 2 * sizeof(block_header) + 8;

// counters
long long int counter_total_allocated = 0;
long long int counter_total_free = 0;
long long int counter_number_of_blocks = 0;
long long int counter_peak_usage = 0;
long long int counter_total_chunks = 0;

// mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


mem_chunk* request_new_chunk(size_t size)
{
    size_t page_size = sysconf(_SC_PAGESIZE);
    // align by page size
    size_t total_overhead = sizeof(mem_chunk) + 2 * sizeof(block_header) + 2 * sizeof(block_header);
    size_t alloc_size = (size + total_overhead + page_size - 1) & ~(page_size-1);

    mem_chunk* new_chunk = mmap(NULL, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (new_chunk == MAP_FAILED) return NULL;

    new_chunk->size = alloc_size;
    new_chunk->next = chunks_list;
    new_chunk->prev = NULL;
    if(chunks_list) chunks_list->prev = new_chunk;
    chunks_list = new_chunk;

    block_header* left_fence = (block_header*)((char*)new_chunk + sizeof(mem_chunk));
    left_fence->size = sizeof(block_header);
    left_fence->free = 0; // always not free
    left_fence->magic = MAGIC_NUMBER;
    left_fence->parent = new_chunk;

    block_header* right_fence = (block_header*)((char*)new_chunk + alloc_size - sizeof(block_header));
    right_fence->size = sizeof(block_header);
    right_fence->free = 0; // always not free
    right_fence->magic = MAGIC_NUMBER;
    right_fence->parent = new_chunk;

    // initialize first empty block inside new chunk
    block_header* first = (block_header*) ((char*)left_fence + left_fence->size);
    first->free = 1;
    first->size = alloc_size - sizeof(mem_chunk) - 2 * sizeof(block_header);
    first->magic = MAGIC_NUMBER;
    first->parent = new_chunk;

    // footer
    block_header* footer = (block_header*) ((char*)first + first->size - sizeof(block_header));
    footer->free = 1;
    footer->size = first->size;
    footer->magic = MAGIC_NUMBER;
    footer->parent = new_chunk;

    /* Counter section */
    counter_total_chunks += 1;
    counter_total_free += first->size;
    counter_number_of_blocks += 1;

    return new_chunk;
}


void* allocate_block(block_header* block, size_t needed, mem_chunk* chunk)
{
    size_t total_block_size = block->size;
    size_t actual_taken = (total_block_size - needed >= 2 * sizeof(block_header) + MIN_DATA_SIZE) ? needed : total_block_size;
    

    if(total_block_size - needed >= 2 * sizeof(block_header) + MIN_DATA_SIZE)
    {
        // split the block
        block->size = needed;
        block->free = 0;
        block->magic = MAGIC_NUMBER;
        block->parent = chunk;

        // write footer
        block_header* footer = (block_header*)((char*)block + block->size - sizeof(block_header));
        footer->size = block->size;
        footer->free = 0;
        footer->parent = chunk;
        footer->magic = MAGIC_NUMBER;

        // next block from remainings
        block_header* next_block = (block_header*)((char*)block + needed);
        next_block->size = total_block_size - needed;
        next_block->free = 1;
        next_block->magic = MAGIC_NUMBER;
        next_block->parent = chunk;

        // footer at the end of the next block
        block_header* next_footer = (block_header*)((char*)next_block + next_block->size - sizeof(block_header));
        next_footer->size = next_block->size;
        next_footer->free = 1;
        next_footer->parent = chunk;
        next_footer->magic = MAGIC_NUMBER;

        /* Counter section */
        counter_number_of_blocks += 1;
    }
    else
    {
        // data fits
        block->free = 0;
        block_header* footer = (block_header*)((char*)block + block->size - sizeof(block_header));
        footer->free = 0;
    }
    /* Counter section */
    counter_total_allocated += actual_taken;
    counter_total_free -= actual_taken;
    if (counter_total_allocated > counter_peak_usage)
        counter_peak_usage = counter_total_allocated;

    return (void*)((char*)block + sizeof(block_header));
}

void* my_tiny_malloc(size_t size)
{
    size = (size + 15) & ~15; // round up to 16 bytes
    size_t needed_size = size + 2 * sizeof(block_header); // we need 2 block_headers because of header and footer on the block
    
    pthread_mutex_lock(&mutex);

    mem_chunk* curr_chunk = chunks_list;
    while(curr_chunk)
    {
        block_header* current = (block_header*)((char*)curr_chunk + sizeof(mem_chunk) + sizeof(block_header));
        char* chunk_end = (char*)curr_chunk + curr_chunk->size;

        #ifndef ALLOC_STRATEGY_BEST_FIT
        while((char*)current < chunk_end)
        {
            if(current->free && current->size >= needed_size)
            {
                void* result = allocate_block(current, needed_size, curr_chunk);
                pthread_mutex_unlock(&mutex);
                return result;
            }
            current = (block_header*) ((char*)current + current->size);
        }
        #else
        block_header* best_block = NULL;
        while((char*) current < chunk_end)
        {
            if(current->free && current->size >= needed_size)
            {
                if(best_block == NULL || current->size < best_block->size)
                {
                    best_block = current;
                }
            }
            current = (block_header*)((char*)current + current->size);
        }
        if(best_block)
        {
            void* result = allocate_block(best_block, needed_size, curr_chunk);
            pthread_mutex_unlock(&mutex);
            return result;
        }
        #endif
        curr_chunk = curr_chunk->next;
    }

    // we didn't found memory - request some new
    mem_chunk* fresh_chunk = request_new_chunk(needed_size);
    if(!fresh_chunk)
    {
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    // we don't need to check if there are enougth space for block, because we just allocated it
    block_header* first_block = (block_header*)((char*)fresh_chunk + sizeof(mem_chunk) + sizeof(block_header));
    void* result = allocate_block(first_block, needed_size, fresh_chunk);
    pthread_mutex_unlock(&mutex);
    return result;
}


void my_tiny_free(void* ptr)
{
    if(ptr == NULL)
        return; // Well, NULL is already not busy, so we can just return

    pthread_mutex_lock(&mutex);
    block_header* header = (block_header*) ((char*) ptr - sizeof(block_header)); // ptr just point to data, so we need to calculate header location first

    if(header->magic != MAGIC_NUMBER)
    {
        pthread_mutex_unlock(&mutex);
        return; // Invalid pointer
    }

    if(header->free)
    {
        pthread_mutex_unlock(&mutex);
        return; // prevent double free
    }

    header->free = 1; // now block marked as free

    block_header* footer = (block_header*)((char*)header + header->size - sizeof(block_header));
    footer->free = 1; // mark in footer too

    /* Counter section */
    counter_total_free += header->size;
    counter_total_allocated -= header->size;

    // right merge
    char* right_address = (char*)header + header->size;
    block_header* next_header = (block_header*) right_address;

    if(next_header->magic == MAGIC_NUMBER && next_header->free){
        header->size += next_header->size;

        block_header* final_footer = (block_header*)((char*)header + header->size - sizeof(block_header));
        final_footer->size = header->size;
        final_footer->free = 1;
        final_footer->magic = MAGIC_NUMBER;
        final_footer->parent = header->parent;
        
        counter_number_of_blocks -= 1;
    }

    // left merge
    block_header* prev_footer = (block_header*)((char*)header - sizeof(block_header));
    
    if (prev_footer->magic == MAGIC_NUMBER && prev_footer->free) {
        block_header* prev_header = (block_header*)((char*)header - prev_footer->size);
        
        prev_header->size += header->size;
        header = prev_header;
        
        block_header* final_footer = (block_header*)((char*)prev_header + prev_header->size - sizeof(block_header));
        final_footer->size = prev_header->size;
        final_footer->free = 1;
        final_footer->parent = header->parent;

        counter_number_of_blocks -= 1;
    }

    // free chunk if possible
    mem_chunk* my_chunk = header->parent;
    size_t chunk_overhead = sizeof(mem_chunk) + 2 * sizeof(block_header);
    size_t usable_arena_size = my_chunk->size - chunk_overhead;

    if (header->size == usable_arena_size && counter_total_chunks > 1) {
        if (my_chunk->prev) my_chunk->prev->next = my_chunk->next;
        if (my_chunk->next) my_chunk->next->prev = my_chunk->prev;
        if (my_chunk == chunks_list) chunks_list = my_chunk->next;

        counter_total_free -= header->size;

        munmap(my_chunk, my_chunk->size);

        counter_total_chunks -= 1;
    }
    pthread_mutex_unlock(&mutex);
}