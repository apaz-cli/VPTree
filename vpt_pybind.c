// sudo apt-get install python3-dev
#include <python3.7m/Python.h>

#define vpt_t PyObject*
#include "vpt.h"

struct PYVPTree {
    VPTree vpt;
    PyObject comparator;
};
typedef struct PYVPTree PYVPTree;

double PyObject_distance(const vpt_t o1, const vpt_t o2) {
}

void PYVPT_build(PYVPTree* vpt, vpt_t* data, size_t num, PyObject* dist_fn) {
    // Assume the global interpreter lock.
    PyGILState_STATE state = PyGILState_Ensure();

    // Assert callable
    if (!PyCallable_Check(dist_fn)) {
        fprintf(stdout, "dist_fn must be callable.");
        PyGILState_Release(state);
        abort();
    }
    
}

int main() {
    vpt_t obj;
}