#include <assert.h>

#define MEMDEBUG 1
#define PRINT_MEMALLOCS 0
#include "../memdebug.h/memdebug.h"

#define vpt_t void*
#include "../vpt.h"

#define abs(n) (((n) < 0) ? -(n) : (n))

double sort_pointers(void* extra_data, vpt_t p1, vpt_t p2) {
    (void)extra_data;
    double res = p1 - p2;
    return res < 0 ? -res : res;
}

void test(size_t num_datapoints) {
    // Create a bunch of voidptr data which will be tracked by memdebug.
    vpt_t* datapoints = malloc(sizeof(vpt_t) * num_datapoints);
    for (size_t i = 0; i < num_datapoints; i++) {
        datapoints[i] = malloc(1);
    }

    // printf("%zu\n", num_datapoints);
    VPTree vpt;
    VPT_build(&vpt, datapoints, num_datapoints, sort_pointers, NULL);
    free(datapoints);

    size_t torn_size = vpt.size;
    vpt_t* torn = VPT_teardown(&vpt);
    for (size_t i = 0; i < torn_size; i++) {
        free(torn[i]);
    }
    free(torn);

    assert(!get_num_allocs());
}

int main() {
    test(100000);
}