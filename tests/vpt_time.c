#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#define VECDIM 64
#include "../vec.h"

#define vpt_t VEC
#include "../vpt.h"

#define NUM_DATAPOINTS 3000000

static inline double
randfrom(double min, double max) {
    double range = (max - min);
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

static inline float
timedifference_msec(struct timeval t0, struct timeval t1) {
    return (t1.tv_sec - t0.tv_sec) * 1000.0f + (t1.tv_usec - t0.tv_usec) / 1000.0f;
}

int main() {
    // Construct data
    srand(0);
    vpt_t* datapoints = malloc(sizeof(vpt_t) * NUM_DATAPOINTS);
    for (size_t i = 0; i < NUM_DATAPOINTS; i++) {
        for (size_t j = 0; j < VECDIM; j++) {
            datapoints[i].data[j] = randfrom(-1.0, 1.0);
        }
    }

    // Construct query point
    VEC query_point;
    for (size_t j = 0; j < VECDIM; j++) {
        query_point.data[j] = randfrom(-1.0, 1.0);
    }

    VPEntry result;

    /**********/
    /* Create */
    /**********/

    // Start timer
    struct timeval start, end;
    gettimeofday(&start, NULL);
    // Create the tree
    VPTree vpt;
    VPT_build(&vpt, datapoints, NUM_DATAPOINTS, VEC_distance, NULL);
    // End timer
    gettimeofday(&end, NULL);
    printf("Building the tree took %f ms.\n", timedifference_msec(start, end));

    /******/
    /* NN */
    /******/

    // Start timer
    gettimeofday(&start, NULL);
    // Search the tree
    VPT_nn(&vpt, query_point, &result);
    // End timer
    gettimeofday(&end, NULL);
    printf("Searching the tree took %f ms.\n", timedifference_msec(start, end));

    return 0;
}