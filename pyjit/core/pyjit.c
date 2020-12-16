#include "pyjit.h"

__thread _PyFrameEvalFunction orig_eval_frame;
__thread int extra_slot;
__thread pyjit_options_t pyjit_options;

void pyjit_free(void *object) {
  pyjit_header_t *header = (pyjit_header_t *)object;
  header->func = NULL;
  if (header->dlhandle != NULL) {
    dlclose(header->dlhandle);
  }
  PyMem_Free(header);
}

static inline pyjit_header_t *pyjit_ensure_extra(PyObject *code) {
  if (extra_slot == -1) {
    extra_slot = _PyEval_RequestCodeExtraIndex(pyjit_free);
    if (extra_slot == -1) {
      return NULL;
    }
  }
  pyjit_header_t *header = NULL;
  if (_PyCode_GetExtra(code, extra_slot, (void **)&header)) {
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
    header->is_completed = 0;
    header->dlhandle = NULL;
    if (_PyCode_SetExtra(code, extra_slot, header)) {
      PyErr_Clear();
      PyMem_Free(header);
      return NULL;
    }
  }

  return header;
}

static inline void pyjit_load_library(const char *filename,
                                      pyjit_header_t *co_extra) {

  void *handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
  if (handle == NULL) {
    fprintf(stderr, "Error dlopen %s %s\n", filename, dlerror());
    return;
  }

  pyjit_func func = dlsym(handle, "pyjit_func");
  if (func == NULL) {
    fprintf(stderr, "Error loading func %s\n", filename);
    dlclose(handle);
    return;
  }

  co_extra->func = func;
  co_extra->dlhandle = handle;
  // unlink(filename);
}

PyObject *__attribute__((flatten))
PyJit_EvalFrame(PyThreadState *tstate, PyFrameObject *frame, int throw) {
  pyjit_header_t *co_extra = pyjit_ensure_extra((PyObject *)frame->f_code);

  if (!co_extra->is_completed &&
      ++co_extra->call_count > pyjit_options.compile_threshold) {

    co_extra->is_completed = 1;
    const char *filename = pyjit_compile(frame, pyjit_options.temp_dir);

    if (filename) {
      pyjit_load_library(filename, co_extra);
    }
  }

  if (co_extra->func) {
    return co_extra->func(tstate, frame, throw);
  }

  return _PyEval_EvalFrameDefault(tstate, frame, throw);
}

static PyObject *pyjit_disable(void) {
  if (pyjit_options.is_enabled) {
    PyThreadState *ts = PyThreadState_GET();
    if (orig_eval_frame == NULL) {
      orig_eval_frame = _PyEval_EvalFrameDefault;
    }
    _PyInterpreterState_SetEvalFrameFunc(ts->interp, orig_eval_frame);
    printf("JIT compiler disabled\n");
    pyjit_options.is_enabled = 0;
  }
  return Py_None;
}

static PyObject *pyjit_enable(void) {
  if (!pyjit_options.is_enabled) {
    PyThreadState *ts = PyThreadState_GET();
    orig_eval_frame = _PyInterpreterState_GetEvalFrameFunc(ts->interp);
    _PyInterpreterState_SetEvalFrameFunc(ts->interp, PyJit_EvalFrame);
    printf("JIT compiler enabled\n");
    pyjit_options.is_enabled = 1;
  }
  return Py_None;
}

static PyObject *pyjit_compile_threshold(PyObject *self, PyObject *args) {
  unsigned int threshold;
  if (!PyArg_ParseTuple(args, "i", &threshold)) {
    return NULL;
  }
  pyjit_options.compile_threshold = threshold;
  printf("Compile threshold set to: %d\n", threshold);
  return PyLong_FromLong(threshold);
}

static PyMethodDef PyjitMethods[] = {
    {"disable", (PyCFunction)pyjit_disable, METH_VARARGS,
     "Disable the JIT compiler"},
    {"enable", (PyCFunction)pyjit_enable, METH_VARARGS,
     "Enable the JIT compiler"},
    {"compile_threshold", (PyCFunction)pyjit_compile_threshold, METH_VARARGS,
     "Set the JIT compilation threshold"},
    {NULL, NULL, 0, NULL}};

static struct PyModuleDef PyjitModule = {
    PyModuleDef_HEAD_INIT, "pyjit.core", "Python JIT module", -1, PyjitMethods,
};

PyMODINIT_FUNC PyInit_core(void) {
  extra_slot = -1;
  pyjit_options.is_enabled = false;
  pyjit_options.compile_threshold = DEFAULT_COMPILE_THRESHOLD;
  pyjit_options.temp_dir = "/tmp";

  pyjit_enable();
  return PyModule_Create(&PyjitModule);
}