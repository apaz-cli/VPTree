#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"

// To appease the linter. This is never active.
#ifndef __VPTree
#ifndef vpt_t
#define vpt_t void*
#endif /*vpt_t*/

struct VPEntry {
    vpt_t* item;
    double distance;
};
typedef struct VPEntry VPEntry;
#endif /*__VPTree*/

// Debugging
#if DEBUG
void print_list(VPEntry* arr, size_t n) {
    double* p;
    double d;
    printf("[");
    for (size_t i = 0; i < n - 1; i++) {
        p = arr[i].item.data;
        d = arr[i].distance;
        printf("[%p,%.2f],", p, d);
    }
    p = arr[n - 1].item.data;
    d = arr[n - 1].distance;
    printf("[%p,%.2f]]\n", p, d);
}

void assert_sorted(VPEntry* arr, size_t n, vpt_t datapoint, VPTree* tree) {
    for (size_t i = 1; i < n; i++) {
        assert(tree->dist_fn(tree->extra_data, arr[i - 1].item, datapoint) == arr[i - 1].distance);
        assert(tree->dist_fn(tree->extra_data, arr[i].item, datapoint) == arr[i].distance);
        assert(arr[i - 1].distance <= arr[i].distance);
    }
}

void assert_locally_sorted(VPEntry* arr, size_t n) {
    for (size_t i = 1; i < n; i++) {
        assert(arr[i - 1].distance <= arr[i].distance);
    }
}

#else
void print_list(VPEntry* arr, size_t n) {}
void assert_sorted(VPEntry* arr, size_t n, vpt_t datapoint, VPTree* tree) {}
void assert_locally_sorted(VPEntry* arr, size_t n) {}
#endif

// Large
#define MERGESORT 0
#define LARGESORTALG MERGESORT
void mergesort(VPEntry* arr, size_t n);
void largesort(VPEntry* arr, size_t n) {
#if LARGESORTALG == MERGESORT
    mergesort(arr, n);
#endif
}

// Small
#define SHELLSORT 0
#define INSERTIONSORT 1
#define BUBBLESORT 2
#define SMALLSORTALG SHELLSORT
void shellsort(VPEntry* arr, size_t n);
void insersort(VPEntry* arr, size_t n);
void bubblsort(VPEntry* arr, size_t n);
void smallsort(VPEntry* arr, size_t n) {
#if SMALLSORTALG == SHELLSORT
    shellsort(arr, n);
#elif SMALLSORTALG == INSERTIONSORT
    insersort(arr, n);
#elif SMALLSORTALG == BUBBLESORT
    bubblesort(arr, n);
#endif
}

#define SORT_THRESHOLD 2000
void sort(VPEntry* arr, size_t n) {
    if (n < SORT_THRESHOLD) {
        smallsort(arr, n);
    } else {
        largesort(arr, n);
    }
}

/***************/
/* LARGE SORTS */
/***************/
#define MERGESORT_NUM_THREADS 8
struct Sublist {
    VPEntry* arr;
    size_t n;
};
typedef struct Sublist Sublist;
void* __mergesort_subsort(void* sublist) {
    VPEntry* arr = ((Sublist*)sublist)->arr;
    size_t n = ((Sublist*)sublist)->n;
    shellsort(arr, n);
    return NULL;
}
void mergesort(VPEntry* arr, size_t n) {
    const size_t num_threads = MERGESORT_NUM_THREADS;
    pthread_t threadpool[num_threads - 1];
    size_t each = n / num_threads;

    VPEntry* scratch_space;
    size_t mergepos[num_threads];
    Sublist sublists[num_threads];
    size_t start = 0;

    VPEntry big, smallest;
    size_t i = 0, j = 0, smallest_idx = 0;

    for (; i < num_threads; i++) {
        mergepos[i] = 0;
    }

    // Start threads
    // Hopefully this loop gets unrolled by the compiler
    for (i = 0; i < num_threads - 1; i++) {
        sublists[i].arr = arr + start;
        sublists[i].n = each;
        if (pthread_create(threadpool + i, NULL, __mergesort_subsort, &sublists[i])) {
            fprintf(stdout, "Error creating pthread.");
            exit(1);
        }

        debug_printf("  Started thread %lu from: %lu with %lu items.\n", i, start, each);

        start = start + each;
    }
    sublists[num_threads - 1].arr = arr + start;
    sublists[num_threads - 1].n = n - start;

    // Sort the last list on the current thread
    debug_printf("  Started thread %lu from: %lu with %lu items.\n", num_threads - 1, start, n - start);
    __mergesort_subsort(sublists + (num_threads - 1));

    // Allocate some memory for merging the lists, then wait for the other threads.
    scratch_space = (VPEntry*)malloc(n * sizeof(VPEntry));
    for (i = 0; i < num_threads - 1; i++) {
        if (pthread_join(threadpool[i], NULL)) {
            fprintf(stdout, "Error joining pthread.");
            exit(1);
        }
    }

    big.distance = INFINITY;

    // Merge the arrays
    // Here too, I hope that the compiler is going ham.
    for (i = 0; i < n; i++) {
        smallest = big;
        for (j = 0; j < num_threads; j++) {
            if ((mergepos[j] < sublists[j].n) && (sublists[j].arr[mergepos[j]].distance <= smallest.distance)) {
                smallest_idx = j;
                smallest = sublists[j].arr[mergepos[j]];
            }
        }
        mergepos[smallest_idx]++;
        scratch_space[i] = smallest;
    }
    // Copy the list back into the array. Debug = 1 in "vpt.h" to assert that the array is getting sorted.
    memcpy(arr, scratch_space, n * sizeof(VPEntry));
    free(scratch_space);
}

/***************/
/* SMALL SORTS */
/***************/
void shellsort(VPEntry* arr, size_t n) {
    size_t interval, i, j;
    VPEntry temp;
    for (interval = n / 2; interval > 0; interval /= 2) {
        for (i = interval; i < n; i += 1) {
            temp = arr[i];
            for (j = i; j >= interval && arr[j - interval].distance > temp.distance; j -= interval) {
                arr[j] = arr[j - interval];
            }
            arr[j] = temp;
        }
    }
}
void insersort(VPEntry* arr, size_t n) {
    size_t i, j;
    VPEntry key;
    for (j = 0; j < n - 1; j++) {
        i = j + 1;
        key = arr[i];

        /* Move elements of arr[0..i-1], that are  
        greater than key, to one position ahead  
        of their current position */
        for (; arr[j].distance > key.distance; j--) {
            arr[j + 1] = arr[j];
        }
        arr[j + 1] = key;
    }
}
void bubblsort(VPEntry* arr, size_t n) {
    bool swapped = false;
    VPEntry t;
    size_t c = 0, d;
    for (;;) {
        swapped = false;
        for (d = 0; d < n - c - 1; d++) {
            if (arr[d].distance > arr[d + 1].distance) {
                /* Swapping */
                t = arr[d];
                arr[d] = arr[d + 1];
                arr[d + 1] = t;
                swapped = true;
            }
        }
        if (!swapped) return;
        c++;
    }
}
