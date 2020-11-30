#ifndef __INCLUDED_MEMDEBUG
#define __INCLUDED_MEMDEBUG

#ifndef MEMDEBUG
#define MEMDEBUG 1
#endif

#if MEMDEBUG
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/***********************/
/* Allocation Tracking */
/***********************/

struct MemAlloc {
    void* ptr;
    size_t size;
    size_t line;
    char* file;
};
typedef struct MemAlloc MemAlloc;
#define MEMDEBUG_START_NUM_ALLOCS 5000
size_t num_allocs = 0;
size_t allocs_cap = 0;
MemAlloc* allocs;

#ifndef MEMPANIC_EXIT_STATUS
#define MEMPANIC_EXIT_STATUS 10
#endif
#ifndef OOM_EXIT_STATUS
#define OOM_EXIT_STATUS 11
#endif

/**********************/
/* Externally visible */
/**********************/
extern void print_heap();

// At some point I may add callbacks to remedy this, but it shouldn't be too hard to just edit this file directly.
/**************************/
/* Not Externally visible */
/**************************/
static inline void mempanic(void* badptr, char* message, size_t line, char* file);
static inline void OOM(size_t line, char* file);

static inline void
mempanic(void* badptr, char* message, size_t line, char* file) {
    printf("MEMORY PANIC: %s\nPointer: %p\nOn line: %lu\nIn file: %s\nAborted.\n", message, badptr, line, file);
    fflush(stdout);
    exit(MEMPANIC_EXIT_STATUS);
}

static inline void
OOM(size_t line, char* file) {
    printf("Out of memory on line %lu in file: %s.\nDumping heap:\n", line, file);
    print_heap();
    exit(OOM_EXIT_STATUS);
}

extern void
print_heap() {
    size_t total_allocated = 0;
    size_t i;

    printf("\n*************\n* HEAP DUMP *\n*************\n");
    for (i = 0; i < num_allocs; i++) {
        printf("Heap ptr: %p of size: %lu Allocated in file: %s On line: %lu\n",
               allocs[i].ptr, allocs[i].size, allocs[i].file, allocs[i].line);
        total_allocated += allocs[i].size;
    }
    printf("\nTotal Heap size: %lu, number of items: %lu\n\n\n", total_allocated, num_allocs);
    fflush(stdout);
}

#define MEM_FAIL_TO_FIND 4294967295
static inline size_t
alloc_find_index(void* ptr) {
    size_t i;
    for (i = 0; i < num_allocs; i++) {
        if (allocs[i].ptr == ptr) {
            return i;
        }
    }
    return MEM_FAIL_TO_FIND;
}

static inline void
alloc_push(MemAlloc alloc) {
    size_t new_allocs_cap;
    MemAlloc* newptr;

    // Allocate more memory to store the information about the allocations if necessary
    if (num_allocs >= allocs_cap) {
        // If the list hasn't actually been initialized, initialize it. Otherwise, it needs to be grown.
        if (allocs_cap == 0) {
            allocs_cap = MEMDEBUG_START_NUM_ALLOCS;
            allocs = (MemAlloc*)malloc(sizeof(MemAlloc) * allocs_cap);
        } else {
            new_allocs_cap = allocs_cap * 1.4;
            newptr = (MemAlloc*)realloc(allocs, sizeof(MemAlloc) * new_allocs_cap);
            if (!newptr) {
                printf("Failed to allocate more space to track allocations.\n");
                OOM(__LINE__, __FILE__);
            }
            allocs_cap = new_allocs_cap;
            allocs = newptr;
        }
    }

    // Append the new allocation to the list
    allocs[num_allocs] = alloc;
    num_allocs++;
}

static inline void
alloc_remove(size_t index) {
    // Shift the elements one left to remove it from the list.
    num_allocs--;
    for (; index < num_allocs; index++) {
        allocs[index] = allocs[index + 1];
    }
}

static inline void
alloc_update(size_t index, MemAlloc new_info) {
    // Update the entry in the list
    allocs[index] = new_info;
}

/*********************************************/
/* malloc(), realloc(), free() Redefinitions */
/*********************************************/

extern void*
memdebug_malloc(size_t n, size_t line, char* file) {
    // Call malloc()
    void* ptr = malloc(n);
    if (!ptr) OOM(line, file);

    // Print message
    printf("malloc(%lu) -> %p in %s, line %lu.\n", n, ptr, file, line);
    fflush(stdout);

    // Keep a record of it
    MemAlloc newalloc;
    newalloc.ptr = ptr;
    newalloc.size = n;
    newalloc.line = line;
    newalloc.file = file;
    alloc_push(newalloc);
    return ptr;
}

extern void*
memdebug_realloc(void* ptr, size_t n, size_t line, char* file) {
    // Check to make sure the allocation exists, and keep track of the location
    size_t alloc_index = alloc_find_index(ptr);
    if (ptr != NULL && alloc_index == MEM_FAIL_TO_FIND) {
        mempanic(ptr, "Tried to realloc() an invalid pointer.", line, file);
    }

    // Call realloc()
    void* newptr = realloc(ptr, n);
    if (!newptr) OOM(line, file);

    // Print message
    printf("realloc(%p, %lu) -> %p in %s, line %lu.\n", ptr, n, newptr, file, line);
    fflush(stdout);

    // Update the record of allocations
    MemAlloc newalloc;
    newalloc.ptr = newptr;
    newalloc.size = n;
    newalloc.line = line;
    newalloc.file = file;
    alloc_update(alloc_index, newalloc);
    return newptr;
}

extern void
memdebug_free(void* ptr, size_t line, char* file) {
    // Check to make sure the allocation exists, and keep track of the location
    size_t alloc_index = alloc_find_index(ptr);
    if (ptr != NULL && alloc_index == MEM_FAIL_TO_FIND) {
        mempanic(ptr, "Tried to free() an invalid pointer.", line, file);
    }

    // Call free()
    free(ptr);

    // Print message
    printf("free(%p) in %s, line %lu.\n", ptr, file, line);
    fflush(stdout);

    // Remove from the list of allocations
    alloc_remove(alloc_index);
}

#define malloc(n) memdebug_malloc(n, __LINE__, __FILE__)
#define realloc(ptr, n) memdebug_realloc(ptr, n, __LINE__, __FILE__)
#define free(ptr) memdebug_free(ptr, __LINE__, __FILE__)

#else  // MEMDEBUG flag
void print_heap() {}
#endif
#endif  // Include guard
