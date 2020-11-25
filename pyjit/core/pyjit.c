#include "pyjit.h"

__thread _PyFrameEvalFunction orig_eval_frame;
__thread int extra_slot;
__thread unsigned int compile_threshold = DEFAULT_COMPILE_THRESHOLD;

void pyjit_free(void* object) {
    pyjit_header_t* header = (pyjit_header_t*)object;
    header->func = NULL;
    if (header->dlhandle != NULL) {
        dlclose(header->dlhandle);
    }
    PyMem_Free(header);
}

pyjit_header_t* pyjit_ensure_extra(PyObject* code) {
    if (extra_slot == -1) {
        extra_slot = _PyEval_RequestCodeExtraIndex(pyjit_free);
        if (extra_slot == -1) {
            return NULL;
        }
    }
    pyjit_header_t* header = NULL;
    if (_PyCode_GetExtra(code, extra_slot, (void**)&header)) {
        PyErr_Clear();
        return NULL;
    }

    if (header == NULL) {
        header = PyMem_Malloc(sizeof(pyjit_header_t));
        if (header == NULL) {
            PyErr_Clear();
            return NULL;
        }
        header->call_count = 1;
        header->func = NULL;
        header->is_completed = false;
        header->dlhandle = NULL;
        if (_PyCode_SetExtra(code, extra_slot, header)) {
            PyErr_Clear();
            PyMem_Free(header);
            return NULL;
        }
    }

    return header;
}

PyObject* PyJit_EvalFrame(PyThreadState* tstate, PyFrameObject* frame, int throw) {
    //PyCodeObject* code = PyFrame_GetCode(frame);
    pyjit_header_t* co_extra = pyjit_ensure_extra((PyObject*)frame->f_code);
    //printf("pointer:%p\n", co_extra);
    if (co_extra->func != NULL) {
        //Py_DECREF(code);
        return co_extra->func(tstate, frame, throw);
    }

    if (!co_extra->is_completed) {
        if (co_extra->call_count > compile_threshold) {
            co_extra->is_completed = true;
            if (pyjit_compile(frame->f_code) != false) {
                void* handle = dlopen("./func.so", RTLD_NOW | RTLD_LOCAL);
                if (handle != NULL) {
                    pyjit_func func = dlsym(handle, "pyjit_func");
                    if (func != NULL) {
                        co_extra->func = func;
                        co_extra->dlhandle = handle;
                    }
                    else {
                        dlclose(handle);
                    }
                }
                unlink("./func.so");
            }
        }
        else {
            co_extra->call_count++;
        }
    }

    //Py_DECREF(code);
    return orig_eval_frame(tstate, frame, throw);
}

static PyObject* pyjit_disable(void) {
    PyThreadState* ts = PyThreadState_GET();
    if (orig_eval_frame == NULL) {
        orig_eval_frame = _PyEval_EvalFrameDefault;
    }
    _PyInterpreterState_SetEvalFrameFunc(ts->interp, orig_eval_frame);
    printf("JIT compiler disabled\n");
    return Py_None;
}

static PyObject* pyjit_enable(void) {
    PyThreadState* ts = PyThreadState_GET();
    orig_eval_frame = _PyInterpreterState_GetEvalFrameFunc(ts->interp);
    _PyInterpreterState_SetEvalFrameFunc(ts->interp, PyJit_EvalFrame);
    printf("JIT compiler enabled\n");
    return Py_None;
}

static PyObject* pyjit_compile_threshold(PyObject* self, PyObject* args) {
    unsigned int threshold;
    if (!PyArg_ParseTuple(args, "i", &threshold)) {
        return NULL;
    }
    compile_threshold = threshold;
    printf("Compile threshold set to: %d\n", threshold);
    return PyLong_FromLong(threshold);
}

static PyMethodDef PyjitMethods[] = {
    {"disable", (PyCFunction)pyjit_disable, METH_VARARGS,"Disable the JIT compiler"},
    {"enable", (PyCFunction)pyjit_enable, METH_VARARGS,"Enable the JIT compiler"},
    {"compile_threshold", (PyCFunction)pyjit_compile_threshold, METH_VARARGS,"Set the JIT compilation threshold"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef PyjitModule = {
    PyModuleDef_HEAD_INIT,
    "pyjit.core",
    "Python JIT module",
    -1,
    PyjitMethods,
};

PyMODINIT_FUNC PyInit_core(void) {
    extra_slot = -1;
    pyjit_enable();
    return PyModule_Create(&PyjitModule);
}