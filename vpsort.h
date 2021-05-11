#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

// To appease the clangd linter. This is actually defined already in vpt.h.
#ifndef __VPTree
#ifndef vpt_t
#define vpt_t void *
#endif

#ifndef dist_t
#define dist_t double
#endif

#ifndef dist_max
#include <math.h>
#define dist_max INFINITY
#endif


struct VPEntry {
  vpt_t item;
  dist_t distance;
};
typedef struct VPEntry VPEntry;
#endif

/********************/
/* Debug Assertions */
/********************/
#if DEBUG
static inline void 
assert_sorted(VPEntry *arr, size_t n) {
  for (size_t i = 1; i < n; i++) {
    assert(arr[i - 1].distance <= arr[i].distance);
  }
}
#else
static inline void 
assert_sorted(VPEntry *arr, size_t n) {
  (void)arr;
  (void)n;
}
#endif

/**************/
/* SHELL SORT */
/**************/
static inline void 
shellsort(VPEntry *arr, size_t n) {
  size_t interval, i, j;
  VPEntry temp;
  for (interval = n / 2; interval > 0; interval /= 2) {
    for (i = interval; i < n; i += 1) {
      temp = arr[i];
      j = i;
      for (; j >= interval && arr[j - interval].distance > temp.distance;
           j -= interval) {
        arr[j] = arr[j - interval];
      }
      arr[j] = temp;
    }
  }
}

/**************/
/* MERGE SORT */
/**************/
#define MERGESORT_NUM_THREADS 8
struct Sublist {
  VPEntry *arr;
  size_t n;
};
typedef struct Sublist Sublist;

static void *__mergesort_subsort(void *sublist) {
  VPEntry *arr = ((Sublist *)sublist)->arr;
  size_t n = ((Sublist *)sublist)->n;
  shellsort(arr, n);
  return NULL;
}
static inline void 
mergesort(VPEntry *arr, size_t n, VPEntry *scratch_space) {
  const size_t num_threads = MERGESORT_NUM_THREADS;
  pthread_t threadpool[num_threads - 1];
  size_t each = n / num_threads;

  size_t mergepos[num_threads];
  Sublist sublists[num_threads];
  size_t start = 0;

  VPEntry big, smallest;
  size_t i, j, smallest_idx = 0;

  for (i = 0; i < num_threads; i++) {
    mergepos[i] = 0;
  }

  // Start threads
  for (i = 0; i < num_threads - 1; i++) {
    sublists[i].arr = arr + start;
    sublists[i].n = each;
    if (pthread_create(threadpool + i, NULL, __mergesort_subsort,
                       &sublists[i])) {
      fprintf(stdout, "Error creating pthread.");
      exit(1);
    }

    start = start + each;
  }
  sublists[num_threads - 1].arr = arr + start;
  sublists[num_threads - 1].n = n - start;

  // Sort the last list on the current thread
  __mergesort_subsort(sublists + (num_threads - 1));

  // Wait for the other threads.
  for (i = 0; i < num_threads - 1; i++) {
    if (pthread_join(threadpool[i], NULL)) {
      fprintf(stdout, "Error joining pthread.");
      exit(1);
    }
  }

  big.distance = (dist_t) INFINITY;
  (void)(big.item);

  // Merge the arrays
  for (i = 0; i < n; i++) {
    smallest = big;
    for (j = 0; j < num_threads; j++) {
      if ((mergepos[j] < sublists[j].n) &&
          (sublists[j].arr[mergepos[j]].distance <= smallest.distance)) {
        smallest_idx = j;
        smallest = sublists[j].arr[mergepos[j]];
      }
    }
    mergepos[smallest_idx]++;
    scratch_space[i] = smallest;
  }
  // Copy the list back into the array. Debug = 1 in "vpt.h" to assert that the
  // array is getting sorted.
  memcpy(arr, scratch_space, n * sizeof(VPEntry));
}

/***************/
/* MASTER SORT */
/***************/
#define SORT_THRESHOLD 2000
static inline void 
VPSort(VPEntry *arr, size_t n, VPEntry *scratch_space) {
  if (n < SORT_THRESHOLD) {
    shellsort(arr, n);
  } else {
    mergesort(arr, n, scratch_space);
  }
  assert_sorted(arr, n);
}