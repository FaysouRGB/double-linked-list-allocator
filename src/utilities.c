#include "utilities.h"

#include <stdlib.h>

bool utilities_validate_calloc(void *ptr, size_t size)
{
    uint8_t *ptr_p = ptr;
    for (size_t i = 0; i < size; ++i)
    {
        if (ptr_p[i])
        {
            return false;
        }
    }

    return true;
}

bool utilities_blka_snapshot(blk_allocator *blka)
{
    // Track the snapshot number across calls.
    static int number = -1;
    number += 1;

    // Create the filename.
    size_t size = snprintf(NULL, 0, "%i.snapshot", number);
    char *filename = malloc(size + 1);
    if (!filename)
    {
        return false;
    }

    snprintf(filename, size + 1, "%i.snapshot", number);

    // Create the snapshot file.
    FILE *fd = fopen(filename, "w");
    free(filename);
    if (!fd)
    {
        return false;
    }

    // Write logs.
    utilities_print_allocator(blka, fd);
    utilities_print_blocks(blka, fd);

    fclose(fd);

    return true;
}

void utilities_print_block(blk_meta *blk, size_t index, FILE *fd)
{
    void *blk_p = blk;
    void *data_p = blk->data;
    void *next_p = blk->next;
    void *prev_p = blk->prev;
    void *next_free_p = blk->next_free;
    void *prev_free_p = blk->prev_free;

    fprintf(fd, "\n┏━━━━━━━━━━━━━━━━━━━╸BLOCK╺━━━━━━━━━━━━━━━━━━━┓\n");
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Address", blk_p);
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Address Aligned",
            IS_ALIGNED(blk_p) ? "Yes" : "No");
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "Allocator Index", index);
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Free", blk->is_free ? "Yes" : "No");
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Next", next_p);
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Prev", prev_p);
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Next Free", next_free_p);
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Prev Free", prev_free_p);
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20u ┃\n", "Checksum", blk->checksum);
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Checksum Valid",
            blk_validate_checksum(blk) ? "Yes" : "No");
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "Size (bytes)", blk->size);
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Data", data_p);
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Data Aligned",
            IS_ALIGNED(data_p) ? "Yes" : "No");
    fprintf(fd, "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n\n");
}

void utilities_print_blocks(blk_allocator *blka, FILE *fd)
{
    blk_meta *current = blka->meta;
    size_t index = 0;
    while (current->next)
    {
        utilities_print_block(current, index, fd);
        fprintf(fd, "  %-20s ⇅ %-20s  \n", " ", " ");
        current = current->next;
        ++index;
    }

    utilities_print_block(current, index, fd);
}

int utilities_number_of_blocks(blk_allocator *blka)
{
    blk_meta *current = blka->meta;
    int number = 0;
    while (current)
    {
        ++number;
        current = current->next;
    }

    return number;
}

int utilities_number_of_free_blocks(blk_allocator *blka)
{
    blk_meta *current = blka->meta;
    int number = 0;
    while (current)
    {
        if (current->is_free)
        {
            ++number;
        }

        current = current->next;
    }

    return number;
}

bool utilities_validate_allocator_size(blk_allocator *blka)
{
    size_t total_memory = sizeof(blk_allocator);
    struct blk_meta *current = blka->meta;
    while (current)
    {
        total_memory += sizeof(blk_meta) + current->size;
        current = current->next;
    }

    return total_memory == blka->size;
}

float utilities_memory_footprint(blk_allocator *blka)
{
    size_t data_memory = 0;
    size_t total_needed_memory = sizeof(blk_allocator);
    struct blk_meta *current = blka->meta;
    while (current)
    {
        total_needed_memory += sizeof(blk_meta) + current->size;
        data_memory += current->size;
        current = current->next;
    }

    return data_memory / (total_needed_memory);
}

bool utilities_validate_list(blk_allocator *blka)
{
    blk_meta *prev = blka->meta;
    if (prev->prev)
    {
        return false;
    }

    for (blk_meta *current = prev->next; current; current = current->next)
    {
        if (current->prev != prev)
        {
            return false;
        }

        prev = current;
    }

    if (prev->next)
    {
        return false;
    }

    return true;
}

bool utilities_validate_free_list(blk_allocator *blka)
{
    blk_meta *prev = blka->free_list;
    if (!prev || prev->prev_free || !prev->is_free)
    {
        return false;
    }

    for (blk_meta *current = prev->next_free; current;
         current = current->next_free)
    {
        if (current->prev_free != prev || !current->is_free)
        {
            return false;
        }

        prev = current;
    }

    return prev->next_free ? false : true;
}

void utilities_print_allocator(blk_allocator *blka, FILE *fd)
{
    void *blka_p = blka;
    void *meta_p = blka->meta;
    void *free_list_p = blka->free_list;

    fprintf(fd, "\n┏━━━━━━━━━━━━━━━━╸ ALLOCATOR ╺━━━━━━━━━━━━━━━━┓\n");
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Address", blka_p);
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Address Aligned",
            IS_ALIGNED(blka) ? "Yes" : "No");
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Meta", meta_p);
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Free List", free_list_p);
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20i ┃\n", "Blocks",
            utilities_number_of_blocks(blka));
    fprintf(fd, "┃ %-20s : %-20i ┃\n", "Free Blocks",
            utilities_number_of_free_blocks(blka));
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "Size (bytes)", blka->size);
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Size Valid",
            utilities_validate_allocator_size(blka) ? "Yes" : "No");
    fprintf(fd, "┃ %-20s : %06.2f%-14s ┃\n", "Wasted Memory",
            utilities_memory_footprint(blka) * 100, "%");
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "List Valid",
            utilities_validate_list(blka) ? "Yes" : "No");
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Free List Valid",
            utilities_validate_free_list(blka) ? "Yes" : "No");
    fprintf(fd, "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n\n");
}

void utilities_print_sizes(FILE *fd)
{
    char data[] = { 0 };
    fprintf(fd, "\n┏━━━━━━━━━━━━━━━━━━╸ SIZES ╺━━━━━━━━━━━━━━━━━━┓\n");
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "uint32_t", sizeof(uint32_t));
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "struct blk_meta *",
            sizeof(struct blk_meta *));
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "size_t", sizeof(size_t));
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "bool", sizeof(bool));
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "char data[]", sizeof(data));
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "pthread_mutex_t",
            sizeof(pthread_mutex_t));
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "blk_allocator", sizeof(blk_allocator));
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "blk_meta", sizeof(blk_meta));
    fprintf(fd, "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n\n");
}
