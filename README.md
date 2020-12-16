# Intro

This is an experimental JIT (Just-In-Time) compiler for Python 3. Which basically inlines opcodes replacing the big switch statement in CPython's [ceval.c](https://github.com/python/cpython/blob/master/Python/ceval.c).

# How it works

This code uses [PEP523](https://www.python.org/dev/peps/pep-0523/) API to inject a function before executing an evaluation frame.

This function will count the number of invocations a particular frame had and if it exceeds a threshold it will compile the evalutation frame will compile the bytecode to machine code using a C compiler.

The machine code is placed in a shared library that it is then loaded by the python runtime. Any further invocation to the evaluation frame will use the compiled version.

# Status

As of now, only a few opcodes are supported (enough to test the concept).

# License

MIT
