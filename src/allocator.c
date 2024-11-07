#include "allocator.h"

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

#include "convert.h"
#include "utilities.h"

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

blk_allocator *blk_init_allocator(size_t size)
{
    size_t aligned_size = blk_align_size(size);
    size_t to_allocate = PAGE_SIZE;
    while (to_allocate
           < aligned_size + sizeof(blk_meta) + sizeof(blk_allocator))
    {
        to_allocate += PAGE_SIZE;
    }

    // Allocate a default memory page.
    void *addr = mmap(NULL, to_allocate, PROT_FLAGS, MAP_FLAGS, -1, 0);

    // Initialize size and mutex.
    blk_allocator *blka = addr;
    blka->size = to_allocate;

    // Check if we can't initialize the mutex.
    if (pthread_mutex_init(&blka->lock, NULL))
    {
        munmap(addr, to_allocate);
        return NULL;
    }

    // Create the metadata of the first block.
    uint8_t *blka_p = BLKA_TO_U8(blka);
    blka_p += sizeof(blk_allocator);
    struct blk_meta *blk = U8_TO_BLK(blka_p);

    // Initialize the first block.
    blk->prev = NULL;
    blk->next = NULL;
    blk->prev_free = NULL;
    blk->next_free = NULL;

    blk->size = to_allocate - sizeof(blk_allocator) - sizeof(blk_meta);
    blk->is_free = true;
    blk->checksum = blk_compute_checksum(blk);

    // Link the first block.
    blka->free_list = blk;
    blka->meta = blk;

    // Return the allocator.
    return blka;
}

void blk_cleanup_allocator(blk_allocator *blka)
{
    // Unmap all the memory associated to the allocator.
    pthread_mutex_destroy(&blka->lock);
    munmap(blka, blka->size);
}

void blk_extend_allocator(blk_allocator *blka, size_t size)
{
    size_t aligned_size = blk_align_size(size);
    size_t to_allocate = PAGE_SIZE;
    while (to_allocate
           < aligned_size + sizeof(blk_meta) + sizeof(blk_allocator))
    {
        to_allocate += PAGE_SIZE;
    }

    blk_meta *new_blk = mmap(NULL, to_allocate, PROT_FLAGS, MAP_FLAGS, -1, 0);
    if (new_blk == MAP_FAILED)
    {
        return;
    }

    new_blk->size = to_allocate - sizeof(blk_meta);
    new_blk->is_free = true;
    new_blk->prev = NULL;
    new_blk->next = NULL;
    new_blk->prev_free = NULL;
    new_blk->next_free = NULL;

    if (blka->free_list)
    {
        blka->free_list->prev_free = new_blk;
        new_blk->next_free = blka->free_list;
    }

    blka->free_list = new_blk;
    new_blk->checksum = blk_compute_checksum(new_blk);

    blka->size += to_allocate;
}

void *blk_malloc(blk_allocator *blka, size_t size)
{
    // Align the size.
    size_t aligned_size = blk_align_size(size);

    blk_meta *best_blk = blka->free_list;

    // Check if all blocks are locked.
    if (!best_blk)
    {
        // Extend allocator.
        blk_extend_allocator(blka, size);
        best_blk = blka->free_list;
    }

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

    // Split the block if it is large enough.
    if (best_blk->size >= aligned_size + sizeof(blk_meta) + MIN_DATA_SIZE)
    {
        // Split
        blk_split(best_blk, aligned_size);

        // Push front new_blk into the double linked free list.
        blk_meta *child = best_blk->next;
        child->next_free = blka->free_list;
        child->prev_free = NULL;

        if (blka->free_list)
        {
            blka->free_list->prev_free = child;
        }

        blka->free_list = child;
    }

    // Remove the block from the free list.
    blk_remove_from_free_list(blka, best_blk);

    // Set current block as reserved.
    best_blk->is_free = false;

    // Compute blk checksum.
    best_blk->checksum = blk_compute_checksum(best_blk);

    // Return the block.
    return best_blk->data - 1;
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
    ptr_p -= sizeof(blk_meta) - sizeof(char *) + 1;
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

    // Push front the block into the double linked free list.
    blk->prev_free = NULL;
    if (blka->free_list)
    {
        blk->next_free = blka->free_list;
        blka->free_list->prev_free = blk;
    }
    else
    {
        blk->next_free = NULL;
    }

    blka->free_list = blk;

    // Prevent double free.
    blk->checksum = blk_compute_checksum(blk);
}

void *blk_calloc(blk_allocator *blka, size_t size)
{
    // Call malloc.
    size_t aligned_size = blk_align_size(size);
    void *ptr = blk_malloc(blka, aligned_size);
    if (!ptr)
    {
        return NULL;
    }

    // Set all bytes of data to 0.
    memset(ptr, 0, aligned_size);

    // Return the data pointer.
    return ptr;
}

void *blk_realloc(blk_allocator *blka, void *ptr, size_t new_size)
{
    // Get the block header.
    uint8_t *ptr_p = ptr;
    ptr_p -= sizeof(blk_meta) - sizeof(char *) + 1;
    void *temp = ptr_p;
    blk_meta *blk = temp;

    // If the block is big enough, do nothing.
    if (blk->size >= new_size)
    {
        // Return ptr.
        return ptr;
    }

    // If the block that follows is free, merge and allocate enough.
    if (blk->next && blk->next->is_free)
    {
        // Merge with block after me.
        blk = blk_merge(blka, blk);

        // Update checksum.
        blk->checksum = blk_compute_checksum(blk);

        // Then split the block.
        blk_split(blk, new_size);

        // Push front new_blk into the double linked free list.
        blk_meta *child = blk->next;
        child->next_free = blka->free_list;
        child->prev_free = NULL;

        if (blka->free_list)
        {
            blka->free_list->prev_free = child;
        }

        blka->free_list = child;

        // Return ptr.
        return ptr;
    }

    // If the block that preceed is free, merge and allocate enough.
    if (blk->prev && blk->prev->is_free)
    {
        // Merge with block after me.
        blk = blk_merge(blka, blk);

        // Then split the block.
        blk_split(blk, new_size);

        // Push front new_blk into the double linked free list.
        blk_meta *child = blk->next;
        child->next_free = blka->free_list;
        child->prev_free = NULL;

        if (blka->free_list)
        {
            blka->free_list->prev_free = child;
        }

        blka->free_list = child;

        // Return ptr.
        return ptr;
    }

    // Else sadly we need to find a new place.
    void *new_ptr = blk_malloc(blka, new_size);

    // Copy old block data to the new one.
    memcpy(new_ptr, ptr, blk->size);

    // Set the old block to free.
    blk_free(blka, blk);

    // Return the data pointer.
    return ptr;
}

void blk_split(blk_meta *blk, size_t size)
{
    // Create the new block.
    uint8_t *blk_p = BLK_TO_U8(blk);
    blk_p += size + sizeof(blk_meta);
    blk_meta *new_blk = U8_TO_BLK(blk_p);

    // Initialize the new block.
    new_blk->size = blk->size - size - sizeof(blk_meta);
    new_blk->is_free = true;

    // Insert new_blk into the double linked list.
    new_blk->next = blk->next;
    new_blk->prev = blk;
    if (blk->next)
    {
        blk->next->prev = new_blk;
    }

    blk->next = new_blk;

    // Adjust blk size.
    blk->size = size;

    // Compute new_blk checksum.
    new_blk->checksum = blk_compute_checksum(new_blk);
}

blk_meta *blk_merge(blk_allocator *blka, blk_meta *blk)
{
    // Check if next and prev are continuous in memory.
    uint8_t *blk_p = BLK_TO_U8(blk);
    uint8_t *next_p = BLK_TO_U8(blk->next);
    uint8_t *prev_p = BLK_TO_U8(blk->prev);

    // Merge with the previous block if it's free.
    if (blk->prev && blk->prev->is_free
        && prev_p == blk_p - blk->prev->size - sizeof(blk_meta))
    {
        // Remove the this block from the free list.
        blk_remove_from_free_list(blka, blk);

        // Merge with the previous block.
        blk_meta *prev = blk->prev;
        prev->next = blk->next;
        if (blk->next)
        {
            blk->next->prev = prev;
        }

        prev->size += blk->size + sizeof(blk_meta);

        // Corrupt checksum just for security (free called on a merge one).
        blk->checksum = MIN_DATA_SIZE - 1;
        blk = prev;
    }

    // Merge with the next block if it's free.
    if (blk->next && blk->next->is_free
        && next_p == blk_p + blk->size + sizeof(blk_meta))
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

        // Corrupt checksum just for security (free called on a merge one).
        next->checksum = MIN_DATA_SIZE - 1;
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
    else if (blk->prev_free && blk->next_free)
    {
        blk->prev_free->next_free = blk->next_free;
        blk->next_free->prev_free = blk->prev_free;
    }
    // Case III: at the end of the list.
    else
    {
        blk->prev_free->next_free = NULL;
    }

    // Detach blk from the free list.
    blk->next_free = NULL;
    blk->prev_free = NULL;
}
