#include <Python.h>
#include <stdbool.h>
#include <dlfcn.h>

#define COMPILE_THRESHOLD 1000
PyAPI_FUNC(PyCodeObject*) PyFrame_GetBack(PyFrameObject* frame);

_PyFrameEvalFunction orig_eval_frame;

typedef PyObject* (*call_function)(PyThreadState*, PyFrameObject*, PyCodeObject*, int);

struct PyJIT_Info {
    unsigned int call_count; /* function call count */
    bool compile_failed;
    call_function func; /* compilation failed */
};

struct PyJIT_Info* PyJit_create_info() {
    struct PyJIT_Info* info = (struct PyJIT_Info*)malloc(sizeof(struct PyJIT_Info));
    info->call_count = 1;
    info->func = NULL;
    return info;
}

bool pyjit_compile(PyCodeObject* code) {
    char* opcodes = PyBytes_AS_STRING(code->co_code);
    unsigned int opcodes_size = PyBytes_Size(code->co_code);
    for (unsigned int i = 0; i < opcodes_size; i++) {
        printf("%d\n", opcodes[i]);
    }
    return true;
}



PyObject* PyJit_EvalFrame(PyThreadState* tstate, PyFrameObject* frame, int throw) {
    //printf("Calling function %s\n", PyUnicode_AsUTF8(PyFrame_GetCode(frame)->co_name));
    //printf("file: %s\n", PyUnicode_AsUTF8(PyFrame_GetCode(frame)->co_filename));
    PyCodeObject* code = PyFrame_GetCode(frame);
    struct PyJIT_Info* co_extra;

    if (code->co_extra == NULL) {
        co_extra = PyJit_create_info();
        code->co_extra = (void*)co_extra;
    }
    else {
        co_extra = (struct PyJIT_Info*)code->co_extra;
        if (co_extra->func != NULL) {
            return co_extra->func(tstate, frame, code, throw);
        }
        else {
            co_extra->call_count += 1;
        }
    }

    if (!co_extra->compile_failed && co_extra->call_count > COMPILE_THRESHOLD) {
        //co_extra->compile_failed = pyjit_compile(code);
        printf("Loading function\n");
        void* handle = dlopen("./function.so", RTLD_NOW);
        call_function func = dlsym(handle, "call_function");
        co_extra->func = func;
    }

    return orig_eval_frame(tstate, frame, throw);
}




static struct PyModuleDef PyjitModule = {
    PyModuleDef_HEAD_INIT,
    "pyjit",
    "Python JIT module",
    -1,
    NULL };

PyMODINIT_FUNC PyInit_pyjit(void) {
    printf("Initializing Module PyJIT\n");
    PyThreadState* ts = PyThreadState_GET();
    orig_eval_frame = _PyInterpreterState_GetEvalFrameFunc(ts->interp);
    _PyInterpreterState_SetEvalFrameFunc(ts->interp, PyJit_EvalFrame);
    return PyModule_Create(&PyjitModule);
}