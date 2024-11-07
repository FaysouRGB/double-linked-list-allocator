#include "utilities.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "allocator.h"

uint8_t *utilities_blka_uint(blk_allocator *blka)
{
    void *blka_void_p = blka;
    return blka_void_p;
}

uint8_t *utilities_blk_uint(blk_meta *blk)
{
    void *blka_void_p = blk;
    return blka_void_p;
}

blk_allocator *utilities_uint_blka(uint8_t *blka)
{
    void *blka_void_p = blka;
    return blka_void_p;
}

blk_meta *utilities_uint_blk(uint8_t *blk)
{
    void *blka_void_p = blk;
    return blka_void_p;
}

static int utilities_number_of_blocks(blk_allocator *blka)
{
    int count = 0;
    struct blk_meta *blk = blka->meta;
    while (blk)
    {
        ++count;
        blk = blk->next;
    }

    return count;
}

static float utilities_memory_footprint(blk_allocator *blka)
{
    size_t data_memory = 0;
    size_t total_needed_memory = 0;
    struct blk_meta *blk = blka->meta;
    while (blk)
    {
        total_needed_memory += sizeof(blk_meta) + blk->size;
        data_memory += blk->size;
        blk = blk->next;
    }

    return data_memory / (total_needed_memory + sizeof(blk_allocator));
}

static int utilities_block_number(blk_allocator *blka, blk_meta *blk)
{
    int number = 0;
    blk_meta *meta = blka->meta;
    while (meta != blk)
    {
        number += 1;
        meta = meta->next;
    }

    return number;
}

static int utilities_free_number(blk_allocator *blka)
{
    int number = 0;
    blk_meta *meta = blka->meta;
    while (meta)
    {
        if (meta->is_free)
        {
            number += 1;
        }

        meta = meta->next;
    }

    return number;
}

static int utilities_validate_lists(blk_allocator *blka)
{
    // Check that lists are the same both ways and that they are bounded by
    // null.
    // Validate double linked list.
    blk_meta *prev = blka->meta;
    if (prev->prev)
    {
        return 0;
    }

    blk_meta *current = prev->next;
    while (current)
    {
        if (current->prev != prev)
        {
            return 0;
        }

        prev = current;
        current = current->next;
    }

    if (prev->next)
    {
        return 0;
    }

    // Validate double linked free list.
    prev = blka->free_list;
    if (!prev)
    {
        return 0;
    }

    if (prev->prev_free || prev->is_free == false)
    {
        return 0;
    }

    current = prev->next_free;
    while (current)
    {
        if (current->prev != prev || current->is_free == false)
        {
            return 0;
        }

        prev = current;
        current = current->next;
    }

    if (prev->next_free)
    {
        return 0;
    }

    return 1;
}

static int is_aligned(void *ptr)
{
    return ((uintptr_t)ptr % sizeof(long double)) == 0;
}

void utilities_print_allocator(blk_allocator *blka, FILE *fd)
{
    void *blka_p = blka;
    void *meta_p = blka->meta;
    void *free_list_p = blka->free_list;

    fprintf(fd, "\n┏━━━━━━━━━━━━━━━━╸ ALLOCATOR ╺━━━━━━━━━━━━━━━━┓\n");
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Address", blka_p);
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Aligned",
            is_aligned(blka) ? "Yes" : "No");
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Meta", meta_p);
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Free List", free_list_p);
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20i ┃\n", "Blocks",
            utilities_number_of_blocks(blka));
    fprintf(fd, "┃ %-20s : %-20i ┃\n", "Free Blocks",
            utilities_free_number(blka));
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20zu ┃\n", "Size (bytes)", blka->size);
    fprintf(fd, "┃ %-20s : %06.2f%-14s ┃\n", "Wasted Memory",
            utilities_memory_footprint(blka) * 100, "%");
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Lists Valid",
            utilities_validate_lists(blka) ? "Yes" : "No");
    fprintf(fd, "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n\n");
}

void utilities_print_block(blk_allocator *blka, blk_meta *blk, FILE *fd)
{
    void *blk_p = blk;
    void *next_p = blk->next;
    void *prev_p = blk->prev;
    void *next_free_p = blk->next_free;
    void *prev_free_p = blk->prev_free;

    fprintf(fd, "\n┏━━━━━━━━━━━━━━━━━━╸ BLOCK ╺━━━━━━━━━━━━━━━━━━┓\n");
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Address", blk_p);
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Aligned",
            is_aligned(blk) ? "Yes" : "No");
    fprintf(fd, "┠╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌╌┨\n");
    fprintf(fd, "┃ %-20s : %-20i ┃\n", "Index",
            utilities_block_number(blka, blk));
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
    fprintf(fd, "┃ %-20s : %-20p ┃\n", "Data", blk->data);
    fprintf(fd, "┃ %-20s : %-20s ┃\n", "Aligned",
            is_aligned(blk->data) ? "Yes" : "No");
    fprintf(fd, "┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\n\n");
}

void utilities_print_all_blocks(blk_allocator *blka, FILE *fd)
{
    blk_meta *blk = blka->meta;
    while (blk->next)
    {
        utilities_print_block(blka, blk, fd);
        fprintf(fd, "  %-20s ⇅ %-20s  \n", " ", " ");
        blk = blk->next;
    }

    utilities_print_block(blka, blk, fd);
}
