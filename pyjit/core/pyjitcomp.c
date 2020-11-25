#include "pyjitcomp.h"

#define WRITE(args...) if (dprintf(args) < 0) return false;
#define EVAL(exp) result = exp; if (result == false) return false;
#define PUSH(val) "*sp++ = " val ";"
#define POP(val) val "= *--sp;"
#define PUSH_ACC() PUSH("acc")
#define POP_ACC() POP("acc")
#define TOP(val) val " = sp[-1];"
#define TOP_ACC() TOP("acc")
#define SET_TOP(val) "sp[-1] = " val ";"
#define SET_TOP_ACC() SET_TOP("acc")
#define STACK_SHRINK(val) "*sp += " val ";"

bool emit_header(int fd) {
    WRITE(fd,
        "#include <Python.h>\n"
        "#include <frameobject.h>\n"
        "PyObject* pyjit_func(PyThreadState* tstate, PyFrameObject* frame, int throw) {"
        "PyObject* consts = frame->f_code->co_consts;"
        "PyObject** sp = frame->f_stacktop;"
        "PyObject** fastlocals = frame->f_localsplus;"
        "PyObject* acc;"
    );

    //printf("Offset %d\n", offsetof(PyFrameObject, f_valuestack));
    return true;
}

bool emit_footer(int fd) {
    WRITE(fd, "}");
    return true;
}

bool emit_load_const(int fd, int oparg) {
    WRITE(fd,
        "acc = PyTuple_GetItem(consts, %d);"
        "Py_INCREF(acc);"
        PUSH_ACC(),
        oparg);
    return true;
}

bool emit_return_value(int fd) {
    WRITE(fd,
        POP_ACC()
        "return acc;");
    return true;
}

bool emit_store_fast(int fd, int oparg) {
    WRITE(fd,
        POP_ACC()
        "Py_XDECREF(fastlocals[%d]);"
        "fastlocals[%d] = acc;", oparg, oparg);
    return true;
}

bool emit_load_fast(int fd, int oparg) {
    WRITE(fd,
        "acc = fastlocals[%d];"
        "Py_INCREF(acc);"
        PUSH_ACC(), oparg);
    return true;
}

bool emit_unary_not(int fd) {
    WRITE(fd,
        "{"
        TOP_ACC()
        "  Py_DECREF(acc);"
        "  int err = PyObject_IsTrue(acc);"
        "  if (err == 0) {"
        "    Py_INCREF(Py_True);"
        SET_TOP("Py_True")
        "  } else if (err > 0) {"
        "    Py_INCREF(Py_False);"
        SET_TOP("Py_False")
        "  }"
        STACK_SHRINK("1")
        "}");
    //TODO: Goto error
    return true;
}

bool emit_load_global(int fd, int oparg) {
    WRITE(fd,
        "{ PyObject *name = PyTuple_GetItem(frame->f_code->co_names, %d);"
        "if (PyDict_CheckExact(frame->f_globals) && PyDict_CheckExact(frame->f_builtins)) {"
        "   acc = _PyDict_LoadGlobal((PyDictObject *)frame->f_globals,"
        "                           (PyDictObject *)frame->f_builtins,"
        "                            name);"
        // TOOD: Error handling
        "} else {"
        "  acc = PyObject_GetItem(frame->f_globals, name);"
        // TODO: Error handling
        "  if (acc == NULL) {"
        "    acc = PyObject_GetItem(frame->f_builtins, name);"
        // TODO: Error handling
        "  }"
        "}"
        "Py_INCREF(acc);"
        PUSH_ACC()
        "}"
        , oparg);
    return true;
}

bool emit_call_function(int fd, int oparg) {
    WRITE(fd,
        "{"
        "  PyObject **pfunc = (sp) - %d - 1;"
        "  PyObject *func = *pfunc;"
        "  PyObject **stack = (sp) - %d;"
        "  PyObject* res = PyObject_Vectorcall(func, stack, %d | PY_VECTORCALL_ARGUMENTS_OFFSET, NULL);"
        //TODO: Error handling
        "  while(sp > pfunc) {"
        POP_ACC()
        "  Py_DECREF(acc);"
        " }"
        PUSH("res")
        "}"
        //TODO: Error handling
        , oparg, oparg, oparg
    );
    return true;
}

bool emit_body(int fd, PyCodeObject* code) {
    bool result;
    _Py_CODEUNIT* bytecode = (_Py_CODEUNIT*)PyBytes_AS_STRING(code->co_code);
    int bytecode_size = PyBytes_Size(code->co_code) / sizeof(_Py_CODEUNIT);

    for (int pc = 0; pc < bytecode_size; pc++) {
        _Py_CODEUNIT unit = bytecode[pc];
        int opcode = _Py_OPCODE(unit);
        int oparg = _Py_OPARG(unit);

        switch (opcode) {
        case LOAD_CONST:
            EVAL(emit_load_const(fd, oparg));
            break;
        case RETURN_VALUE:
            EVAL(emit_return_value(fd));
            break;
        case STORE_FAST:
            EVAL(emit_store_fast(fd, oparg));
            break;
        case LOAD_FAST:
            EVAL(emit_load_fast(fd, oparg));
            break;
        case UNARY_NOT:
            EVAL(emit_unary_not(fd));
            break;
        case LOAD_GLOBAL:
            EVAL(emit_load_global(fd, oparg));
            break;
        case CALL_FUNCTION:
            EVAL(emit_call_function(fd, oparg));
            break;
        default:
            return false;
        }
    }
    return true;
}

bool pyjit_compile(PyCodeObject* code) {
    bool result;
    char filename[] = "funcXXXXXX.c";

    int fd = mkstemps(filename, 2);
    if (fd == -1) {
        printf("mkstemp error\n");
        return false;
    }

    EVAL(emit_header(fd));
    EVAL(emit_body(fd, code));
    EVAL(emit_footer(fd));
    close(fd);

    char* args[] = { "-v", "-Os","-DNDEBUG", "-Wall", "-shared", "-undefined", "dynamic_lookup",
        "-I/Users/mariano.wahlmann/.pyenv/versions/3.9.0/include/python3.9", "-o", "func.so", filename, NULL };
    exec_process("/usr/bin/clang", args);
    unlink(filename);
    return true;
}

static int
exec_process(char* path, char* argv[]) {
    pid_t pid = vfork();
    if (pid == 0) {
        pid = execvp(path, argv);
        exit(0);
    }
    wait(&pid);
    return 0;
}
// 2           0 LOAD_GLOBAL              0 (b)
//2 CALL_FUNCTION            0
// 4 RETURN_VALUE