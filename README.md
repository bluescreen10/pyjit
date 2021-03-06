# Intro

This is a python module that provides an experimental JIT (Just-In-Time) compiler for Python 3. Which basically inlines opcodes replacing the big switch statement in CPython's [ceval.c](https://github.com/python/cpython/blob/master/Python/ceval.c) file.

# How it works

This code uses [PEP523](https://www.python.org/dev/peps/pep-0523/) API to inject a function before executing an evaluation frame.

This function will count the number of invocations a particular frame had and if it exceeds a threshold it will compile the evalutation frame's bytecode to machine code using a C compiler.

The machine code is placed in a shared library that it is then loaded by the runtime. Any further invocation to the evaluation frame will use the compiled version.

This project is heavily inspired by [Ruby's MJIT](https://github.com/ruby/ruby/pull/1782/files).

# Status

As of now, only a few opcodes are supported (enough to test the concept).

Is it faster? Well, it depends...

The JIT adds a little overhead to every frame evaluation to track number of invocations. This makes python slower. The compiled code is a little more efficient than Python's evaluation loop but not by much.

This JIT does not uses any advanced optimization techniques such as using native types instead of Python objects.

# How to use it

To install the module run

```
python setup.py install
```

Then on your program simply add

```python
import pyjit
```

# License

MIT
