#include <Python.h>

PyObject* call_function(PyThreadState* tstate, PyFrameObject* frame, PyCodeObject* code, int throw) {
    PyObject* value = PyTuple_GetItem(code->co_consts, 1);
    Py_INCREF(value);
    PyObject* retval = value;
    return retval;
}