#define MIN_DATA_SIZE 1
#define HEAP_SIZE (1024 * 1024) // 1 MB

static unsigned char heap[HEAP_SIZE];
void* heap_start = heap;
void* heap_end   = heap + HEAP_SIZE;


typedef struct block_header {
    size_t size;      // size of the block: header + data
    int free;          // 1 = free, 0 = filled
} block_header;


void heap_init(void)
{
    block_header* first = (block_header*)heap_start;
    first->size = HEAP_SIZE;
    first->free = 1;
    block_header* footer = (block_header*)((char*)heap_start + first->size - sizeof(block_header));
    footer->size = HEAP_SIZE;
    footer->free = 1;
}


void* my_tiny_malloc(size_t size)
{
    size = (size + 7) & ~7; // round up to 8 bytes
    block_header* current = (block_header*) heap_start;
    while ((char*) current < (char*) heap_end) 
    {
        if(current->free && current->size >= (size + 2 * sizeof(block_header)) )
        {
            // we found a big block!
            size_t total_block_size = current->size;
            size_t needed_size = size + 2 * sizeof(block_header); // we need 2 block_headers because of header and footer on the block

            if(total_block_size - needed_size >= 2 * sizeof(block_header) + MIN_DATA_SIZE)
            {
                // split the block
                current->size = needed_size;
                current->free = 0;

                // write footer
                block_header* footer = (block_header*)((char*)current + current->size - sizeof(block_header));
                footer->size = current->size;
                footer->free = 0;

                // next block from remainings
                block_header* next_block = (block_header*)((char*)current + needed_size);
                next_block->size = total_block_size - needed_size;
                next_block->free = 1;

                // footer at the end of the next block
                block_header* next_footer = (block_header*)((char*)next_block + next_block->size - sizeof(block_header));
                next_footer->size = next_block->size;
                next_footer->free = 1;
            }
            else
            {
                // data fits
                current->free = 0;
                block_header* footer = (block_header*)((char*)current + current->size - sizeof(block_header));
                footer->free = 0;
            }
            return (void*)((char*)current + sizeof(block_header));
        }
        current = (block_header*)((char*)current + current->size);
    }
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
        }
    }
}