#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdio.h>

#include "allocator.h"

/// @brief Macro for red.
#define RED "\x1b[31m"

/// @brief Macro for green.
#define GREEN "\x1b[32m"

/// @brief Macro to end color.
#define RESET "\x1b[0m"

/// @brief Macro to print an error log.
#define PRINT_ERROR(message) fprintf(stderr, "%s\n", message)

/// @brief Macro to check if a pointer is aligned.
#define IS_ALIGNED(ptr) (((uintptr_t)(ptr) % sizeof(long double)) == 0)

/// @brief Check that calloc initialized all bytes to 0.
/// @param ptr The data pointer.
/// @param size The size to check.
/// @return true if no error, false if any.
bool utilities_validate_calloc(void *ptr, size_t size);

/// @brief Create a snapshot file representing the current state of the block
/// allocator.
/// @param blka The block allocator.
/// @return true if it succeeded. false otherwise.
bool utilities_blka_snapshot(blk_allocator *blka);

/// @brief Print the block representation to fd.
/// @param blk The block to print.
/// @param index The index of the block.
/// @param fd The file descriptor to write to.
void utilities_print_block(blk_meta *blk, size_t index, FILE *fd);

/// @brief Print all blocks of the specified block allocator to fd.
/// @param blka The block allocator.
/// @param fd The file descriptor to write to.
void utilities_print_blocks(blk_allocator *blka, FILE *fd);

/// @brief Get the number of blocks in the block allocator.
/// @param blka The block allocator.
/// @return The number of blocks.
int utilities_number_of_blocks(blk_allocator *blka);

/// @brief Get the number of free blocks in the block allocator.
/// @param blka The block allocator.
/// @return The number of free blocks.
int utilities_number_of_free_blocks(blk_allocator *blka);

/// @brief Check that the size of the allocator matches its state.
/// @param blka The block allocator.
/// @return true if it matches, false otherwise.
bool utilities_validate_allocator_size(blk_allocator *blka);

/// @brief Get the total allocator size.
/// @param blka The blockc allocator.
/// @return The total size mapped.
size_t utilities_total_allocator_size(blk_allocator *blka);

/// @brief Validate the normal double list of the allocator.
/// @param blka The block allocator.
/// @return true if it is valid, false otherwise.
bool utilities_validate_normal_list(blk_allocator *blka);

/// @brief Validate the double free list of the allocator.
/// @param blka The block allocator.
/// @return true if it is valid, false otherwise.
bool utilities_validate_free_list(blk_allocator *blka);

/// @brief Print the block allocator representation to fd
/// @param blka The block allocator to print.
/// @param fd The file descriptor to write to.
void utilities_print_allocator(blk_allocator *blka, FILE *fd);

/// @brief Print different type sizes, useful for debugging.
void utilities_print_sizes(FILE *fd);

#endif /* ! UTILITIES_H */
