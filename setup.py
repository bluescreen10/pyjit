import subprocess
from setuptools import setup, find_packages, Extension
import sysconfig

setup(
    name="pyjit",
    version="0.0.1",
    description="CPython JIT module",
    author="Mariano Wahlmann",
    author_email="dichoso@gmail.com",
    ext_modules=[
        Extension(
            "pyjit.core",
            sources=["pyjit/core/pyjit.c", "pyjit/core/pyjitcomp.c"],
            include_dirs=["./include"],
        ),
    ],
    packages=find_packages(),
    setup_requires=["pytest-runner", "pycparser"],
    tests_require=["pytest"],
)
