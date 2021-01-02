#pragma once

#include "buddy.h"
#include "slab.h"
#include "utils.h"
#include "types.h"
#include <math.h>
#include <stdio.h>

kmem_cache_t* kmem_cache_create(const char* name, size_t size, void(*ctor)(void*), void(*dtor)(void*)) {

}