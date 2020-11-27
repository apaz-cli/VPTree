#define MAIN
#ifdef MAIN

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "memdebug.h"

#define VECDIM 64

struct VEC {
    double data[VECDIM];
};
typedef struct VEC VEC;

#define UNUSED(x) (void)(x)

// CLANG VECTORIZES THIS BUT GCC DOES NOT FOR SOME DUMBASS REASON
// SERIOUSLY GCC WHAT THE FUCK THERE'S LITERALLY A SINGLE INSTRUCTION THAT DOES EACH OF THESE
double VEC_distance(void* extra_data, VEC v1, VEC v2) {
    UNUSED(extra_data);

    size_t i;
    double sum = 0;
    double diffs[VECDIM];
    for (i = 0; i < VECDIM; i++)
        diffs[i] = v1.data[i] - v2.data[i];

    for (i = 0; i < VECDIM; i++)
        diffs[i] = diffs[i] * diffs[i];

    for (i = 0; i < VECDIM; i++)
        sum += diffs[i];

    return sqrt(sum);
}

#define vpt_t VEC
#include "vpt.h"

static inline void print_inorder(VPNode* node, size_t depth) {
    size_t indent_size = 4 * depth;
    char spaces[4 * VPT_MAX_HEIGHT];
    memset(spaces, ' ', indent_size);
    spaces[indent_size] = '\0';

    if (node->ulabel == 'b') {
        print_inorder(node->u.branch.left, depth + 1);

        printf("%s<", spaces);
        for (size_t i = 0; i < VECDIM - 1; i++) {
            printf("%.2f,", node->u.branch.item.data[i]);
        }
        printf("%.2f>\n", node->u.branch.item.data[VECDIM - 1]);
        fflush(stdout);

        print_inorder(node->u.branch.right, depth + 1);
    } else {
        printf("%s<list>\n", spaces);
        fflush(stdout);
    }
}

int main() {
    // Generate some random data
    const size_t num = 1200000;

    VEC* entries = malloc(num * sizeof(VEC));
    for (size_t i = 0; i < num; i++) {
        for (size_t j = 0; j < VECDIM; j++) {
            entries[i].data[j] = (float)rand() / (RAND_MAX / 50.0);
        }
    }
    LOGs("Generated data.");

    // Build
    VPTree vpt;
    VPT_build(&vpt, entries, num, VEC_distance, NULL);

    // print_inorder(vpt.root, 0);
}
#endif