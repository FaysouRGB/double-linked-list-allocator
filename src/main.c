#include <stdio.h>
#include <string.h>

#include "allocator.h"
#include "utilities.h"

void snapshot(blk_allocator *blka)
{
    // Name buffer.
    char buffer[1024];

    // Snapshot number.
    static int number = -1;
    number += 1;

    // Create a file.
    snprintf(buffer, 1024, "%i.snapshot", number);
    FILE *fd = fopen(buffer, "w");

    // Write to file.
    utilities_print_allocator(blka, fd);
    utilities_print_all_blocks(blka, fd);

    // Close file.
    fclose(fd);
}

int main()
{
    blk_allocator *blka = blk_init_allocator(5);
    snapshot(blka);

    char *p1 = blk_malloc(blka, 5);
    snapshot(blka);

    char *p2 = blk_malloc(blka, 100);
    snapshot(blka);

    blk_free(blka, p2);
    snapshot(blka);

    // Now let's free.
    blk_free(blka, p1);
    snapshot(blka);

    p2 = blk_calloc(blka, 300);
    for (size_t i = 0; i < 300; ++i)
    {
        if (p2[i])
        {
            printf("calloc() don't work lol.\n");
        }
    }

    p1 = blk_malloc(blka, PAGE_SIZE * 2);
    snapshot(blka);

    blk_free(blka, p2);
    blk_free(blka, p1);
    blk_cleanup_allocator(blka);
}
