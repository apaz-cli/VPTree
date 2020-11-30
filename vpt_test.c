#define MAIN
#ifdef MAIN

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEBUG 0
#include "log.h"

#define MEMDEBUG 0
#include "memdebug.h"

#define VECDIM 64
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
static inline void assert_in_range(VEC* entries, size_t num) {}
#endif

static inline void
print_inorder(VPNode* node, size_t depth) {
    size_t indent_size = 4 * depth;
    char spaces[128];
    memset(spaces, ' ', indent_size);
    spaces[indent_size] = '\0';

    if (node->ulabel == 'b') {
        print_inorder(node->u.branch.left, depth + 1);

        print_VEC(&(node->u.branch.item));

        print_inorder(node->u.branch.right, depth + 1);
    } else {
        printf("%s<list>\n", spaces);
        fflush(stdout);
    }
}

void knn_test(size_t k) {
    // Generate some random data
    const size_t num = 1200000;

    VEC* entries = malloc(num * sizeof(VEC));
    for (size_t i = 0; i < num; i++) {
        for (size_t j = 0; j < VECDIM; j++) {
            entries[i].data[j] = rand_zero_fifty();
        }
    }
    LOGs("Generated data.");
    printf("Generated data.\n");

    // Build
    VPTree vpt;
    VPT_build(&vpt, entries, num, VEC_distance, NULL);

    printf("Built tree.\n");

    VPEntry* knns = VPT_knn(&vpt, entries[0], k);
    printf("Nearest Neighbors to: ");
    print_VEC(entries);
    for (size_t i = 0; i < k; i++) {
        printf("dist: %f, ", knns[i].distance);
        print_VEC(&(knns[i].item));
    }

    printf("Finished KNN.\n");
}

void build_destroy_test() {
    // Generate some random data
    const size_t num = 1200000;

    VEC* entries = malloc(num * sizeof(VEC));
    for (size_t i = 0; i < num; i++) {
        for (size_t j = 0; j < VECDIM; j++) {
            entries[i].data[j] = rand_zero_fifty();
        }
    }
    LOGs("Generated data.");

    assert_in_range(entries, num);

    // Build
    VPTree vpt;
    VPT_build(&vpt, entries, num, VEC_distance, NULL);

    vpt_t* reclaimed = VPT_teardown(&vpt);

    assert_in_range(reclaimed, num);
}

int main() {
    knn_test(20);
    // build_destroy_test();
}
#endif