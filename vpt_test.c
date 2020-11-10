#define MAIN
#ifdef MAIN

#include <math.h>
#include <stdio.h>
#include <string.h>

#define VECDIM 16

struct VEC {
    double data[VECDIM];
};
typedef struct VEC VEC;

double VEC_distance(const VEC v1, const VEC v2) {
    size_t i;
    double sum = 0;
    double diffs[VECDIM];
    for (i = 0; i < VECDIM; i++) {
        diffs[i] = v1.data[i] - v2.data[i];
        diffs[i] = diffs[i] * diffs[i];
        sum += diffs[i];
    }
    return sqrt(sum);
}

#undef vpt_t
#define vpt_t VEC
#include "vpt.h"

void print_inorder(VPNode* node, size_t depth) {
    size_t indent_size = 4 * depth;
    char spaces[50];
    memset(spaces, ' ', indent_size);
    spaces[indent_size] = '\0';

    if (node->ulabel == 'b') {
        print_inorder(node->u.branch.left, depth + 1);
        printf("%s<%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f>\n", spaces, node->u.branch.item.data[0], node->u.branch.item.data[1], node->u.branch.item.data[2], node->u.branch.item.data[3], node->u.branch.item.data[4], node->u.branch.item.data[5], node->u.branch.item.data[6], node->u.branch.item.data[7], node->u.branch.item.data[8], node->u.branch.item.data[9], node->u.branch.item.data[10], node->u.branch.item.data[11], node->u.branch.item.data[12], node->u.branch.item.data[13], node->u.branch.item.data[14], node->u.branch.item.data[15]);
        fflush(stdout);
        print_inorder(node->u.branch.right, depth + 1);
    } else {
        printf("%s<list>\n", spaces);
        fflush(stdout);
    }
}

int main() {
    // Generate some random data
    const size_t num = 6000000;

    VEC* entries = malloc(num * sizeof(VEC));
    for (size_t i = 0; i < num; i++) {
        for (size_t j = 0; j < VECDIM; j++) {
            entries[i].data[j] = (float)rand() / (RAND_MAX / 50.0);
        }
    }

    // Build
    VPTree vpt;
    VPT_build(&vpt, entries, num, VEC_distance);

    // Print
    print_inorder(vpt.root, 0);

    // Rebuild and print
    VPT_rebuild(&vpt);
    print_inorder(vpt.root, 0);
}
#endif