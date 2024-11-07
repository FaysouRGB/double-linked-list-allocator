#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdint.h>
#include <stdio.h>

#include "allocator.h"

#define BLKA_TO_UINT(blka) utilities_blka_uint(blka);
#define BLK_TO_UINT(blk) utilities_blk_uint(blk);

#define UINT_To_BLKA(blka) utilities_uint_blka(blka);
#define UINT_TO_BLK(blk) utilities_uint_blk(blk);

uint8_t *utilities_blka_uint(blk_allocator *blka);

uint8_t *utilities_blk_uint(blk_meta *blk);

blk_allocator *utilities_uint_blka(uint8_t *blka);

blk_meta *utilities_uint_blk(uint8_t *blk);

void utilities_print_allocator(blk_allocator *blka, FILE *fd);

void utilities_print_block(blk_allocator *blka, blk_meta *blk, FILE *fd);

void utilities_print_all_blocks(blk_allocator *blka, FILE *fd);

#endif /* ! UTILITIES_H */
