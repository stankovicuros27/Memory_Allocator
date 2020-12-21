#pragma once

#include "slab.h"

typedef struct Block {
	int next_block;
	char data[BLOCK_SIZE - sizeof(int)];
} Block;

typedef struct BuddiesData {
	int number_of_blocks;
	void* starting_block;
	int max_buddy_size;
	int* available_buddies_index[64];
} BuddiesData;