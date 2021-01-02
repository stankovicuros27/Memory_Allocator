#pragma once

#include "buddy.h"
#include "slab.h"
#include "utils.h"
#include "types.h"
#include <math.h>
#include <stdio.h>

BuddyManager* buddy_manager = NULL;


void print_buddy_list(Block* head) {
	Block* iter = head;
	while (iter) {
		printf("%x -> ", iter);
		iter = iter->next;
	}
	printf("\n");
}


void print_buddy_manager() {
	printf("\n\n\n");
	printf("~~~BUDDY MANAGER~~~\n\n");
	printf("Number of blocks: %d\n", buddy_manager->number_of_blocks);
	printf("Largest block degree: %d\n", buddy_manager->largest_block_degree2);
	printf("Starting block address: %x\n", (unsigned)buddy_manager->starting_block_adr);
	printf("Headers:\n\n");
	for (int index = 0; index <= buddy_manager->largest_block_degree2; index++) {
		printf("[%03d]: ", (int)pow(2,index));
		print_buddy_list(buddy_manager->headers[index]);
	}
}


int find_minimum_sized_buddy(int minimum_index) {
	for (int index = minimum_index; index <= buddy_manager->largest_block_degree2; index++) {
		if (buddy_manager->headers[index]) {
			return index;
		}
	}
	return -1;
}


Block* get_buddy(int size) {
	if (size == 0) {
		printf("\nSize cannot be 0!\n");
		return NULL;
	}

	int minimum_index = (int)log2(next_power_of_two(size));
	int block_to_take_index = find_minimum_sized_buddy(minimum_index);

	if (block_to_take_index == -1) {
		//printf("Not enough memory to allocate buddy with size %d\n", size);
		return NULL;		//not enough memory
	}

	Block* to_take = buddy_manager->headers[block_to_take_index];
	buddy_manager->headers[block_to_take_index] = buddy_manager->headers[block_to_take_index]->next;
	to_take->next = NULL;

	int index = block_to_take_index;

	while (index > minimum_index) {
		index--;
		int offset = 1 << index;

		Block* right_half = to_take + offset;

		to_take->next = buddy_manager->headers[index];
		buddy_manager->headers[index] = to_take;
		to_take = right_half;
	}

	return to_take;
}


Block* get_potential_buddy_of(Block* block, int size_of_block) {

	int index = (int)log2(next_power_of_two(size_of_block));
	int block_offset = (unsigned)block - (unsigned)buddy_manager->starting_block_adr;

	unsigned parity_bit_mask = (1 << (block_offset_bit_cnt + index));
	int odd = parity_bit_mask & block_offset;

	unsigned buddy = buddy_manager->starting_block_adr;

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
	iterator = buddy_manager->headers[index];
	prev = NULL;

	if (!iterator) {
		block->next = NULL;
		buddy_manager->headers[index] = block;
		return;
	}

	while (iterator) {
		if (iterator == buddy) {

			if (!prev) {
				buddy_manager->headers[index] = iterator->next;
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
			return;
		}
		prev = iterator;
		iterator = iterator->next;
	}

	block->next = buddy_manager->headers[index];
	buddy_manager->headers[index] = block;
}


void kmem_init(void* space, int block_num) {

	if (block_num < 2) {
		printf("\nNot enough memory!\n");
		exit(-1);
	}

	Block* first_block = (Block*)space;	
	buddy_manager = (BuddyManager*)space;
	first_block++;
	block_num--;

	buddy_manager->starting_block_adr = first_block;
	buddy_manager->number_of_blocks = block_num;
	buddy_manager->largest_block_degree2 = (int)log2(previous_power_of_two(block_num));

	for (int i = 0; i <= buddy_manager->largest_block_degree2; i++) {
		buddy_manager->headers[i] = NULL;
	}

	for (int i = 0; i < buddy_manager->number_of_blocks; i++) {
		Block* block_to_add = buddy_manager->starting_block_adr + i;
		put_buddy(block_to_add, 1);
	}
}


typedef struct Node {
	struct Node* next;
	Block* block;
	int memory;
} Node;


int main() {

	for (int i = 0; i < 10; i++) {
		int size = rand() % 2000;


		void* space = malloc(BLOCK_SIZE * size);
		kmem_init(space, size);
		print_buddy_manager();

		Node* head = NULL;
		int taken_mem = 0;
		int cnt_wrong = 0;
		int cnt = 10000;

		while (1) {
			//getchar();
			int first_block_size = rand() % (int)pow(2, buddy_manager->largest_block_degree2) / 2;
			printf("%d size", first_block_size);
			if (!first_block_size) {
				first_block_size = 1;
			}
			Block* first_block = get_buddy(first_block_size);
			if (first_block == NULL) {
				printf("Not enough memory to allocate %d block size\n", first_block_size);
				++cnt_wrong;
			}
			else {
				cnt_wrong = 0;
				printf("Allocated new buddy block with size %d blocks", first_block_size);
			}

			if (!cnt_wrong) {
				taken_mem += first_block_size;
				Node* p = malloc(sizeof(Node));
				p->block = first_block;
				p->memory = first_block_size;

				p->next = head;
				head = p;
			}

			if (cnt_wrong >= 5) {
				while (head != NULL) {
					Node* to_free = head;
					head = head->next;
					put_buddy(to_free->block, to_free->memory);
					printf("Freed buddy block with size %d blocks", to_free->memory);
					print_buddy_manager();
					free(to_free);
				}
			}

			printf("\n\n%d", cnt);
			if (cnt-- == 0) break;
			print_buddy_manager();
		}
	}
}