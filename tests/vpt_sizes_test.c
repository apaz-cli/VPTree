#define PRINT_STEPS 0

#define MEMDEBUG 0
#define PRINT_MEMALLOCS 0
#include "../memdebug.h/memdebug.h"

#define vpt_t double
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../vpt.h"

#define RMAX 50.0
#define RMIN 0.0
static inline double rand_zero_fifty() {
    return RMIN + (rand() / (RAND_MAX / (RMAX - RMIN)));
}

#define abs(x) (((x) < 0) ? -(x) : (x))

double double_dist(void* extra_data, vpt_t d1, vpt_t d2) {
    (void)extra_data;
    return abs(d1 - d2);
}

struct ArgStruct {
    vpt_t* rand_data;
    size_t treesize;
};
typedef struct ArgStruct ArgStruct;

void test(void* argstruct) {
    vpt_t* rand_data = ((ArgStruct*)argstruct)->rand_data;
    size_t treesize = ((ArgStruct*)argstruct)->treesize;
    // build
    VPTree vpt;
    bool success = VPT_build(&vpt, rand_data, treesize, double_dist, NULL);
    assert(success);

    // knn
    size_t k;
    while (!(k = (size_t)rand_zero_fifty())) {
    }
    // printf("k = %zu\n", k);

    size_t num_results;
    VPEntry result_space[k];
    VPT_knn(&vpt, 25.0, k, result_space, &num_results);
    assert(num_results);
    for (size_t i = 0; i < num_results; i++) {
        assert((result_space[i].item >= RMIN) && (result_space[i].item <= RMAX));
    }

    // nn
    VPEntry nn_result;
    VPT_nn(&vpt, 25.0, &nn_result);

    assert(nn_result.distance == result_space[0].distance);

    // destroy
    if (rand() & 1) {
        VPT_destroy(&vpt);
    } else {
        vpt_t* items = VPT_teardown(&vpt);
        assert(vpt.size == treesize);
        for (size_t i = 0; i < treesize; i++) {
            assert((items[i] >= RMIN) && (items[i] <= RMAX));
        }
        free(items);
    }

    if (PRINT_STEPS) printf("Finished testing tree of size: %zu.\n", treesize);
}

#include "threadpool.h/threadpool.h"

int main() {
    const size_t num_trees = 10000;

    srand(time(0));
    ArgStruct* args = malloc(num_trees * sizeof(ArgStruct));
    vpt_t* rand_data = malloc(num_trees * sizeof(vpt_t));
    for (size_t i = 0; i < num_trees; i++) {
        rand_data[i] = rand_zero_fifty();
        args[i].rand_data = rand_data;
        args[i].treesize = i + 1;
    }

    Threadpool pool;
    POOL_create(&pool, 16);
    for (size_t i = 0; i < num_trees; i++) {
        POOL_exectask(&pool, test, args + i);
    }
    POOL_destroy(&pool);

    free(args);
    free(rand_data);
    assert(!get_num_allocs());
}