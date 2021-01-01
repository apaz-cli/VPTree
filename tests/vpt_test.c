#define MAIN
#ifdef MAIN

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define DEBUG 0
#define DEBUG_LOG 0
#include "../log.h"

#define MEMDEBUG 1
#define PRINT_MEMALLOCS 0
#include "../memdebug.h/memdebug.h"

#define NUM_ENTRIES 1200000
#define VECDIM 32
#include "../vec.h"

#define vpt_t VEC
#include "../vpt.h"

#define RMAX 50.0
#define RMIN 0.0
double rand_zero_fifty() {
    return RMIN + (rand() / (RAND_MAX / (RMAX - RMIN)));
}

#define PRINT_STEPS 0

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
    size_t num_knns;
    VPEntry knns[k];
    VPT_knn(vpt, *query_point, k, knns, &num_knns);
    if (!num_knns) {
        return false;
    }

    // print results
    if (PRINT_STEPS) {
        printf("Finished KNN.\n");
        printf("Closest %zu to: ", num_knns);
        print_VEC(query_point);
    }

    for (size_t i = 0; i < num_knns; i++) {
        if (PRINT_STEPS) {
            printf("dist: %f, ", knns[i].distance);
            print_VEC(&(knns[i].item));
        }
    }

    free(query_point);
    return true;
}

static inline bool
nn_test(VPTree* vpt, vpt_t* query_point) {
    // nn
    VPEntry nn;
    VPT_nn(vpt, *query_point, &nn);

    // print results
    if (PRINT_STEPS) {
        printf("Finished NN.\n");
        printf("Closest VEC to: ");
        print_VEC(query_point);
        printf("dist: %f, ", nn.distance);
        print_VEC(&(nn.item));
    }

    free(query_point);
    return true;
}

int main() {
    // Generate some random data
    srand(time(0));
    size_t num_entries = NUM_ENTRIES;
    vpt_t* entries = gen_entries(num_entries);
    if (!entries) {
        printf("Failed to allocate enough memory for the points to put in the tree.\n");
        return 1;
    }
    if (PRINT_STEPS) printf("Generated random data.\n");

    // Build
    VPTree vpt;
    bool success = VPT_build(&vpt, entries, num_entries, VEC_distance, NULL);
    if (!success) {
        printf("Ran out of memory building the tree.\n");
        return 1;
    }

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

    // Assert no memory leaks
    assert(!get_num_allocs());
}
#endif