#ifndef PYJIT_H
#define PYJIT_H

#include "pyjitcomp.h"
#include <Python.h>
#include <dlfcn.h>
#include <frameobject.h>
#include <stddef.h>
#include <unistd.h>

#define DEFAULT_COMPILE_THRESHOLD 1000

static PyObject *pyjit_disable(void);
static PyObject *pyjit_enable(void);
static PyObject *pyjit_compile_threshold(PyObject *, PyObject *);
typedef PyObject *(*pyjit_func)(PyThreadState *, PyFrameObject *, int);

typedef struct {
  int is_enabled;
  unsigned int compile_threshold;
  char *temp_dir;
} pyjit_options_t;
typedef struct {
  unsigned int call_count;
  int is_completed;
  pyjit_func func;
  void *dlhandle;
} pyjit_header_t;
#endif
