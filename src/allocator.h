#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_SIZE sysconf(_SC_PAGE_SIZE)
#define MIN_DATA_SIZE sizeof(long double)
#define PROT_FLAGS (PROT_READ | PROT_WRITE)
#define MAP_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE)

struct blk_meta
{
    // Checksum
    uint32_t checksum;

    // Double linked list
    struct blk_meta *next;
    struct blk_meta *prev;

    // Double linked free list
    struct blk_meta *next_free;
    struct blk_meta *prev_free;

    // Block info
    size_t size;
    bool is_free;
    char data[];
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

size_t blk_align_size(size_t size);

blk_allocator *blk_init_allocator(size_t size);

void blk_cleanup_allocator(blk_allocator *blka);

void blk_extend_allocator(blk_allocator *blka, size_t size);

void *blk_malloc(blk_allocator *blka, size_t size);

void blk_free(blk_allocator *blka, void *ptr);

void *blk_calloc(blk_allocator *blka, size_t size);

void *blk_realloc(blk_allocator *blka, void *ptr, size_t new_size);

void blk_split(blk_meta *blk, size_t size);

blk_meta *blk_merge(blk_allocator *blka, blk_meta *blk);

uint32_t blk_compute_checksum(blk_meta *blk);

bool blk_validate_checksum(blk_meta *blk);

void blk_remove_from_free_list(blk_allocator *blka, blk_meta *blk);

#endif /* ! ALLOCATOR_H */
