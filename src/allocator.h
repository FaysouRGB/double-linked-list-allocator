#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

/// @brief Macro to get system page size.
#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)

/// @brief Macro that define the minimum size of a block.
#define MIN_DATA_SIZE sizeof(long double)

/// @brief Macro that define mmap() protection flag.
#define PROT_FLAGS (PROT_READ | PROT_WRITE)

/// @brief Macro that define mmap() map flag.
#define MAP_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE)

struct blk_meta
{
    // Checksum
    uint32_t checksum;

    // Double normal linked list
    struct blk_meta *next;
    struct blk_meta *prev;

    // Double linked free list
    struct blk_meta *next_free;
    struct blk_meta *prev_free;

    // Block info
    size_t size;
    size_t garbage;
    bool is_free;
};

typedef struct blk_meta blk_meta;

struct blk_allocator
{
    // Double linked lists
    struct blk_meta *meta;
    struct blk_meta *free_list;

    // Allocator info
    pthread_mutex_t lock;
    size_t size;
};

typedef struct blk_allocator blk_allocator;

/// @brief Allocate a page and setup it.
/// @param size The size needed for this page.
/// @param first_one If set to yes, create a block allocator at the start of the
/// page.
/// @return Return the address of the first block or the block allocator.
/// static void *blk_new_page(size_t size, bool first_one);

/// @brief Align the size.
/// @param size The size value.
/// @return The greatest multiple of sizeof(long double).
/// static size_t blk_align_size(size_t size);

/// @brief Initialize the allocator.
/// @param size The size it should be able to hold directly.
/// @return A new block allocator.
blk_allocator *blk_init_allocator(void);

/// @brief Try to free the page of blk.
/// @param blka The block allocator.
/// @param blk The block.
/// static void blk_try_free_page(blk_allocator *blka, blk_meta *blk);

/// @brief Unmap the memory of the allocator. It should not be used afterwards.
/// @param blka The block allocator to destroy.
void blk_cleanup_allocator(blk_allocator *blka);

/// @brief Extend the memory mapped to this allocator.
/// @param blka The block allocator.
/// @param size The size of the new block.
/// static void blk_extend_allocator(blk_allocator *blka, size_t size);

/// @brief Allocate a block to the caller.
/// @param blka The block allocator.
/// @param size The size of the block.
/// @return A pointer to a region where the caller can write.
void *blk_malloc(blk_allocator *blka, size_t size);

/// @brief Free the block of the region ptr returned by a blk_malloc(2).
/// @param blka The block allocator.
/// @param ptr A pointer previously returned by blk_malloc(2).
void blk_free(blk_allocator *blka, void *ptr);

/// @brief Allocate a block to the caller. Set all bytes to 0.
/// @param blka The block allocator.
/// @param size The size of the block.
/// @return A pointer to a region initialized to 0 where the caller can write.
void *blk_calloc(blk_allocator *blka, size_t size);

/// @brief Shrink or expand the block associated with ptr.
/// @param blka The block allocator.
/// @param ptr A pointer previously returned by blk_malloc(2).
/// @param new_size The new size of the block.
/// @return A pointer to a region where the caller can write.
void *blk_realloc(blk_allocator *blka, void *ptr, size_t new_size);

/// @brief Split a block in two.
/// @param blk The block to split.
/// @param size The size of the first block.
/// static void blk_split(blk_meta *blk, size_t size);

/// @brief Merge free neighbor of blk with blk.
/// @param blka The block allocator.
/// @param blk The block we try to merge with its neighbor.
/// @return Same block or the one before if we merged with.
/// static blk_meta *blk_merge(blk_allocator *blka, blk_meta *blk);

/// @brief Compute the block checksum.
/// @param blk The block to compute its checksum.
/// @return The checksum of the block.
/// static uint32_t blk_compute_checksum(blk_meta *blk);

/// @brief Validate the checksum of the block.
/// @param blk The block to validate.
/// @return True if it matches, false otherwise.
/// static bool blk_validate_checksum(blk_meta *blk);

/// @brief Remove the block from the free list.
/// @param blka The block allocator.
/// @param blk The block to remove.
/// static void blk_remove_from_free_list(blk_allocator *blka, blk_meta *blk);

#endif /* ! ALLOCATOR_H */
