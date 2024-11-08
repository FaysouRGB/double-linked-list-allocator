#include "allocator.h"

// Global allocator.
static blk_allocator *blka;

__attribute__((visibility("default"))) void *malloc(size_t size)
{
    // Check if we need to create an allocator.
    if (!blka)
    {
        blka = blk_init_allocator();
    }

    // Lock the mutex.
    pthread_mutex_lock(&blka->lock);

    // Call blk_malloc.
    void *ptr = blk_malloc(blka, size);

    // Unlock the mutex.
    pthread_mutex_unlock(&blka->lock);

    return ptr;
}

__attribute__((visibility("default"))) void free(void *ptr)
{
    // If no allocator, do nothing.
    if (!blka)
    {
        return;
    }

    // Lock the mutex.
    pthread_mutex_lock(&blka->lock);

    // Call blk_free.
    blk_free(blka, ptr);

    // Unlock the mutex.
    pthread_mutex_unlock(&blka->lock);
}

__attribute__((visibility("default"))) void *realloc(void *ptr, size_t size)
{
    // Check if we need to create an allocator.
    if (!blka)
    {
        blka = blk_init_allocator();
    }

    // Lock the mutex.
    pthread_mutex_lock(&blka->lock);

    // If no ptr, realloc = malloc.
    if (!ptr)
    {
        // Call blk_malloc.
        ptr = blk_malloc(blka, size);

        // Unlock the mutex.
        pthread_mutex_unlock(&blka->lock);

        return ptr;
    }

    // If size = 0, realloc = free.
    if (!size)
    {
        // Call blk_free.
        blk_free(blka, ptr);

        // Unlock the mutex.
        pthread_mutex_unlock(&blka->lock);

        return ptr;
    }

    // Call blk_realloc.
    ptr = blk_realloc(blka, ptr, size);

    // Unlock the mutex.
    pthread_mutex_unlock(&blka->lock);

    return ptr;
}

__attribute__((visibility("default"))) void *calloc(size_t nmemb, size_t size)
{
    if (!blka)
    {
        blka = blk_init_allocator();
    }

    // Lock the mutex.
    pthread_mutex_lock(&blka->lock);

    // Check for an overflow.
    unsigned int total_size;
    if (__builtin_umul_overflow(nmemb, size, &total_size))
    {
        // If it overflows, return null.
        return NULL;
    }

    void *ptr = blk_calloc(blka, nmemb * size);

    // Unlock the mutex.
    pthread_mutex_unlock(&blka->lock);

    return ptr;
}
