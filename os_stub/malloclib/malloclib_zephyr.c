/**
 *  Copyright Notice:
 *  Copyright 2026 DMTF. All rights reserved.
 *  License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libspdm/blob/main/LICENSE.md
 **/

/*
 * Zephyr-backed allocator for libspdm.
 *
 * Zephyr's minimal libc and picolibc both expose malloc/free that route to
 * the system heap (CONFIG_COMMON_LIBC_MALLOC=y / CONFIG_PICOLIBC=y), so we
 * just reuse them. The application is responsible for sizing the heap via
 * CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE / CONFIG_HEAP_MEM_POOL_SIZE.
 */

#include <base.h>
#include <stdlib.h>
#include <string.h>

void *allocate_pool(size_t AllocationSize)
{
    return malloc(AllocationSize);
}

void *allocate_zero_pool(size_t AllocationSize)
{
    void *buffer = malloc(AllocationSize);

    if (buffer == NULL) {
        return NULL;
    }
    memset(buffer, 0, AllocationSize);
    return buffer;
}

void free_pool(void *buffer)
{
    free(buffer);
}
