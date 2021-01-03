#pragma once

#include "slab.h"
#include <Windows.h>

#define block_offset_bit_cnt 12


typedef union BuddyUnion {
	union BuddyUnion* next;
	char data[BLOCK_SIZE];
} Block;

typedef struct BuddyManager {
	int number_of_blocks;
	int largest_block_degree2;
	Block* starting_block_adr;
	Block* headers[64];
	HANDLE dhMutex;
} BuddyManager;

void init_buddy_manager(void* space, int block_num);
BuddyManager* get_buddy_manager();
void print_buddy_manager();
void put_buddy(Block* block, int size_of_block);