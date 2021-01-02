#pragma once

#include "slab.h"

typedef union BuddyUnion {
	union BuddyUnion* next;
	char data[BLOCK_SIZE];
} Block;

typedef struct BuddyManager {
	int number_of_blocks;
	int largest_block_degree2;
	Block* starting_block_adr;
	Block* headers[64];
} BuddyManager;

typedef struct slab {
	struct slab* next;
	kmem_cache_t* my_cache;
} Slab ;

typedef struct kmem_cache_s {
	char name[30];
	struct kmem_cache_s* next;

	int objects_in_slab;
	int slab_size;

	Slab* empty_slabs;
	Slab* mixed_slabs;
	Slab* full_slabs;

	void(*ctor)(void*);
	void(*dtor)(void*);
} kmem_cache_s;