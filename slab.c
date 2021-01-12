#pragma once

#include "buddy.h"
#include "slab.h"
#include "utils.h"
#include <math.h>
#include <stdio.h>
#include <Windows.h>

#define _CRT_SECURE_NO_WARNINGS
#define L1_CACHE_ALIGNMENT 1

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

typedef enum error_code {
	OK,
	BUDDY_ALLOCATION_ERROR,
	SLAB_SLOT_ALLOCATION_ERROR,
	CACHE_CANNOT_BE_DELETED
} error_code;

typedef struct slab {
	struct slab* next;
	kmem_cache_t* my_cache;
	void* starting_slot;
	unsigned* bitvector_start;
	int free_slot_cnt;
	char dummy[4];
} SlabMetaData;

typedef struct kmem_cache_s {
	char name[30];
	struct kmem_cache_s* next;

	int num_of_objects_in_slab;
	int object_size_in_bytes;
	int slab_size_in_blocks;
	int bitvector_size_in_unsigned;

	int unused_space_in_bytes;

	SlabMetaData* empty_slabs;
	SlabMetaData* mixed_slabs;
	SlabMetaData* full_slabs;

	HANDLE mutex;
	error_code err;

	void(*ctor)(void*);
	void(*dtor)(void*);
} kmem_cache_s;

typedef struct SlabManager {
	kmem_cache_t cache_of_caches;
	kmem_cache_t small_buffer_caches[NUMBER_OF_BUFFER_DEGREES];

	HANDLE slab_mutex;
	HANDLE print_mutex;
	HANDLE free_mutex;
	HANDLE list_mutex;
	HANDLE allocation_mutex;

	HANDLE main_mutex;

} SlabManager;

static BuddyManager* buddy_manager;
static SlabManager* slab_manager;


// -------------------------------------------------------------------------------------------------------------------------------


void initialize_cache_of_caches() {

	kmem_cache_t* cache_of_caches = &slab_manager->cache_of_caches;
	cache_of_caches->next = NULL;
	strcpy(cache_of_caches->name, cache_of_caches_name);
	cache_of_caches->object_size_in_bytes = sizeof(kmem_cache_t);

	int slab_size_in_blocks = 1;
	while (slab_size_in_blocks * BLOCK_SIZE / sizeof(kmem_cache_t) < 64) {
		slab_size_in_blocks++;
	}

	cache_of_caches->slab_size_in_blocks = next_power_of_two(slab_size_in_blocks);

	cache_of_caches->bitvector_size_in_unsigned = 
		((cache_of_caches->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - 2) / (sizeof(kmem_cache_t) + 1)) / bits_in_unsigned;

	cache_of_caches->num_of_objects_in_slab = cache_of_caches->bitvector_size_in_unsigned * bits_in_unsigned - 1;

	cache_of_caches->ctor = cache_of_caches->dtor = NULL;
	cache_of_caches->empty_slabs = cache_of_caches->full_slabs = cache_of_caches->mixed_slabs = NULL;
	cache_of_caches->mutex = CreateMutex(NULL, FALSE, NULL);

	cache_of_caches->unused_space_in_bytes = 
		(cache_of_caches->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - cache_of_caches->bitvector_size_in_unsigned * sizeof(unsigned)) % sizeof(kmem_cache_t);
}

void initialize_small_buffer_caches() {

	kmem_cache_t* small_buffer_caches = &slab_manager->small_buffer_caches;
	for (int i = STARTING_BUFFER_DEGREE; i <= NUMBER_OF_BUFFER_DEGREES + STARTING_BUFFER_DEGREE; i++) {

		kmem_cache_t* current_cache = small_buffer_caches + (i - STARTING_BUFFER_DEGREE);
		current_cache->next = NULL;
		strcpy(current_cache->name, small_buffer_cache_name);
		current_cache->object_size_in_bytes = pow(2, i);

		int slab_size_in_blocks = 1;
		while (slab_size_in_blocks * BLOCK_SIZE / current_cache->object_size_in_bytes < 64) {
			slab_size_in_blocks++;
		}
		current_cache->slab_size_in_blocks = next_power_of_two(slab_size_in_blocks);

		current_cache->bitvector_size_in_unsigned = 
			((current_cache->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - 2) / (current_cache->object_size_in_bytes + 1)) / bits_in_unsigned;

		current_cache->num_of_objects_in_slab =
			((current_cache->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - sizeof(unsigned) * current_cache->bitvector_size_in_unsigned) / current_cache->object_size_in_bytes);
		current_cache->num_of_objects_in_slab = current_cache->bitvector_size_in_unsigned * bits_in_unsigned - 1;


		current_cache->ctor = current_cache->dtor = NULL;
		current_cache->empty_slabs = current_cache->full_slabs = current_cache->mixed_slabs = NULL;
		current_cache->mutex = CreateMutex(NULL, FALSE, NULL);

		current_cache->unused_space_in_bytes =
			(current_cache->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - current_cache->bitvector_size_in_unsigned * sizeof(unsigned)) % current_cache->object_size_in_bytes;
	}
}

void kmem_init(void* space, int block_num) {
	init_buddy_manager(space, block_num);
	buddy_manager = get_buddy_manager();

	BuddyManager* slab_manager_adr = buddy_manager + 1;
	slab_manager = (SlabManager*)slab_manager_adr;

	slab_manager->slab_mutex = CreateMutex(NULL, FALSE, NULL);
	slab_manager->print_mutex = CreateMutex(NULL, FALSE, NULL);
	slab_manager->free_mutex = CreateMutex(NULL, FALSE, NULL);
	slab_manager->list_mutex = CreateMutex(NULL, FALSE, NULL);
	slab_manager->allocation_mutex = CreateMutex(NULL, FALSE, NULL);
	slab_manager->main_mutex = CreateMutex(NULL, FALSE, NULL);

	initialize_cache_of_caches();
	initialize_small_buffer_caches();
}

kmem_cache_t* kmem_cache_create(const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*)) {

	WaitForSingleObject(slab_manager->main_mutex, INFINITE);

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
	}

	created_cache->object_size_in_bytes = size;
	int slab_size_in_blocks = 1;
	while (slab_size_in_blocks * BLOCK_SIZE / size < 32) {
		slab_size_in_blocks++;
	}

	created_cache->slab_size_in_blocks = next_power_of_two(slab_size_in_blocks);

	created_cache->bitvector_size_in_unsigned = 
		(created_cache->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - 2) / (created_cache->object_size_in_bytes + 1) / bits_in_unsigned;

	created_cache->num_of_objects_in_slab =
		(created_cache->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - sizeof(unsigned) * created_cache->bitvector_size_in_unsigned) / created_cache->object_size_in_bytes;
	created_cache->num_of_objects_in_slab = created_cache->bitvector_size_in_unsigned * bits_in_unsigned - 1;

	created_cache->unused_space_in_bytes =
		(created_cache->slab_size_in_blocks * BLOCK_SIZE - sizeof(SlabMetaData) - created_cache->bitvector_size_in_unsigned * sizeof(unsigned)) % created_cache->object_size_in_bytes;

	created_cache->mutex = CreateMutex(NULL, FALSE, NULL);

	ReleaseMutex(slab_manager->main_mutex);
	return created_cache;
}

void* kmem_cache_alloc(kmem_cache_t* cachep) {

	WaitForSingleObject(slab_manager->allocation_mutex, INFINITE);
	WaitForSingleObject(cachep->mutex, INFINITE);

	if (!cachep->empty_slabs && !cachep->mixed_slabs) {
		get_slab(cachep);
	}

	SlabMetaData* slab = NULL;

	if (cachep->mixed_slabs) {
		slab = cachep->mixed_slabs;
		cachep->mixed_slabs = slab->next;
		slab->next = NULL;
	}
	else if (cachep->empty_slabs) {
		slab = cachep->empty_slabs;
		cachep->empty_slabs = slab->next;
		slab->next = NULL;
	}
	else {
		printf("\n\nSLAB_SLOT_ALLOCATION_ERROR\n\n");
		cachep->err = SLAB_SLOT_ALLOCATION_ERROR;
		ReleaseMutex(cachep->mutex);
		ReleaseMutex(slab_manager->allocation_mutex);
		return NULL;
	}

	int free_index = get_free_index_bitvector(cachep, slab);
	if (free_index == -1) {
		printf("\n\nSLAB_SLOT_ALLOCATION_ERROR\n\n");
		cachep->err = SLAB_SLOT_ALLOCATION_ERROR;
		ReleaseMutex(cachep->mutex);
		ReleaseMutex(slab_manager->allocation_mutex);
		return NULL;
	}

	int index = free_index / bits_in_unsigned;
	int deg = free_index % bits_in_unsigned;
	unsigned mask = 1 << deg;
	slab->bitvector_start[index] |= mask;

	slab->free_slot_cnt--;

	if (!slab->free_slot_cnt) {
		slab->next = cachep->full_slabs;
		cachep->full_slabs = slab;
	}
	else {
		slab->next = cachep->mixed_slabs;
		cachep->mixed_slabs = slab;
	}

	void* obj = (void*)((unsigned)slab->starting_slot + free_index * cachep->object_size_in_bytes);
	if (cachep->ctor) {
		cachep->ctor(obj);
	}

	ReleaseMutex(cachep->mutex);
	ReleaseMutex(slab_manager->allocation_mutex);
	return obj;
}

void get_slab(kmem_cache_t* cachep) {

	WaitForSingleObject(slab_manager->slab_mutex, INFINITE);
	WaitForSingleObject(cachep->mutex, INFINITE);

	Block* block = get_buddy(cachep->slab_size_in_blocks);
	if (!block) {
		printf("\n\nBUDDY_ALLOCATION_ERROR\n\n");
		cachep->err = BUDDY_ALLOCATION_ERROR;
		ReleaseMutex(cachep->mutex);
		ReleaseMutex(slab_manager->allocation_mutex);
		return;
	}

	SlabMetaData* slab = (SlabMetaData*)block;
	slab->my_cache = cachep;

	SlabMetaData* bitvector_start = slab + 1;
	slab->bitvector_start = (unsigned*)bitvector_start;
	for (int i = 0; i < cachep->bitvector_size_in_unsigned; i++) {
		*(slab->bitvector_start + i) = 0;
	}

	unsigned* starting_slot = slab->bitvector_start + cachep->bitvector_size_in_unsigned + 1;

	if (L1_CACHE_ALIGNMENT && (cachep->unused_space_in_bytes / CACHE_L1_LINE_SIZE)) {
		unsigned cache_offset = rand() % (cachep->unused_space_in_bytes / CACHE_L1_LINE_SIZE);
		unsigned starting_slot_with_l1_offset = ((unsigned)starting_slot + cache_offset * CACHE_L1_LINE_SIZE);
		slab->starting_slot = (void*)starting_slot_with_l1_offset;
	}
	else {
		slab->starting_slot = (void*)starting_slot;
	}

	slab->next = cachep->empty_slabs;
	cachep->empty_slabs = slab;

	slab->free_slot_cnt = cachep->num_of_objects_in_slab;

	ReleaseMutex(cachep->mutex);
	ReleaseMutex(slab_manager->slab_mutex);
}

SlabMetaData* get_slab_by_object_from_cache(kmem_cache_t* cachep, void* obj, int* type) {

	WaitForSingleObject(slab_manager->list_mutex, INFINITE);

	SlabMetaData* iterator = cachep->full_slabs;
	while (iterator) {
		void* starting_slot = iterator->starting_slot;
		if ((unsigned)obj >= (unsigned)iterator->starting_slot && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
			*type = 1;
			ReleaseMutex(slab_manager->list_mutex);
			return iterator;
		}
		iterator = iterator->next;
	}

	iterator = cachep->mixed_slabs;
	while (iterator) {
		if ((unsigned)obj >= (unsigned)iterator->starting_slot && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
			*type = 2;
			ReleaseMutex(slab_manager->list_mutex);
			return iterator;
		}
		iterator = iterator->next;
	}

	iterator = cachep->empty_slabs;
	while (iterator) {
		if ((unsigned)obj >= (unsigned)iterator->starting_slot && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
			*type = 3;
			ReleaseMutex(slab_manager->list_mutex);
			return iterator;
		}
		iterator = iterator->next;
	}

	ReleaseMutex(slab_manager->list_mutex);
	return NULL;
}

void kmem_cache_free(kmem_cache_t* cachep, void* objp) {

	WaitForSingleObject(slab_manager->free_mutex, INFINITE);
	WaitForSingleObject(cachep->mutex, INFINITE);

	int type = -1;	//1 full, 2 mixed, 3 empty
	SlabMetaData* slab = get_slab_by_object_from_cache(cachep, objp, &type);

	if (!slab) {
		ReleaseMutex(cachep->mutex);
		ReleaseMutex(slab_manager->free_mutex);
		return;
	}

	int slot = ((unsigned)objp - (unsigned)slab->starting_slot) / cachep->object_size_in_bytes;
	int index = slot / bits_in_unsigned;
	int deg = slot % bits_in_unsigned;
	unsigned mask = ~(int)(1 << deg);
	slab->bitvector_start[index] &= mask;

	SlabMetaData* iterator = NULL, * prev = NULL;;
	if (type == 1) {
		iterator = cachep->full_slabs;
	}
	else if (type == 2) {
		iterator = cachep->mixed_slabs;
	}
	else {
		iterator = cachep->empty_slabs;
	}

	while (iterator != slab) {
		prev = iterator;
		iterator = iterator->next;
	}

	if (prev) {
		prev->next = iterator->next;
	}
	else {
		if (type == 1) {
			cachep->full_slabs = cachep->full_slabs->next;
		}
		else if (type == 2) {
			cachep->mixed_slabs = cachep->mixed_slabs->next;
		}
		else if (type == 3) {
			cachep->empty_slabs = cachep->empty_slabs->next;
		}
	}

	slab->next = NULL;
	slab->free_slot_cnt++;


	if (slab->free_slot_cnt == cachep->num_of_objects_in_slab) {
		slab->next = cachep->empty_slabs;
		cachep->empty_slabs = slab;
	} else {
		slab->next = cachep->mixed_slabs;
		cachep->mixed_slabs = slab;
	}

	ReleaseMutex(cachep->mutex);
	ReleaseMutex(slab_manager->free_mutex);
}

// Alloacate one small memory buffer
void* kmalloc(size_t size) {
	//WaitForSingleObject(slab_manager->main_mutex, INFINITE);
	int deg = log2(next_power_of_two(size));
	deg = 0;
	int cnt = 1;
	while (cnt < size) {
		cnt <<= 1;
		deg++;
	}

	kmem_cache_t* cachep = &slab_manager->small_buffer_caches[deg - STARTING_BUFFER_DEGREE];
	unsigned ptr =(unsigned)kmem_cache_alloc(cachep);
	//ReleaseMutex(slab_manager->main_mutex, INFINITE);
	return (void*)ptr;
}

SlabMetaData* get_slab_by_object_from_buffer(void* obj, int *type) {

	WaitForSingleObject(slab_manager->list_mutex, INFINITE);

	for (int i = 0; i <= NUMBER_OF_BUFFER_DEGREES; i++) {
		kmem_cache_t* cachep = &slab_manager->small_buffer_caches[i];

		SlabMetaData* iterator = cachep->full_slabs;
		while (iterator) {
			if ((unsigned)obj >= (unsigned)iterator->starting_slot && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
				*type = 1;
				ReleaseMutex(slab_manager->list_mutex);
				return iterator;
			}
			iterator = iterator->next;
		}

		iterator = cachep->mixed_slabs;
		while (iterator) {
			if ((unsigned)obj >= (unsigned)iterator->starting_slot && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
				*type = 2;
				ReleaseMutex(slab_manager->list_mutex);
				return iterator;
			}
			iterator = iterator->next;
		}

		iterator = cachep->empty_slabs;
		while (iterator) {
			if ((unsigned)obj >= (unsigned)iterator->starting_slot && (unsigned)obj < ((unsigned)iterator + BLOCK_SIZE * cachep->slab_size_in_blocks)) {
				*type = 3;
				ReleaseMutex(slab_manager->list_mutex);
				return iterator;
			}
			iterator = iterator->next;
		}
	}

	return NULL;
}

void kfree(const void* objp) {

	WaitForSingleObject(slab_manager->main_mutex, INFINITE);

	int type = -1;	//1 full, 2 mixed, 3 empty
	SlabMetaData* slab = get_slab_by_object_from_buffer(objp, &type);
	kmem_cache_t* cachep = slab->my_cache;

	ReleaseMutex(slab_manager->main_mutex);


	WaitForSingleObject(cachep->mutex, INFINITE);

	int slot = ((unsigned)objp - (unsigned)slab->starting_slot) / cachep->object_size_in_bytes;
	int index = slot / bits_in_unsigned;
	int deg = slot % bits_in_unsigned;
	unsigned mask = ~(int)pow(2, deg);
	slab->bitvector_start[index] &= mask;

	SlabMetaData* iterator = NULL, * prev = NULL;;
	if (type == 1) {
		iterator = cachep->full_slabs;
	}
	else if (type == 2) {
		iterator = cachep->mixed_slabs;
	}
	else {
		iterator = cachep->empty_slabs;
	}

	while (iterator != slab) {
		prev = iterator;
		iterator = iterator->next;
	}

	if (prev) {
		prev->next = iterator->next;
	}
	else {
		if (type == 1) {
			cachep->full_slabs = cachep->full_slabs->next;
		}
		else if (type == 2) {
			cachep->mixed_slabs = cachep->mixed_slabs->next;
		}
		else if (type == 3) {
			cachep->empty_slabs = cachep->empty_slabs->next;
		}
	}

	slab->next = NULL;
	slab->free_slot_cnt++;


	if (slab->free_slot_cnt == cachep->num_of_objects_in_slab) {
		slab->next = cachep->empty_slabs;
		cachep->empty_slabs = slab;
	}
	else {
		slab->next = cachep->mixed_slabs;
		cachep->mixed_slabs = slab;
	}

	ReleaseMutex(cachep->mutex);
}


int get_free_index_bitvector(kmem_cache_t* cachep, SlabMetaData* slab) {

	WaitForSingleObject(slab_manager->main_mutex, INFINITE);

	unsigned* bitvector = slab->bitvector_start;
	int free_index = -1;
	for (int i = 0; i < cachep->bitvector_size_in_unsigned; i++) {
		int finished = 0;
		for (int deg = 0; deg < 32; deg++) {
			unsigned mask = 1 << deg;
			if (!(mask & *(bitvector + i))) {
				free_index = i * 32 + deg;
				finished = 1;
				//bitvector[i] |= mask;
				break;
			}
		}
		if (finished) {
			break;
		}
	}

	ReleaseMutex(slab_manager->main_mutex);
	return free_index;
}

int kmem_cache_shrink(kmem_cache_t* cachep) {

	WaitForSingleObject(cachep->mutex, INFINITE);

	int freed = 0;
	while (cachep->empty_slabs) {
		SlabMetaData* to_delete_slab = cachep->empty_slabs;
		cachep->empty_slabs = to_delete_slab->next;

		Block* to_delete_block = (Block*)to_delete_slab;
		put_buddy(to_delete_block, cachep->slab_size_in_blocks);
		freed += cachep->slab_size_in_blocks;
	}

	ReleaseMutex(cachep->mutex);
	return freed;
}

void kmem_cache_destroy(kmem_cache_t* cachep) {

	if (cachep == &slab_manager->cache_of_caches) {
		return;
	}

	WaitForSingleObject(cachep->mutex, INFINITE);	

	/*if (cachep->full_slabs || cachep->mixed_slabs) {
		printf("\n\nCACHE_CANNOT_BE_DELETED\n\n");
		cachep->err = CACHE_CANNOT_BE_DELETED;
		ReleaseMutex(cachep->mutex);
		return;
	}*/

	while (cachep->full_slabs) {
		SlabMetaData* to_delete_slab = cachep->full_slabs;
		cachep->full_slabs = to_delete_slab->next;

		Block* to_delete_block = (Block*)to_delete_slab;
		put_buddy(to_delete_block, cachep->slab_size_in_blocks);
	}

	while (cachep->mixed_slabs) {
		SlabMetaData* to_delete_slab = cachep->mixed_slabs;
		cachep->mixed_slabs = to_delete_slab->next;

		Block* to_delete_block = (Block*)to_delete_slab;
		put_buddy(to_delete_block, cachep->slab_size_in_blocks);
	}

	kmem_cache_shrink(cachep);


	kmem_cache_t* iterator = &slab_manager->cache_of_caches, * prev = NULL;
	while (iterator != cachep) {
		prev = iterator;
		iterator = iterator->next;
	}
	if (prev) {
		prev->next = iterator->next;
	}


	ReleaseMutex(cachep->mutex);
}

void kmem_cache_info(kmem_cache_t* cachep) {

	WaitForSingleObject(slab_manager->print_mutex, INFINITE);

	printf("\n\Cache name -> %s\n", cachep->name);
	printf("Object size in bytes -> %d\n", cachep->object_size_in_bytes);
	printf("Slab size in Blocks -> %d\n", cachep->slab_size_in_blocks);
	printf("Max num of objects in slab -> %d\n", cachep->num_of_objects_in_slab);

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
	
	printf("Empty slabs number -> %d\n", empty_slabs);
	printf("Full slabs number -> %d\n", full_slabs);
	printf("Mixed slabs number -> %d\n", mixed_slabs);
	printf("Free space left -> %d\n", free_space);
	int taken_space = (empty_slabs + full_slabs + mixed_slabs) * cachep->num_of_objects_in_slab - free_space;
	printf("Total objects created -> %d\n", taken_space);
	printf("Percentage of space used -> %lf\n", (double)taken_space / (double)(taken_space + free_space) * 100);
	printf("Unused space inside slab -> %d\n", cachep->unused_space_in_bytes);
	if (L1_CACHE_ALIGNMENT) {
		printf("Different L1_Cache alignments -> %d\n", (cachep->unused_space_in_bytes / CACHE_L1_LINE_SIZE));
	}
	printf("\n");
	ReleaseMutex(slab_manager->print_mutex);
}

int kmem_cache_error(kmem_cache_t* cachep) {
	return cachep->err;
}