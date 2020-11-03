
#ifndef __VPTree
#define __VPTree
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "list.h"

// #define DEBUG
#ifdef DEBUG
#include <assert.h>
#include <stdio.h>
#endif

#ifndef vpt_t
#define vpt_t void*
#endif

/**********************/
/* Struct Definitions */
/**********************/

struct VPNode;
typedef struct VPNode VPNode;

struct VPBranch {
    VPNode* left;
    VPNode* right;
    vpt_t item;
    double radius;
};
typedef struct VPBranch VPBranch;

struct PList {
    vpt_t* items;
    size_t size;
    size_t capacity;
};
typedef struct PList PList;

union NodeUnion {
    VPBranch branch;
    PList pointlist;
} u;
typedef union NodeUnion NodeUnion;

struct VPNode {  // 40 with vpt_t = void*
    char ulabel;
    NodeUnion u;
};

struct VPTree {
    VPNode root;
    size_t size;
    double (*dist_fn)(const vpt_t first, const vpt_t second);
};
typedef struct VPTree VPTree;

struct VPEntry {
    vpt_t item;
    double distance;
};
typedef struct VPEntry VPEntry;

/***********************************/
/* Sort (Necessary for tree build) */
/***********************************/

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

#ifdef DEBUG
    for (size_t i = 0; i < n - 2; ++i) {
        assert(arr[i].distance <= arr[i + 1].distance);
    }
    printf("Sorted array len: %lu\n", n);
#endif
}

/****************/
/* Tree Methods */
/****************/
inline size_t
align_to_page(size_t requested) {
    size_t pagesize = sysconf(_SC_PAGESIZE);
    return (requested % pagesize) + pagesize;
}

// Returns first
inline vpt_t
sort_by_first(VPEntry* entries, size_t n, double (*dist_fn)(const vpt_t first, const vpt_t second)) {
    vpt_t sort_by = entries[0].item;
    for (size_t i = 0; i < n; ++i) {
        entries[i].distance = dist_fn(sort_by, entries[i].item);
    }
    shellsort(entries + 1, n - 1);
    return sort_by;
}

inline bool
VPT_build(VPTree* vpt, vpt_t* data, size_t num_items,
          double (*dist_fn)(const vpt_t first, const vpt_t second)) {
    vpt->dist_fn = dist_fn;
    vpt->size = num_items;

    // Pop the first datapoint and make it the root
    if (num_items == 1) {
        vpt->root.ulabel = 1;
        vpt->root.u.pointlist.items = (vpt_t*)malloc(sizeof(vpt_t));
        vpt->root.u.pointlist.items[0] = data[0];
        return;
    } else {
        vpt->root.ulabel = 0;
    }
    num_items--;
    data += 1;

    // Copy the rest of the data into an array so it can be sorted and resorted as the tree is built
    VPEntry pointlist[num_items];
    for (int i = 0; i < num_items; ++i)
        pointlist[i].item = data[i];

    // Hold information about the right node that needs to be created
    VPNode* parent;
    VPEntry* toBuild;
    size_t num_to_build;
    while (true) {
    }
}

inline bool
VPT_build_empty(VPTree* vpt, double (*dist_fn)(const vpt_t first, const vpt_t second));

inline void
VPT_destroy(VPTree* vpt);

inline vpt_t
VPT_nn(VPTree* vpt, vpt_t datapoint);

/* Creates a dynamic allocation that needs to be destroyed using List_destroy and then also freed */
inline List
VPT_knn(VPTree* vpt, vpt_t datapoint, size_t k) {
    List nns;
    List_create(&nns);
}

inline bool
VPT_add(VPTree* vpt, vpt_t datapoint);

#endif

#define MAIN
#ifdef MAIN

#include <stdio.h>
int main() {
    const size_t num = 500000;

    VPEntry entries[num];
    for (size_t i = 0; i < num; ++i) {
        double rand_dist = (float)rand() / (RAND_MAX / 50.0);
        entries[i].distance = rand_dist;
    }

    shellsort(entries, num);

    VPNode n;
    printf("VPNode %lu\n", sizeof(n));
    printf("NodeUnion %lu\n", sizeof(n.u));
    printf("VPBranch %lu\n", sizeof(n.u.branch));
    printf("PList %lu\n", sizeof(n.u.pointlist));

#ifdef DEBUG
    for (size_t i = 0; i < num - 2; ++i) {
        assert(entries[i].distance <= entries[i + 1].distance);
        printf("%f, ", entries[i].distance);
    }
    printf("%f\n", entries[num - 1].distance);
#endif
}
#endif