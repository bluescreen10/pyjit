#include "pyjitcomp.h"

#define PC_LABEL(PC) PC *(int)sizeof(_Py_CODEUNIT)

int emit_header(int fd, PyFrameObject *frame) {
  if (dprintf(fd,
              "// %s: %s \n"
              "#include <Python.h>\n"
              "#include <frameobject.h>\n"
              "PyObject* pyjit_func(PyThreadState* tstate, PyFrameObject* f, "
              "int throw) {\n"
              "tstate->frame = f;\n"
              "PyObject** sp = f->f_stacktop;\n"
              "PyObject* retval;\n",
              PyUnicode_AsUTF8(frame->f_code->co_name),
              PyUnicode_AsUTF8(frame->f_code->co_filename)) == -1) {
    return 0;
  };

  return 1;
}

int emit_load_const(int fd, int pc, int oparg, PyFrameObject *frame) {
  PyObject *v = PyTuple_GetItem(frame->f_code->co_consts, oparg);
  if (dprintf(fd,
              "label_%d: /* load_const */\n"
              "{\n"
              //"  PyObject *v = PyTuple_GetItem(f->f_code->co_consts, %d);\n"
              "  PyObject *v = (PyObject *)%p;\n"
              "  Py_INCREF(v);\n"
              "  *sp++ = v;\n"
              "}\n",
              PC_LABEL(pc), v) == -1)
    return 0;
  return 1;
}

int emit_return_value(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* return_value */\n"
              "{\n"
              "  retval = *--sp;\n"
              "  f->f_executing = 0;\n"
              "  goto exiting;\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_unary_not(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* unary_not */\n"
              "{\n"
              "  PyObject *v = sp[-1];\n"
              "  int err = PyObject_IsTrue(v);\n"
              "  Py_DECREF(v);\n"
              "  if (err == 0) {\n"
              "    Py_INCREF(Py_True);\n"
              "    sp[-1] = Py_True;\n"
              "  } else if (err > 0) {\n"
              "    Py_INCREF(Py_False);\n"
              "    sp[-1] = Py_False;\n"
              "  } else {\n"
              "    *sp--;\n"
              // ERROR
              "  }\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_load_fast(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* load_fast */\n"
              "{\n"
              "  PyObject **fastlocals = f->f_localsplus;\n"
              "  PyObject *v = fastlocals[%d];\n"
              "  Py_INCREF(v);\n"
              "  *sp++ = v;\n"
              "}\n",
              PC_LABEL(pc), oparg) == -1)
    return 0;
  return 1;
}
int emit_load_global(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (PyDict_CheckExact(frame->f_globals) &&
      PyDict_CheckExact(frame->f_builtins)) {
    if (dprintf(
            fd,
            "label_%d: /* load_global */\n"
            "{\n"
            "  static uint64_t globals_ver = 0;\n"
            "  static uint64_t builtins_ver = 0;\n"
            "  static PyObject *v = NULL;\n"
            "  uint64_t cur_globals_ver = ((PyDictObject *)f->f_globals)\n"
            "                             ->ma_version_tag;\n"
            "  uint64_t cur_builtins_ver = ((PyDictObject *)f->f_builtins)\n"
            "                              ->ma_version_tag;\n"
            "  if (cur_globals_ver != globals_ver || \n"
            "      cur_builtins_ver != builtins_ver) {\n"
            "    PyObject *name = PyTuple_GetItem(f->f_code->co_names, %d);\n"
            "    v = PyDict_GetItem(f->f_globals, name);\n"
            "    if (v == NULL) {\n"
            "      if (!PyErr_ExceptionMatches(PyExc_KeyError)) {\n"
            "         printf(\"error here1\\n\");//Error Handling\n"
            "      }\n"
            "      PyErr_Clear();\n"
            "      v = PyDict_GetItem(f->f_builtins, name);\n"
            "      if (v == NULL) {\n"
            "         printf(\"error here2\\n\"); //Error Handling\n"
            "      }\n"
            "    }\n"
            "    globals_ver = cur_globals_ver;\n"
            "    builtins_ver = cur_builtins_ver;\n"
            "  }\n"
            "  Py_INCREF(v);\n"
            "  *sp++ = v;\n"
            "}\n",
            PC_LABEL(pc), oparg) == -1)
      return 0;
  } else {
    if (dprintf(fd,
                "label_%d: /* load_global */\n"
                "{\n"
                "  PyObject *name = PyTuple_GetItem(f->f_code->co_names, %d);\n"
                "  PyObject *v = PyObject_GetItem(f->f_globals, name);\n"
                "  if (v == NULL) {\n"
                "    if (!PyErr_ExceptionMatches(PyExc_KeyError)) {\n"
                "       //Error Handling\n"
                "    }\n"
                "    PyErr_Clear();\n"
                "    v = PyObject_GetItem(f->f_builtins, name);\n"
                "    if (v == NULL) {\n"
                "      // Error Handling\n"
                "    }\n"
                "  }\n"
                "  Py_INCREF(v);\n"
                "  *sp++ = v;\n"
                "}\n",
                PC_LABEL(pc), oparg) == -1)
      return 0;
  }
  return 1;
}

int emit_call_function(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* call_function */\n"
              "{\n"
              "  PyObject **pfunc = (sp) - %d - 1;\n"
              "  PyObject *func = *pfunc;\n"
              "  PyObject **stack = (sp) - %d;\n"
              "  PyObject* res = PyObject_Vectorcall(func, stack, %d | "
              "PY_VECTORCALL_ARGUMENTS_OFFSET, NULL);\n"
              "  while(sp > pfunc) {\n"
              "    PyObject *v = *--sp;\n"
              "    Py_DECREF(v);\n"
              "  }\n"
              "  *sp++ = res;\n"
              "  if (res == NULL) {\n"
              "    printf(\"error here3\\n\"); //Error Handling\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc), oparg, oparg, oparg) == -1)
    return 0;
  return 1;
}

int emit_store_fast(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* store_fast */\n"
              "{\n"
              "  PyObject **fastlocals = f->f_localsplus;\n"
              "  PyObject *v = *--sp;\n"
              "  PyObject *temp = fastlocals[%d];\n"
              "  Py_XDECREF(temp);\n"
              "  fastlocals[%d] = v;\n"
              "}\n",
              PC_LABEL(pc), oparg, oparg) == -1)
    return 0;
  return 1;
}

int emit_unpack_sequence(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* unpack_sequenece */\n"
              "{\n"
              "  PyObject *seq = *--sp;\n"
              "  PyObject **items;\n"
              "  PyObject *v;\n"
              "  int index;\n"
              "  if (PyTuple_CheckExact(seq) &&\n"
              "      PyTuple_GET_SIZE(seq) == %d) {\n"
              "    items = ((PyTupleObject *)seq)->ob_item;\n"
              "    for(index = %d; index--;) {\n"
              "      v = items[index];\n"
              "      Py_INCREF(v);\n"
              "      *sp++ = v;\n"
              "    }\n"
              "  }\n"
              "  else if (PyList_CheckExact(seq) &&\n"
              "      PyList_GET_SIZE(seq) == %d) {\n"
              "    items = ((PyListObject *)seq)->ob_item;\n"
              "    for(index = %d; index--;) {\n"
              "      v = items[index];\n"
              "      Py_INCREF(v);\n"
              "      *sp++ = v;\n"
              "  }\n"
              "  //else if(unpack_iterable(tstate, seq, oparg, -1)"
              "  else {\n"
              "    Py_DECREF(seq);"
              "    // Error handling\n"
              "  }\n"
              "  Py_DECREF(seq);\n"
              "}\n",
              PC_LABEL(pc), oparg, oparg, oparg, oparg) == -1)
    return 0;
  return 1;
}

int emit_binary_add(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* binary_add */\n"
              "{\n"
              "  PyObject *a = *--sp;\n"
              "  PyObject *b = sp[-1];\n"
              "  PyObject *res;"
              "  if (PyUnicode_CheckExact(b) && \n"
              "      PyUnicode_CheckExact(a)) {\n"
              "    //res = unicode_concatenate(tstate, b, a, f, next_instr);\n"
              "  }\n"
              "  else {\n"
              "    res = PyNumber_Add(b, a);\n"
              "    Py_DECREF(b);\n"
              "  }\n"
              "  Py_DECREF(a);\n"
              "  sp[-1] = res;\n"
              "  if (res == NULL) {\n"
              "    fprintf(stderr,\"err:binary_add\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_binary_floor_divide(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* binary_floor_divide */\n"
              "{\n"
              "  PyObject *a = *--sp;\n"
              "  PyObject *b = sp[-1];\n"
              "  PyObject *res = PyNumber_FloorDivide(b,a);\n"
              "  Py_DECREF(b);\n"
              "  Py_DECREF(a);\n"
              "  sp[-1] = res;\n"
              "  if (res == NULL) {\n"
              "    fprintf(stderr,\"err:binary_floor_divide\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_dup_top_two(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* dup_top_two */\n"
              "{\n"
              "  PyObject *a = sp[-1];\n"
              "  PyObject *b = sp[-2];\n"
              "  Py_INCREF(a);\n"
              "  Py_INCREF(b);\n"
              "  *sp++ = b;\n"
              "  *sp++ = a;\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_binary_subscr(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* binary_subscr */\n"
              "{\n"
              "  PyObject *a = *--sp;\n"
              "  PyObject *b = sp[-1];\n"
              "  PyObject *res = PyObject_GetItem(b, a);\n"
              "  Py_DECREF(b);\n"
              "  Py_DECREF(a);\n"
              "  sp[-1] = res;\n"
              "  if (res == NULL) {\n"
              "    fprintf(stderr,\"err:binary_subscr\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_binary_modulo(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* binary_modulo */\n"
              "{\n"
              "  PyObject *a = *--sp;\n"
              "  PyObject *b = sp[-1];\n"
              "  PyObject *res;"
              "  if (PyUnicode_CheckExact(b) && \n"
              "      !PyUnicode_Check(a) || PyUnicode_CheckExact(a)) {\n"
              "    res = PyUnicode_Format(b, a);\n"
              "  }\n"
              "  else {\n"
              "    res = PyNumber_Remainder(b, a);\n"
              "  }\n"
              "  Py_DECREF(a);\n"
              "  Py_DECREF(b);\n"
              "  sp[-1] = res;\n"
              "  if (res == NULL) {\n"
              "    fprintf(stderr,\"err:binary_modulo\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_binary_subtract(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* binary_subtract */\n"
              "{\n"
              "  PyObject *a = *--sp;\n"
              "  PyObject *b = sp[-1];\n"
              "  PyObject *res = PyNumber_Subtract(b, a);\n"
              "  Py_DECREF(a);\n"
              "  Py_DECREF(b);\n"
              "  sp[-1] = res;\n"
              "  if (res == NULL) {\n"
              "    fprintf(stderr,\"err:binary_subtract\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_binary_lshift(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* binary_lshift */\n"
              "{\n"
              "  PyObject *a = *--sp;\n"
              "  PyObject *b = sp[-1];\n"
              "  PyObject *res = PyNumber_Lshift(b, a);\n"
              "  Py_DECREF(b);\n"
              "  Py_DECREF(a);\n"
              "  sp[-1] = res;\n"
              "  if (res == NULL) {\n"
              "    fprintf(stderr,\"err:binary_lshift\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_inplace_add(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* inplace_add */\n"
              "{\n"
              "  PyObject *a = *--sp;\n"
              "  PyObject *b = sp[-1];\n"
              "  PyObject *res;"
              "  if (PyUnicode_CheckExact(b) && \n"
              "      PyUnicode_CheckExact(a)) {\n"
              "    //res = unicode_concatenate(tstate, b, a, f, next_instr);\n"
              "  }\n"
              "  else {\n"
              "    res = PyNumber_InPlaceAdd(b, a);\n"
              "    Py_DECREF(b);\n"
              "  }\n"
              "  Py_DECREF(a);\n"
              "  sp[-1] = res;\n"
              "  if (res == NULL) {\n"
              "    fprintf(stderr,\"err:inplace_add\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_rot_three(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* rot_three */\n"
              "{\n"
              "  PyObject *a = sp[-1];\n"
              "  PyObject *b = sp[-2];\n"
              "  PyObject *c = sp[-3];\n"
              "  sp[-1] = b;\n"
              "  sp[-2] = c;\n"
              "  sp[-3] = a;\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_store_subscr(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* store_subscr */\n"
              "{\n"
              "  PyObject *a = sp[-1];\n"
              "  PyObject *b = sp[-2];\n"
              "  PyObject *c = sp[-3];\n"
              "  int err;\n"
              "  sp -= 3;\n"
              "  err = PyObject_SetItem(b, a, c);\n"
              "  Py_DECREF(c);\n"
              "  Py_DECREF(b);\n"
              "  Py_DECREF(a);\n"
              "  if (err != 0) {\n"
              "    fprintf(stderr,\"err:store_subscr\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_build_tuple(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* build_tuple */\n"
              "{\n"
              "  PyObject *v = PyTuple_New(%d);\n"
              "  int index;\n"
              "  if (v == NULL) {\n"
              "    fprintf(stderr,\"err:build_tuple\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "  for(index = %d; index--;) {\n"
              "    PyObject *item = *--sp;\n"
              "    PyTuple_SET_ITEM(v, index, item);\n"
              "  }\n"
              "  *sp++ = v;\n"
              "}\n",
              PC_LABEL(pc), oparg, oparg) == -1)
    return 0;
  return 1;
}

int emit_inplace_and(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* inplace_and */\n"
              "{\n"
              "  PyObject *a = *--sp;\n"
              "  PyObject *b = sp[-1];\n"
              "  PyObject *res = PyNumber_InPlaceAnd(b, a);\n"
              "  Py_DECREF(b);\n"
              "  Py_DECREF(a);\n"
              "  sp[-1] = res;\n"
              "  if (res == NULL) {\n"
              "    fprintf(stderr,\"err:inplace_and\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc)) == -1)
    return 0;
  return 1;
}

int emit_store_attr(int fd, int pc, int oparg, PyFrameObject *frame) {
  if (dprintf(fd,
              "label_%d: /* store_attr */\n"
              "{\n"
              "  PyObject *name = PyTuple_GetItem(f->f_code->co_names, %d);\n"
              "  PyObject *a = *--sp;\n"
              "  PyObject *b = *--sp;\n"
              "  int err = PyObject_SetAttr(a, name, b);\n"
              "  Py_DECREF(b);\n"
              "  Py_DECREF(a);\n"
              "  if (err != 0) {\n"
              "    fprintf(stderr,\"err:store_attr\\n\");\n"
              "    goto error;\n"
              "  }\n"
              "}\n",
              PC_LABEL(pc), oparg) == -1)
    return 0;
  return 1;
}

int emit_footer(int fd) {
  if (dprintf(fd, "error:\n"
                  "fprintf(stderr,\"ERROR!\\n\");\n"
                  "exiting:\n"
                  "tstate->frame = f->f_back;\n"
                  "return retval;\n"
                  "}\n") == -1) {
    return 0;
  };
  return 1;
}

int emit_body(int fd, PyFrameObject *frame) {
  PyCodeObject *code = frame->f_code;
  _Py_CODEUNIT *bytecode = (_Py_CODEUNIT *)PyBytes_AS_STRING(code->co_code);
  int bytecode_size = PyBytes_Size(code->co_code) / sizeof(_Py_CODEUNIT);

  for (int pc = 0; pc < bytecode_size; pc++) {
    _Py_CODEUNIT unit = bytecode[pc];
    int opcode = _Py_OPCODE(unit);
    int oparg = _Py_OPARG(unit);

    switch (opcode) {
    case LOAD_CONST:
      if (emit_load_const(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case RETURN_VALUE:
      if (emit_return_value(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case UNARY_NOT:
      if (emit_unary_not(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case LOAD_FAST:
      if (emit_load_fast(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case STORE_FAST:
      if (emit_store_fast(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case LOAD_GLOBAL:
      if (emit_load_global(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case CALL_FUNCTION:
      if (emit_call_function(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case BINARY_ADD:
      if (emit_binary_add(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case UNPACK_SEQUENCE:
      if (emit_unpack_sequence(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case BINARY_FLOOR_DIVIDE:
      if (emit_binary_floor_divide(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case DUP_TOP_TWO:
      if (emit_dup_top_two(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case BINARY_MODULO:
      if (emit_binary_modulo(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case BINARY_SUBTRACT:
      if (emit_binary_subtract(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case BINARY_LSHIFT:
      if (emit_binary_lshift(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case BINARY_SUBSCR:
      if (emit_binary_subscr(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case INPLACE_ADD:
      if (emit_inplace_add(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case INPLACE_AND:
      if (emit_inplace_and(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case ROT_THREE:
      if (emit_rot_three(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case STORE_SUBSCR:
      if (emit_store_subscr(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case STORE_ATTR:
      if (emit_store_attr(fd, pc, oparg, frame) == 0)
        return 0;
      break;
    case BUILD_TUPLE:
      if (emit_build_tuple(fd, pc, oparg, frame) == 0)
        return 0;
      break;

    default:
      fprintf(stderr, "Unimplemented opcode %d\n\n", opcode);
      return 0;
    }
  }
  fprintf(stderr, "Finished compilation\n\n");
  return 1;
}

const char *pyjit_compile(PyFrameObject *frame, const char *tempdir) {
  char filename[] = "/tmp/funcXXXXXX.c";
  char library[] = "/tmp/funcXXXXXX.dylib";

  int fd = mkstemps(filename, 2);
  if (fd == -1) {
    printf("mkstemp error\n");
    return NULL;
  }
  strncpy(library + 9, filename + 9, 6);

  if (!emit_header(fd, frame)) {
    close(fd);
    unlink(filename);
    return NULL;
  };

  if (!emit_body(fd, frame)) {
    printf("Error body\n");

    close(fd);
    unlink(filename);
    return NULL;
  }

  if (!emit_footer(fd)) {
    close(fd);
    unlink(filename);
    return NULL;
  };

  char *args[] = {"-O2", "-I./include", "-fPIC", "-shared",
                  //"-dynamiclib",
                  "-I~/.pyenv/versions/3.9.0/include/python3.9",
                  "-L~/.pyenv/versions/3.9.0/lib",
                  //"-lpython3.9",
                  "-Wl,-export_dynamic", "-Wl,-undefined", "-Wl,dynamic_lookup",
                  "-w", "-o", library, filename, NULL};
  exec_process("/usr/bin/clang", args);
  // unlink(filename);
  char *file = malloc(strlen(library) + 1);
  strcpy(file, library);
  return file;
}

static int exec_process(char *path, char *argv[]) {
  pid_t pid = vfork();
  if (pid == 0) {
    pid = execvp(path, argv);
    exit(0);
  }
  wait(&pid);
  return 0;
}
