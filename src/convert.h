#ifndef CONVERT_H
#define CONVERT_H

#include "allocator.h"

/// @brief Macro to call utilities_blka_to_u8(1).
#define BLKA_TO_U8(blka) utilities_blka_to_u8(blka)

/// @brief Macro to call utilities_blk_to_u8(1).
#define BLK_TO_U8(blk) utilities_blk_to_u8(blk)

/// @brief Macro to call utilities_u8_to_blka(1).
#define U8_TO_BLKA(blka) utilities_u8_to_blka(blka)

/// @brief Macro to call utilities_u8_to_blk(1).
#define U8_TO_BLK(blk) utilities_u8_to_blk(blk)

/// @brief Convert a blk_allocator* to a uint8_t* (used for pointer arithmetic).
uint8_t *utilities_blka_to_u8(blk_allocator *blka);

/// @brief Convert a blk_meta* to a uint8_t* (used for pointer arithmetic).
uint8_t *utilities_blk_to_u8(blk_meta *blk);

/// @brief Convert a uint8_t* to a blk_allocator*.
blk_allocator *utilities_u8_to_blka(uint8_t *blka);

/// @brief Convert a uint8_t* to a blk_meta*.
blk_meta *utilities_u8_to_blk(uint8_t *blk);

#endif /* ! CONVERT_H */
