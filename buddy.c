#pragma once

#include "buddy.h"
#include "slab.h"
#include "types.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>

BuddiesManager* buddies_manager = NULL;


void print_buddy_list(Block* head) {
	Block* iter = head;
	while (iter) {
		printf("%x -> ", iter);
		iter = iter->next;
	}
	printf("\n");
}


void print_buddy_manager() {
	printf("\n\n");
	printf("Number of blocks: %d\n", buddies_manager->number_of_blocks);
	printf("Largest block degree: %d\n", buddies_manager->largest_block_degree2);
	printf("Starting block address: %x\n", (unsigned)buddies_manager->starting_block_adr);
	printf("Headers:\n\n");
	for (int index = 0; index <= buddies_manager->largest_block_degree2; index++) {
		printf("[%d]: ", index);
		print_buddy_list(buddies_manager->headers[index]);
	}
}


int find_minimum_sized_buddy(int minimum_index) {
	for (int index = minimum_index; index <= buddies_manager->largest_block_degree2; index++) {
		if (buddies_manager->headers[index]) {
			return index;
		}
	}
	return -1;
}


Block* get_buddy(int size) {

	int minimum_index = (int)log2(next_power_of_two(size));
	int block_to_take_index = find_minimum_sized_buddy(minimum_index);

	if (block_to_take_index == -1) {
		//printf("Not enough memory to allocate buddy with size %d\n", size);
		return NULL;		//not enough memory
	}

	Block* to_take = buddies_manager->headers[block_to_take_index];
	buddies_manager->headers[block_to_take_index] = buddies_manager->headers[block_to_take_index]->next;
	to_take->next = NULL;

	int index = block_to_take_index;

	while (index > minimum_index) {
		index--;
		int offset = 1 << index;

		Block* right_half = to_take + offset;

		to_take->next = buddies_manager->headers[index];
		buddies_manager->headers[index] = to_take;
		to_take = right_half;
	}

	return to_take;
}


Block* get_potential_buddy_of(Block* block, int size_of_block) {

	int block_offset = (unsigned)block - (unsigned)buddies_manager->starting_block_adr;
	int index = (int)log2(next_power_of_two(size_of_block));

	unsigned parity_bit_mask = (1 << (block_offset_bit_cnt + index));
	int odd = parity_bit_mask & block_offset;

	unsigned buddy = buddies_manager->starting_block_adr;

	if (odd) {
		buddy += (block_offset - parity_bit_mask);
	}
	else {
		buddy += (block_offset + parity_bit_mask);
	}

	return (Block*)buddy;
}


void put_buddy(Block* block, int size_of_block) {

	int index = (int)log2(next_power_of_two(size_of_block));
	Block* buddy = get_potential_buddy_of(block, size_of_block);

	Block* iterator, * prev;
	iterator = buddies_manager->headers[index];
	prev = NULL;

	if (!iterator) {
		block->next = NULL;
		buddies_manager->headers[index] = block;
		return;
	}

	int insert_alone = 1;

	while (iterator) {
		if (iterator == buddy) {
			insert_alone = 0;

			if (!prev) {
				buddies_manager->headers[index] = iterator->next;
			} else {
				prev->next = iterator->next;
			}

			Block* to_insert;
			if ((unsigned)block < (unsigned)buddy) {
				to_insert = block;
			} else {
				to_insert = buddy;
			}

			put_buddy(to_insert, 2 * size_of_block);
			break;
		}
		prev = iterator;
		iterator = iterator->next;
	}

	if (insert_alone) {
		block->next = buddies_manager->headers[index];
		buddies_manager->headers[index] = block;
	}
}


void kmem_init(void* space, int block_num) {

	Block* first_block = (Block*)space;	
	buddies_manager = (BuddiesManager*)space;
	first_block++;
	block_num--;

	buddies_manager->starting_block_adr = first_block;
	int max_buddy_size = previous_power_of_two(block_num);
	buddies_manager->number_of_blocks = block_num;	
	buddies_manager->largest_block_degree2 = (int)log2(max_buddy_size);

	for (int i = 0; i <= buddies_manager->largest_block_degree2; i++) {
		buddies_manager->headers[i] = NULL;
	}

	for (int i = 0; i < buddies_manager->number_of_blocks; i++) {
		Block* block_to_add = buddies_manager->starting_block_adr + i;
		put_buddy(block_to_add, 1);
	}
}


int main() {
	void* space = malloc(BLOCK_SIZE * 1000);
	kmem_init(space, 1000);
	print_buddy_manager();

	for (int i = 1000; i > 0; i--) {
		int first_block_size = i;
		Block* first_block = get_buddy(first_block_size);
		if (first_block == NULL) continue;
		put_buddy(first_block, first_block_size);
		
	}

	print_buddy_manager();

	/*int first_block_size = 7;
	Block* first_block = get_buddy(first_block_size);
	print_buddy_manager();

	put_buddy(first_block, first_block_size);
	print_buddy_manager();*/
}