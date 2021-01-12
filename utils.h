#pragma once

const char small_buffer_cache_name[] = "small_buffer_cache";
const char cache_of_caches_name[] = "cache_of_caches";
#define NUMBER_OF_BUFFER_DEGREES 13
#define STARTING_BUFFER_DEGREE 5
const int bits_in_unsigned = sizeof(unsigned) * 8;

unsigned int next_power_of_two(unsigned int n) {
    unsigned int p = 1;
    if (n && !(n & (n - 1))) {
        return n;
    }
    while (p < n) {
        p <<= 1;
    }
    return p;
}

int previous_power_of_two(unsigned int n) {
    if (n < 1) {
        return 0;
    }
    int res = 1;
    for (int i = 0; i < 8 * sizeof(unsigned int); i++)
    {
        int curr = 1 << i;
        if (curr > n) {
            break;
        }
        res = curr;
    }
    return res;
}