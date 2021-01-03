#pragma once

#include "buddy.h"
#include "slab.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <Windows.h>

#define _CRT_SECURE_NO_WARNINGS

void get_slab(kmem_cache_t* cachep);
void kmem_init(void* space, int block_num);
kmem_cache_t* kmem_cache_create(const char* name, size_t size, void (*ctor)(void*), void (*dtor)(void*)); // Allocate cache
int kmem_cache_shrink(kmem_cache_t* cachep); // Shrink cache
void* kmem_cache_alloc(kmem_cache_t* cachep); // Allocate one object from cache
void kmem_cache_free(kmem_cache_t* cachep, void* objp); // Deallocate one object from cache
void* kmalloc(size_t size); // Alloacate one small memory buffer
void kfree(const void* objp); // Deallocate one small memory buffer
void kmem_cache_destroy(kmem_cache_t* cachep); // Deallocate cache
void kmem_cache_info(kmem_cache_t* cachep); // Print cache info
int kmem_cache_error(kmem_cache_t* cachep); // Print error message






typedef struct slab {
	struct slab* next;
	kmem_cache_t* my_cache;

	void* starting_slot;
	unsigned* bitvector_start;
	int free_slot_cnt;

} SlabMetaData;

typedef struct kmem_cache_s {
	char name[30];
	struct kmem_cache_s* next;

	int num_of_objects_in_slab;
	int object_size_in_bytes;
	int slab_size_in_blocks;
	int bitvector_size_in_unsigned;

	SlabMetaData* empty_slabs;
	SlabMetaData* mixed_slabs;
	SlabMetaData* full_slabs;

	HANDLE ghMutex;

	void(*ctor)(void*);
	void(*dtor)(void*);
} kmem_cache_s;

typedef struct SlabManager {
	kmem_cache_t cache_of_caches;
	kmem_cache_t small_buffer_caches[14];
} SlabManager;

static BuddyManager* buddy_manager;
static SlabManager* slab_manager;


// ---------------------------------------------------------------------------------------------------------------


void initialize_cache_of_caches() {

	kmem_cache_t* cache_of_caches = &slab_manager->cache_of_caches;

	cache_of_caches->next = NULL;
	strcpy(cache_of_caches->name, cache_of_caches_name);

	cache_of_caches->object_size_in_bytes = sizeof(kmem_cache_t);
	cache_of_caches->slab_size_in_blocks = 1;

	cache_of_caches->bitvector_size_in_unsigned = 
		((cache_of_caches->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData)) / sizeof(kmem_cache_t)) / bits_in_unsigned + 1;

	cache_of_caches->num_of_objects_in_slab =
		((cache_of_caches->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - sizeof(unsigned) * cache_of_caches->bitvector_size_in_unsigned) / sizeof(kmem_cache_t));

	cache_of_caches->ctor = cache_of_caches->dtor = NULL;
	cache_of_caches->empty_slabs = cache_of_caches->full_slabs = cache_of_caches->mixed_slabs = NULL;

	cache_of_caches->ghMutex = CreateMutex(NULL, FALSE, NULL);
}

void initialize_small_buffer_caches() {

	kmem_cache_t* small_buffer_caches = &slab_manager->small_buffer_caches;

	for (int i = starting_buffer_deg; i < number_of_different_buffer_degs + starting_buffer_deg; i++) {

		kmem_cache_t* current_cache = small_buffer_caches + (i - starting_buffer_deg);

		current_cache->next = NULL;
		strcpy(current_cache->name, small_buffer_cache_name);

		current_cache->object_size_in_bytes = pow(2, i);
		current_cache->slab_size_in_blocks = next_power_of_two(current_cache->object_size_in_bytes / BLOCK_SIZE + 1);

		current_cache->bitvector_size_in_unsigned = 
			((current_cache->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData)) / current_cache->object_size_in_bytes) / bits_in_unsigned + 1;

		current_cache->num_of_objects_in_slab =
			((current_cache->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - sizeof(unsigned) * current_cache->bitvector_size_in_unsigned) / current_cache->object_size_in_bytes);

		current_cache->ctor = current_cache->dtor = NULL;
		current_cache->empty_slabs = current_cache->full_slabs = current_cache->mixed_slabs = NULL;

		current_cache->ghMutex = CreateMutex(NULL, FALSE, NULL);
	}
}

void kmem_init(void* space, int block_num) {
	init_buddy_manager(space, block_num);
	buddy_manager = get_buddy_manager();

	BuddyManager* slab_manager_adr = buddy_manager + 1;
	slab_manager = (SlabManager*)slab_manager_adr;

	initialize_cache_of_caches();
	initialize_small_buffer_caches();
}

kmem_cache_t* kmem_cache_create(const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*)) {

	WaitForSingleObject(slab_manager->cache_of_caches.ghMutex, INFINITE);
	kmem_cache_t* created_cache = (kmem_cache_t*)kmem_cache_alloc(&(slab_manager->cache_of_caches));

	strcpy(created_cache->name, name);
	created_cache->empty_slabs = created_cache->full_slabs = created_cache->mixed_slabs = NULL;
	created_cache->ctor = ctor;
	created_cache->dtor = dtor;
	created_cache->next = NULL;

	kmem_cache_t* iterator = &slab_manager->cache_of_caches, * prev = NULL;
	while (iterator) {
		prev = iterator;
		iterator = iterator->next;
	}
	if (prev) {
		prev->next = created_cache;
	} // mozda ovde neka greska???

	created_cache->object_size_in_bytes = size;
	created_cache->slab_size_in_blocks = next_power_of_two(size / BLOCK_SIZE + 1);

	created_cache->bitvector_size_in_unsigned = 
		(created_cache->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData)) / created_cache->object_size_in_bytes / bits_in_unsigned + 1;

	created_cache->num_of_objects_in_slab =
		(created_cache->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - sizeof(unsigned) * created_cache->bitvector_size_in_unsigned) / created_cache->object_size_in_bytes;

	created_cache->ghMutex = CreateMutex(NULL, FALSE, NULL);


	if (ctor) {
		printf("");
	}

	ReleaseMutex(slab_manager->cache_of_caches.ghMutex);
	return created_cache;
}

// Allocates one object in cache
void* kmem_cache_alloc(kmem_cache_t* cachep) {

	WaitForSingleObject(cachep->ghMutex, INFINITE);

	if (!cachep->empty_slabs && !cachep->mixed_slabs) {
		get_slab(cachep);
	}

	SlabMetaData* slab = NULL;

	if (cachep->mixed_slabs) {
		slab = cachep->mixed_slabs;
	}
	else if (cachep->empty_slabs) {
		slab = cachep->empty_slabs;
	}
	else {
		printf("Error! No free slabs");
		exit(-1);
	}

	int free_index = get_free_index_bitvector(cachep, slab);
	if (free_index == -1) {
		printf("Error, slab with no space!");
		exit(-1);
	}

	int was_empty = 0;

	if (slab == cachep->empty_slabs) {
		cachep->empty_slabs = slab->next;
		slab->next = NULL;
		was_empty = 1;
	}

	if (slab->free_slot_cnt) {
		if (was_empty) {
			slab->next = cachep->mixed_slabs;
			cachep->mixed_slabs = slab;
		}
	}
	else {
		if (!was_empty) {
			cachep->mixed_slabs = slab->next;
			slab->next = NULL;
		}
		slab->next = cachep->full_slabs;
		cachep->full_slabs = slab;
	}

	slab->free_slot_cnt--;

	void* obj = (void*)((unsigned)slab->starting_slot + free_index * cachep->object_size_in_bytes);
	if (cachep->ctor) {
		cachep->ctor(obj);
	}

	ReleaseMutex(cachep->ghMutex);
	return obj;
}

void get_slab(kmem_cache_t* cachep) {

	Block* block = get_buddy(cachep->slab_size_in_blocks);
	if (!block) {
		printf("Error! Slab couldn't allocate buddy block!");
		exit(-1);
	}

	SlabMetaData* slab = (SlabMetaData*)block;
	slab->my_cache = cachep;

	SlabMetaData* bitvector_start = slab + 1;
	slab->bitvector_start = (unsigned*)bitvector_start;
	for (int i = 0; i < cachep->bitvector_size_in_unsigned; i++) {
		*(slab->bitvector_start + i) = 0;
	}

	unsigned* starting_slot = slab->bitvector_start + cachep->bitvector_size_in_unsigned;
	slab->starting_slot = (void*)starting_slot;

	slab->next = cachep->empty_slabs;
	cachep->empty_slabs = slab;

	slab->free_slot_cnt = cachep->num_of_objects_in_slab;

	if (cachep->ctor) {
		for (unsigned i = 0; i < cachep->num_of_objects_in_slab; i++) {
			char* adr = (char*)slab->starting_slot + i * cachep->object_size_in_bytes;
			cachep->ctor((void*)adr);
		}
	}
}

SlabMetaData* get_slab_by_object_from_cache(kmem_cache_t* cachep, void* obj) {
	SlabMetaData* iterator = cachep->full_slabs;
	while (iterator) {
		void* starting_slot = iterator->starting_slot;
		if ((unsigned)obj > (unsigned)iterator && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
			return iterator;
		}
		iterator = iterator->next;
	}

	iterator = cachep->mixed_slabs;
	while (iterator) {
		if ((unsigned)obj > (unsigned)iterator && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
			return iterator;
		}
		iterator = iterator->next;
	}

	iterator = cachep->empty_slabs;
	while (iterator) {
		if ((unsigned)obj > (unsigned)iterator && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
			return iterator;
		}
		iterator = iterator->next;
	}

	return NULL;
}

void kmem_cache_free(kmem_cache_t* cachep, void* objp) {

	WaitForSingleObject(cachep->ghMutex, INFINITE);

	SlabMetaData* slab = get_slab_by_object_from_cache(cachep, objp);

	int slot = ((unsigned)objp - (unsigned)slab->starting_slot) / cachep->object_size_in_bytes;
	int index = slot / bits_in_unsigned;
	int deg = slot % bits_in_unsigned;
	unsigned mask = ~(int)pow(2, deg);
	slab->bitvector_start[index] &= mask;

	if (slab->my_cache->dtor) {
		slab->my_cache->dtor(objp);
	}

	if (!slab->free_slot_cnt) {
		SlabMetaData* iter = cachep->full_slabs, * prev = NULL;
		while (iter != slab) {
			prev = iter;
			iter = iter->next;
		}
		if (prev) {
			prev->next = iter->next;
		}
		else {
			cachep->full_slabs = slab->next;
		}
	}

	if (slab->free_slot_cnt == cachep->num_of_objects_in_slab - 1) {
		if (slab->free_slot_cnt) {
			SlabMetaData* iter = cachep->mixed_slabs, * prev = NULL;
			while (iter != slab) {
				prev = iter;
				iter = iter->next;
			}
			if (prev) {
				prev->next = iter->next;
			}
			else {
				cachep->mixed_slabs = slab->next;
			}
		}
		slab->next = cachep->empty_slabs;
		cachep->empty_slabs = slab;
	}
	else if (!slab->free_slot_cnt) {
		slab->next = cachep->mixed_slabs;
		cachep->mixed_slabs = slab;
	}

	slab->free_slot_cnt++;

	ReleaseMutex(cachep->ghMutex);
}

// Alloacate one small memory buffer
void* kmalloc(size_t size) {
	int deg = log2(next_power_of_two(size));
	kmem_cache_t* cachep = &slab_manager->small_buffer_caches[deg - starting_buffer_deg];

	if (!cachep->empty_slabs && !cachep->mixed_slabs) {
		get_slab(cachep);
	}

	SlabMetaData* slab = NULL;

	if (cachep->mixed_slabs) {
		slab = cachep->mixed_slabs;
	}
	else if (cachep->empty_slabs) {
		slab = cachep->empty_slabs;
	}
	else {
		printf("Error! No free slabs");
		exit(-1);
	}

	int free_index = get_free_index_bitvector(slab->my_cache, slab);
	if (free_index == -1) {
		printf("Error, slab with no space!");
		exit(-1);
	}

	int was_empty = 0;

	if (slab == cachep->empty_slabs) {
		cachep->empty_slabs = slab->next;
		slab->next = NULL;
		was_empty = 1;
	}

	if (slab->free_slot_cnt) {
		if (was_empty) {
			slab->next = cachep->mixed_slabs;
			cachep->mixed_slabs = slab;
		}
	}
	else {
		if (!was_empty) {
			cachep->mixed_slabs = slab->next;
			slab->next = NULL;
		}
		slab->next = cachep->full_slabs;
		cachep->full_slabs = slab;
	}

	slab->free_slot_cnt--;

	return (void*)((unsigned)slab->starting_slot + free_index * cachep->object_size_in_bytes);
}

SlabMetaData* get_slab_by_object_from_buffer(void* obj) {

	for (int i = 0; i < number_of_different_buffer_degs; i++) {
		kmem_cache_t* cachep = &slab_manager->small_buffer_caches[i];

		SlabMetaData* iterator = cachep->full_slabs;
		while (iterator) {
			if ((unsigned)obj > (unsigned)iterator && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
				return iterator;
			}
			iterator = iterator->next;
		}

		iterator = cachep->mixed_slabs;
		while (iterator) {
			if ((unsigned)obj > (unsigned)iterator && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
				return iterator;
			}
			iterator = iterator->next;
		}
	}

	return NULL;
}

void kfree(const void* objp) {

	SlabMetaData* slab = get_slab_by_object_from_buffer(objp);

	int slot = ((unsigned)objp - (unsigned)slab->starting_slot) / slab->my_cache->object_size_in_bytes;
	int index = slot / sizeof(char);
	int deg = slot % sizeof(char);
	char mask = ~(int)pow(2, deg);
	slab->bitvector_start[index] &= mask;

	if (slab->my_cache->dtor) {
		slab->my_cache->dtor(objp);
	}

	/*if (!slab->free_slot_cnt) {
		SlabMetaData* iter = slab->my_cache->full_slabs, * prev = NULL;
		while (iter != slab) {
			prev = iter;
			iter = iter->next;
		}
		if (prev) {
			prev->next = iter->next;
		}
		else {
			slab->my_cache->full_slabs = slab->next;
		}
	}

	if (slab->free_slot_cnt == slab->my_cache->num_of_objects_in_slab - 1) {
		if (slab->free_slot_cnt) {
			SlabMetaData* iter = slab->my_cache->mixed_slabs, * prev = NULL;
			while (iter != slab) {
				prev = iter;
				iter = iter->next;
			}
			if (prev) {
				prev->next = iter->next;
			}
			else {
				slab->my_cache->mixed_slabs = slab->next;
			}
		}
		slab->next = slab->my_cache->empty_slabs;
		slab->my_cache->empty_slabs = slab;
	}
	else if (!slab->free_slot_cnt) {
		slab->next = slab->my_cache->mixed_slabs;
		slab->my_cache->mixed_slabs = slab;
	}*/

	slab->free_slot_cnt++;
}


int get_free_index_bitvector(kmem_cache_t* cachep, SlabMetaData* slab) {

	static int cnt = 0;

	unsigned* bitvector = slab->bitvector_start;
	int free_index = -1;

	for (int i = 0; i < cachep->bitvector_size_in_unsigned; i++) {
		int finished = 0;
		for (int deg = 0; deg < 32; deg++) {
			char mask = pow(2, deg);
			if (!(mask & *(bitvector + i))) {
				free_index = i * 32 + deg;
				finished = 1;
				break;
			}
		}
		if (finished) {
			break;
		}
	}

	return free_index;
}

int kmem_cache_shrink(kmem_cache_t* cachep) {
	int freed = 0;
	while (cachep->empty_slabs) {
		SlabMetaData* to_delete_slab = cachep->empty_slabs;
		cachep->empty_slabs = to_delete_slab->next;

		Block* to_delete_block = (Block*)to_delete_slab;
		put_buddy(to_delete_block, cachep->slab_size_in_blocks);
		freed += cachep->slab_size_in_blocks;
	}
	return freed;
}

void kmem_cache_destroy(kmem_cache_t* cachep) {

}

void kmem_cache_info(kmem_cache_t* cachep) {
	printf("\n\nCache %s\n", cachep->name);
	printf("Object size: %d\n", cachep->object_size_in_bytes);
	printf("Objects in slab: %d\n", cachep->object_size_in_bytes);
	printf("Slab size in Blocks: %d\n", cachep->slab_size_in_blocks);

	int empty_slabs = 0;
	int full_slabs = 0;
	int mixed_slabs = 0;
	int free_space = 0;
	SlabMetaData* iterator = NULL;

	iterator = cachep->empty_slabs;
	while (iterator) {
		free_space += iterator->free_slot_cnt;
		empty_slabs++;
		iterator = iterator->next;
	}

	iterator = cachep->full_slabs;
	while (iterator) {
		free_space += iterator->free_slot_cnt;
		full_slabs++;
		iterator = iterator->next;
	}

	iterator = cachep->mixed_slabs;
	while (iterator) {
		free_space += iterator->free_slot_cnt;
		mixed_slabs++;
		iterator = iterator->next;
	}
	
	printf("Empty slabs number: %d\n", empty_slabs);
	printf("Full slabs number: %d\n", full_slabs);
	printf("Mixed slabs number: %d\n", mixed_slabs);
	printf("Free space left: %d\n", free_space);
}

int kmem_cache_error(kmem_cache_t* cachep) {
	return -1;
}