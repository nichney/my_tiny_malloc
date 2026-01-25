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
}


void* my_tiny_malloc(size_t size)
{
    block_header* current = (block_header*) heap_start;
    while ((char*) current < (char*) heap_end) 
    {
        if(current->free && current->size >= (size + sizeof(block_header)) )
        {
            // we found a big block!
            size_t total_block_size = current->size;
            size_t needed_size = size + sizeof(block_header);

            if(total_block_size - needed_size >= sizeof(block_header) + MIN_DATA_SIZE)
            {
                // split the block
                current->size = needed_size;
                current->free = 0;

                block_header* next_block = (block_header*)((char*)current + needed_size);
                next_block->size = total_block_size - needed_size;
                next_block->free = 1;
            }
            else
            {
                // data fits
                current->free = 0;
            }
            return (void*)((char*)current + sizeof(block_header));
        }
        current = (block_header*)((char*)current + current->size);
    }
    return NULL;
}

