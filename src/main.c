#include "utilities.h"

#define P1_SIZE 50
#define P2_SIZE 120

int main(void)
{
    utilities_print_sizes(stdout);

    // Snapshot 0.
    // Initialize the allocator.
    blk_allocator *blka = blk_init_allocator(5);
    if (!blka || !utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("blk_init_allocator or utilities_blka_snapshot failed.");
        goto error;
    }

    // Snapshot 1.
    // Malloc p1.
    char *p1 = blk_malloc(blka, P1_SIZE);
    if (!p1 || !utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("blk_malloc for p1 or utilities_blka_snapshot failed.");
        goto error;
    }

    // Snapshot 2.
    // Malloc p2.
    char *p2 = blk_malloc(blka, P2_SIZE);
    if (!p2 || !utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("blk_malloc for p2 or utilities_blka_snapshot failed.");
        goto error;
    }

    // Snapshot 3.
    // Free p2.
    blk_free(blka, p2);
    if (!utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("utilities_blka_snapshot failed.");
        goto error;
    }

    // Snapshot 4.
    // Realloc p1.
    p1 = blk_realloc(blka, p1, P1_SIZE * 4);
    if (!p1 || !utilities_blka_snapshot(blka))
    {
        PRINT_ERROR("blk_realloc for p1 or utilities_blka_snapshot failed.");
        goto error;
    }

    // Snapshot 5.
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

    // Snapshot 6.
    // Free all.
    blk_free(blka, p1);
    blk_free(blka, p2);
    blk_free(blka, p3);
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
