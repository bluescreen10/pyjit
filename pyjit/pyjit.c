#include "pyjit.h"

#define COMPILE_THRESHOLD 1000

__thread _PyFrameEvalFunction orig_eval_frame;
__thread int extra_slot;


void pyjit_free(void* object) {
    pyjit_header_t* header = (pyjit_header_t*)object;
    header->func = NULL;
    dlclose(header->dlhandle);
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
        if (co_extra->call_count > COMPILE_THRESHOLD) {
            co_extra->is_completed = true;
            pyjit_compile(frame->f_code);
            void* handle = dlopen("./func.so", RTLD_NOW | RTLD_LOCAL);
            pyjit_func func = dlsym(handle, "pyjit_func");
            unlink("./func.so");
            printf("function pointer: %p\n", func);
            co_extra->func = func;
            co_extra->dlhandle = handle;
        }
        else {
            co_extra->call_count++;
        }
    }

    //Py_DECREF(code);
    return orig_eval_frame(tstate, frame, throw);
}
static struct PyModuleDef PyjitModule = {
    PyModuleDef_HEAD_INIT,
    "pyjit",
    "Python JIT module",
    -1,
    NULL
};

PyMODINIT_FUNC PyInit_pyjit(void) {
    printf("Initializing Module PyJIT\n");
    PyThreadState* ts = PyThreadState_GET();
    extra_slot = -1;
    orig_eval_frame = _PyInterpreterState_GetEvalFrameFunc(ts->interp);
    _PyInterpreterState_SetEvalFrameFunc(ts->interp, PyJit_EvalFrame);
    return PyModule_Create(&PyjitModule);
}