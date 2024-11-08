#include "utilities.h"

#define P1_SIZE 1000
#define P2_SIZE 5000

int main(void)
{
    // utilities_print_sizes(stdout);

    // Initialize the allocator.
    blk_allocator *blka = blk_init_allocator();

    // Snapshot 0.
    if (!blka || !utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("blk_init_allocator or utilities_blka_snapshot failed.");
        goto error;
    }

    // Malloc p1.
    char *p1 = blk_malloc(blka, P1_SIZE);

    // Snapshot 1.
    if (!p1 || !utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("blk_malloc for p1 or utilities_blka_snapshot failed.");
        goto error;
    }

    // Malloc p2.
    char *p2 = blk_malloc(blka, P2_SIZE);

    // Snapshot 2.
    if (!p2 || !utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("blk_malloc for p2 or utilities_blka_snapshot failed.");
        goto error;
    }

    // Free p2.
    blk_free(blka, p2);

    // Snapshot 3.
    if (!utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("utilities_blka_snapshot failed.");
        goto error;
    }

    // Realloc p1.
    p1 = blk_realloc(blka, p1, P1_SIZE * 4);

    // Snapshot 4.
    if (!p1 || !utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("blk_realloc for p1 or utilities_blka_snapshot failed.");
        goto error;
    }

    // Calloc p2.
    p2 = blk_calloc(blka, P2_SIZE * 2);

    if (!p2 || !utilities_validate_calloc(p2, P2_SIZE * 2))
    {
        PRINT_ERROR("blk_calloc for p2 or utilities_blka_snapshot failed.");
        goto error;
    }

    // Malloc PAGE_SIZE * 2.
    char *p3 = blk_malloc(blka, PAGE_SIZE * 2);
    if (!p3 || !utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("blk_malloc for p3 or utilities_blka_snapshot failed.");
        goto error;
    }

    // Free all.
    blk_free(blka, p1);
    blk_free(blka, p2);
    blk_free(blka, p3);

    // Snapshot 6.
    if (!utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("utilities_blka_snapshot failed.");
        goto error;
    }

    // Clean up and return success.
    blk_cleanup_allocator(blka);
    return 0;

error:
    // Clean up and return error.
    blk_cleanup_allocator(blka);
    return 1;
}
