// sudo apt-get install python3-dev
#include <Python.h>

#define vpt_t PyObject*
#include "../vpt.h"

struct PyVPTree {
    PyObject_HEAD head;
    VPTree vpt;
};

double PyObject_distance_fptr(const void* extra_data, PyObject* first, PyObject* second) {
    PyObject* dist_fn = (PyObject*)extra_data;
    PyObject* args = PyTuple_Pack(2, first, second);
    PyObject* eval_result = PyObject_CallObject(dist_fn, args);
    double result = PyFloat_AS_DOUBLE(eval_result);
    return result;
}

static PyObject*
PYVPT_build(PyObject* data, PyObject* dist_fn) {
    PyGILState_STATE gil = PyGILState_Ensure();
    if (!PyIter_Check(data)) {
        PyErr_SetString(PyExc_ValueError, "data must be an iterable.");
        PyGILState_Release(gil);
        return NULL;
    }
    if (!PyCallable_Check(dist_fn)) {
        PyErr_SetString(PyExc_ValueError, "dist_fn must be callable.");
        PyGILState_Release(gil);
        return NULL;
    }

    size_t num_items = 0;
    size_t databuf_len = 5000;
    vpt_t* databuf = (vpt_t*)malloc(sizeof(vpt_t) * databuf_len);
    if (!databuf) return PyErr_NoMemory();

    PyObject* nextitem = data;
    while (nextitem != NULL) {
        nextitem = PyIter_Next(data);
        if (PyErr_Occurred()) {
            free(databuf);
            PyGILState_Release(gil);
            return NULL;
        }
    }

    VPTree* vpt = malloc(sizeof(VPTree));
    if (!vpt) return PyErr_NoMemory();

    VPT_build(vpt, databuf, databuf_len, PyObject_distance_fptr, &dist_fn);

    PyGILState_Release(gil);
    Py_RETURN_NONE;
}

static inline PyObject*
PYVPT_create(VPTree* vpt, vpt_t* data, size_t num, PyObject* dist_fn, PyGILState_STATE gil) {
}

PyObject*
PyVPT_nn(VPTree* vpt, PyObject* datapoint) {
}