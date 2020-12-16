import pyjit
from datetime import datetime


def test(n=1002):
    for _ in range(0, n):
        assert const_function() == True


def const_function():
    return True


if __name__ == "__main__":
    test()
    iterations = 10 ** 8

    start = datetime.now()
    test(iterations)
    ellapsed_pyjit = datetime.now() - start

    pyjit.disable()
    start = datetime.now()
    test(iterations)
    ellapsed_cpython = datetime.now() - start

    pct = (ellapsed_pyjit - ellapsed_cpython) / ellapsed_cpython * 100
    print(f"Pyjit: {ellapsed_pyjit} Cpython: {ellapsed_cpython} Change: {pct}")

    pyjit.enable()
    start = datetime.now()
    test(iterations)
    ellapsed_pyjit = datetime.now() - start
    pyjit.disable()

    start = datetime.now()
    test(iterations)
    ellapsed_cpython = datetime.now() - start

    pct = (ellapsed_pyjit - ellapsed_cpython) / ellapsed_cpython * 100
    print(f"Pyjit: {ellapsed_pyjit} Cpython: {ellapsed_cpython} Change: {pct}")
