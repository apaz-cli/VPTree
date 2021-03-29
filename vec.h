#ifndef __VEC
#define __VEC
#include <stdbool.h>
#include <math.h>

#ifndef VECDIM
#define VECDIM 64
#endif

struct VEC {
    double data[VECDIM];
};
typedef struct VEC VEC;

// Suppresses compiler warning for unused variable
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

static inline bool
VEC_equal(VEC v1, VEC v2) {
    for (size_t i = 0; i < VECDIM; i++) {
        if (v1.data[i] - v2.data[i]) return false;
    }
    return true;
}

static inline bool
VEC_order(VEC v1, VEC v2) {
    double sum1 = 0;
    double sum2 = 0;
    for (size_t i = 0; i < VECDIM; i++) {
        sum1 += v1.data[i];
        sum2 += v2.data[i];
    }
    return sum1 < sum2;
}

static inline void
VEC_sort(VEC* arr, size_t n) {
    size_t interval, i, j;
    VEC temp;
    for (interval = n / 2; interval > 0; interval /= 2) {
        for (i = interval; i < n; i += 1) {
            temp = arr[i];
            for (j = i; j >= interval && VEC_order(temp, arr[j - interval]); j -= interval) {
                arr[j] = arr[j - interval];
            }
            arr[j] = temp;
        }
    }
}

static inline void
print_VEC(VEC* vec) {
    printf("<");
    for (size_t i = 0; i < VECDIM - 1; i++) {
        printf("%.2f,", vec->data[i]);
    }
    printf("%.2f>\n", vec->data[VECDIM - 1]);
    fflush(stdout);
}

#endif