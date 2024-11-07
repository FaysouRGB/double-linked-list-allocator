#include "my_malloc.h"

#include <stddef.h>

#include "allocator.h"

static blk_allocator *blka;

void *my_malloc(size_t size)
{
    if (!blka)
    {
        blka = blk_init_allocator(size);
    }

    // Lock the mutex.
    pthread_mutex_lock(&blka->lock);

    return blk_malloc(blka, size);

    // Unlock the mutex.
    pthread_mutex_unlock(&blka->lock);
}
void my_free(void *ptr)
{
    if (!blka)
    {
        return;
    }

    // Lock the mutex.
    pthread_mutex_lock(&blka->lock);

    blk_free(blka, ptr);

    // Unlock the mutex.
    pthread_mutex_unlock(&blka->lock);
}

void *my_realloc(void *ptr, size_t size)
{
    if (!blka)
    {
        blka = blk_init_allocator(size);
    }

    // Lock the mutex.
    pthread_mutex_lock(&blka->lock);

    if (!ptr)
    {
        ptr = blk_malloc(blka, size);

        // Unlock the mutex.
        pthread_mutex_unlock(&blka->lock);

        return ptr;
    }

    if (!size)
    {
        blk_free(blka, ptr);

        // Unlock the mutex.
        pthread_mutex_unlock(&blka->lock);

        return ptr;
    }

    ptr = blk_realloc(blka, ptr, size);

    // Unlock the mutex.
    pthread_mutex_unlock(&blka->lock);

    return ptr;
}

void *my_calloc(size_t nmemb, size_t size)
{
    if (!blka)
    {
        blka = blk_init_allocator(size);
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
