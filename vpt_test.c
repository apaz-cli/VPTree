#define MAIN
#ifdef MAIN

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define DEBUG 0
#include "log.h"

#define MEMDEBUG 0
#define PRINT_MEMALLOCS 0
#include "memdebug.h/memdebug.h"

#define VECDIM 4
#include "vec.h"

#define vpt_t VEC
#include "vpt.h"

#define RMAX 50.0
#define RMIN 0.0
double rand_zero_fifty() {
    return RMIN + (rand() / (RAND_MAX / (RMAX - RMIN)));
}

#if DEBUG
static inline void
assert_in_range(VEC* entries, size_t num) {
    for (size_t i = 0; i < num; i++) {
        for (size_t j = 0; j < VECDIM; j++) {
            assert(entries[i].data[j] >= RMIN);
            assert(entries[i].data[j] <= RMAX);
        }
    }
}
#else
static inline void assert_in_range(VEC* entries, size_t num) {
    (void)entries;
    (void)num;
}
#endif

static inline void
print_inorder(VPNode* node, size_t depth) {
    size_t indent_size = 4 * depth;
    char spaces[128];
    memset(spaces, ' ', indent_size);
    spaces[indent_size] = '\0';

    if (node->ulabel == 'b') {
        print_inorder(node->u.branch.left, depth + 1);

        printf("%s", spaces);
        print_VEC(&(node->u.branch.item));
        fflush(stdout);

        print_inorder(node->u.branch.right, depth + 1);
    } else {
        printf("%s<list>\n", spaces);
        fflush(stdout);
    }
}

static inline vpt_t*
gen_entries(size_t num_entries) {
    // Generate some random data
    VEC* entries = malloc(num_entries * sizeof(VEC));
    if (!entries) {
        printf("Ran out of memory allocating points either to build the tree, or to search the tree with.\n");
        return NULL;
    }
    for (size_t i = 0; i < num_entries; i++) {
        for (size_t j = 0; j < VECDIM; j++) {
            entries[i].data[j] = rand_zero_fifty();
        }
    }

    return entries;
}

static inline bool
knn_test(VPTree* vpt, vpt_t* query_point, size_t k) {
    // knn
    VPEntry* knns = VPT_knn(vpt, *query_point, k);
    if (!knns) {
        free(query_point);
        return false;
    }
    size_t num_knns = min(k, vpt->size);

    // print results
    printf("Finished KNN.\n");
    printf("Closest %lu to: ", num_knns);
    print_VEC(query_point);

    for (size_t i = 0; i < num_knns; i++) {
        printf("dist: %f, ", knns[i].distance);
        print_VEC(&(knns[i].item));
    }

    free(knns);
    free(query_point);
    return true;
}

static inline bool
nn_test(VPTree* vpt, vpt_t* query_point) {
    // nn
    VPEntry* nn = VPT_nn(vpt, *query_point);
    if (!nn) {
        printf("Failed to find a nearest neighbor.\n");
        free(query_point);
        return false;
    }

    // print results
    printf("Finished NN.\n");
    printf("Closest VEC to: ");
    print_VEC(query_point);

    printf("dist: %f, ", nn->distance);
    print_VEC(&(nn->item));

    free(nn);
    free(query_point);
    return true;
}

int main() {
    // Generate some random data
    srand(time(0));
    size_t num_entries = 12000000;
    vpt_t* entries = gen_entries(num_entries);
    if (!entries) {
        printf("Failed to allocate enough memory for the points to put in the tree.\n");
        return 1;
    }
    printf("Generated random data.\n");

    // Build
    VPTree vpt;
    bool success = VPT_build(&vpt, entries, num_entries, VEC_distance, NULL);
    if (!success) {
        printf("Ran out of memory building the tree.\n");
        return 1;
    }
    printf("Built the tree.\n");

    // knn
    success = knn_test(&vpt, gen_entries(1), 20);
    if (!success) {
        printf("Ran out of memory during tree knn.\n");
        return 1;
    }

    // knn
    success = knn_test(&vpt, gen_entries(1), 50);
    if (!success) {
        printf("Ran out of memory during tree knn.\n");
        return 1;
    }

    // nn
    success = nn_test(&vpt, gen_entries(1));
    if (!success) {
        printf("Ran out of memory during tree nn.\n");
        return 1;
    }

    // Rebuild
    success = VPT_rebuild(&vpt);
    if (!success) {
        printf("Ran out of memory rebuilding the tree.\n");
        return 1;
    }

    // Free the remaining memory
    VPT_destroy(&vpt);
    free(entries);
    print_heap();
}
#endif