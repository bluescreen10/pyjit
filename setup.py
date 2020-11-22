import subprocess
from setuptools import setup, find_packages, Extension
import sysconfig


def llvm_compile_args():
    cflags = subprocess.check_output(["llvm-config", "--cflags"]).decode("utf8").strip().split(" ")
    cflags.extend(["-Wno-nullability-completeness"])

    return cflags


setup(
    name="pyjit",
    version="0.0.1",
    description="CPython JIT module",
    author="Mariano Wahlmann",
    author_email="dichoso@gmail.com",
    packages=find_packages(),
    ext_modules=[Extension("pyjit", sources=["pyjit/pyjit.c", "pyjit/pyjitcomp.c"], include_dirs=["./include"])],
    # test_suite='setup.my_test_suite',
    setup_requires=[
        "pytest-runner",
    ],
    tests_require=[
        "pytest",
    ],
)
