#pragma once

#include "buddy.h"
#include "slab.h"
#include "types.h"
#include "utils.h"
#include <math.h>

static BuddiesData* buddies_data = NULL;

Block* get_block_at(int n) {
	Block* block = (Block*)buddies_data->starting_block;
	block += n;
	return block;
}

void fill_buddies_array(BuddiesData* buddies_data) {
	int buddy_array_size = (int)log2(buddies_data->max_buddy_size);
	for (int i = 0; i <= buddy_array_size; i++) {
		buddies_data->available_buddies_index[i] = -1;
	}

	int current_block = 0;
	int remaining_blocks = buddies_data->number_of_blocks;

	while (remaining_blocks > 0) {
		int max_buddy_size_in_blocks = previous_power_of_two(remaining_blocks);
		int max_buddy_index = (int)log2(max_buddy_size_in_blocks);
		buddies_data->available_buddies_index[max_buddy_index] = current_block;
		// need to make list of buddies (inside blocks?)
		get_block_at(current_block)->next_block = -1;
		current_block += max_buddy_size_in_blocks;
		remaining_blocks -= max_buddy_size_in_blocks;
	}
}

void kmem_init(void* space, int block_num) {

	Block* first_block = (Block*)space;
	buddies_data = (BuddiesData*)space;
	first_block++;

	buddies_data->number_of_blocks = --block_num;
	buddies_data->starting_block = first_block;
	buddies_data->max_buddy_size = previous_power_of_two(buddies_data->number_of_blocks);
}

