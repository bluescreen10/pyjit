#ifndef PYJIT_H
#define PYJIT_H

#include <Python.h>
#include <frameobject.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <stddef.h>
#include <unistd.h>
#include "pyjitcomp.h" 


typedef PyObject* (*pyjit_func)(PyThreadState*, PyFrameObject*, int);

typedef struct {
    unsigned int call_count;
    bool is_completed;
    pyjit_func func;
    void* dlhandle;
} pyjit_header_t;
#endif
