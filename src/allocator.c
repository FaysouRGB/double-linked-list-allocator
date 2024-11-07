#include "allocator.h"

#include <string.h>

#include "convert.h"
#include "utilities.h"

// TODO: Free unused maps.

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

void *blk_new_page(size_t size, bool first_one)
{
    // Compute size neeeded.
    size_t memory_used = PAGE_SIZE;
    size_t memory_needed = 2 * sizeof(blk_meta) + blk_align_size(size)
        + (first_one ? sizeof(blk_allocator) : 0);
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

    // If it is the first one, initialize the block allocator.
    if (first_one)
    {
        blk_allocator *blka = addr;

        // Reset all bytes of the struct.
        memset(addr, 0, sizeof(blk_allocator));

        // Linked lists.
        uint8_t *addr_p = addr;
        addr_p += sizeof(blk_allocator);
        blka->meta = U8_TO_BLK(addr_p);
        blka->free_list = blka->meta;

        // Info.
        pthread_mutex_init(&blka->lock, NULL);
        blka->size = memory_used;

        // Set the start at the first block.
        addr = blka->meta;
    }

    // Create metadata of the first block.
    blk_meta *blk = addr;

    // Reset all the bytes.
    memset(addr, 0, sizeof(blk_meta));
    blk->size = memory_used - (first_one ? sizeof(blk_allocator) : 0)
        - 2 * sizeof(blk_meta);
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
    return addr_p - (first_one ? sizeof(blk_allocator) : 0);
}

blk_allocator *blk_init_allocator(size_t size)
{
    return blk_new_page(4096, true);
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

void __blk_insert_to_free_list(blk_allocator *blka, blk_meta *blk)
{
    blk->next_free = blka->free_list;
    blk->prev_free = NULL;

    if (blka->free_list)
    {
        blka->free_list->prev_free = blk;
        blka->free_list->checksum = blk_compute_checksum(blka->free_list);
    }

    blka->free_list = blk;
}

void blk_extend_allocator(blk_allocator *blka, size_t size)
{
    // Create a new page.
    blk_meta *new_blk = blk_new_page(size, false);

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
    void *temp = ptr_p;
    blk_meta *blk = temp;

    // If the block is big enough, do nothing.
    if (blk->size >= new_size)
    {
        // Return ptr.
        return ptr;
    }

    // If a neighbor is free, merge (and split).
    if ((blk->next && blk->next->is_free) || (blk->prev && blk->prev->is_free))
    {
        // Merge.
        blk = blk_merge(blka, blk);

        // If we can split, split.
        size_t aligned_size = blk_align_size(new_size);
        if (blk->size >= aligned_size + sizeof(blk_meta) + MIN_DATA_SIZE)
        {
            // Split
            blk_split(blk, aligned_size);

            // Insert the new block into the free list.
            blk_meta *child = blk->next;
            __blk_insert_to_free_list(blka, child);
            child->checksum = blk_compute_checksum(child);
        }

        // Compute the block's checksum.
        blk->checksum = blk_compute_checksum(blk);

        // Return ptr back or a new pointer.
        return BLK_TO_U8(blk) + sizeof(blk_meta);
    }

    // Else sadly we need to find a new place.
    void *new_ptr = blk_malloc(blka, new_size);

    // Copy old block data to the new one.
    memcpy(new_ptr, ptr, blk->size);

    // Set the old block to free.
    blk_free(blka, blk);

    // Compute the block's checksum.
    blk->checksum = blk_compute_checksum(blk);

    // Return the data pointer.
    return new_ptr;
}

void blk_split(blk_meta *blk, size_t size)
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

blk_meta *blk_merge(blk_allocator *blka, blk_meta *blk)
{
    // Merge with the previous block if it's free.
    if (blk->prev && blk->prev->is_free)
    {
        // Remove the this block from the free list.
        blk_remove_from_free_list(blka, blk);

        // Merge with the previous block.
        blk_meta *prev = blk->prev;
        prev->next = blk->next;
        if (prev->next)
        {
            prev->next->prev = prev;
        }

        prev->size += blk->size + sizeof(blk_meta);
        blk = prev;
    }

    // Merge with the next block if it's free.
    if (blk->next && blk->next->is_free)
    {
        // Remove the next block from the free list.
        blk_remove_from_free_list(blka, blk->next);

        // Merge with the next block.
        blk_meta *next = blk->next;
        blk->next = next->next;
        if (next->next)
        {
            next->next->prev = blk;
        }

        blk->size += next->size + sizeof(blk_meta);
    }

    // Return the block.
    return blk;
}

uint32_t blk_compute_checksum(blk_meta *blk)
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

void blk_remove_from_free_list(blk_allocator *blka, blk_meta *blk)
{
    // Case I: first block of the free list.
    if (blka->free_list == blk)
    {
        blka->free_list = blk->next_free;
    }
    // Case II: in the middle of the list.
    else if (blk->next_free)
    {
        blk->prev_free->next_free = blk->next_free;
        blk->prev_free->checksum = blk_compute_checksum(blk->prev_free);

        blk->next_free->prev_free = blk->prev_free;
        blk->next_free->checksum = blk_compute_checksum(blk->next_free);
    }
    // Case III: at the end of the list.
    else
    {
        blk->prev_free->next_free = NULL;
        blk->prev_free->checksum = blk_compute_checksum(blk->prev_free);
    }

    // Detach blk from the free list.
    blk->next_free = NULL;
    blk->prev_free = NULL;
}
