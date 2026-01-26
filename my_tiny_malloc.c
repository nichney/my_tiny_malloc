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
    size = (size + 7) & ~7; // round up to 8 bytes
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


void my_tiny_free(void* ptr)
{
    if(ptr == NULL || ptr < heap_start || ptr >= heap_end)
        return; // Well, NULL is already not busy, so we can just return

    block_header* header = (block_header*) ((char*) ptr - sizeof(block_header)); // ptr just point to data, so we need to calculate header location first

    if(header->free)
        return; // prevent double free

    header->free = 1; // now block marked as free

    // right merge
    char* right_address = (char*)header + header->size;
    if(right_address < (char*) heap_end)
    {
        if(((block_header*) right_address)->free)
        {
            header->size = header->size + ((block_header*) right_address)->size;
        }
    }

    // left merge
    block_header* current = (block_header*) heap_start;
    while ((char*) current < (char*) header)
    {
        if( (char*) ((char*)current + current->size) == (char*) header)
        {
            if(current->free)
            {
                current->size = current->size + header->size;
            }
            
            break;
        }
        current = (block_header*)((char*)current + current->size);
    } 
}