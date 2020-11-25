#ifndef PYJIT_H
#define PYJIT_H

#include <Python.h>
#include <frameobject.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <stddef.h>
#include <unistd.h>
#include "pyjitcomp.h" 

#define DEFAULT_COMPILE_THRESHOLD 1000

static PyObject* pyjit_disable(void);
static PyObject* pyjit_enable(void);
static PyObject* pyjit_compile_threshold(PyObject*, PyObject*);
typedef PyObject* (*pyjit_func)(PyThreadState*, PyFrameObject*, int);

typedef struct {
    unsigned int call_count;
    bool is_completed;
    pyjit_func func;
    void* dlhandle;
} pyjit_header_t;
#endif
