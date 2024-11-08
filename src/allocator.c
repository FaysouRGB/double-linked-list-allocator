#include "allocator.h"

#include <string.h>

#include "convert.h"

static void *blk_new_page(size_t size);
static void blk_try_free_page(blk_allocator *blka, blk_meta *blk);
static void blk_extend_allocator(blk_allocator *blka, size_t size);
static void blk_split(blk_meta *blk, size_t size);
static blk_meta *blk_merge(blk_allocator *blka, blk_meta *blk);
static uint32_t blk_compute_checksum(blk_meta *blk);
static void blk_remove_from_free_list(blk_allocator *blka, blk_meta *blk);

size_t blk_align_size(size_t size)
{
    // If this is already aligned, do nothing.
    if (size % sizeof(long double) == 0)
    {
        return size;
    }

    // Calculate the padding to add.
    size_t padding = sizeof(long double) - size % sizeof(long double);

    // Check for an overflow.
    unsigned int aligned_size;
    if (__builtin_uadd_overflow(size, padding, &aligned_size))
    {
        // If it overflows, return SIZE_MAX.
        return SIZE_MAX;
    }

    // Return the result.
    return aligned_size;
}

static void *blk_new_page(size_t size)
{
    // Compute size neeeded.
    size_t memory_used = PAGE_SIZE;
    size_t memory_needed = 2 * sizeof(blk_meta) + blk_align_size(size);
    while (memory_used < memory_needed)
    {
        memory_used += PAGE_SIZE;
    }

    // Map the memory.
    void *addr = mmap(NULL, memory_used, PROT_FLAGS, MAP_FLAGS, -1, 0);
    if (addr == MAP_FAILED)
    {
        return NULL;
    }

    // Create metadata of the first block.
    blk_meta *blk = addr;

    // Reset all the bytes.
    memset(addr, 0, sizeof(blk_meta));
    blk->size = memory_used - 2 * sizeof(blk_meta);
    blk->is_free = true;

    // Create the last block.
    uint8_t *addr_p = addr;
    addr_p += sizeof(blk_meta) + blk->size;
    blk_meta *page_end_blk = U8_TO_BLK(addr_p);
    memset(addr_p, 0, sizeof(blk_meta));
    page_end_blk->is_free = false;
    page_end_blk->garbage = memory_used;

    // Link blocks.
    page_end_blk->prev = blk;
    blk->next = page_end_blk;

    // Compute the checksums.
    blk->checksum = blk_compute_checksum(blk);
    page_end_blk->checksum = blk_compute_checksum(page_end_blk);

    addr_p = addr;
    return addr_p;
}

void blk_init_allocator(blk_allocator *blka, size_t size)
{
    blka->meta = blk_new_page(size);
    blka->free_list = blka->meta;

    // Compute size neeeded.
    size_t memory_used = PAGE_SIZE;
    size_t memory_needed = 2 * sizeof(blk_meta) + blk_align_size(size);
    while (memory_used < memory_needed)
    {
        memory_used += PAGE_SIZE;
    }

    blka->size = memory_used;
    pthread_mutex_init(&blka->lock, NULL);
}

static void blk_try_free_page(blk_allocator *blka, blk_meta *blk)
{
    bool is_first = false;
    if (blka->meta == blk)
    {
        is_first = true;
    }

    // Check if the whole page is free.
    if (blk->next && blk->next->garbage
        && (!blk->prev || (blk->prev && blk->prev->garbage)))
    {
        // Remove the big block from the free list.
        blk_remove_from_free_list(blka, blk);

        // Remove it from the normal list.
        if (!is_first)
        {
            blk->prev->next = blk->next->next;
            blk->prev->checksum = blk_compute_checksum(blk->prev);
        }
        else
        {
            blka->meta = blk->next->next;
        }

        if (blk->next->next)
        {
            blk->next->next->prev = blk->prev;
            blk->next->next->checksum = blk_compute_checksum(blk->next->next);
        }

        // Unmap memory.
        munmap(BLK_TO_U8(blk), blk->next->garbage);
    }
}

void blk_cleanup_allocator(blk_allocator *blka)
{
    pthread_mutex_destroy(&blka->lock);

    // Get the last block of the previous page.
    blk_meta *page_end_blk = blka->meta;
    while (page_end_blk->next)
    {
        page_end_blk = page_end_blk->next;
    }

    // Save the size of this page and go back 1 time.
    size_t current_page_size = page_end_blk->garbage;
    page_end_blk = page_end_blk->prev;

    // Stop when we are at the first block.
    while (page_end_blk && page_end_blk->prev)
    {
        while (page_end_blk->prev && page_end_blk->size == 0)
        {
            page_end_blk = page_end_blk->prev;
        }

        munmap(page_end_blk->next, current_page_size);

        current_page_size = page_end_blk->garbage;
        page_end_blk = page_end_blk->prev;
    }

    munmap(blka, blka->size);
}

static void __blk_insert_to_free_list(blk_allocator *blka, blk_meta *blk)
{
    // Validate that block isn't already in free list.
    blk_meta *current = blka->free_list;
    while (current)
    {
        if (current == blk)
        {
            blk_remove_from_free_list(blka, blk);
            break;
        }

        current = current->next_free;
    }

    // Insert at the front of the list.
    blk->next_free = blka->free_list;
    if (blka->free_list)
    {
        blka->free_list->prev_free = blk;
        blka->free_list->checksum = blk_compute_checksum(blka->free_list);
    }

    blka->free_list = blk;
}

static void blk_extend_allocator(blk_allocator *blka, size_t size)
{
    // Create a new page.
    blk_meta *new_blk = blk_new_page(size);

    // Get the last block of the previous page.
    blk_meta *last_blk = blka->meta;
    while (last_blk->next)
    {
        last_blk = last_blk->next;
    }

    // Link the two pages.
    last_blk->next = new_blk;
    new_blk->prev = last_blk;

    // Insert new block in the free list.
    __blk_insert_to_free_list(blka, new_blk);

    // Compute the checksums.
    last_blk->checksum = blk_compute_checksum(last_blk);
    new_blk->checksum = blk_compute_checksum(new_blk);
}

void *blk_malloc(blk_allocator *blka, size_t size)
{
    // Align the size.
    size_t aligned_size = blk_align_size(size);

    // Check if all blocks are locked.
    blk_meta *best_blk = blka->free_list;
    if (!best_blk)
    {
        // Extend allocator.
        blk_extend_allocator(blka, size);
        best_blk = blka->free_list;
    }
    else
    {
        // Find the best block (aka. closest size to aligned_size).
        blk_meta *blk = best_blk->next_free;
        while (blk)
        {
            if (blk->size >= aligned_size
                && blk->size - aligned_size > best_blk->size - aligned_size)
            {
                best_blk = blk;
            }

            blk = blk->next_free;
        }

        // Check if we found a block with enough size.
        if (best_blk->size < aligned_size)
        {
            // Extend allocator.
            blk_extend_allocator(blka, size);
            best_blk = blka->free_list;
        }
    }

    // Split the block if it is large enough.
    if (best_blk->size >= aligned_size + sizeof(blk_meta) + MIN_DATA_SIZE)
    {
        // Split
        blk_split(best_blk, aligned_size);

        // Insert the new block into the free list.
        blk_meta *child = best_blk->next;
        __blk_insert_to_free_list(blka, child);
        child->checksum = blk_compute_checksum(child);
    }

    // Remove the block from the free list.
    blk_remove_from_free_list(blka, best_blk);

    // Set current block as reserved.
    best_blk->is_free = false;

    // Compute blk checksum.
    best_blk->checksum = blk_compute_checksum(best_blk);

    // Return the block.
    uint8_t *temp = BLK_TO_U8(best_blk);
    temp += sizeof(blk_meta);
    return temp;
}

void blk_free(blk_allocator *blka, void *ptr)
{
    if (!ptr)
    {
        // Exit.
        return;
    }

    // Get the block header.
    uint8_t *ptr_p = ptr;
    ptr_p -= sizeof(blk_meta);
    ptr = ptr_p;
    blk_meta *blk = ptr;

    // Check for block integrity.
    if (!blk_validate_checksum(blk))
    {
        // Exit.
        return;
    }

    // Mark the block as free.
    blk->is_free = true;

    // Try to merge.
    blk = blk_merge(blka, blk);

    // Insert the new block into the free list.
    __blk_insert_to_free_list(blka, blk);

    // Compute the checksum of the block.
    blk->checksum = blk_compute_checksum(blk);

    blk_try_free_page(blka, blk);
}

void *blk_calloc(blk_allocator *blka, size_t size)
{
    // Call malloc.
    void *ptr = blk_malloc(blka, size);
    if (!ptr)
    {
        return NULL;
    }

    // Set all bytes of data to 0.
    memset(ptr, 0, size);

    // Return the data pointer.
    return ptr;
}

void *blk_realloc(blk_allocator *blka, void *ptr, size_t new_size)
{
    // Get the block header.
    uint8_t *ptr_p = ptr;
    ptr_p -= sizeof(blk_meta);
    blk_meta *blk = U8_TO_BLK(ptr_p);

    // If the block is already big enough, return the same pointer.
    if (blk->size >= new_size)
    {
        return ptr;
    }

    // Check if the next block is free and has enough space to merge.
    if (blk->next && blk->next->is_free)
    {
        size_t total_available = blk->size + blk->next->size + sizeof(blk_meta);

        // If merging with the next block provides enough space, merge and
        // return the same pointer.
        if (total_available >= new_size)
        {
            // Remove the next block from the free list before merging.
            blk_remove_from_free_list(blka, blk->next);

            // Merge the current block with the next block.
            blk->size = total_available;
            blk->next = blk->next->next;

            if (blk->next)
            {
                blk->next->prev = blk;
            }

            // Update the checksum for the merged block.
            blk->checksum = blk_compute_checksum(blk);

            // Return the same pointer, as we were able to resize in place.
            return ptr;
        }
    }

    // Try to merge with the previous block if it's free and can accommodate the
    // new size.
    if (blk->prev && blk->prev->is_free
        && blk->prev->size + blk->size + sizeof(blk_meta) >= new_size)
    {
        // Remove the current block from the free list before merging.
        blk_remove_from_free_list(blka, blk);

        // Merge with the previous block.
        blk_meta *prev = blk->prev;
        prev->size += blk->size + sizeof(blk_meta);

        // Update the linked list.
        prev->next = blk->next;
        if (blk->next)
        {
            blk->next->prev = prev;
        }

        // Now the merged block can be returned.
        return BLK_TO_U8(blk) + sizeof(blk_meta); // Return the new block.
    }

    // If we cannot resize in place, allocate a new block.
    void *new_ptr = blk_malloc(blka, new_size);
    if (!new_ptr)
    {
        return NULL; // Return NULL if allocation fails.
    }

    // Copy the data from the old block to the new block.
    memcpy(new_ptr, ptr, blk->size);

    // Free old block.
    blk_free(blka, ptr);

    // Update the checksum for the freed block.
    blk->checksum = blk_compute_checksum(blk);

    // Return the new pointer.
    return new_ptr;
}

static void blk_split(blk_meta *blk, size_t size)
{
    // Create the new block.
    uint8_t *blk_p = BLK_TO_U8(blk);
    blk_p += size + sizeof(blk_meta);
    memset(blk_p, 0, sizeof(blk_meta));
    blk_meta *new_blk = U8_TO_BLK(blk_p);

    // Initialize the new block.
    new_blk->size = blk->size - size - sizeof(blk_meta);
    new_blk->is_free = true;

    // Insert new_blk into the double linked list.
    new_blk->next = blk->next;
    new_blk->prev = blk;
    if (new_blk->next)
    {
        new_blk->next->prev = new_blk;
        new_blk->next->checksum = blk_compute_checksum(new_blk->next);
    }

    blk->next = new_blk;

    // Adjust blk size.
    blk->size = size;
}

static blk_meta *blk_merge(blk_allocator *blka, blk_meta *blk)
{
    // Merge with the previous block if it's free.
    if (blk->prev && blk->prev->is_free)
    {
        blk_meta *prev = blk->prev;

        // Remove the previous block from the free list before merging.
        blk_remove_from_free_list(blka, prev);

        // Merge the current block into the previous block.
        prev->size += blk->size + sizeof(blk_meta);
        prev->next = blk->next;

        if (blk->next)
        {
            blk->next->prev = prev;
        }

        // Update checksum for the merged previous block.
        prev->checksum = blk_compute_checksum(prev);

        // Point to the merged block.
        blk = prev;
    }

    // Merge with the next block if it's free.
    if (blk->next && blk->next->is_free)
    {
        blk_meta *next = blk->next;

        // Remove the next block from the free list before merging.
        blk_remove_from_free_list(blka, next);

        // Merge the next block into the current block.
        blk->size += next->size + sizeof(blk_meta);
        blk->next = next->next;

        if (next->next)
        {
            next->next->prev = blk;
        }

        // Update checksum for the merged current block.
        blk->checksum = blk_compute_checksum(blk);
    }

    // Return the merged block.
    return blk;
}

static uint32_t blk_compute_checksum(blk_meta *blk)
{
    // Add every fields except the checksum field.
    uint32_t checksum = 0;
    uint8_t *blk_p = BLK_TO_U8(blk);
    for (size_t i = sizeof(uint32_t); i < sizeof(blk_meta); ++i)
    {
        checksum += blk_p[i];
    }

    return checksum;
}

bool blk_validate_checksum(blk_meta *blk)
{
    // Compare actual and newly computed checksum.
    uint32_t current_checksum = blk_compute_checksum(blk);
    return current_checksum == blk->checksum;
}

static void blk_remove_from_free_list(blk_allocator *blka, blk_meta *blk)
{
    if (blka->free_list == blk)
    {
        blka->free_list = blk->next_free;
        if (blka->free_list != NULL)
        {
            blka->free_list->prev_free = NULL;
            blka->free_list->checksum = blk_compute_checksum(blka->free_list);
        }
    }
    else if (blk->prev_free != NULL)
    {
        blk->prev_free->next_free = blk->next_free;
        if (blk->next_free != NULL)
        {
            blk->next_free->prev_free = blk->prev_free;
            blk->next_free->checksum = blk_compute_checksum(blk->next_free);
        }

        blk->prev_free->checksum = blk_compute_checksum(blk->prev_free);
    }

    // Detach blk from the free list.
    blk->next_free = NULL;
    blk->prev_free = NULL;
}
