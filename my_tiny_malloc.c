#include "my_tiny_malloc.h"


void* heap_start = NULL;
void* heap_end   = NULL;
const long int MIN_DATA_SIZE = 2 * sizeof(block_header) + 8;

// counters
long long int counter_total_allocated = 0;
long long int counter_total_free = 0;
long long int counter_number_of_blocks = 0;
long long int counter_peak_usage = 0;


void heap_init(void)
{
    void* mem = mmap(
        NULL,
        HEAP_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (mem == MAP_FAILED)
        return;

    heap_start = mem;
    heap_end = (char*)mem + HEAP_SIZE;

    block_header* first = (block_header*)heap_start;
    first->size = HEAP_SIZE;
    first->free = 1;
    block_header* footer = (block_header*)((char*)heap_start + first->size - sizeof(block_header));
    footer->size = HEAP_SIZE;
    footer->free = 1;

    /* Counter section */
    counter_total_free = HEAP_SIZE;
    counter_number_of_blocks = 1;
}


void* allocate_block(block_header* block, size_t needed)
{
    size_t total_block_size = block->size;
    size_t actual_taken = (total_block_size - needed >= 2 * sizeof(block_header) + MIN_DATA_SIZE) ? needed : total_block_size;
    

    if(total_block_size - needed >= 2 * sizeof(block_header) + MIN_DATA_SIZE)
    {
        // split the block
        block->size = needed;
        block->free = 0;

        // write footer
        block_header* footer = (block_header*)((char*)block + block->size - sizeof(block_header));
        footer->size = block->size;
        footer->free = 0;

        // next block from remainings
        block_header* next_block = (block_header*)((char*)block + needed);
        next_block->size = total_block_size - needed;
        next_block->free = 1;

        // footer at the end of the next block
        block_header* next_footer = (block_header*)((char*)next_block + next_block->size - sizeof(block_header));
        next_footer->size = next_block->size;
        next_footer->free = 1;

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
    size = (size + 7) & ~7; // round up to 8 bytes
    block_header* current = (block_header*) heap_start;
    size_t needed_size = size + 2 * sizeof(block_header); // we need 2 block_headers because of header and footer on the block
    #ifdef ALLOC_STRATEGY_FIRST_FIT
    while ((char*) current < (char*) heap_end) 
    {
        if(current->free && current->size >= needed_size)
        {
            return allocate_block(current, needed_size);
        }
        current = (block_header*)((char*)current + current->size);
    }
    #endif
    #ifdef ALLOC_STRATEGY_BEST_FIT
    block_header* best_block = NULL;
    while((char*) current < (char*) heap_end)
    {
            if(current->free && current->size >= needed_size)
            {
                if (best_block == NULL || current->size < best_block->size)
                {
                    best_block = current;
                }
            }
            current = (block_header*)((char*)current + current->size);
    }
    if (best_block)
    {
        return allocate_block(best_block, needed_size);
    }
    #endif
    return NULL;
}


void my_tiny_free(void* ptr)
{
    if(ptr == NULL || ptr < heap_start || ptr >= heap_end)
        return; // Well, NULL is already not busy, so we can just return

    block_header* header = (block_header*) ((char*) ptr - sizeof(block_header)); // ptr just point to data, so we need to calculate header location first

    if(header->free)
        return; // prevent double free

    header->free = 1; // now block marked as free

    block_header* footer = (block_header*)((char*)header + header->size - sizeof(block_header));
    footer->free = 1; // mark in footer too

    /* Counter section */
    counter_total_free += header->size;
    counter_total_allocated -= header->size;

    // right merge
    char* right_address = (char*)header + header->size;
    if(right_address < (char*) heap_end)
    {
        block_header* right_header = (block_header*) right_address;
        if(right_header->free)
        {
            header->size += right_header->size;
            block_header* final_footer = (block_header*)((char*)header + header->size - sizeof(block_header));
            final_footer->size = header->size;
            final_footer->free = 1;

            /* Counter section */
            counter_number_of_blocks -= 1;
        }
    }

    // left merge
    if ((char*)header > (char*)heap_start) 
    {
        // look up at the previous block footer
        block_header* prev_footer = (block_header*)((char*)header - sizeof(block_header));
        if (prev_footer->free) {
            block_header* prev_header = (block_header*)((char*)header - prev_footer->size);
            prev_header->size += header->size;

            block_header* final_footer = (block_header*)((char*)prev_header + prev_header->size - sizeof(block_header));
            final_footer->size = prev_header->size;
            final_footer->free = 1;

            header = prev_header;
            
            /* Counter section */
            counter_number_of_blocks -= 1;
        }
    }
}