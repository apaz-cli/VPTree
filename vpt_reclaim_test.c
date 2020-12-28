
#include "memdebug.h/memdebug.h"

#define vpt_t void*
#include "vpt.h"

#define NUM_DATAPOINTS 100000

#define abs(n) (((n) < 0) ? -(n) : (n))

double sort_pointers(void* extra_data, vpt_t p1, vpt_t p2) {
    (void)extra_data;
    return (double)(abs(p1 - p2));
}

int main() {
    // Create a bunch of voidptr data which will be tracked by memdebug.
    vpt_t* datapoints = malloc(sizeof(vpt_t) * NUM_DATAPOINTS);
    for (size_t i = 0; i < NUM_DATAPOINTS; i++) {
        datapoints[i] = malloc(1);
    }

    VPTree vpt;
    VPT_build(&vpt, datapoints, NUM_DATAPOINTS, sort_pointers, NULL);
    free(datapoints);

    size_t torn_size = vpt.size;
    vpt_t* torn = VPT_teardown(&vpt);
    for (size_t i = 0; i < torn_size; i++) {
        free(torn[i]);
    }
    free(torn);
    print_heap();
}