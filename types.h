#pragma once

#include "slab.h"

#define block_offset_bit_cnt 12

typedef union BuddyUnion {
	union BuddyUnion* next;
	char data[BLOCK_SIZE];
} Block;

typedef struct BuddiesManager {

	int number_of_blocks;
	int largest_block_degree2;
	Block* starting_block_adr;
	Block* headers[64];

} BuddiesManager;